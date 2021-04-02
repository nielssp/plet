/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "contentmap.h"

#include "build.h"
#include "html.h"
#include "interpreter.h"
#include "parser.h"
#include "reader.h"
#include "strings.h"

#include <alloca.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct PathStack PathStack;

struct PathStack {
   PathStack *next;
   PathStack *prev;
   char *name;
};

static void read_front_matter(Object *obj, FILE *file, const Path *path, Env *env);
static Value read_file_content(Object *obj, FILE *file, const Path *path, Env *env);
static Value parse_content(Value content, const Path *path, Env *env);

static Value path_stack_to_string(PathStack *path_stack, Arena *arena) {
  if (!path_stack) {
    return copy_c_string(".", arena);
  }
  while (path_stack->prev) {
    path_stack = path_stack->prev;
  }
  StringBuffer buffer = create_string_buffer(0, arena);
  while (path_stack) {
    string_buffer_printf(&buffer, path_stack->name);
    path_stack = path_stack->next;
    if (path_stack) {
      string_buffer_put(&buffer, PATH_SEP);
    }
  }
  return finalize_string_buffer(buffer);
}

typedef struct {
  Env *env;
  const Path *asset_base;
} ContentLinkArgs;

static int is_url(String *string) {
  return string_starts_with("//", string) || memchr(string->bytes, ':', string->size) != NULL;
}

static int transform_content_link(Value node, const char *attribute_name, ContentLinkArgs *args) {
  Value src = html_get_attribute(node, attribute_name);
  if (src.type == V_STRING) {
    if (!is_url(src.string_value)) {
      Path *path = string_to_path(src.string_value);
      if (path_is_absolute(path)) {
        StringBuffer buffer = create_string_buffer(path->size + sizeof("tsclink:") - 1, args->env->arena);
        string_buffer_printf(&buffer, "tsclink:%s", path->path);
        html_set_attribute(node, attribute_name, finalize_string_buffer(buffer).string_value, args->env);
      } else  {
        Path *asset_path = path_join(args->asset_base, path, 1);
        if (path_is_descending(asset_path)) {
          StringBuffer buffer = create_string_buffer(asset_path->size + sizeof("tscasset:") - 1, args->env->arena);
          string_buffer_printf(&buffer, "tscasset:%s", asset_path->path);
          html_set_attribute(node, attribute_name, finalize_string_buffer(buffer).string_value, args->env);
        }
        delete_path(asset_path);
      }
      delete_path(path);
      return 1;
    }
  }
  return 0;
}

static HtmlTransformation transform_content_links(Value node, void *context) {
  ContentLinkArgs *args = context;
  if (!transform_content_link(node, "src", args)) {
    transform_content_link(node, "href", args);
  }
  return HTML_NO_ACTION;
}

typedef struct {
  Env *env;
  const Path *src_file;
  const Path *dir;
} ContentIncludeArgs;

static HtmlTransformation transform_content_includes(Value node, void *context) {
  ContentIncludeArgs *args = context;
  if (node.type == V_OBJECT) {
    Value comment;
    if (object_get_symbol(node.object_value, "comment", &comment) && comment.type == V_STRING) {
      if (string_starts_with("include:", comment.string_value)) {
        Path *path = create_path((char *) comment.string_value->bytes + sizeof("include:") - 1,
            comment.string_value->size - sizeof("include:") + 1);
        Path *abs_path = path_join(args->dir, path, 1);
        Value replacement = create_string(NULL, 0, args->env->arena);
        FILE *file = fopen(abs_path->path, "r");
        if (file) {
          Value front_matter = create_object(0, args->env->arena);
          Value type = copy_c_string(path_get_extension(abs_path), args->env->arena);
          object_def(front_matter.object_value, "type", type, args->env);
          read_front_matter(front_matter.object_value, file, abs_path, args->env);
          Value content = read_file_content(front_matter.object_value, file, abs_path, args->env);
          fclose(file);
          replacement = parse_content(content, abs_path, args->env);
        } else {
          html_error(node, args->src_file, "include failed: %s: %s", path->path, strerror(errno));
        }
        delete_path(abs_path);
        delete_path(path);
        return HTML_REPLACE(replacement);
      }
    }
  }
  return HTML_NO_ACTION;
}

static int has_read_more(Value node) {
  if (node.type == V_OBJECT) {
    Value comment;
    if (object_get_symbol(node.object_value, "comment", &comment) && comment.type == V_STRING) {
      return string_equals("more", comment.string_value);
    } else {
      Value children;
      if (object_get_symbol(node.object_value, "children", &children) && children.type == V_ARRAY) {
        for (size_t i = 0; i < children.array_value->size; i++) {
          if (has_read_more(children.array_value->cells[i])) {
            return 1;
          }
        }
      }
    }
  }
  return 0;
}

static Array *toc_get_section(Array *toc, int level, String **number, Env *env) {
  if (!toc->size || level <= 0) {
    return toc;
  }
  Object *last_entry = toc->cells[toc->size - 1].object_value;
  Value children;
  if (!object_get_symbol(last_entry, "children", &children)) {
    children = create_array(0, env->arena);
    object_def(last_entry, "children", children, env);
  }
  Value number_value;
  if (object_get_symbol(last_entry, "number", &number_value) && number_value.type == V_STRING) {
    *number = number_value.string_value;
  }
  return toc_get_section(children.array_value, level - 1, number, env);
}

static Value slugify(String *string, Arena *arena) {
  StringBuffer buffer = create_string_buffer(string->size, arena);
  for (size_t i = 0; i < string->size; i++) {
    uint8_t byte = string->bytes[i];
    if ((byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'z')) {
      string_buffer_put(&buffer, byte);
    } else if (byte >= 'A' && byte <= 'Z') {
      string_buffer_put(&buffer, tolower(byte));
    } else if (byte == '-' || byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r') {
      if (buffer.string->size && buffer.string->bytes[buffer.string->size - 1] != '-') {
        string_buffer_put(&buffer, '-');
      }
    }
  }
  return finalize_string_buffer(buffer);
}

static void build_toc(Value node, Array *toc, int min_level, int max_level, int numbered_headings,
    String *sep1, String *sep2, Env *env) {
  if (node.type == V_OBJECT) {
    Value node_tag;
    if (object_get_symbol(node.object_value, "tag", &node_tag) && node_tag.type == V_SYMBOL) {
      if (node_tag.symbol_value[0] == 'h' && node_tag.symbol_value[1] >= '1'
          && node_tag.symbol_value[1] <= '6' && node_tag.symbol_value[2] == '\0') {
        int level = node_tag.symbol_value[1] - '0';
        if (level >= min_level && level <= max_level && html_get_attribute(node, "data-toc-ignore").type == V_NIL) {
          String *parent_number = NULL;
          Array *section = toc_get_section(toc, level - min_level, &parent_number, env);
          Value entry = create_object(0, env->arena);
          StringBuffer buffer = create_string_buffer(0, env->arena);
          html_text_content(node, &buffer);
          Value title = string_trim(finalize_string_buffer(buffer).string_value, (uint8_t *) " \r\n\t", 4, env->arena);
          if (numbered_headings >= level) {
            StringBuffer number_buffer = create_string_buffer(0, env->arena);
            if (parent_number) {
              string_buffer_append(&number_buffer, parent_number);
              string_buffer_append(&number_buffer, sep1);
            }
            string_buffer_printf(&number_buffer, "%zd", section->size + 1);
            Value number = finalize_string_buffer(number_buffer);
            object_def(entry.object_value, "number", number, env);
            html_prepend_child(node, (Value) { .type = V_STRING, .string_value = sep2 }, env->arena);
            html_prepend_child(node, number, env->arena);
          }
          object_def(entry.object_value, "title", title, env);
          Value id = html_get_attribute(node, "id");
          if (id.type != V_STRING) {
            id = slugify(title.string_value, env->arena);
            html_set_attribute(node, "id", id.string_value, env);
          }
          object_def(entry.object_value, "id", id, env);
          array_push(section, entry, env->arena);
        }
      }
    }
    Value children;
    if (object_get_symbol(node.object_value, "children", &children) && children.type == V_ARRAY) {
      for (size_t i = 0; i < children.array_value->size; i++) {
        build_toc(children.array_value->cells[i], toc, min_level, max_level, numbered_headings,
            sep1, sep2, env);
      }
    }
  }
}

typedef struct {
  Env *env;
  Array *toc;
} InsertTocArgs;

static Value print_toc(Array *toc, Env *env) {
  Value list = html_create_element("ol", 0, env);
  for (size_t i = 0; i < toc->size; i++) {
    Value entry = toc->cells[i];
    if (entry.type != V_OBJECT) {
      continue;
    }
    Value title, id;
    if (!object_get_symbol(entry.object_value, "title", &title) || title.type != V_STRING) {
      continue;
    }
    if (!object_get_symbol(entry.object_value, "id", &id) || id.type != V_STRING) {
      continue;
    }
    Value list_element = html_create_element("li", 0, env);
    Value link = html_create_element("a", 0, env);
    StringBuffer href = create_string_buffer(id.string_value->size + 1, env->arena);
    string_buffer_put(&href, '#');
    string_buffer_append(&href, id.string_value);
    html_set_attribute(link, "href", finalize_string_buffer(href).string_value, env);
    html_append_child(link, title, env->arena);
    html_append_child(list_element, link, env->arena);
    Value children;
    if (object_get_symbol(entry.object_value, "children", &children) && children.type == V_ARRAY) {
      html_append_child(list_element, print_toc(children.array_value, env), env->arena);
    }
    html_append_child(list, list_element, env->arena);
  }
  return list;
}

static HtmlTransformation insert_toc(Value node, void *context) {
  InsertTocArgs *args = context;
  if (node.type == V_OBJECT) {
    Value comment;
    if (object_get_symbol(node.object_value, "comment", &comment) && comment.type == V_STRING) {
      if (string_equals("toc", comment.string_value)) {
        return HTML_REPLACE(print_toc(args->toc, args->env));
      }
    }
  }
  return HTML_NO_ACTION;
}

static void read_front_matter(Object *obj, FILE *file, const Path *path, Env *env) {
  Reader *reader = open_reader(file, path, env->symbol_map);
  if (reader_errors(reader)) {
    close_reader(reader);
    return;
  }
  set_reader_silent(1, reader);
  TokenStream tokens = read_all(reader, 0);
  while (peek_token(tokens)->type == T_LF) {
    pop_token(tokens);
  }
  if (peek_token(tokens)->type == T_PUNCT && peek_token(tokens)->punct_value == '{') {
    set_reader_silent(0, reader);
    Module *front_matter = parse_object_notation(tokens, path, 0);
    close_reader(reader);
    if (!front_matter->data_value.parse_error) {
      Value front_matter_obj = interpret(*front_matter->data_value.root, env).value;
      if (front_matter_obj.type == V_OBJECT) {
        ObjectIterator it = iterate_object(front_matter_obj.object_value);
        Value entry_key, entry_value;
        while (object_iterator_next(&it, &entry_key, &entry_value)) {
          object_put(obj, entry_key, entry_value, env->arena);
        }
      } else {
        fprintf(stderr, SGR_BOLD "%s: " INFO_LABEL "unexpected front matter of type %s" SGR_RESET "\n", path->path,
            value_name(front_matter_obj.type));
      }
    } else {
      rewind(file);
    }
    delete_module(front_matter);
  } else {
    close_reader(reader);
    rewind(file);
  }
}

static Value read_file_content(Object *obj, FILE *file, const Path *path, Env *env) {
  Buffer buffer = create_buffer(8192);
  size_t n;
  do {
    if (buffer.size) {
      buffer.capacity += 8192;
      buffer.data = reallocate(buffer.data, buffer.capacity);
    }
    n = fread(buffer.data + buffer.size, 1, 8192, file);
    buffer.size += n;
  } while (n == 8192);
  if (!feof(file)) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "read error: %s" SGR_RESET "\n", path->path, strerror(errno));
  }
  Value content = create_string(buffer.data, buffer.size, env->arena);
  delete_buffer(buffer);
  Value content_handlers;
  if (env_get(get_symbol("CONTENT_HANDLERS", env->symbol_map), &content_handlers, env)
      && content_handlers.type == V_OBJECT) {
    Value type;
    if (object_get(obj, create_symbol(get_symbol("type", env->symbol_map)), &type)
        && type.type == V_STRING) {
      Value handler;
      if (object_get(content_handlers.object_value, type, &handler)) {
        if (handler.type == V_FUNCTION || handler.type == V_CLOSURE) {
          Tuple *func_args = alloca(sizeof(Tuple) + sizeof(Value));
          func_args->size = 1;
          func_args->values[0] = content;
          if (!apply(handler, func_args, &content, env)) {
            env->error_arg = -1;
          }
        } else {
          char *type_string = string_to_c_string(type.string_value);
          fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "invalid handler for content type '%s'" SGR_RESET "\n",
              path->path, type_string);
          free(type_string);
        }
      } else {
        char *type_string = string_to_c_string(type.string_value);
        fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "handler not found for content type '%s'" SGR_RESET "\n",
            path->path, type_string);
        free(type_string);
      }
    } else {
      fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "unknown content type" SGR_RESET "\n", path->path);
    }
  } else {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "CONTENT_HANDLERS not found or invalid" SGR_RESET "\n", path->path);
  }
  return content;
}

static Value parse_content(Value content, const Path *path, Env *env) {
  Value html;
  if (content.type == V_STRING) {
    html = html_parse(content.string_value, env);
    if (html.type != V_NIL) {
      Value src_root;
      if (env_get_symbol("SRC_ROOT", &src_root, env) && src_root.type == V_STRING) {
        Path *src_root_path = string_to_path(src_root.string_value);
        Path *abs_asset_base = path_get_parent(path);
        Path *asset_base = path_get_relative(src_root_path, abs_asset_base);
        delete_path(abs_asset_base);
        ContentLinkArgs content_link_args = {env, asset_base};
        html_transform(html, transform_content_links, &content_link_args);
        ContentIncludeArgs content_include_args = {env, path, asset_base};
        html_transform(html, transform_content_includes, &content_include_args);
        delete_path(asset_base);
        delete_path(src_root_path);
      }
    } else {
      html = create_string(NULL, 0, env->arena);
    } 
  } else {
    html = create_string(NULL, 0, env->arena);
  }
  return html;
}

static Value create_content_object(const Path *path, const char *name, PathStack *path_stack, Env *env) {
  Value obj = create_object(0, env->arena);
  object_def(obj.object_value, "path", path_to_string(path, env->arena), env);
  object_def(obj.object_value, "relative_path", path_stack_to_string(path_stack, env->arena), env);
  Value name_value = copy_c_string(name, env->arena);
  for (size_t i = name_value.string_value->size - 1; i > 0; i--) {
    if (name_value.string_value->bytes[i] == '.') {
      Value type_value = create_string(name_value.string_value->bytes + i + 1,
          name_value.string_value->size - i - 1, env->arena);
      object_def(obj.object_value, "type", type_value, env);
      name_value.string_value->size = i;
      break;
    }
  }
  object_def(obj.object_value, "name", name_value, env);
  object_def(obj.object_value, "modified", create_time(get_mtime(path->path)), env);
  FILE *file = fopen(path->path, "r");
  if (!file) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "%s" SGR_RESET "\n", path->path, strerror(errno));
    return nil_value;
  }
  read_front_matter(obj.object_value, file, path, env);
  Value content = read_file_content(obj.object_value, file, path, env);
  fclose(file);
  object_def(obj.object_value, "content", content, env);
  Value html = parse_content(content, path, env);
  object_def(obj.object_value, "html", html, env);
  Value title_tag = html_find_tag(get_symbol("h1", env->symbol_map), html);
  if (title_tag.type != V_NIL) {
    StringBuffer title_buffer = create_string_buffer(0, env->arena);
    html_text_content(title_tag, &title_buffer);
    object_def(obj.object_value, "title", finalize_string_buffer(title_buffer), env);
  }
  object_def(obj.object_value, "read_more", has_read_more(html) ? true_value : false_value, env);
  int max_toc_level = 6;
  Value temp;
  if (object_get_symbol(obj.object_value, "toc_depth", &temp) && temp.type == V_INT) {
    max_toc_level = temp.int_value;
  }
  int numbered_headings = 0;
  if (object_get_symbol(obj.object_value, "numbered_headings", &temp) && temp.type == V_INT) {
    numbered_headings = temp.int_value;
  }
  Value toc = create_array(0, env->arena);
  if (max_toc_level > 1) {
    Value sep1 = copy_c_string(".", env->arena);
    Value sep2 = copy_c_string(". ", env->arena);
    build_toc(html, toc.array_value, 2, max_toc_level, numbered_headings, sep1.string_value,
       sep2.string_value,  env); 
    InsertTocArgs insert_toc_args = {env, toc.array_value};
    html_transform(html, insert_toc, &insert_toc_args);
  }
  object_def(obj.object_value, "toc", toc, env);
  return obj;
}

static int find_content(const Path *path, int recursive, const char *suffix, size_t suffix_length,
    PathStack *path_stack, Array *content, Env *env) {
  DIR *dir = opendir(path->path);
  int status = 1;
  if (dir) {
    struct dirent *file;
    while ((file = readdir(dir))) {
      if (file->d_name[0] != '.') {
        Path *subpath = path_append(path, file->d_name);
        if (!is_dir(subpath->path)) {
          int match = 1;
          if (suffix_length) {
            size_t name_length = strlen(file->d_name);
            if (name_length < suffix_length) {
              match = 0;
            } else if (memcmp(file->d_name + name_length - suffix_length, suffix, suffix_length) != 0) {
              match = 0;
            }
          }
          if (match) {
            Value obj = create_content_object(subpath, file->d_name, path_stack, env);
            if (obj.type == V_OBJECT) {
              array_push(content, obj, env->arena);
            } else {
              status = 0;
            }
          }
        } else if (recursive) {
          PathStack next = {NULL, path_stack, file->d_name};
          if (path_stack) {
            path_stack->next = &next;
          }
          status = status && find_content(subpath, recursive, suffix, suffix_length, &next, content, env);
        }
        delete_path(subpath);
      }
    }
    closedir(dir);
  }
  return status;
}

static Value list_content(const Tuple *args, Env *env) {
  check_args_between(1, 2, args, env);
  Value path_value = args->values[0];
  if (path_value.type != V_STRING) {
    arg_type_error(1, V_STRING, args, env);
    return nil_value;
  }
  int recursive = 1;
  char *suffix = NULL;
  if (args->size > 1) {
    Value options = args->values[1];
    if (options.type != V_OBJECT) {
      arg_type_error(1, V_OBJECT, args, env);
      return nil_value;
    }
    Value value;
    if (object_get(options.object_value, create_symbol(get_symbol("recursive", env->symbol_map)), &value)) {
      recursive = is_truthy(value);
    }
    if (object_get(options.object_value, create_symbol(get_symbol("suffix", env->symbol_map)), &value)
        && value.type == V_STRING) {
      suffix = string_to_c_string(value.string_value);
    }
  }
  Path *path = string_to_path(path_value.string_value);
  Path *src_path = get_src_path(path, env);
  if (!src_path) {
    if (suffix) {
      free(suffix);
    }
    delete_path(path);
    return nil_value;
  }
  Value content = create_array(0, env->arena);
  int result = find_content(src_path, recursive, suffix, suffix ? strlen(suffix) : 0, NULL, content.array_value, env);
  if (!result) {
    env_error(env, -1, "encountered one or more errors when listing content");
  }
  if (suffix) {
    free(suffix);
  }
  delete_path(src_path);
  delete_path(path);
  return content;
}

static Value read_content(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value path_value = args->values[0];
  if (path_value.type != V_STRING) {
    arg_type_error(1, V_STRING, args, env);
    return nil_value;
  }
  Path *path = string_to_path(path_value.string_value);
  Path *src_path = get_src_path(path, env);
  delete_path(path);
  if (!src_path) {
    return nil_value;
  }
  Value obj = create_content_object(src_path, path_get_name(src_path), NULL, env);
  if (obj.type != V_OBJECT) {
    env_error(env, -1, "content read error");
  }
  delete_path(src_path);
  return obj;
}

void import_contentmap(Env *env) {
  env_def_fn("list_content", list_content, env);
  env_def_fn("read_content", read_content, env);
}

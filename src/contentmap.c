/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "contentmap.h"

#include "build.h"
#include "interpreter.h"
#include "parser.h"
#include "reader.h"
#include "strings.h"

#include <alloca.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

typedef struct PathStack PathStack;

struct PathStack {
   PathStack *next;
   PathStack *prev;
   char *name;
};

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

static Value create_content_object(const char *path, const char *name, PathStack *path_stack, Env *env) {
  Value obj = create_object(0, env->arena);
  object_def(obj.object_value, "path", copy_c_string(path, env->arena), env);
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
  struct stat stat_buffer;
  if (stat(path, &stat_buffer) == 0) {
    object_def(obj.object_value, "modified", create_time(stat_buffer.st_mtime), env);
  } else {
    object_def(obj.object_value, "modified", create_time(0), env);
  }
  FILE *file = fopen(path, "r");
  if (!file) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "%s" SGR_RESET "\n", path, strerror(errno));
    return nil_value;
  }
  Reader *reader = open_reader(file, path, env->symbol_map);
  if (reader_errors(reader)) {
    close_reader(reader);
    fclose(file);
    return nil_value;
  }
  TokenStream tokens = read_all(reader, 0);
  while (peek_token(tokens)->type == T_LF) {
    pop_token(tokens);
  }
  if (peek_token(tokens)->type == T_PUNCT && peek_token(tokens)->punct_value == '{') {
    Module *front_matter = parse_object_notation(tokens, path, 0);
    close_reader(reader);
    if (!front_matter->parse_error) {
      Value front_matter_obj = interpret(*front_matter->root, env);
      if (front_matter_obj.type == V_OBJECT) {
        ObjectIterator it = iterate_object(front_matter_obj.object_value);
        Value entry_key, entry_value;
        while (object_iterator_next(&it, &entry_key, &entry_value)) {
          object_put(obj.object_value, entry_key, entry_value, env->arena);
        }
      } else {
        fprintf(stderr, SGR_BOLD "%s: " INFO_LABEL "unexpected front matter of type %s" SGR_RESET "\n", path,
            value_name(front_matter_obj.type));
      }
    } else {
      rewind(file);
    }
    delete_module(front_matter);
  } else {
    close_reader(reader);
    fprintf(stderr, SGR_BOLD "%s: " INFO_LABEL "no front matter" SGR_RESET "\n", path);
    rewind(file);
  }
  Buffer buffer = create_buffer(8192);
  size_t n;
  do {
    if (buffer.size) {
      buffer.capacity += 8192;
      buffer.data = reallocate(buffer.data, buffer.capacity);
    }
    n = fread(buffer.data, 1, 8192, file);
    buffer.size += n;
  } while (n == 8192);
  if (!feof(file)) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "read error: %s" SGR_RESET "\n", path, strerror(errno));
  }
  Value content = create_string(buffer.data, buffer.size, env->arena);
  delete_buffer(buffer);
  fclose(file);
  Value content_handlers;
  if (env_get(get_symbol("CONTENT_HANDLERS", env->symbol_map), &content_handlers, env)
      && content_handlers.type == V_OBJECT) {
    Value type;
    if (object_get(obj.object_value, create_symbol(get_symbol("type", env->symbol_map)), &type)
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
              path, type_string);
          free(type_string);
        }
      } else {
        char *type_string = string_to_c_string(type.string_value);
        fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "handler not found for content type '%s'" SGR_RESET "\n", path,
            type_string);
        free(type_string);
      }
    } else {
      fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "unknown content type" SGR_RESET "\n", path);
    }
  } else {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "CONTENT_HANDLERS not found or invalid" SGR_RESET "\n", path);
  }
  object_def(obj.object_value, "content", content, env);
  return obj;
}

static int find_content(const char *path, int recursive, const char *suffix, size_t suffix_length,
    PathStack *path_stack, Array *content, Env *env) {
  DIR *dir = opendir(path);
  int status = 1;
  if (dir) {
    struct dirent *file;
    while ((file = readdir(dir))) {
      if (file->d_name[0] != '.') {
        char *subpath = combine_paths(path, file->d_name);
        if (!is_dir(subpath)) {
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
        free(subpath);
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
  char *path = get_src_path(path_value.string_value, env);
  if (!path) {
    if (suffix) {
      free(suffix);
    }
    return nil_value;
  }
  Value content = create_array(0, env->arena);
  int result = find_content(path, recursive, suffix, suffix ? strlen(suffix) : 0, NULL, content.array_value, env);
  if (!result) {
    env_error(env, -1, "encountered one or more errors when listing content");
  }
  if (suffix) {
    free(suffix);
  }
  free(path);
  return content;
}

static Value save_content(const Tuple *args, Env *env) {
  check_args(1, args, env);
  return nil_value;
}

void import_contentmap(Env *env) {
  env_def_fn("list_content", list_content, env);
  env_def_fn("save_content", save_content, env);
}

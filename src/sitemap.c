/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "sitemap.h"

#include "alloca.h"
#include "build.h"
#include "interpreter.h"
#include "strings.h"

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  P_COPY,
  P_TEMPLATE
} PageType;

typedef struct {
  PageType type;
  Path *src;
  Path *dest;
  Value web_path;
  Value data;
} PageInfo;

typedef struct {
  PageType type;
  const Path *src;
  const Path *dest;
  Value web_path;
  Value data;
} ConstPageInfo;

static Value encode_page_info(ConstPageInfo page, Env *env) {
  Value object = create_object(0, env->arena);
  switch (page.type) {
    case P_COPY:
      object_def(object.object_value, "type", create_symbol(get_symbol("copy", env->symbol_map)), env);
      object_def(object.object_value, "src", path_to_string(page.src, env->arena), env);
      object_def(object.object_value, "dest", path_to_string(page.dest, env->arena), env);
      break;
    case P_TEMPLATE:
      object_def(object.object_value, "type", create_symbol(get_symbol("template", env->symbol_map)), env);
      object_def(object.object_value, "src", path_to_string(page.src, env->arena), env);
      object_def(object.object_value, "dest", path_to_string(page.dest, env->arena), env);
      object_def(object.object_value, "web_path", page.web_path, env);
      object_def(object.object_value, "data", page.data, env);
      break;
  }
  return object;
}

static int decode_page_info(Value value, PageInfo *page) {
  if (value.type != V_OBJECT) {
    return 0;
  }
  Value type, src, dest;
  if (!object_get_symbol(value.object_value, "type", &type) || type.type != V_SYMBOL) {
    return 0;
  }
  if (!object_get_symbol(value.object_value, "src", &src) || src.type != V_STRING) {
    return 0;
  }
  if (!object_get_symbol(value.object_value, "dest", &dest) || dest.type != V_STRING) {
    return 0;
  }
  if (strcmp(type.symbol_value, "copy") == 0) {
    page->type = P_COPY;
    page->src = string_to_path(src.string_value);
    page->dest = string_to_path(dest.string_value);
    return 1;
  } else if (strcmp(type.symbol_value, "template") == 0) {
    Value web_path, data;
    if (!object_get_symbol(value.object_value, "web_path", &web_path) || web_path.type != V_STRING) {
      return 0;
    }
    if (!object_get_symbol(value.object_value, "data", &data)) {
      data = nil_value;
    }
    page->type = P_TEMPLATE;
    page->src = string_to_path(src.string_value);
    page->dest = string_to_path(dest.string_value);
    page->web_path = web_path;
    page->data = data;
    return 1;
  }
  return 0;
}

static int copy_static_files(const Path *src_path, const Path *dest_path, Array *site_map, Env *env) {
  if (is_dir(src_path->path)) {
    if (!mkdir_rec(dest_path->path)) {
      return 0;
    }
    DIR *dir = opendir(src_path->path);
    int status = 1;
    if (dir) {
      struct dirent *file;
      while ((file = readdir(dir))) {
        if (file->d_name[0] != '.') {
          Path *child_src_path = path_append(src_path, file->d_name);
          Path *child_dest_path = path_append(dest_path, file->d_name);
          status = status && copy_static_files(child_src_path, child_dest_path, site_map, env);
          delete_path(child_dest_path);
          delete_path(child_src_path);
        }
      }
      closedir(dir);
    }
    return status;
  }
  ConstPageInfo page_info = { P_COPY, src_path, dest_path };
  array_push(site_map, encode_page_info(page_info, env), env->arena);
  return 1;
}

static Value add_static(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value src_value = args->values[0];
  if (src_value.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  Path *src_path = string_to_src_path(src_value.string_value, env);
  if (!src_path) {
    return nil_value;
  }
  Path *dest_path = string_to_dist_path(src_value.string_value, env);
  if (!dest_path) {
    delete_path(src_path);
    return nil_value;
  }
  Value site_map;
  if (!env_get_symbol("SITE_MAP", &site_map, env) || site_map.type != V_ARRAY) {
    env_error(env, -1, "SITE_MAP is missign or not an object");
    return nil_value;
  }
  if (!copy_static_files(src_path, dest_path, site_map.array_value, env)) {
    env_error(env, -1, "failed copying one or more files to dist");
  }
  delete_path(dest_path);
  delete_path(src_path);
  return nil_value;
}

static Value add_reverse(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src_value = args->values[0];
  if (src_value.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  Value link_value = args->values[1];
  if (link_value.type != V_STRING) {
    arg_type_error(1, V_STRING, args, env);
    return nil_value;
  }
  Value reverse_paths;
  if (!env_get_symbol("REVERSE_PATHS", &reverse_paths, env) || reverse_paths.type != V_OBJECT) {
    env_error(env, -1, "REVERSE_PATHS is missing or not an object");
    return nil_value;
  }
  object_put(reverse_paths.object_value, src_value, link_value, env->arena);
  return nil_value;
}

static void create_site_node(String *site_path, String *template_path, Value data, Env *env) {
  Value site_map;
  if (!env_get_symbol("SITE_MAP", &site_map, env) || site_map.type != V_ARRAY) {
    env_error(env, -1, "SITE_MAP is missign or not an object");
    return;
  }
  site_path = string_trim(site_path, (uint8_t *) "/", 1, env->arena).string_value;
  Path *src_path = string_to_src_path(template_path, env);
  if (!src_path) {
    return;
  }
  Path *dest_path = string_to_dist_path(site_path, env);
  if (!dest_path) {
    delete_path(src_path);
    return;
  }
  Module *module = get_template(src_path, env);
  if (!module) {
    env_error(env, -1, "unable to load template");
  } else {
    array_push(site_map.array_value, encode_page_info((ConstPageInfo) { P_TEMPLATE, src_path, dest_path,
          (Value) { .type = V_STRING, .string_value = site_path }, data }, env), env->arena);
  }
  delete_path(dest_path);
  delete_path(src_path);
}

static Value add_page(const Tuple *args, Env *env) {
  check_args_between(2, 3, args, env);
  Value dest = args->values[0];
  if (dest.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  Value src = args->values[1];
  if (src.type != V_STRING) {
    arg_type_error(1, V_STRING, args, env);
    return nil_value;
  }
  Value data = nil_value;
  if (args->size > 2) {
    data = args->values[2];
    if (data.type != V_OBJECT) {
      arg_type_error(2, V_OBJECT, args, env);
      return nil_value;
    }
  }
  create_site_node(dest.string_value, src.string_value, data, env);
  return nil_value;
}

static Value create_page(int64_t total, int64_t per_page, int64_t page, int64_t pages, int64_t offset,
    Value path_template, Env *env) {
  Value obj = create_object(6, env->arena);
  object_def(obj.object_value, "items", create_array(per_page, env->arena), env);
  object_def(obj.object_value, "total", create_int(total), env);
  object_def(obj.object_value, "page", create_int(page), env);
  object_def(obj.object_value, "pages", create_int(pages), env);
  object_def(obj.object_value, "offset", create_int(offset), env);
  object_def(obj.object_value, "path_template", path_template, env);
  return obj;
}

static void add_item_to_page(Value item, Value page, Env *env) {
  if (page.type != V_OBJECT) {
    return;
  }
  Value items;
  if (object_get(page.object_value, create_symbol(get_symbol("items", env->symbol_map)), &items)
      && items.type == V_ARRAY) {
    array_push(items.array_value, item, env->arena);
  }
}

static void add_page_to_site(Value page, String *src, String *path_template, Value data, Env *env) {
  if (page.type != V_OBJECT) {
    return;
  }
  Value page_number;
  if (!object_get(page.object_value, create_symbol(get_symbol("page", env->symbol_map)), &page_number)
      && page_number.type == V_INT) {
    return;
  }
  Value page_name;
  if (page_number.int_value == 1) {
    page_name = create_string(NULL, 0, env->arena);
  } else {
    StringBuffer page_name_buffer = create_string_buffer(10, env->arena);
    string_buffer_printf(&page_name_buffer, "/page%" PRId64, page_number.int_value);
    page_name = finalize_string_buffer(page_name_buffer);
  }
  Value path = string_replace(copy_c_string("%page%", env->arena).string_value, page_name.string_value,
      path_template, env->arena);
  if (data.type != V_OBJECT) {
    data = create_object(0, env->arena);
  }
  object_def(data.object_value, "PAGE", page, env);
  create_site_node(path.string_value, src, data, env);
}

static Value paginate(const Tuple *args, Env *env) {
  check_args_between(4, 5, args, env);
  Value items = args->values[0];
  if (items.type != V_ARRAY) {
    arg_type_error(0, V_ARRAY, args, env);
    return nil_value;
  }
  Value per_page = args->values[1];
  if (per_page.type != V_INT) {
    arg_type_error(1, V_INT, args, env);
    return nil_value;
  }
  Value path_template = args->values[2];
  if (path_template.type != V_STRING) {
    arg_type_error(2, V_STRING, args, env);
    return nil_value;
  }
  Value src = args->values[3];
  if (src.type != V_STRING) {
    arg_type_error(3, V_STRING, args, env);
    return nil_value;
  }
  Value data = nil_value;
  if (args->size > 4) {
    data = args->values[4];
    if (data.type != V_OBJECT) {
      arg_type_error(4, V_OBJECT, args, env);
      return nil_value;
    }
  }
  int64_t page_count = items.array_value->size
    ?  (items.array_value->size - 1) / per_page.int_value + 1
    : 1;
  int64_t page_number = 1;
  Value page = create_page(items.array_value->size, per_page.int_value, page_number, page_count, 0,
      path_template, env);
  for (int64_t i = 0, counter = 0; i < items.array_value->size; i++, counter++) {
    if (counter >= per_page.int_value) {
      add_page_to_site(page, src.string_value, path_template.string_value, data, env);
      page_number++;
      page = create_page(items.array_value->size, per_page.int_value, page_number, page_count, i,
          path_template, env);
      counter = 0;
    }
    add_item_to_page(items.array_value->cells[i], page, env);
  }
  add_page_to_site(page, src.string_value, path_template.string_value, data, env);
  return nil_value;
}

static Value default_handler(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value input = args->values[0];
  if (input.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  return input;
}

void import_sitemap(Env *env) {
  env_def("SITE_MAP", create_array(0, env->arena), env);
  env_def("REVERSE_PATHS", create_object(0, env->arena), env);
  env_export("REVERSE_PATHS", env);
  env_def("OUTPUT_OBSERVERS", create_array(0, env->arena), env);
  env_export("OUTPUT_OBSERVERS", env);
  env_def_fn("add_static", add_static, env);
  env_def_fn("add_reverse", add_reverse, env);
  env_def_fn("add_page", add_page, env);
  env_def_fn("paginate", paginate, env);
  Value content_handlers;
  if (!env_get(get_symbol("CONTENT_HANDLERS", env->symbol_map), &content_handlers, env)) {
    content_handlers = create_object(0, env->arena);
    env_def("CONTENT_HANDLERS", content_handlers, env);
  }
  env_export("CONTENT_HANDLERS", env);
  if (content_handlers.type == V_OBJECT) {
    object_put(content_handlers.object_value, copy_c_string("txt", env->arena),
        (Value) { .type = V_FUNCTION, .function_value = default_handler }, env->arena);
    object_put(content_handlers.object_value, copy_c_string("htm", env->arena),
        (Value) { .type = V_FUNCTION, .function_value = default_handler }, env->arena);
    object_put(content_handlers.object_value, copy_c_string("html", env->arena),
        (Value) { .type = V_FUNCTION, .function_value = default_handler }, env->arena);
  }
}

static int compile_page(PageInfo page, Env *env) {
  switch (page.type) {
    case P_COPY:
      return copy_file(page.src->path, page.dest->path);
    case P_TEMPLATE: {
      int status = 0;
      Module *module = get_template(page.src, env);
      if (module) {
        Env *template_env = create_template_env(page.data, env);
        env_def("PATH", copy_value(page.web_path, template_env), template_env);
        Value output = eval_template(module, template_env);
        if (output.type == V_STRING) {
          Path *dir = path_get_parent(page.dest);
          if (mkdir_rec(dir->path)) {
            FILE *dest = fopen(page.dest->path, "w");
            if (!dest) {
              fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "%s" SGR_RESET "\n", page.dest->path, strerror(errno));
            } else {
              if (fwrite(output.string_value->bytes, 1, output.string_value->size, dest) != output.string_value->size) {
                fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "write error: %s" SGR_RESET "\n", page.dest->path,
                    strerror(errno));
              } else {
                status = 1;
              }
              fclose(dest);
            }
          } else {
            fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "unable to create output directory", page.dest->path);
          }
          delete_path(dir);
        }
        delete_template_env(template_env);
      }
      return status;
    }
  }
  return 0;
}

void notify_output_observers(const Path *path, Env *env) {
  Value observers;
  if (!env_get_symbol("OUTPUT_OBSERVERS", &observers, env) || observers.type != V_ARRAY) {
    return;
  }
  Tuple *args = alloca(sizeof(Tuple) + sizeof(Value));
  args->size = 1;
  args->values[0] = path_to_string(path, env->arena);
  for (size_t i = 0; i < observers.array_value->size; i++) {
    Value func = observers.array_value->cells[i];
    if (func.type == V_FUNCTION || func.type == V_CLOSURE) {
      Value ignore;
      apply(func, args, &ignore, env);
    }
  }
}

int compile_pages(Env *env) {
  Value site_map;
  if (!env_get_symbol("SITE_MAP", &site_map, env) || site_map.type != V_ARRAY) {
    fprintf(stderr, ERROR_LABEL "SITE_MAP undefined or not an array" SGR_RESET "\n");
    return 0;
  }
  Path *dist_root = get_dist_root(env);
  if (!dist_root) {
    fprintf(stderr, ERROR_LABEL "DIST_ROOT undefined or not a string" SGR_RESET "\n");
    return 0;
  }
  for (size_t i = 0; i < site_map.array_value->size; i++) {
    Value page_value = site_map.array_value->cells[i];
    PageInfo page;
    if (!decode_page_info(page_value, &page)) {
      fprintf(stderr, ERROR_LABEL "invalid page object at index %zd of SITE_MAP" SGR_RESET "\n", i);
      continue;
    }
    Path *site_path = path_get_relative(dist_root, page.dest);
    fprintf(stderr, "[%zd/%zd] Processing %-50.*s\r", i, site_map.array_value->size,
        site_path->size > 50 ? 50 : (int) site_path->size, site_path->path);
    fflush(stderr);
    delete_path(site_path);
    if (compile_page(page, env)) {
      notify_output_observers(page.dest, env);
    }
    delete_path(page.src);
    delete_path(page.dest);
  }
  delete_path(dist_root);
  return 0;
}

/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "sitemap.h"

#include "build.h"
#include "strings.h"

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

static int copy_static_files(const char *src_path, const char *dest_path) {
  if (is_dir(src_path)) {
    if (!mkdir_rec(dest_path)) {
      return 0;
    }
    DIR *dir = opendir(src_path);
    int status = 1;
    if (dir) {
      struct dirent *file;
      while ((file = readdir(dir))) {
        if (file->d_name[0] != '.') {
          char *child_src_path = combine_paths(src_path, file->d_name);
          char *child_dest_path = combine_paths(dest_path, file->d_name);
          status = status && copy_static_files(child_src_path, child_dest_path);
          free(child_dest_path);
          free(child_src_path);
        }
      }
      closedir(dir);
    }
    return status;
  }
  return copy_file(src_path, dest_path);
}

static Value add_static(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value src_value = args->values[0];
  if (src_value.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  char *src_path = get_src_path(src_value.string_value, env);
  if (!src_path) {
    return nil_value;
  }
  char *dest_path = get_dist_path(src_value.string_value, env);
  if (!dest_path) {
    free(src_path);
    return nil_value;
  }
  if (!copy_static_files(src_path, dest_path)) {
    env_error(env, -1, "failed copying one or more files to dist");
  }
  free(dest_path);
  free(src_path);
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
  char *src_path = get_src_path(template_path, env);
  if (!src_path) {
    return;
  }
  char *dest_path = get_dist_path(site_path, env);
  if (!dest_path) {
    free(src_path);
    return;
  }
  Module *module = get_template(src_path, env);
  if (!module) {
    env_error(env, -1, "unable to load module");
  } else {
    Env *template_env = create_template_env(data, env);
    env_def("PATH", copy_value((Value) { .type = V_STRING, .string_value = site_path },
          template_env->arena), template_env);
    Value output = eval_template(module, data, template_env);
    if (output.type == V_STRING) {
      char *dest_path_copy = copy_string(dest_path);
      char *dir = dirname(dest_path_copy);
      if (mkdir_rec(dir)) {
        FILE *dest = fopen(dest_path, "w");
        int error = 0;
        if (!dest) {
          fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "%s" SGR_RESET "\n", dest_path, strerror(errno));
          error = 1;
        } else {
          if (fwrite(output.string_value->bytes, 1, output.string_value->size, dest) != output.string_value->size) {
            fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "write error: %s" SGR_RESET "\n", dest_path, strerror(errno));
            error = 1;
          }
          fclose(dest);
        }
        if (error) {
          env_error(env, -1, "unable to write template output");
        }
      } else {
        env_error(env, -1, "unable to create output directory");
      }
      free(dest_path_copy);
    }
    delete_template_env(template_env);
  }
  free(dest_path);
  free(src_path);
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
  env_def("GLOBAL", create_object(0, env->arena), env);
  env_def("REVERSE_PATHS", create_object(0, env->arena), env);
  env_def_fn("add_static", add_static, env);
  env_def_fn("add_reverse", add_reverse, env);
  env_def_fn("add_page", add_page, env);
  env_def_fn("paginate", paginate, env);
  Value content_handlers;
  if (!env_get(get_symbol("CONTENT_HANDLERS", env->symbol_map), &content_handlers, env)) {
    content_handlers = create_object(0, env->arena);
    env_def("CONTENT_HANDLERS", content_handlers, env);
  }
  if (content_handlers.type == V_OBJECT) {
    object_put(content_handlers.object_value, copy_c_string("txt", env->arena),
        (Value) { .type = V_FUNCTION, .function_value = default_handler }, env->arena);
    object_put(content_handlers.object_value, copy_c_string("htm", env->arena),
        (Value) { .type = V_FUNCTION, .function_value = default_handler }, env->arena);
    object_put(content_handlers.object_value, copy_c_string("html", env->arena),
        (Value) { .type = V_FUNCTION, .function_value = default_handler }, env->arena);
  }
}

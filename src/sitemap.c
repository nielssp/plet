/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "sitemap.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

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
  Value dir_value, dist_root_value;
  if (!env_get(get_symbol("DIR", env->symbol_map), &dir_value, env) || dir_value.type != V_STRING) {
    env_error(env, -1, "missing or invalid DIR");
    return nil_value;
  }
  if (!env_get(get_symbol("DIST_ROOT", env->symbol_map), &dist_root_value, env) || dist_root_value.type != V_STRING) {
    env_error(env, -1, "missing or invalid DIST_ROOT");
    return nil_value;
  }
  char *dir = string_to_c_string(dir_value.string_value);
  char *src = string_to_c_string(src_value.string_value);
  char *dist_root = string_to_c_string(dist_root_value.string_value);
  char *src_path = combine_paths(dir, src);
  char *dest_path = combine_paths(dist_root, src);
  if (!copy_static_files(src_path, dest_path)) {
    env_error(env, -1, "failed copying one or more files to dist");
  }
  free(dest_path);
  free(src_path);
  free(dist_root);
  free(src);
  free(dir);
  return nil_value;
}

static Value add_page(const Tuple *args, Env *env) {
  check_args_between(2, 3, args, env);
  return nil_value;
}

static Value paginate(const Tuple *args, Env *env) {
  check_args_between(4, 5, args, env);
  return nil_value;
}

void import_sitemap(Env *env) {
  env_def_fn("add_static", add_static, env);
  env_def_fn("add_page", add_page, env);
  env_def_fn("paginate", paginate, env);
}

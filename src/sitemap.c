/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "sitemap.h"

#include "build.h"

#include <dirent.h>
#include <errno.h>
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

static Value add_page(const Tuple *args, Env *env) {
  check_args_between(2, 3, args, env);
  Value dest_value = args->values[0];
  if (dest_value.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  Value src_value = args->values[1];
  if (src_value.type != V_STRING) {
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
  char *src_path = get_src_path(src_value.string_value, env);
  if (!src_path) {
    return nil_value;
  }
  char *dest_path = get_dist_path(dest_value.string_value, env);
  if (!dest_path) {
    free(src_path);
    return nil_value;
  }
  Module *module = get_template(src_path, env);
  if (!module) {
    env_error(env, -1, "unable to load module");
  } else {
    Env *template_env = create_template_env(data, env);
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

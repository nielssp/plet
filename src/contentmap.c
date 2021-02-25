/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "contentmap.h"

#include "build.h"

#include <dirent.h>
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
  Buffer buffer = create_buffer(0);
  while (path_stack) {
    buffer_printf(&buffer, path_stack->name);
    path_stack = path_stack->next;
    if (path_stack) {
      buffer_put(&buffer, PATH_SEP);
    }
  }
  Value result = create_string(buffer.data, buffer.size, arena);
  delete_buffer(buffer);
  return result;
}

static Value create_content_object(const char *path, const char *name, PathStack *path_stack, Env *env) {
  Value obj = create_object(0, env->arena);
  object_put(obj.object_value, create_symbol(get_symbol("path", env->symbol_map)), copy_c_string(path, env->arena),
      env->arena);
  object_put(obj.object_value, create_symbol(get_symbol("relative_path", env->symbol_map)),
      path_stack_to_string(path_stack, env->arena), env->arena);
  Value name_value = copy_c_string(name, env->arena);
  for (size_t i = name_value.string_value->size - 1; i > 0; i--) {
    if (name_value.string_value->bytes[i] == '.') {
      name_value.string_value->size = i;
      break;
    }
  }
  object_put(obj.object_value, create_symbol(get_symbol("name", env->symbol_map)), name_value, env->arena);
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
            array_push(content, create_content_object(subpath, file->d_name, path_stack, env), env->arena);
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
  find_content(path, recursive, suffix, suffix ? strlen(suffix) : 0, NULL, content.array_value, env);
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

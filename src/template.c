/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "template.h"

#include "build.h"
#include "strings.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static Value embed(const Tuple *args, Env *env) {
  check_args_between(1, 2, args, env);
  Value src_value = args->values[0];
  if (src_value.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  Value data = nil_value;
  if (args->size > 1) {
    data = args->values[1];
    if (data.type != V_OBJECT) {
      arg_type_error(1, V_OBJECT, args, env);
      return nil_value;
    }
  }
  char *src_path = get_src_path(src_value.string_value, env);
  if (!src_path) {
    return nil_value;
  }
  Module *module = get_template(src_path, env);
  Value output = nil_value;
  if (!module) {
    env_error(env, -1, "unable to load module");
  } else {
    Env *template_env = create_template_env(data, env);
    Value global;
    if (env_get(get_symbol("GLOBAL", env->symbol_map), &global, env) && global.type == V_OBJECT) {
      ObjectIterator it = iterate_object(global.object_value);
      Value entry_key, entry_value;
      while (object_iterator_next(&it, &entry_key, &entry_value)) {
        if (entry_key.type == V_SYMBOL) {
          env_put(entry_key.symbol_value, copy_value(entry_value, template_env->arena), template_env);
        }
      }
    }
    Value current_path;
    if (env_get(get_symbol("PATH", env->symbol_map), &current_path, env)) {
      env_def("PATH", current_path, template_env);
    }
    output = copy_value(eval_template(module, data, template_env), env->arena);
    delete_template_env(template_env);
  }
  free(src_path);
  return output;
}

static Value link(const Tuple *args, Env *env) {
  check_args_between(0, 1, args, env);
  Value path;
  if (args->size > 0) {
    path = args->values[0];
    if (path.type != V_STRING) {
      arg_type_error(0, V_STRING, args, env);
      return nil_value;
    }
  } else if (!env_get(get_symbol("PATH", env->symbol_map), &path, env) || path.type != V_STRING) {
    env_error(env, -1, "PATH is not set or not a string");
    return nil_value;
  }
  if (string_equals("index.html", path.string_value)) {
    path = copy_c_string("", env->arena);
  } else if (string_ends_with("/index.html", path.string_value)) {
    path = create_string(path.string_value->bytes, path.string_value->size - 11, env->arena);
  }
  // TODO: find prefix for path
  return path;
}

static Value url(const Tuple *args, Env *env) {
  check_args_between(0, 1, args, env);
  Value path;
  if (args->size > 0) {
    path = args->values[0];
    if (path.type != V_STRING) {
      arg_type_error(0, V_STRING, args, env);
      return nil_value;
    }
  } else if (!env_get(get_symbol("PATH", env->symbol_map), &path, env) || path.type != V_STRING) {
    env_error(env, -1, "PATH is not set or not a string");
    return nil_value;
  }
  if (string_equals("index.html", path.string_value)) {
    path = copy_c_string("", env->arena);
  } else if (string_ends_with("/index.html", path.string_value)) {
    path = create_string(path.string_value->bytes, path.string_value->size - 11, env->arena);
  }
  // TODO: find prefix for path
  return path;
}

static Value read(const Tuple *args, Env *env) {
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
  FILE *file = fopen(src_path, "r");
  if (!file) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "%s" SGR_RESET "\n", src_path, strerror(errno));
    env_error(env, -1, "error reading file");
    free(src_path);
    return nil_value;
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
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "read error: %s" SGR_RESET "\n", src_path, strerror(errno));
    env_error(env, -1, "error reading file");
  }
  Value content = create_string(buffer.data, buffer.size, env->arena);
  delete_buffer(buffer);
  fclose(file);
  free(src_path);
  return content;
}

void import_template(Env *env) {
  env_def_fn("embed", embed, env);
  env_def_fn("link", link, env);
  env_def_fn("url", url, env);
  //env_def_fn("is_current", is_current, env);
  env_def_fn("read", read, env);
  //env_def_fn("page_list", page_list, env);
  //env_def_fn("page_link", page_link, env);
}

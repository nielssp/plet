/* Plet
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "template.h"

#include "build.h"
#include "module.h"
#include "strings.h"

#include <errno.h>
#include <inttypes.h>
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
  Path *src_path = string_to_src_path(src_value.string_value, env);
  if (!src_path) {
    return nil_value;
  }
  Module *module = get_template(src_path, env);
  Value output = nil_value;
  if (!module) {
    env_error(env, -1, "unable to load template");
  } else {
    Env *template_env = create_child_env(env);
    env_def("LAYOUT", nil_value, template_env);
    output = eval_template(module, template_env);
  }
  delete_path(src_path);
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
  Value root_path;
  if (env_get(get_symbol("ROOT_PATH", env->symbol_map), &root_path, env) && root_path.type == V_STRING) {
    return combine_string_paths(root_path.string_value, path.string_value, env);
  }
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
  Value root_url;
  if (env_get(get_symbol("ROOT_URL", env->symbol_map), &root_url, env) && root_url.type == V_STRING) {
    return combine_string_paths(root_url.string_value, path.string_value, env);
  }
  return path;
}

static Value is_current(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value path = args->values[0];
  if (path.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  if (path_is_current(path.string_value, env)) {
    return true_value;
  }
  return false_value;
}

static Value read(const Tuple *args, Env *env) {
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
  Value content = read_asset_module(src_path, env);
  if (content.type != V_STRING) {
    env_error(env, -1, "error reading file");
  }
  delete_path(src_path);
  return content;
}

static Value asset_link(const Tuple *args, Env *env) {
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
  load_asset_module(src_path, env);
  copy_file(src_path->path, dest_path->path);
  delete_path(src_path);
  delete_path(dest_path);
  if (string_equals("index.html", src_value.string_value)) {
    src_value = copy_c_string("", env->arena);
  } else if (string_ends_with("/index.html", src_value.string_value)) {
    src_value = create_string(src_value.string_value->bytes, src_value.string_value->size - 11, env->arena);
  }
  Value root_path;
  if (env_get(get_symbol("ROOT_PATH", env->symbol_map), &root_path, env) && root_path.type == V_STRING) {
    return combine_string_paths(root_path.string_value, src_value.string_value, env);
  }
  return src_value;
}

static Value page_list(const Tuple *args, Env *env) {
  check_args_between(1, 3, args, env);
  Value n = args->values[0];
  if (n.type != V_INT) {
    arg_type_error(0, V_INT, args, env);
    return nil_value;
  }
  Value page_obj;
  if (args->size < 3) {
    if (!env_get(get_symbol("PAGE", env->symbol_map), &page_obj, env) || page_obj.type != V_OBJECT) {
      env_error(env, -1, "PAGE is not set or not an object");
      return nil_value;
    }
  }
  Value page;
  if (args->size > 1) {
    page = args->values[1];
    if (page.type != V_INT) {
      arg_type_error(1, V_INT, args, env);
      return nil_value;
    }
  } else if (!object_get(page_obj.object_value, create_symbol(get_symbol("page", env->symbol_map)), &page)
      || page.type != V_INT) {
    env_error(env, -1, "PAGE.page is not set or not an integer");
    return nil_value;
  }
  Value pages;
  if (args->size > 2) {
    pages = args->values[2];
    if (pages.type != V_INT) {
      arg_type_error(2, V_INT, args, env);
      return nil_value;
    }
  } else if (!object_get(page_obj.object_value, create_symbol(get_symbol("pages", env->symbol_map)), &pages)
      || pages.type != V_INT) {
    env_error(env, -1, "PAGE.pages is not set or not an integer");
    return nil_value;
  }
  if (pages.int_value > 0xFFFF) {
    env_error(env, -1, "too many pages");
    return nil_value;
  }
  // TODO: use n
  Value result = create_array(pages.int_value > 0 ? pages.int_value : 0, env->arena);
  for (int64_t i = 1; i <= pages.int_value; i++) {
    array_push(result.array_value, create_int(i), env->arena);
  }
  return result;
}

static Value page_link(const Tuple *args, Env *env) {
  check_args_between(1, 2, args, env);
  Value page = args->values[0];
  if (page.type != V_INT) {
    arg_type_error(0, V_INT, args, env);
    return nil_value;
  }
  Value page_obj;
  Value path;
  if (args->size > 1) {
    path = args->values[1];
    if (page.type != V_STRING) {
      arg_type_error(1, V_STRING, args, env);
      return nil_value;
    }
  } else if (!env_get_symbol("PAGE", &page_obj, env) || page_obj.type != V_OBJECT) {
      env_error(env, -1, "PAGE is not set or not an object");
      return nil_value;
  } else if (!object_get_symbol(page_obj.object_value, "path_template", &path) || path.type != V_STRING) {
    env_error(env, -1, "PAGE.path_template is not set or not a string");
    return nil_value;
  }
  Value page_name;
  if (page.int_value == 1) {
    page_name = create_string(NULL, 0, env->arena);
  } else {
    StringBuffer page_name_buffer = create_string_buffer(10, env->arena);
    string_buffer_printf(&page_name_buffer, "/page%" PRId64, page.int_value);
    page_name = finalize_string_buffer(page_name_buffer);
  }
  path = string_replace(copy_c_string("%page%", env->arena).string_value, page_name.string_value,
      path.string_value, env->arena);
  if (string_equals("index.html", path.string_value)) {
    path = copy_c_string("", env->arena);
  } else if (string_ends_with("/index.html", path.string_value)) {
    path = create_string(path.string_value->bytes, path.string_value->size - 11, env->arena);
  }
  Value root_path;
  if (env_get(get_symbol("ROOT_PATH", env->symbol_map), &root_path, env) && root_path.type == V_STRING) {
    return combine_string_paths(root_path.string_value, path.string_value, env);
  }
  return path;
}


void import_template(Env *env) {
  env_def_fn("embed", embed, env);
  env_def_fn("link", link, env);
  env_def_fn("url", url, env);
  env_def_fn("is_current", is_current, env);
  env_def_fn("read", read, env);
  env_def_fn("asset_link", asset_link, env);
  env_def_fn("page_list", page_list, env);
  env_def_fn("page_link", page_link, env);
}

int path_is_current(String *path, Env *env) {
  Value current_path;
  if (!env_get(get_symbol("PATH", env->symbol_map), &current_path, env) || current_path.type != V_STRING) {
    return 0;
  }
  if (string_equals("index.html", current_path.string_value)) {
    current_path = copy_c_string("", env->arena);
  } else if (string_ends_with("/index.html", current_path.string_value)) {
    current_path = create_string(current_path.string_value->bytes, current_path.string_value->size - 11, env->arena);
  }
  return equals(string_trim(path, (uint8_t *) "/", 1, env->arena),
      string_trim(current_path.string_value, (uint8_t *) "/", 1, env->arena));
}

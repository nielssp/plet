/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "build.h"

#include "collections.h"
#include "contentmap.h"
#include "core.h"
#include "datetime.h"
#include "html.h"
#include "images.h"
#include "interpreter.h"
#include "markdown.h"
#include "parser.h"
#include "reader.h"
#include "sitemap.h"
#include "strings.h"
#include "template.h"

#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  Path *src_root;
  Path *dist_root;
  SymbolMap *symbol_map;
  ModuleMap *modules;
} BuildInfo;

static void import_build_info(BuildInfo *build_info, Env *env) {
  env_def("SRC_ROOT", path_to_string(build_info->src_root, env->arena), env);
  env_def("DIST_ROOT", path_to_string(build_info->dist_root, env->arena), env);
  env_export("SRC_ROOT", env);
  env_export("DIST_ROOT", env);
}

Module *get_template(const Path *name, Env *env) {
  Module *m = get_module(name, env->modules);
  if (m) {
    if (m->type != M_USER) {
      return NULL;
    }
    return m;
  }
  FILE *file = fopen(name->path, "r");
  if (!file) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "%s" SGR_RESET "\n", name->path, strerror(errno));
    return NULL;
  }
  Reader *reader = open_reader(file, name, env->symbol_map);
  TokenStream tokens = read_all(reader, 1);
  if (reader_errors(reader)) {
    close_reader(reader);
    fclose(file);
    return NULL;
  }
  m = parse(tokens, name);
  close_reader(reader);
  fclose(file);
  if (m->user_value.parse_error) {
    delete_module(m);
    return NULL;
  }
  add_module(m, env->modules);
  return m;
}

Env *create_template_env(Value data, Env *parent) {
  Arena *arena = create_arena();
  Env *env = create_env(arena, parent->modules, parent->symbol_map);
  import_core(env);
  import_strings(env);
  import_collections(env);
  import_datetime(env);
  import_contentmap(env);
  import_template(env);
  import_html(env);
  import_images(env);
  import_markdown(env);
  Env *export_env = create_env(env->arena, env->modules, env->symbol_map);
  if (data.type == V_OBJECT) {
    ObjectIterator it = iterate_object(data.object_value);
    Value entry_key, entry_value;
    while (object_iterator_next(&it, &entry_key, &entry_value)) {
      if (entry_key.type == V_SYMBOL) {
        env_put(entry_key.symbol_value, copy_value(entry_value, export_env), env);
      }
    }
  }
  for (size_t i = 0; i < parent->exports->size; i++) {
    if (parent->exports->cells[i].type == V_SYMBOL) {
      Symbol symbol = parent->exports->cells[i].symbol_value;
      Value value;
      if (env_get(symbol, &value, parent)) {
        env_put(symbol, copy_value(value, export_env), env);
      }
    }
  }
  return env;
}

void delete_template_env(Env *env) {
  delete_arena(env->arena);
}

Value eval_template(Module *module, Env *env) {
  if (module->type != M_USER) {
    return nil_value;
  }
  env_def("FILE", path_to_string(module->file_name, env->arena), env);
  Path *dir = path_get_parent(module->file_name);
  env_def("DIR", path_to_string(dir, env->arena), env);
  Value content = interpret(*module->user_value.root, env).value;
  Value layout;
  if (env_get_symbol("LAYOUT", &layout, env) && layout.type == V_STRING) {
    env_def("CONTENT", content, env);
    env_def("LAYOUT", nil_value, env);
    Path *layout_name = string_to_path(layout.string_value);
    Path *layout_path = path_join(dir, layout_name, 0);
    Module *layout_module = get_template(layout_path, env);
    if (layout_module) {
      content = eval_template(layout_module, env);
    }
    delete_path(layout_path);
    delete_path(layout_name);
  }
  delete_path(dir);
  return content;
}

static int eval_script(FILE *file, const Path *file_name, BuildInfo *build_info) {
  Reader *reader = open_reader(file, file_name, build_info->symbol_map);
  TokenStream tokens = read_all(reader, 0);
  if (reader_errors(reader)) {
    close_reader(reader);
    return 0;
  }
  Module *module = parse(tokens, file_name);
  close_reader(reader);
  if (module->user_value.parse_error) {
    delete_module(module);
    return 0;
  }
  add_module(module, build_info->modules);

  Env *env = create_user_env(module, build_info->modules, build_info->symbol_map);
  import_sitemap(env);
  import_contentmap(env);
  import_markdown(env);
  import_build_info(build_info, env);
  interpret(*module->user_value.root, env);
  compile_pages(env);
  delete_arena(env->arena);
  return 1;
}

Path *get_dist_path(const Path *path, Env *env) {
  const String *dir_string = get_env_string("DIST_ROOT", env);
  if (!dir_string) {
    env_error(env, -1, "missing or invalid DIST_ROOT");
    return NULL;
  }
  Path *dir = string_to_path(dir_string);
  Path *combined = path_join(dir, path, 1);
  delete_path(dir);
  return combined;
}

Path *string_to_src_path(const String *string, Env *env) {
  Path *path = string_to_path(string);
  Path *result = get_src_path(path, env);
  delete_path(path);
  return result;
}

Path *string_to_dist_path(const String *string, Env *env) {
  Path *path = string_to_path(string);
  Path *result = get_dist_path(path, env);
  delete_path(path);
  return result;
}

static Value path_to_web_path(const Path *path, Arena *arena) {
  Value web_path = allocate_string(path->size, arena);
#if defined(_WIN32)
  for (int32_t i = 0; i < path->size; i++) {
    if (path->path[i] == PATH_SEP) {
      web_path.string_value->bytes[i] = '/';
    } else {
      web_path.string_value->bytes[i] = path->path[i];
    }
  }
#else
  memcpy(web_path.string_value->bytes, path->path, path->size);
#endif
  return web_path;
}

Value get_web_path(const Path *path, int absolute, Env *env) {
  if (!path_is_descending(path)) {
    return copy_c_string("#invalid-path", env->arena);
  }
  String *web_path;
  if (strcmp(path_get_name(path), "index.html") == 0) {
    Path *dir = path_get_parent(path);
    web_path = path_to_web_path(dir, env->arena).string_value;
    delete_path(dir);
  } else {
    web_path = path_to_web_path(path, env->arena).string_value;
  }
  Value root_value;
  String *root = NULL;
  if (absolute) {
    if (env_get(get_symbol("ROOT_URL", env->symbol_map), &root_value, env) && root_value.type == V_STRING) {
      root = root_value.string_value;
    }
  } else if (env_get(get_symbol("ROOT_PATH", env->symbol_map), &root_value, env) && root_value.type == V_STRING) {
    root = root_value.string_value;
  }
  if (web_path->size) {
    StringBuffer buffer = create_string_buffer((root ? root->size : 0) + web_path->size + 1, env->arena);
    if (root) {
      string_buffer_append(&buffer, root);
    }
    if (!buffer.string->size || buffer.string->bytes[buffer.string->size - 1] != '/') {
      string_buffer_put(&buffer, '/');
    }
    if (web_path->bytes[0] != '/') {
      string_buffer_append(&buffer, web_path);
    } else if (web_path->size > 1) {
      string_buffer_append_bytes(&buffer, web_path->bytes + 1, web_path->size - 1);
    }
    return finalize_string_buffer(buffer);
  } else if (root) {
    return (Value) { .type = V_STRING, .string_value = root };
  } else {
    return copy_c_string("/", env->arena);
  }
}

Path *get_src_root(Env *env) {
  const String *src_root = get_env_string("SRC_ROOT", env);
  if (src_root) {
    return string_to_path(src_root);
  }
  return NULL;
}

Path *get_dist_root(Env *env) {
  const String *dist_root = get_env_string("DIST_ROOT", env);
  if (dist_root) {
    return string_to_path(dist_root);
  }
  return NULL;
}

int asset_has_changed(const Path *src, const Path *dest) {
  return get_mtime(src->path) != get_mtime(dest->path);
}

int copy_asset(const Path *src, const Path *dest) {
  int result = 0;
  if (!asset_has_changed(src, dest)) {
    return 1;
  }
  Path *dest_dir = path_get_parent(dest);
  if (mkdir_rec(dest_dir->path)) {
    result = copy_file(src->path, dest->path);
  }
  delete_path(dest_dir);
  return result;
}

int build(GlobalArgs args) {
  Path *cwd = get_cwd_path();
  Path *index_path = path_append(cwd, "index.tss");
  FILE *index = fopen(index_path->path, "r");
  if (!index) {
    while (1) {
      Path *parent = path_get_parent(cwd);
      if (parent->size >= cwd->size) {
        delete_path(parent);
        break;
      }
      delete_path(cwd);
      cwd = parent;
      delete_path(index_path);
      index_path = path_append(cwd, "index.tss");
      index = fopen(index_path->path, "r");
      if (index) {
        break;
      }
    }
  }
  if (index) {
    fprintf(stderr, INFO_LABEL "building %s" SGR_RESET "\n", index_path->path);
    BuildInfo build_info;
    build_info.src_root = cwd;
    build_info.dist_root = path_append(cwd, "dist");
    if (mkdir_rec(build_info.dist_root->path)) {
      build_info.symbol_map = create_symbol_map();
      build_info.modules = create_module_map();
      add_system_modules(build_info.modules);
      eval_script(index, index_path, &build_info);
      delete_symbol_map(build_info.symbol_map);
      delete_module_map(build_info.modules);
    }
    delete_path(build_info.dist_root);
    fclose(index);
  } else {
    fprintf(stderr, ERROR_LABEL "index.tss not found" SGR_RESET "\n");
  }
  delete_path(index_path);
  delete_path(cwd);
  return 0;
}

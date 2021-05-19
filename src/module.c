/* Plet
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "module.h"

#include "collections.h"
#include "contentmap.h"
#include "core.h"
#include "datetime.h"
#include "exec.h"
#include "hashmap.h"
#include "html.h"
#include "images.h"
#include "interpreter.h"
#include "parser.h"
#include "reader.h"
#include "sitemap.h"
#include "strings.h"
#include "util.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

struct ModuleMap {
  GenericHashMap map;
};

typedef struct {
  const Path *key;
  Module *value;
} ModuleEntry;

static Hash module_hash(const void *p) {
  const char *name = ((ModuleEntry *) p)->key->path;
  Hash h = INIT_HASH;
  while (*name) {
    h = HASH_ADD_BYTE(*name, h);
    name++;
  }
  return h;
}

static int module_equals(const void *a, const void *b) {
  return strcmp(((ModuleEntry *) a)->key->path, ((ModuleEntry *) b)->key->path) == 0;
}

ModuleMap *create_module_map(void) {
  ModuleMap *module_map = allocate(sizeof(ModuleMap));
  init_generic_hash_map(&module_map->map, sizeof(ModuleEntry), 0, module_hash, module_equals, NULL);
  return module_map;
}

void delete_module_map(ModuleMap *module_map) {
  ModuleEntry entry;
  HashMapIterator it = generic_hash_map_iterate(&module_map->map);
  while (generic_hash_map_next(&it, &entry)) {
    delete_module(entry.value);
  }
  delete_generic_hash_map(&module_map->map);
  free(module_map);
}

Module *get_module(const Path *file_name, ModuleMap *module_map) {
  ModuleEntry entry;
  ModuleEntry query;
  query.key = file_name;
  if (generic_hash_map_get(&module_map->map, &query, &entry)) {
    return entry.value;
  }
  return NULL;
}

void add_module(Module *module, ModuleMap *module_map) {
  ModuleEntry existing;
  int exists;
  generic_hash_map_set(&module_map->map, &(ModuleEntry) { .key = module->file_name, .value = module }, &exists, &existing);
  if (exists) {
    delete_module(existing.value);
  }
}

void add_system_module(const char *name, void (*import_func)(Env *), ModuleMap *module_map) {
  Module *module = allocate(sizeof(Module));
  module->type = M_SYSTEM;
  module->file_name = create_path(name, -1);
  module->mtime = 0;
  module->dirty = 0;
  module->system_value.import_func = import_func;
  add_module(module, module_map);
}

void add_system_modules(ModuleMap *module_map) {
  add_system_module("core", import_core, module_map);
  add_system_module("strings", import_strings, module_map);
  add_system_module("collections", import_collections, module_map);
  add_system_module("datetime", import_datetime, module_map);
  add_system_module("exec", import_exec, module_map);
  add_system_module("images", import_images, module_map);
  add_system_module("html", import_html, module_map);
  add_system_module("sitemap", import_sitemap, module_map);
  add_system_module("contentmap", import_contentmap, module_map);
}

Module *create_module(const Path *file_name, ModuleType type) {
  Module *module = allocate(sizeof(Module));
  module->type = type;
  module->file_name = copy_path(file_name);
  module->mtime = get_mtime(file_name->path);
  module->dirty = 0;
  switch (type) {
    case M_SYSTEM:
      module->system_value.import_func = NULL;
      break;
    case M_USER:
      module->user_value.root = NULL;
      module->user_value.parse_error = 0;
      break;
    case M_DATA:
      module->data_value.root = NULL;
      module->data_value.parse_error = 0;
      break;
    case M_ASSET:
      module->asset_value.width = -1;
      module->asset_value.height = -1;
      break;
  }
  return module;
}

void delete_module(Module *module) {
  switch (module->type) {
    case M_USER:
      DELETE_NODE(module->user_value.root);
      break;
    case M_DATA:
      DELETE_NODE(module->data_value.root);
      break;
    case M_SYSTEM:
    case M_ASSET:
      break;
  }
  free(module->file_name);
  free(module);
}

Path *get_src_path(const Path *path, Env *env) {
  const String *dir_string = get_env_string("DIR", env);
  if (!dir_string) {
    env_error(env, -1, "missing or invalid DIR");
    return NULL;
  }
  Path *dir = string_to_path(dir_string);
  Path *combined = path_join(dir, path, 0);
  delete_path(dir);
  return combined;
}

Module *load_user_module(const Path *name, Env *env) {
  Module *m = get_module(name, env->modules);
  if (m && !m->dirty) {
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
  if (reader_errors(reader)) {
    close_reader(reader);
    fclose(file);
    return NULL;
  }
  TokenStream tokens = read_all(reader, 0);
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

Module *load_data_module(const Path *name, Env *env) {
  Module *m = get_module(name, env->modules);
  if (m && !m->dirty) {
    if (m->type != M_DATA) {
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
  if (reader_errors(reader)) {
    close_reader(reader);
    fclose(file);
    return NULL;
  }
  TokenStream tokens = read_all(reader, 0);
  m = parse_object_notation(tokens, name, 1);
  close_reader(reader);
  fclose(file);
  if (m->user_value.parse_error) {
    delete_module(m);
    return NULL;
  }
  add_module(m, env->modules);
  return m;
}

Module *load_asset_module(const Path *name, Env *env) {
  Module *m = get_module(name, env->modules);
  if (m && !m->dirty) {
    return m;
  }
  m = create_module(name, M_ASSET);
  add_module(m, env->modules);
  return m;
}

Value read_asset_module(const Path *name, Env *env) {
  Module *m = get_module(name, env->modules);
  if (m && !m->dirty) {
    if (m->type != M_ASSET) {
      return nil_value;
    }
  } else {
    m = create_module(name, M_ASSET);
    add_module(m, env->modules);
  }
  FILE *file = fopen(name->path, "r");
  if (!file) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "%s" SGR_RESET "\n", name->path, strerror(errno));
    return nil_value;
  }
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
  Value content = nil_value;
  if (!feof(file)) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "read error: %s" SGR_RESET "\n", name->path, strerror(errno));
  } else {
    content = create_string(buffer.data, buffer.size, env->arena);
  }
  delete_buffer(buffer);
  fclose(file);
  return content;
}

Module *load_module(const Path *name, Env *env) {
  Module *m = get_module(name, env->modules);
  if (m && !m->dirty) {
    return m;
  }
  Path *path = get_src_path(name, env);
  if (!path) {
    return NULL;
  }
  const char *extension = path_get_extension(path);
  // TODO: case insensitive
  if (strcmp(extension, "plet") == 0) {
    m = load_user_module(path, env);
  } else if (strcmp(extension, "json") == 0 || strcmp(extension, "tson") == 0) {
    m = load_data_module(path, env);
  } else {
    m = load_asset_module(path, env);
  }
  delete_path(path);
  return m;
}

Env *create_user_env(Module *module, ModuleMap *modules, SymbolMap *symbol_map) {
  Arena *arena = create_arena();
  Env *env = create_env(arena, modules, symbol_map);
  import_core(env);
  import_strings(env);
  import_collections(env);
  import_datetime(env);
  import_exec(env);
  env_def("FILE", path_to_string(module->file_name, env->arena), env);
  Path *dir = path_get_parent(module->file_name);
  env_def("DIR", path_to_string(dir, env->arena), env);
  delete_path(dir);
  return env;
}

Value import_module(Module *module, Env *env) {
  switch (module->type) {
    case M_SYSTEM:
      module->system_value.import_func(env);
      return nil_value;
    case M_USER: {
      Env *user_env = create_user_env(module, env->modules, env->symbol_map);
      Value result_value = nil_value;
      InterpreterResult result = interpret(*module->user_value.root, user_env);
      Env *export_env = create_env(env->arena, env->modules, env->symbol_map);
      if (result.type == IR_RETURN) {
        result_value = copy_value(result.value, export_env);
      }
      for (size_t i = 0; i < user_env->exports->size; i++) {
        if (user_env->exports->cells[i].type == V_SYMBOL) {
          Symbol symbol = user_env->exports->cells[i].symbol_value;
          Value value;
          if (env_get(symbol, &value, user_env)) {
            env_put(symbol, copy_value(value, export_env), env);
          }
        }
      }
      delete_arena(user_env->arena);
      return result_value;
    }
    case M_DATA:
      return interpret(*module->data_value.root, env).value;
    case M_ASSET:
      return path_to_string(module->file_name, env->arena);
  }
  return nil_value;
}

int detect_changes(ModuleMap *modules) {
  int changed = 0;
  ModuleEntry entry;
  HashMapIterator it = generic_hash_map_iterate(&modules->map);
  while (generic_hash_map_next(&it, &entry)) {
    if (entry.value->type == M_SYSTEM) {
      continue;
    }
    if (entry.value->dirty || entry.value->mtime != get_mtime(entry.value->file_name->path)) {
      entry.value->dirty = 1;
      changed = 1;
    }
  }
  return changed;
}

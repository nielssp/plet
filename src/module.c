/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "module.h"

#include "hashmap.h"
#include "parser.h"
#include "reader.h"
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
  generic_hash_map_add(&module_map->map, &(ModuleEntry) { .key = module->file_name, .value = module });
}

Module *create_module(const Path *file_name, ModuleType type) {
  Module *module = allocate(sizeof(Module));
  module->type = type;
  module->file_name = copy_path(file_name);
  module->mtime = get_mtime(file_name->path);
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
  if (m) {
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
  if (m) {
    if (m->type != M_ASSET) {
      return NULL;
    }
    return m;
  }
  return create_module(name, M_ASSET);
}

Module *load_module(const Path *name, Env *env) {
  Module *m = get_module(name, env->modules);
  if (m) {
    return m;
  }
  Path *path = get_src_path(name, env);
  if (!path) {
    return NULL;
  }
  const char *extension = path_get_extension(path);
  // TODO: case insensitive
  if (strcmp(extension, "tss") == 0) {
    m = load_user_module(path, env);
  } else if (strcmp(extension, "json") == 0 || strcmp(extension, "tson") == 0) {
    m = load_data_module(path, env);
  } else {
    m = load_asset_module(path, env);
  }
  delete_path(path);
  return m;
}

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
  module->mtime = (time_t) 0;
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

Module *load_module(const Path *name, Env *env) {
  Module *m = get_module(name, env->modules);
  if (m) {
    return m;
  }
  return m;
}

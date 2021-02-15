/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "module.h"

#include "hashmap.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

struct ModuleMap {
  GenericHashMap map;
};

typedef struct {
  const char *key;
  Module *value;
} ModuleEntry;

static Hash module_hash(const void *p) {
  const char *name = ((ModuleEntry *) p)->key;
  Hash h = INIT_HASH;
  while (*name) {
    h = HASH_ADD_BYTE(*name, h);
    name++;
  }
  return h;
}

static int module_equals(const void *a, const void *b) {
  return strcmp(((ModuleEntry *) a)->key, ((ModuleEntry *) b)->key) == 0;
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

Module *get_module(const char *file_name, ModuleMap *module_map) {
  Module *module;
  ModuleEntry query;
  query.key = file_name;
  if (generic_hash_map_get(&module_map->map, &query, &module)) {
    return module;
  }
  return NULL;
}

void add_module(Module *module, ModuleMap *module_map) {
  generic_hash_map_add(&module_map->map, &(ModuleEntry) { .key = module->file_name, .value = module });
}

Module *create_module(const char *file_name) {
  Module *module = allocate(sizeof(Module));
  module->file_name = copy_string(file_name);
  module->root = NULL;
  return module;
}

void delete_module(Module *module) {
  DELETE_NODE(module->root);
  free(module->file_name);
  free(module);
}

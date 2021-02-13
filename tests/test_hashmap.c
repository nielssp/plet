/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "../src/hashmap.h"

#include "test.h"

#include <string.h>

typedef struct {
  char *key;
  char *value;
} DictEntry;

static Hash dict_hash(const void *p) {
  char *s = ((DictEntry *) p)->key;
  Hash h = INIT_HASH;
  while (*s) {
    h = HASH_ADD_BYTE(*(s++), h);
  }
  return h;
}

static int dict_equals(const void *a, const void *b) {
  return strcmp(((DictEntry *) a)->key, ((DictEntry *) b )->key) == 0;
}

static void test_iterator(void) {
  GenericHashMap dict;
  init_generic_hash_map(&dict, sizeof(DictEntry), 0, dict_hash, dict_equals, NULL);
  generic_hash_map_add(&dict, &(DictEntry) { .key = "key1", .value = "value1" });
  generic_hash_map_add(&dict, &(DictEntry) { .key = "key2", .value = "value2" });
  generic_hash_map_add(&dict, &(DictEntry) { .key = "key3", .value = "value3" });
  HashMapIterator it = generic_hash_map_iterate(&dict);
  int seen1 = 0;
  int seen2 = 0;
  int seen3 = 0;
  DictEntry entry;
  while (generic_hash_map_next(&it, &entry)) {
    if (strcmp(entry.key, "key1") == 0) {
      assert(!seen1);
      assert(strcmp(entry.value, "value1") == 0);
      seen1 = 1;
    } else if (strcmp(entry.key, "key2") == 0) {
      assert(!seen2);
      assert(strcmp(entry.value, "value2") == 0);
      seen2 = 1;
    } else if (strcmp(entry.key, "key3") == 0) {
      assert(!seen3);
      assert(strcmp(entry.value, "value3") == 0);
      seen3 = 1;
    } else {
      assert(0 && "Unknown key");
    }
  }
  assert(seen1);
  assert(seen2);
  assert(seen3);
  delete_generic_hash_map(&dict);
}

void test_hashmap(void) {
  run_test(test_iterator);
}


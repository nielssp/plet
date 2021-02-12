/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef HASHMAP_H
#define HASHMAP_H

#include "util.h"

#include <stddef.h>
#include <stdint.h>

#define GET_BYTE(N, OBJ) (((uint8_t *)&(OBJ))[N])
#define HASH_ADD_BYTE(BYTE, HASH) (((HASH) * FNV_PRIME) ^ (BYTE))

#if UINT64_MAX == UINTPTR_MAX
#define HASH_SIZE_64
typedef uint64_t Hash;
#define INIT_HASH 0xcbf29ce484222325ul
#define FNV_PRIME 1099511628211ul
#define HASH_ADD_PTR(PTR, HASH) \
  HASH_ADD_BYTE(GET_BYTE(0, PTR), \
      HASH_ADD_BYTE(GET_BYTE(1, PTR), \
        HASH_ADD_BYTE(GET_BYTE(2, PTR), \
          HASH_ADD_BYTE(GET_BYTE(3, PTR), \
            HASH_ADD_BYTE(GET_BYTE(4, PTR), \
              HASH_ADD_BYTE(GET_BYTE(5, PTR), \
                HASH_ADD_BYTE(GET_BYTE(6, PTR), \
                  HASH_ADD_BYTE(GET_BYTE(7, PTR), HASH))))))))
#else
#define HASH_SIZE_32
typedef uint32_t Hash;
#define INIT_HASH 0x811c9dc5
#define FNV_PRIME 16777619
#define HASH_ADD_PTR(PTR, HASH) \
  HASH_ADD_BYTE(GET_BYTE(0, PTR), \
      HASH_ADD_BYTE(GET_BYTE(1, PTR), \
        HASH_ADD_BYTE(GET_BYTE(2, PTR), \
          HASH_ADD_BYTE(GET_BYTE(3, PTR), HASH))))
#endif

typedef Hash (* HashFunc)(const void *);
typedef int (* EqualityFunc)(const void *, const void *);

typedef struct {
  Hash hash;
  char defined;
  char deleted;
} GenericBucket;

typedef struct {
  size_t size;
  size_t capacity;
  size_t mask;
  size_t upper_cap;
  size_t lower_cap;
  size_t entry_size;
  size_t bucket_size;
  size_t bucket_size_shift;
  HashFunc hash_code_func;
  EqualityFunc equals_func;
  Arena *arena;
  uint8_t *buckets;
} GenericHashMap;

typedef struct {
  GenericHashMap *map;
  size_t next_bucket;
} HashMapIterator;

int init_generic_hash_map(GenericHashMap *map, size_t entry_size, size_t initial_capacity,
    HashFunc hash_code_func, EqualityFunc equals_func, Arena *arena);

void delete_generic_hash_map(GenericHashMap *map);

HashMapIterator generic_hash_map_iterate(GenericHashMap *map);

int generic_hash_map_next(HashMapIterator *iterator, void *result);

int generic_hash_map_resize(GenericHashMap *map, size_t new_capacity);

int generic_hash_map_add(GenericHashMap *map, const void *entry);

int generic_hash_map_set(GenericHashMap *map, const void *entry, int *exists, void *existing);

int generic_hash_map_remove(GenericHashMap *map, const void *entry, void *removed);

int generic_hash_map_get(GenericHashMap *map, const void *entry, void *result);

#endif

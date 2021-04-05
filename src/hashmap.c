/* Plet
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "hashmap.h"

#include <stdlib.h>
#include <string.h>

int init_generic_hash_map(GenericHashMap *map, size_t entry_size, size_t initial_capacity,
    HashFunc hash_code_func, EqualityFunc equals_func, Arena *arena) {
  map->size = 0;
  map->capacity = 8;
  if (initial_capacity > 8) {
    while (map->capacity < initial_capacity) {
      map->capacity <<= 1;
    }
  }
  map->mask = map->capacity - 1;
  map->upper_cap = map->capacity / 2;
  map->lower_cap = map->capacity / 8;
  map->hash_code_func = hash_code_func;
  map->equals_func = equals_func;
  map->entry_size = entry_size;
  map->bucket_size_shift = 0;
  map->arena = arena;
  size_t bucket_size = sizeof(GenericBucket) + entry_size;
  if (bucket_size > 1) {
    bucket_size--;
    do {
      map->bucket_size_shift++;
    } while (bucket_size >>= 1);
  }
  map->bucket_size = 1 << map->bucket_size_shift;
  size_t size = map->capacity * map->bucket_size;
  map->buckets = arena_allocate(size, map->arena);
  memset(map->buckets, 0, size);
  return 1;
}

void delete_generic_hash_map(GenericHashMap *map) {
  if (!map->arena) {
    free(map->buckets);
  }
}

HashMapIterator generic_hash_map_iterate(GenericHashMap *map) {
  return (HashMapIterator){ .map = map, .next_bucket =  0 };
}

int generic_hash_map_next(HashMapIterator *iterator, void *result) {
  GenericBucket *current = NULL;
  while (iterator->next_bucket < iterator->map->capacity) {
    current = (GenericBucket *)(iterator->map->buckets + (iterator->next_bucket << iterator->map->bucket_size_shift));
    iterator->next_bucket++;
    if (current->defined && !current->deleted) {
      memcpy(result, current + 1, iterator->map->entry_size);
      return 1;
    }
  }
  return 0;
}

int generic_hash_map_resize(GenericHashMap *map, size_t new_capacity) {
  size_t old_capacity = map->capacity;
  uint8_t *old_buckets = map->buckets;
  map->capacity = new_capacity;
  size_t size = map->capacity * map->bucket_size;
  map->buckets = arena_allocate(size, map->arena);
  memset(map->buckets, 0, size);
  map->mask = new_capacity - 1;
  map->upper_cap = map->capacity / 2;
  map->lower_cap = map->capacity / 8;
  uint8_t *next = old_buckets;
  for (size_t i = 0; i < old_capacity; i++, next += map->bucket_size) {
    GenericBucket *old_bucket = (GenericBucket *)next;
    if (old_bucket->defined && !old_bucket->deleted) {
      Hash hash_code = old_bucket->hash;
      Hash hash = hash_code & map->mask;
      GenericBucket *bucket = (GenericBucket *)(map->buckets + (hash << map->bucket_size_shift));
      while (bucket->defined) {
        hash = (hash + 1) & map->mask;
        bucket = (GenericBucket *)(map->buckets + (hash << map->bucket_size_shift));
      }
      bucket->hash = hash_code;
      bucket->defined = 1;
      memcpy(bucket + 1, old_bucket + 1, map->entry_size);
    }
  }
  if (!map->arena) {
    free(old_buckets);
  }
  return 1;
}

int generic_hash_map_add(GenericHashMap *map, const void *entry) {
  if (map->size > map->upper_cap) {
    if (!generic_hash_map_resize(map, map->capacity << 1)) {
      return 0;
    }
  }
  Hash hash_code = map->hash_code_func(entry);
  Hash hash = hash_code & map->mask;
  GenericBucket *bucket = (GenericBucket *)(map->buckets + (hash << map->bucket_size_shift));
  while (bucket->defined && !bucket->deleted) {
    if (map->equals_func(bucket + 1, entry)) {
      return 0;
    }
    hash = (hash + 1) & map->mask;
    bucket = (GenericBucket *)(map->buckets + (hash << map->bucket_size_shift));
  }
  bucket->hash = hash_code;
  bucket->defined = 1;
  bucket->deleted = 0;
  memcpy(bucket + 1, entry, map->entry_size);
  map->size++;
  return 1;
}

int generic_hash_map_set(GenericHashMap *map, const void *entry, int *exists, void *existing) {
  if (exists) {
    *exists = 0;
  }
  if (map->size > map->upper_cap) {
    if (!generic_hash_map_resize(map, map->capacity << 1)) {
      return 0;
    }
  }
  Hash hash_code = map->hash_code_func(entry);
  Hash hash = hash_code & map->mask;
  GenericBucket *bucket = (GenericBucket *)(map->buckets + (hash << map->bucket_size_shift));
  while (bucket->defined && !bucket->deleted) {
    if (map->equals_func(bucket + 1, entry)) {
      if (exists) {
        *exists = 1;
      }
      if (existing) {
        memcpy(existing, bucket + 1, map->entry_size);
      }
      map->size--;
      break;
    }
    hash = (hash + 1) & map->mask;
    bucket = (GenericBucket *)(map->buckets + (hash << map->bucket_size_shift));
  }
  bucket->hash = hash_code;
  bucket->defined = 1;
  bucket->deleted = 0;
  memcpy(bucket + 1, entry, map->entry_size);
  map->size++;
  return 1;
}

int generic_hash_map_remove(GenericHashMap *map, const void *entry, void *removed) {
  Hash hash_code = map->hash_code_func(entry);
  Hash hash = hash_code & map->mask;
  GenericBucket *bucket = (GenericBucket *)(map->buckets + (hash << map->bucket_size_shift));
  while (bucket->defined) {
    if (!bucket->deleted && hash_code == bucket->hash && map->equals_func(bucket + 1, entry)) {
      if (removed) {
        memcpy(removed, bucket + 1, map->entry_size);
      }
      bucket->deleted = 1;
      map->size--;
      if (!map->arena && map->size < map->lower_cap && map->capacity > 8) {
        generic_hash_map_resize(map, map->capacity >> 1);
      }
      return 1;
    }
    hash = (hash + 1) & map->mask;
    bucket = (GenericBucket *)(map->buckets + (hash << map->bucket_size_shift));
  }
  return 0;
}

int generic_hash_map_get(GenericHashMap *map, const void *entry, void *result) {
  Hash hash_code = map->hash_code_func(entry);
  Hash hash = hash_code & map->mask;
  GenericBucket *bucket = (GenericBucket *)(map->buckets + (hash << map->bucket_size_shift));
  while (bucket->defined) {
    if (!bucket->deleted && hash_code == bucket->hash && map->equals_func(bucket + 1, entry)) {
      if (result) {
        memcpy(result, bucket + 1, map->entry_size);
      }
      return 1;
    }
    hash = (hash + 1) & map->mask;
    bucket = (GenericBucket *)(map->buckets + (hash << map->bucket_size_shift));
  }
  return 0;
}

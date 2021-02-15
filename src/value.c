/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "value.h"

#include "util.h"

#include <inttypes.h>
#include <string.h>

#define INITIAL_ARRAY_CAPACITY 16

typedef struct RefStack RefStack;
struct RefStack {
  RefStack *next;
  void *old;
  void *new;
};

static Hash entry_hash(const void *entry) {
  return value_hash(INIT_HASH, ((Entry *) entry)->key);
}

static int entry_equals(const void *a, const void *b) {
  return equals(((Entry *) a)->key, ((Entry *) b)->key);
}

String *empty_string = &(String) { .size = 0 };

Env *create_env(Arena *arena, ModuleMap *modules) {
  Env *env = arena_allocate(sizeof(Env), arena);
  env->arena = arena;
  env->modules = modules;
  init_generic_hash_map(&env->global, sizeof(Entry), 0, entry_hash, entry_equals, arena);
  return env;
}

void env_put(Symbol symbol, Value value, Env *env) {
  generic_hash_map_set(&env->global, &(Entry) { .key = create_symbol(symbol), .value = value }, NULL, NULL);\
}

int env_get(Symbol name, Value *value, Env *env) {
  Entry entry;
  if (generic_hash_map_get(&env->global, &(Entry) { .key = create_symbol(name) }, &entry)) {
    *value = entry.value;
    return 1;
  }
  return 0;
}

int equals(Value a, Value b) {
  if (a.type != b.type) {
    return 0;
  }
  switch (a.type) {
    case V_NIL:
    case V_TRUE:
      return 1;
    case V_INT:
      return a.int_value == b.int_value;
    case V_FLOAT:
      return a.float_value == b.float_value;
    case V_SYMBOL:
      return a.symbol_value == b.symbol_value;
    case V_STRING:
      if (a.string_value == b.string_value) {
        return 1;
      }
      if (a.string_value->size != b.string_value->size) {
        return 0;
      }
      return memcmp(a.string_value->bytes, b.string_value->bytes, a.string_value->size) == 0;
    case V_ARRAY:
      if (a.array_value == b.array_value) {
        return 1;
      }
      if (a.array_value->size != b.array_value->size) {
        return 0;
      }
      for (size_t i = 0; i < a.array_value->size; i++) {
        if (!equals(a.array_value->cells[i], b.array_value->cells[i])) {
          return 0;
        }
      }
      return 1;
    case V_OBJECT:
      if (a.object_value == b.object_value) {
        return 1;
      }
      if (a.object_value->size != b.object_value->size) {
        return 0;
      }
      for (size_t i = 0; i < a.object_value->size; i++) {
        Value value;
        if (!object_get(a.object_value, a.object_value->entries[i].key, &value)) {
          return 0;
        }
        if (!equals(a.object_value->entries[i].value, value)) {
          return 0;
        }
      }
      return 1;
    case V_TIME:
      return a.time_value == b.time_value;
    case V_FUNCTION:
      return a.function_value == b.function_value;
    case V_CLOSURE:
      return a.closure_value == b.closure_value;
  }
  return 0;
}

int is_truthy(Value value) {
  switch (value.type) {
    case V_NIL:
      return 0;
    case V_TRUE:
      return 1;
    case V_INT:
      return value.int_value != 0;
    case V_FLOAT:
      return value.float_value != 0.0;
    case V_SYMBOL:
      return 1;
    case V_STRING:
      return value.string_value->size > 0;
    case V_ARRAY:
      return value.array_value->size > 0;
    case V_OBJECT:
      return value.object_value->size > 0;
    case V_TIME:
    case V_FUNCTION:
    case V_CLOSURE:
      return 1;
  }
  return 0;
}

Hash value_hash(Hash h, Value value) {
  h = HASH_ADD_BYTE(value.type, h);
  switch (value.type) {
    case V_NIL:
    case V_TRUE:
      break;
    case V_INT:
      for (int i = 0; i < sizeof(int64_t); i++) {
        h = HASH_ADD_BYTE(GET_BYTE(i, value.int_value), h);
      }
      break;
    case V_FLOAT:
      for (int i = 0; i < sizeof(double); i++) {
        h = HASH_ADD_BYTE(GET_BYTE(i, value.int_value), h);
      }
      break;
    case V_SYMBOL:
      h = HASH_ADD_PTR(value.symbol_value, h);
      break;
    case V_STRING:
      for (size_t i = 0; i < value.string_value->size; i++) {
        h = HASH_ADD_BYTE(value.string_value->bytes[i], h);
      }
      break;
    case V_ARRAY:
      for (size_t i = 0; i < value.array_value->size; i++) {
        h = value_hash(h, value.array_value->cells[i]);
      }
      break;
    case V_OBJECT:
      for (size_t i = 0; i < value.object_value->size; i++) {
        h = value_hash(h, value.object_value->entries[i].key);
        h = value_hash(h, value.object_value->entries[i].value);
      }
      break;
    case V_TIME:
      for (int i = 0; i < sizeof(time_t); i++) {
        h = HASH_ADD_BYTE(GET_BYTE(i, value.int_value), h);
      }
      break;
    case V_FUNCTION:
      h = HASH_ADD_PTR(value.function_value, h);
      break;
    case V_CLOSURE:
      h = HASH_ADD_PTR(value.closure_value, h);
      break;
  }
  return h;
}

static void *get_existing_ref(RefStack *ref_stack, void *old) {
  while (ref_stack) {
    if (ref_stack->old == old) {
      return ref_stack->new;
    }
    ref_stack = ref_stack->next;
  }
  return NULL;
}

static Value copy_value_detect_cycles(Value value, Arena *arena, RefStack *ref_stack) {
  switch (value.type) {
    case V_NIL:
    case V_TRUE:
    case V_INT:
    case V_FLOAT:
    case V_SYMBOL:
      return value;
    case V_STRING:
      return create_string(value.string_value->bytes, value.string_value->size, arena);
    case V_ARRAY: {
      Array *existing = get_existing_ref(ref_stack, value.array_value);
      if (existing) {
        return (Value) { .type = V_ARRAY, .array_value = existing };
      }
      Value copy = create_array(value.array_value->size, arena);
      RefStack nested = (RefStack) { .next = ref_stack, .old = value.array_value, .new = copy.array_value };
      for (size_t i = 0; i < value.array_value->size; i++) {
        array_push(copy.array_value, copy_value_detect_cycles(value.array_value->cells[i], arena, &nested), arena);
      }
      return copy;
    }
    case V_OBJECT: {
      Object *existing = get_existing_ref(ref_stack, value.object_value);
      if (existing) {
        return (Value) { .type = V_OBJECT, .object_value = existing };
      }
      Value copy = create_object(value.object_value->size, arena);
      RefStack nested = (RefStack) { .next = ref_stack, .old = value.object_value, .new = copy.object_value };
      for (size_t i = 0; i < value.object_value->size; i++) {
        object_put(copy.object_value, copy_value_detect_cycles(value.object_value->entries[i].key, arena, &nested),
            copy_value_detect_cycles(value.object_value->entries[i].value, arena, &nested), arena);
      }
      return copy;
    }
    case V_TIME:
    case V_FUNCTION:
      return value;
    case V_CLOSURE: {
      Closure *existing = get_existing_ref(ref_stack, value.closure_value);
      if (existing) {
        return (Value) { .type = V_CLOSURE, .closure_value = existing };
      }
      Closure *copy = arena_allocate(sizeof(Closure), arena);
      copy->params = value.closure_value->params;
      copy->body = value.closure_value->body;
      RefStack nested = (RefStack) { .next = ref_stack, .old = value.closure_value, .new = copy };
      copy->env = copy_value_detect_cycles((Value) { .type = V_OBJECT, .object_value = value.closure_value->env },
          arena, &nested).object_value;
      return (Value) { .type = V_CLOSURE, .closure_value = copy };
    }
  }
  return nil_value;
}

Value copy_value(Value value, Arena *arena) {
  return copy_value_detect_cycles(value, arena, NULL);
}

void value_to_string(Value value, Buffer *buffer) {
  switch (value.type) {
    case V_NIL:
      break;
    case V_TRUE:
      buffer_printf(buffer, "true");
      break;
    case V_INT:
      buffer_printf(buffer, "%" PRId64, value.int_value);
      break;
    case V_FLOAT:
      buffer_printf(buffer, "%lg", value.float_value);
      break;
    case V_SYMBOL:
      buffer_printf(buffer, "%s", value.symbol_value);
      break;
    case V_STRING:
      for (size_t i = 0; i < value.string_value->size; i++) {
        buffer_put(buffer, value.string_value->bytes[i]);
      }
      break;
    case V_ARRAY:
    case V_OBJECT:
      break;
    case V_TIME:
      // TODO: ISO 8601
      break;
    case V_FUNCTION:
    case V_CLOSURE:
      break;
  }
}

const char *value_name(ValueType type) {
  switch (type) {
    case V_NIL:
      return "nil";
    case V_TRUE:
      return "true";
    case V_INT:
      return "int";
    case V_FLOAT:
      return "float";
    case V_SYMBOL:
      return "symbol";
    case V_STRING:
      return "string";
    case V_ARRAY:
      return "array";
    case V_OBJECT:
      return "object";
    case V_TIME:
      return "time";
    case V_FUNCTION:
    case V_CLOSURE:
      return "function";
  }
  return "";
}

Value create_string(const uint8_t *bytes, size_t size, Arena *arena) {
  if (!size) {
    return (Value) { .type = V_STRING, .string_value = empty_string };
  }
  String *string = arena_allocate(sizeof(String) + size, arena);
  string->size = size;
  memcpy(string->bytes, bytes, size);
  return (Value) { .type = V_STRING, .string_value = string };
}

Value create_array(size_t capacity, Arena *arena) {
  Array *array = arena_allocate(sizeof(Array), arena);
  array->capacity = capacity < INITIAL_ARRAY_CAPACITY ? INITIAL_ARRAY_CAPACITY : capacity;
  array->size = 0;
  array->cells = arena_allocate(array->capacity * sizeof(Value), arena);
  return (Value) { .type = V_ARRAY, .array_value = array };
}

void array_push(Array *array, Value elem, Arena *arena) {
  if (array->size >= array->capacity) {
    size_t new_capacity = array->capacity << 1;
    Value *new_cells = arena_allocate(new_capacity * sizeof(Value), arena);
    memcpy(new_cells, array->cells, array->capacity * sizeof(Value));
    array->capacity = new_capacity;
  }
  array->cells[array->size] = elem;
  array->size++;
}

int array_pop(Array *array, Value *elem) {
  if (array->size) {
    array->size--;
    *elem = array->cells[array->size];
    return 1;
  }
  return 0;
}

Value create_object(size_t capacity, Arena *arena) {
  Object *object = arena_allocate(sizeof(Object), arena);
  object->capacity = capacity < INITIAL_ARRAY_CAPACITY ? INITIAL_ARRAY_CAPACITY : capacity;
  object->size = 0;
  object->entries = arena_allocate(object->capacity * sizeof(Entry), arena);
  return (Value) { .type = V_OBJECT, .object_value = object };
}

void object_put(Object *object, Value key, Value value, Arena *arena) {
  Value old;
  object_remove(object, key, &old);
  if (object->size >= object->capacity) {
    object->capacity <<= 1;
    size_t new_capacity = object->capacity << 1;
    Entry *new_entries = arena_allocate(new_capacity * sizeof(Entry), arena);
    memcpy(new_entries, object->entries, object->capacity * sizeof(Entry));
    object->capacity = new_capacity;
  }
  object->entries[object->size] = (Entry) { .key = key, .value = value };
  object->size++;
}

int object_get(Object *object, Value key, Value *value) {
  for (size_t i = 0; i < object->size; i++) {
    if (equals(key, object->entries[i].key)) {
      *value = object->entries[i].value;
      return 1;
    }
  }
  return 0;
}

int object_remove(Object *object, Value key, Value *value) {
  for (size_t i = 0; i < object->size; i++) {
    if (equals(key, object->entries[i].key)) {
      *value = object->entries[i].value;
      if (i < object->size - 1) {
        memmove(&object->entries[i], &object->entries[i + 1], (object->size - i - 1) * sizeof(Entry));
      }
      object->size--;
      return 1;
    }
  }
  return 0;
}

ObjectIterator iterate_object(Object *object) {
  return (ObjectIterator) { .object = object, .next_index = 0 };
}

int object_iterator_next(ObjectIterator *it, Value *key, Value *value) {
  if (it->next_index < it->object->size) {
    if (key) {
      *key = it->object->entries[it->next_index].key;
    }
    if (value) {
      *value = it->object->entries[it->next_index].value;
    }
    it->next_index++;
    return 1;
  }
  return 0;
}

Value create_closure(NameList *params, NameList *free_variables, Node body, Env *env, Arena *arena) {
  Closure *closure = arena_allocate(sizeof(Closure), arena);
  closure->params = params;
  closure->body = body;
  closure->env = create_object(0, arena).object_value;
  for (NameList *name = free_variables; name; name = name->tail) {
    Value value;
    if (env_get(name->head, &value, env)) {
      object_put(closure->env, create_symbol(name->head), value, arena);
    }
  }
  return (Value) { .type = V_CLOSURE, .closure_value = closure };
}

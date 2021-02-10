/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "value.h"

#include "util.h"

#include <string.h>

#define INITIAL_ARRAY_CAPACITY 16

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

Value copy_value(Value value) {
  switch (value.type) {
    case V_NIL:
    case V_TRUE:
    case V_INT:
    case V_FLOAT:
      return value;
    case V_STRING:
      return create_string(value.string_value->bytes, value.string_value->size);
    case V_ARRAY: {
      Value copy = create_array();
      for (size_t i = 0; i < value.array_value->size; i++) {
        array_push(copy.array_value, copy_value(value.array_value->cells[i]));
      }
      return copy;
    }
    case V_OBJECT: {
      Value copy = create_object();
      for (size_t i = 0; i < value.object_value->size; i++) {
        object_put(copy.object_value, copy_value(value.object_value->entries[i].key),
            copy_value(value.object_value->entries[i].value));
      }
      return copy;
    }
    case V_TIME:
    case V_FUNCTION:
      return value;
    case V_CLOSURE:
      // TODO: copy nodes and env
      return nil_value;
  }
  return nil_value;
}

void delete_value(Value value) {
  switch (value.type) {
    case V_NIL:
    case V_TRUE:
    case V_INT:
    case V_FLOAT:
      return;
    case V_STRING:
      free(value.string_value);
      return;
    case V_ARRAY:
      free(value.array_value->cells);
      free(value.array_value);
      return;
    case V_OBJECT:
      free(value.object_value->entries);
      free(value.object_value);
      return;
    case V_TIME:
    case V_FUNCTION:
      return;
    case V_CLOSURE:
      // TODO
      return;
  }
}

Value create_string(const uint8_t *bytes, size_t size) {
  String *string = allocate(sizeof(String) + size);
  string->size = size;
  memcpy(string->bytes, bytes, size);
  return (Value) { .type = V_STRING, .string_value = string };
}

Value create_array() {
  Array *array = allocate(sizeof(Array));
  array->capacity = INITIAL_ARRAY_CAPACITY;
  array->size = 0;
  array->cells = allocate(sizeof(Value) * array->capacity);
  return (Value) { .type = V_ARRAY, .array_value = array };
}

void array_push(Array *array, Value elem) {
  if (array->size >= array->capacity) {
    array->capacity <<= 1;
    array->cells = reallocate(array->cells, sizeof(Value) * array->capacity);
  }
  array->cells[array->size] = elem;
  array->size++;
}

Result array_pop(Array *array, Value *elem) {
  if (array->size) {
    array->size--;
    *elem = array->cells[array->size];
    return 1;
  }
  return 0;
}

Value create_object() {
  Object *object = allocate(sizeof(Object));
  object->capacity = INITIAL_ARRAY_CAPACITY;
  object->size = 0;
  object->entries = allocate(sizeof(Entry) * object->capacity);
  return (Value) { .type = V_OBJECT, .object_value = object };
}

void object_put(Object *object, Value key, Value value) {
  Value old;
  object_remove(object, key, &old);
  if (object->size >= object->capacity) {
    object->capacity <<= 1;
    object->entries = reallocate(object->entries, sizeof(Entry) * object->capacity);
  }
  object->entries[object->size] = (Entry) { .key = key, .value = value };
  object->size++;
}

Result object_get(Object *object, Value key, Value *value) {
  for (size_t i = 0; i < object->size; i++) {
    if (equals(key, object->entries[i].key)) {
      *value = object->entries[i].value;
      return 1;
    }
  }
  return 0;
}

Result object_remove(Object *object, Value key, Value *value) {
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

Value create_closure(const NameList *params, const Node *body, Env *env) {
}

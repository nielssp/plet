/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "value.h"

#include "util.h"

#include <string.h>

#define MIN_CONTEXT_SIZE 4096
#define INITIAL_ARRAY_CAPACITY 16

Context *create_context() {
  Context *context = allocate(sizeof(Context) + MIN_CONTEXT_SIZE);
  context->next = NULL;
  context->last = context;
  context->capacity = MIN_CONTEXT_SIZE;
  context->size = 0;
  return context;
}

void delete_context(Context *context) {
  if (context->next) {
    delete_context(context->next);
  }
  free(context);
}

void *context_allocate(Context *context, size_t size) {
  Context *last = context->last;
  if (size < last->capacity - last->size) {
    void *p = last->data + last->size;
    last->size += size;
    return p;
  } else {
    size_t new_size = size > MIN_CONTEXT_SIZE ? size : MIN_CONTEXT_SIZE;
    Context *new = allocate(sizeof(Context) + new_size);
    new->next = NULL;
    new->last = new;
    new->capacity = new_size;
    new->size = size;
    last->next = new;
    last->last = new;
    context->last = new;
    return new->data;
  }
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

Value copy_value(Value value, Context *context) {
  switch (value.type) {
    case V_NIL:
    case V_TRUE:
    case V_INT:
    case V_FLOAT:
      return value;
    case V_STRING:
      return create_string(value.string_value->bytes, value.string_value->size, context);
    case V_ARRAY: {
      Value copy = create_array(value.array_value->size, context);
      for (size_t i = 0; i < value.array_value->size; i++) {
        array_push(copy.array_value, copy_value(value.array_value->cells[i], context), context);
      }
      return copy;
    }
    case V_OBJECT: {
      Value copy = create_object(value.object_value->size, context);
      for (size_t i = 0; i < value.object_value->size; i++) {
        object_put(copy.object_value, copy_value(value.object_value->entries[i].key, context),
            copy_value(value.object_value->entries[i].value, context), context);
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

Value create_string(const uint8_t *bytes, size_t size, Context *context) {
  String *string = context_allocate(context, sizeof(String) + size);
  string->size = size;
  memcpy(string->bytes, bytes, size);
  return (Value) { .type = V_STRING, .string_value = string };
}

Value create_array(size_t capacity, Context *context) {
  Array *array = context_allocate(context, sizeof(Array));
  array->capacity = capacity < INITIAL_ARRAY_CAPACITY ? INITIAL_ARRAY_CAPACITY : capacity;
  array->size = 0;
  array->cells = context_allocate(context, array->capacity * sizeof(Value));
  return (Value) { .type = V_ARRAY, .array_value = array };
}

void array_push(Array *array, Value elem, Context *context) {
  if (array->size >= array->capacity) {
    //array->cells = reallocate(array->cells, sizeof(Value) * array->capacity);
    size_t new_capacity = array->capacity << 1;
    Value *new_cells = context_allocate(context, new_capacity * sizeof(Value));
    memcpy(new_cells, array->cells, array->capacity * sizeof(Value));
    array->capacity = new_capacity;
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

Value create_object(size_t capacity, Context *context) {
  Object *object = allocate(sizeof(Object));
  object->capacity = capacity < INITIAL_ARRAY_CAPACITY ? INITIAL_ARRAY_CAPACITY : capacity;
  object->size = 0;
  object->entries = context_allocate(context, object->capacity * sizeof(Entry));
  return (Value) { .type = V_OBJECT, .object_value = object };
}

void object_put(Object *object, Value key, Value value, Context *context) {
  Value old;
  object_remove(object, key, &old);
  if (object->size >= object->capacity) {
    object->capacity <<= 1;
    //object->entries = reallocate(object->entries, sizeof(Entry) * object->capacity);
    size_t new_capacity = object->capacity << 1;
    Entry *new_entries = context_allocate(context, new_capacity * sizeof(Entry));
    memcpy(new_entries, object->entries, object->capacity * sizeof(Entry));
    object->capacity = new_capacity;
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

Value create_closure(const NameList *params, const Node body, Env *env, Context *context) {
}

static Node copy_node(Node node, Context *context) {
  Node copy = node;
  switch (node.type) {
    case N_NAME:
    case N_INT:
    case N_FLOAT:
    case N_STRING:
    case N_LIST:
    case N_OBJECT:
    case N_APPLY:
    case N_SUBSCRIPT:
    case N_DOT:
    case N_PREFIX:
    case N_INFIX:
    case N_FN:
    case N_IF:
    case N_FOR:
    case N_SWITCH:
    case N_ASSIGN:
    case N_BLOCK:
      break;
  }
  return copy;
}

static char *context_copy_string(const char *src, Context *context) {
  size_t l = strlen(src) + 1;
  char *dest = context_allocate(context, l);
  memcpy(dest, src, l);
  return dest;
}

static Module *copy_module(Module *module, Context *context) {
  Module *copy = context_allocate(context, sizeof(Module));
  copy->file_name = context_copy_string(module->file_name, context);
  copy->root = NULL;
  return copy;
}

static NodeList copy_node_list(NodeList *list, Context *context) {
}

static NodeList copy_property_list(PropertyList *list, Context *context) {
}

static NameList copy_name_list(NameList *list, Context *context) {
}

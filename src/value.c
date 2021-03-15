/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "value.h"

#include "util.h"

#include <errno.h>
#include <inttypes.h>
#include <string.h>

#define INITIAL_ARRAY_CAPACITY 16

static Node copy_node(Node node, Arena *arena);

struct Object {
  Entry *entries;
  size_t capacity;
  size_t size;
};

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

Env *create_env(Arena *arena, ModuleMap *modules, SymbolMap *symbol_map) {
  Env *env = arena_allocate(sizeof(Env), arena);
  env->arena = arena;
  env->modules = modules;
  env->symbol_map = symbol_map;
  env->calling_node = NULL;
  env->error = NULL;
  env->error_arg = -1;
  env->error_level = ENV_ERROR;
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

int env_get_symbol(const char *name, Value *value, Env *env) {
  return env_get(get_symbol(name, env->symbol_map), value, env);
}

char *get_env_string(const char *name, Env *env) {
  Value value;
  if (!env_get(get_symbol(name, env->symbol_map), &value, env) || value.type != V_STRING) {
    return NULL;
  }
  return string_to_c_string(value.string_value);
}

static void display_env_error_va(Node node, EnvErrorLevel level, int show_line, const char *format, va_list va) {
  va_list va2;
  const char *label;
  switch (level) {
    case ENV_INFO:
      label = INFO_LABEL;
      break;
    case ENV_WARN:
      label = WARN_LABEL;
      break;
    case ENV_ERROR:
    default:
      label = ERROR_LABEL;
      break;
  }
  fprintf(stderr, SGR_BOLD "%s:%d:%d: %s", node.module.file_name, node.start.line, node.start.column, label);
  va_copy(va2, va);
  vfprintf(stderr, format, va2);
  va_end(va2);
  fprintf(stderr, SGR_RESET "\n");
  if (show_line && node.module.file_name) {
    print_error_line(node.module.file_name, node.start, node.end);
  }
}

void display_env_error(Node node, EnvErrorLevel level, int show_line, const char *format, ...) {
  va_list va;
  va_start(va, format);
  display_env_error_va(node, level, show_line, format, va);
  va_end(va);
}

void env_error(Env *env, int arg, const char *format, ...) {
  va_list va;
  if (arg < 0 && env->calling_node) {
    va_start(va, format);
    display_env_error_va(*env->calling_node, ENV_ERROR, arg != ENV_ARG_NONE, format, va);
    va_end(va);
  } else {
    env->error_arg = arg;
    Buffer buffer = create_buffer(0);
    va_start(va, format);
    buffer_vprintf(&buffer, format, va);
    va_end(va);
    buffer_put(&buffer, '\0');
    env->error = arena_allocate(buffer.size, env->arena);
    memcpy(env->error, buffer.data, buffer.size);
    env->error_level = ENV_ERROR;
    delete_buffer(buffer);
  }
}

void env_warn(Env *env, int arg, const char *format, ...) {
  va_list va;
  if (arg < 0 && env->calling_node) {
    va_start(va, format);
    display_env_error_va(*env->calling_node, ENV_WARN, arg != ENV_ARG_NONE, format, va);
    va_end(va);
  } else {
    env->error_arg = arg;
    Buffer buffer = create_buffer(0);
    va_start(va, format);
    buffer_vprintf(&buffer, format, va);
    va_end(va);
    buffer_put(&buffer, '\0');
    env->error = arena_allocate(buffer.size, env->arena);
    memcpy(env->error, buffer.data, buffer.size);
    env->error_level = ENV_WARN;
    delete_buffer(buffer);
  }
}

void env_info(Env *env, int arg, const char *format, ...) {
  va_list va;
  if (arg < 0 && env->calling_node) {
    va_start(va, format);
    display_env_error_va(*env->calling_node, ENV_INFO, arg != ENV_ARG_NONE, format, va);
    va_end(va);
  } else {
    env->error_arg = arg;
    Buffer buffer = create_buffer(0);
    va_start(va, format);
    buffer_vprintf(&buffer, format, va);
    va_end(va);
    buffer_put(&buffer, '\0');
    env->error = arena_allocate(buffer.size, env->arena);
    memcpy(env->error, buffer.data, buffer.size);
    env->error_level = ENV_INFO;
    delete_buffer(buffer);
  }
}

void env_clear_error(Env *env) {
  env->error = NULL;
  env->error_arg = -1;
  env->error_level = ENV_ERROR;
}

int equals(Value a, Value b) {
  if (a.type != b.type) {
    return 0;
  }
  switch (a.type) {
    case V_NIL:
    case V_TRUE:
    case V_FALSE:
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
      ObjectIterator it = iterate_object(a.object_value);
      Value entry_key, entry_value;
      while (object_iterator_next(&it, &entry_key, &entry_value)) {
        Value value;
        if (!object_get(a.object_value, entry_key, &value)) {
          return 0;
        }
        if (!equals(entry_value, value)) {
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
    case V_FALSE:
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
    case V_FALSE:
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
    case V_OBJECT: {
      ObjectIterator it = iterate_object(value.object_value);
      Value entry_key, entry_value;
      while (object_iterator_next(&it, &entry_key, &entry_value)) {
        h = value_hash(h, entry_key);
        h = value_hash(h, entry_value);
      }
      break;
    }
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

static Value copy_value_detect_cycles(Value value, Arena *arena, RefStack *ref_stack);

static Env *copy_env(Env *env, Arena *arena, RefStack *ref_stack) {
  Env *copy = create_env(arena, env->modules, env->symbol_map);
  HashMapIterator it = generic_hash_map_iterate(&env->global);
  Entry entry;
  while (generic_hash_map_next(&it, &entry)) {
    generic_hash_map_set(&copy->global, &(Entry) { .key = copy_value_detect_cycles(entry.key, arena, ref_stack),
        .value = copy_value_detect_cycles(entry.value, arena, ref_stack) }, NULL, NULL);\
  }
  return copy;
}

static Value copy_value_detect_cycles(Value value, Arena *arena, RefStack *ref_stack) {
  switch (value.type) {
    case V_NIL:
    case V_TRUE:
    case V_FALSE:
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
      ObjectIterator it = iterate_object(value.object_value);
      Value entry_key, entry_value;
      while (object_iterator_next(&it, &entry_key, &entry_value)) {
        object_put(copy.object_value, copy_value_detect_cycles(entry_key, arena, &nested),
            copy_value_detect_cycles(entry_value, arena, &nested), arena);
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
      copy->body = copy_node(value.closure_value->body, arena);
      RefStack nested = (RefStack) { .next = ref_stack, .old = value.closure_value, .new = copy };
      copy->env = copy_env(value.closure_value->env, arena, &nested);
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
    case V_FALSE:
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
    case V_TIME: {
      struct tm *t;
      char date[26];
      t = localtime(&value.time_value);
      if (t) {
        if (strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%S%z", t)) {
          buffer_printf(buffer, "%s", date);
        } else {
          buffer_printf(buffer, "(invalid time: %s)", strerror(errno));
        }
      } else {
        buffer_printf(buffer, "(invalid time: %s)", strerror(errno));
      }
      break;
    }
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
    case V_FALSE:
      return "false";
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

Value copy_c_string(const char *str, Arena *arena) {
  return create_string((uint8_t *) str, strlen(str), arena);
}

char *string_to_c_string(String *string) {
  char *c_str = allocate(string->size + 1);
  memcpy(c_str, string->bytes, string->size);
  c_str[string->size] = '\0';
  return c_str;
}

Value allocate_string(size_t size, Arena *arena) {
  if (!size) {
    return (Value) { .type = V_STRING, .string_value = empty_string };
  }
  String *string = arena_allocate(sizeof(String) + size, arena);
  string->size = size;
  return (Value) { .type = V_STRING, .string_value = string };
}

Value reallocate_string(String *string, size_t size, Arena *arena) {
  if (!size) {
    return (Value) { .type = V_STRING, .string_value = empty_string };
  }
  string = arena_reallocate(string, sizeof(String) + string->size, sizeof(String) + size, arena);
  string->size = size;
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
    array->cells = new_cells;
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

void array_unshift(Array *array, Value elem, Arena *arena) {
  size_t new_capacity = array->capacity + 1;
  Value *new_cells = arena_allocate(new_capacity * sizeof(Value), arena);
  memcpy(new_cells + 1, array->cells, array->capacity * sizeof(Value));
  array->capacity = new_capacity;
  array->cells[0] = elem;
  array->size++;
}

int array_shift(Array *array, Value *elem) {
  if (array->size) {
    array->size--;
    array->capacity--;
    *elem = array->cells[0];
    array->cells++;
    return 1;
  }
  return 0;
}

int array_remove(Array *array, int index) {
  if (index >= 0 && index < array->size) {
    array->size--;
    if (!index) {
      array->capacity--;
      array->cells++;
    } else if (index < array->size) {
      memcpy(array->cells + index, array->cells + index + 1,
          (array->size - index + 1) * sizeof(Value));
    }
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
    object->entries = new_entries;
    object->capacity = new_capacity;
  }
  object->entries[object->size] = (Entry) { .key = key, .value = value };
  object->size++;
}

int object_get(Object *object, Value key, Value *value) {
  for (size_t i = 0; i < object->size; i++) {
    if (equals(key, object->entries[i].key)) {
      if (value) {
        *value = object->entries[i].value;
      }
      return 1;
    }
  }
  return 0;
}

int object_get_symbol(Object *object, const char *key, Value *value) {
  for (size_t i = 0; i < object->size; i++) {
    if (object->entries[i].key.type == V_SYMBOL && strcmp(object->entries[i].key.symbol_value, key) == 0) {
      if (value) {
        *value = object->entries[i].value;
      }
      return 1;
    }
  }
  return 0;
}

int object_remove(Object *object, Value key, Value *value) {
  for (size_t i = 0; i < object->size; i++) {
    if (equals(key, object->entries[i].key)) {
      if (value) {
        *value = object->entries[i].value;
      }
      if (i < object->size - 1) {
        memmove(&object->entries[i], &object->entries[i + 1], (object->size - i - 1) * sizeof(Entry));
      }
      object->size--;
      return 1;
    }
  }
  return 0;
}

size_t object_size(Object *object) {
  return object->size;
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
  closure->body = copy_node(body, arena);
  closure->env = create_env(arena, env->modules, env->symbol_map);
  for (NameList *name = free_variables; name; name = name->tail) {
    Value value;
    if (env_get(name->head, &value, env)) {
      env_put(name->head, value, closure->env);
    }
  }
  return (Value) { .type = V_CLOSURE, .closure_value = closure };
}

static NodeList *copy_node_list(NodeList *list, Arena *arena) {
  if (!list) {
    return NULL;
  }
  NodeList *copy = arena_allocate(sizeof(NodeList), arena);
  copy->size = list->size;
  copy->tail = copy_node_list(list->tail, arena);
  copy->head = copy_node(list->head, arena);
  return copy;
}

static PropertyList *copy_property_list(PropertyList *list, Arena *arena) {
  if (!list) {
    return NULL;
  }
  PropertyList *copy = arena_allocate(sizeof(PropertyList), arena);
  copy->size = list->size;
  copy->tail = copy_property_list(list->tail, arena);
  copy->key = copy_node(list->key, arena);
  copy->value = copy_node(list->value, arena);
  return copy;
}

static NameList *arena_copy_name_list(NameList *list, Arena *arena) {
  if (!list) {
    return NULL;
  }
  NameList *copy = arena_allocate(sizeof(NameList), arena);
  copy->size = list->size;
  copy->tail = arena_copy_name_list(list->tail, arena);
  copy->head = list->head;
  return copy;
}

static Node *copy_node_pointer(Node *node, Arena *arena) {
  Node *copy = arena_allocate(sizeof(Node), arena);
  *copy = *node;
  return copy;
}

static Node copy_node(Node node, Arena *arena) {
  switch (node.type) {
    case N_NAME:
    case N_INT:
    case N_FLOAT:
      break;
    case N_STRING: {
      uint8_t *bytes = arena_allocate(node.string_value.size, arena);
      memcpy(bytes, node.string_value.bytes, node.string_value.size);
      node.string_value.bytes = bytes;
      break;
    }
    case N_LIST:
      node.list_value = copy_node_list(node.list_value, arena);
      break;
    case N_OBJECT:
      node.object_value = copy_property_list(node.object_value, arena);
      break;
    case N_APPLY:
      node.apply_value.callee = copy_node_pointer(node.apply_value.callee, arena);
      node.apply_value.args = copy_node_list(node.apply_value.args, arena);
      break;
    case N_SUBSCRIPT:
      node.subscript_value.list = copy_node_pointer(node.subscript_value.list, arena);
      node.subscript_value.index = copy_node_pointer(node.subscript_value.index, arena);
      break;
    case N_DOT:
      node.dot_value.object = copy_node_pointer(node.dot_value.object, arena);
      break;
    case N_PREFIX:
      node.prefix_value.operand = copy_node_pointer(node.prefix_value.operand, arena);
      break;
    case N_INFIX:
      node.infix_value.left = copy_node_pointer(node.infix_value.left, arena);
      node.infix_value.right = copy_node_pointer(node.infix_value.right, arena);
      break;
    case N_TUPLE:
      node.tuple_value = arena_copy_name_list(node.tuple_value, arena);
      break;
    case N_FN:
      node.fn_value.params = arena_copy_name_list(node.fn_value.params, arena);
      node.fn_value.free_variables = arena_copy_name_list(node.fn_value.free_variables, arena);
      node.fn_value.body = copy_node_pointer(node.fn_value.body, arena);
      break;
    case N_IF:
      node.if_value.cond = copy_node_pointer(node.if_value.cond, arena);
      node.if_value.cons = copy_node_pointer(node.if_value.cons, arena);
      node.if_value.alt = copy_node_pointer(node.if_value.alt, arena);
      break;
    case N_FOR:
      node.for_value.collection = copy_node_pointer(node.for_value.collection, arena);
      node.for_value.body = copy_node_pointer(node.for_value.body, arena);
      node.for_value.alt = copy_node_pointer(node.for_value.alt, arena);
      break;
    case N_SWITCH:
      node.switch_value.expr = copy_node_pointer(node.switch_value.expr, arena);
      node.switch_value.default_case = copy_node_pointer(node.switch_value.default_case, arena);
      node.switch_value.cases = copy_property_list(node.switch_value.cases, arena);
      break;
    case N_ASSIGN:
      node.assign_value.left = copy_node_pointer(node.assign_value.left, arena);
      node.assign_value.right = copy_node_pointer(node.assign_value.right, arena);
      break;
    case N_BLOCK:
      node.block_value = copy_node_list(node.block_value, arena);
      break;
    case N_SUPPRESS:
      node.suppress_value = copy_node_pointer(node.suppress_value, arena);
      break;
  }
  return node;
}

/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef VALUE_H
#define VALUE_H

#include "reader.h"
#include "ast.h"
#include "hashmap.h"

#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef struct Value Value;
typedef struct String String;
typedef struct Array Array;
typedef struct Object Object;
typedef struct ObjectIterator ObjectIterator;
typedef struct Entry Entry;
typedef struct Closure Closure;


typedef struct Tuple Tuple;

typedef struct {
  Arena *arena;
  GenericHashMap global;
} Env;

typedef enum {
  V_NIL,
  V_TRUE,
  V_INT,
  V_FLOAT,
  V_SYMBOL,
  V_STRING,
  V_ARRAY,
  V_OBJECT,
  V_TIME,
  V_FUNCTION,
  V_CLOSURE
} ValueType;

struct Value {
  ValueType type;
  union {
    int64_t int_value;
    double float_value;
    Symbol symbol_value;
    String *string_value;
    Array *array_value;
    Object *object_value;
    time_t time_value;
    Value (*function_value)(const Tuple *, Env *);
    Closure *closure_value;
  };
};

#define nil_value ((Value) { .type = V_NIL, .int_value = 0 })

#define true_value ((Value) { .type = V_TRUE, .int_value = 0 })

#define create_int(i) ((Value) { .type = V_INT, .int_value = (i) })

#define create_float(f) ((Value) { .type = V_FLOAT, .float_value = (f) })

#define create_symbol(s) ((Value) { .type = V_SYMBOL, .symbol_value = (s) })

struct String {
  size_t size;
  uint8_t bytes[];
};

struct Array {
  Value *cells;
  size_t capacity;
  size_t size;
};

struct Object {
  Entry *entries;
  size_t capacity;
  size_t size;
};

struct ObjectIterator {
  Object *object;
  size_t next_index;
};

struct Entry {
  Value key;
  Value value;
};

struct Closure {
  NameList *params;
  Node body;
  Object *env;
};

struct Tuple {
  size_t size;
  Value values[];
};

Env *create_env(Arena *arena);

void env_put(Symbol name, Value value, Env *env);

int env_get(Symbol name, Value *value, Env *env);

int equals(Value a, Value b);

int is_truthy(Value value);

Hash value_hash(Hash h, Value value);

Value copy_value(Value value, Arena *arena);

Value create_string(const uint8_t *bytes, size_t size, Arena *arena);

Value create_array(size_t capacity, Arena *arena);

void array_push(Array *array, Value elem, Arena *arena);

int array_pop(Array *array, Value *elem);

Value create_object(size_t capacity, Arena *arena);

void object_put(Object *object, Value key, Value value, Arena *arena);

int object_get(Object *object, Value key, Value *value);

int object_remove(Object *object, Value key, Value *value);

ObjectIterator iterate_object(Object *object);

int object_iterator_next(ObjectIterator *it, Value *key, Value *value);

Value create_closure(NameList *params, NameList *free_variables, Node body, Env *env, Arena *arena);

#endif

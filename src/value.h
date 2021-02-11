/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef VALUE_H
#define VALUE_H

#include "ast.h"

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

typedef int Result;
#define OK 1
#define ERROR 0

typedef struct Context Context;

typedef struct Value Value;
typedef struct String String;
typedef struct Array Array;
typedef struct Object Object;
typedef struct Entry Entry;
typedef struct Closure Closure;

typedef struct Tuple Tuple;
typedef struct Enve Env;

struct Context {
  Context *next;
  Context *last;
  size_t capacity;
  size_t size;
  uint8_t data[];
};

typedef enum {
  V_NIL,
  V_TRUE,
  V_INT,
  V_FLOAT,
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

struct Entry {
  Value key;
  Value value;
};

struct Closure {
  NameList *params;
  Node body;
  Env *env;
};

struct Tuple {
  size_t size;
  Value values[];
};

struct Env {
  Env *parent;
  char *name;
  Value value;
};

Context *create_context();

void delete_context(Context *context);

void *context_allocate(Context *context, size_t size);

int equals(Value a, Value b);

Value copy_value(Value value, Context *context);

Value create_string(const uint8_t *bytes, size_t size, Context *context);

Value create_array(size_t capacity, Context *context);

void array_push(Array *array, Value elem, Context *context);

Result array_pop(Array *array, Value *elem);

Value create_object(size_t capacity, Context *context);

void object_put(Object *object, Value key, Value value, Context *context);

Result object_get(Object *object, Value key, Value *value);

Result object_remove(Object *object, Value key, Value *value);

Value create_closure(const NameList *params, const Node body, Env *env, Context *context);

#endif

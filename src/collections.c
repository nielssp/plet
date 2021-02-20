/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "collections.h"

#include "interpreter.h"

#include <alloca.h>
#include <stdlib.h>
#include <string.h>

#if defined(_GNU_SOURCE) || defined(__GNU__) || defined(__linux__)
#define SORT_R(BASE, NMEMB, SIZE, COMPAR, ARG) \
  qsort_r((BASE), (NMEMB), (SIZE), (COMPAR), (ARG))
#elif defined(__APPLE__) || defined(__MACH__) || defined(__DARWIN__) || \
    defined (__FREEBSD__) || defined(__BSD__) || \
    defined (OpenBSD3_1) || defined (OpenBSD3_9)
#define SORT_R(BASE, NMEMB, SIZE, COMPAR, ARG) \
  do {\
    SortData data = {(ARG), (COMPAR)};\
    qsort_r((BASE), (NMEMB), (SIZE), &data, &sort_arg_swap);\
  } while (0)
#elif (defined _WIN32 || defined _WIN64 || defined __WINDOWS__)
#define SORT_R(BASE, NMEMB, SIZE, COMPAR, ARG) \
  do {\
    SortData data = {(ARG), (COMPAR)};\
    qsort_r(*(BASE), (NMEMB), (SIZE), &data, &sort_arg_swap);\
  } while (0)
#else
#error No substitute for qsort_r found
#endif

typedef struct {
  void *arg;
  int (*compar)(const void *a1, const void *a2, void *aarg);
} SortData;

static int sort_arg_swap(void *p, const void *a, const void *b)
{
  SortData *data = (SortData *) p;
  return (data->compar)(a, b, data->arg);
}

static Value length(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value arg = args->values[0];
  switch (arg.type) {
    case V_ARRAY:
      return create_int(arg.array_value->size);
    case V_OBJECT:
      return create_int(arg.object_value->size);
    case V_STRING:
      return create_int(arg.string_value->size);
    default:
      arg_error(0, "array|object|string", args, env);
      return nil_value;
  }
}

static Value keys(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value arg = args->values[0];
  if (arg.type != V_OBJECT) {
    arg_type_error(0, V_OBJECT, args, env);
    return nil_value;
  }
  Value array = create_array(arg.object_value->size, env->arena);
  ObjectIterator it = iterate_object(arg.object_value);
  Value key;
  while (object_iterator_next(&it, &key, NULL)) {
    array_push(array.array_value, key, env->arena);
  }
  return array;
}

static Value values(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value arg = args->values[0];
  if (arg.type != V_OBJECT) {
    arg_type_error(0, V_OBJECT, args, env);
    return nil_value;
  }
  Value array = create_array(arg.object_value->size, env->arena);
  ObjectIterator it = iterate_object(arg.object_value);
  Value value;
  while (object_iterator_next(&it, NULL, &value)) {
    array_push(array.array_value, value, env->arena);
  }
  return array;
}

static Value map(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  Value func = args->values[1];
  if (func.type != V_FUNCTION && func.type != V_CLOSURE) {
    arg_type_error(1, V_FUNCTION, args, env);
    return nil_value;
  }
  Tuple *func_args = alloca(sizeof(Tuple) + 2 * sizeof(Value));
  func_args->size = 2;
  if (src.type == V_ARRAY) {
    Value dest = create_array(src.array_value->size, env->arena);
    for (size_t i = 0; i < src.array_value->size; i++) {
      func_args->values[0] = src.array_value->cells[i];
      func_args->values[1] = create_int(i);
      Value value;
      if (!apply(func, func_args, &value, env)) {
        env->error_arg = 1;
        return nil_value;
      }
      array_push(dest.array_value, value, env->arena);
    }
    return dest;
  } else if (src.type == V_OBJECT) {
    Value dest = create_object(src.object_value->size, env->arena);
    ObjectIterator it = iterate_object(src.object_value);
    Value key, value;
    while (object_iterator_next(&it, &key, &value)) {
      func_args->values[0] = value;
      func_args->values[1] = key;
      if (!apply(func, func_args, &value, env)) {
        env->error_arg = 1;
        return nil_value;
      }
      object_put(dest.object_value, key, value, env->arena);
    }
    return dest;
  } else {
    arg_error(0, "array|object", args, env);
    return nil_value;
  }
}

static Value map_keys(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  Value func = args->values[1];
  if (func.type != V_FUNCTION && func.type != V_CLOSURE) {
    arg_type_error(1, V_FUNCTION, args, env);
    return nil_value;
  }
  Tuple *func_args = alloca(sizeof(Tuple) + sizeof(Value));
  func_args->size = 1;
  if (src.type == V_OBJECT) {
    Value dest = create_object(src.object_value->size, env->arena);
    ObjectIterator it = iterate_object(src.object_value);
    Value key, value;
    while (object_iterator_next(&it, &key, &value)) {
      func_args->values[0] = key;
      if (!apply(func, func_args, &key, env)) {
        env->error_arg = 1;
        return nil_value;
      }
      object_put(dest.object_value, key, value, env->arena);
    }
    return dest;
  } else {
    arg_type_error(0, V_OBJECT, args, env);
    return nil_value;
  }
}

static Value filter(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  Value func = args->values[1];
  if (func.type != V_FUNCTION && func.type != V_CLOSURE) {
    arg_type_error(1, V_FUNCTION, args, env);
    return nil_value;
  }
  Tuple *func_args = alloca(sizeof(Tuple) + 2 * sizeof(Value));
  func_args->size = 2;
  if (src.type == V_ARRAY) {
    Value dest = create_array(0, env->arena);
    for (size_t i = 0; i < src.array_value->size; i++) {
      func_args->values[0] = src.array_value->cells[i];
      func_args->values[1] = create_int(i);
      Value include;
      if (!apply(func, func_args, &include, env)) {
        env->error_arg = 1;
        return nil_value;
      }
      if (is_truthy(include)) {
        array_push(dest.array_value, src.array_value->cells[i], env->arena);
      }
    }
    return dest;
  } else if (src.type == V_OBJECT) {
    Value dest = create_object(0, env->arena);
    ObjectIterator it = iterate_object(src.object_value);
    Value key, value;
    while (object_iterator_next(&it, &key, &value)) {
      func_args->values[0] = value;
      func_args->values[1] = key;
      Value include;
      if (!apply(func, func_args, &include, env)) {
        env->error_arg = 1;
        return nil_value;
      }
      if (is_truthy(include)) {
        object_put(dest.object_value, key, value, env->arena);
      }
    }
    return dest;
  } else {
    arg_error(0, "array|object", args, env);
    return nil_value;
  }
}

static Value exclude(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  Value func = args->values[1];
  if (func.type != V_FUNCTION && func.type != V_CLOSURE) {
    arg_type_error(1, V_FUNCTION, args, env);
    return nil_value;
  }
  Tuple *func_args = alloca(sizeof(Tuple) + 2 * sizeof(Value));
  func_args->size = 2;
  if (src.type == V_ARRAY) {
    Value dest = create_array(0, env->arena);
    for (size_t i = 0; i < src.array_value->size; i++) {
      func_args->values[0] = src.array_value->cells[i];
      func_args->values[1] = create_int(i);
      Value include;
      if (!apply(func, func_args, &include, env)) {
        env->error_arg = 1;
        return nil_value;
      }
      if (!is_truthy(include)) {
        array_push(dest.array_value, src.array_value->cells[i], env->arena);
      }
    }
    return dest;
  } else if (src.type == V_OBJECT) {
    Value dest = create_object(0, env->arena);
    ObjectIterator it = iterate_object(src.object_value);
    Value key, value;
    while (object_iterator_next(&it, &key, &value)) {
      func_args->values[0] = value;
      func_args->values[1] = key;
      Value include;
      if (!apply(func, func_args, &include, env)) {
        env->error_arg = 1;
        return nil_value;
      }
      if (!is_truthy(include)) {
        object_put(dest.object_value, key, value, env->arena);
      }
    }
    return dest;
  } else {
    arg_error(0, "array|object", args, env);
    return nil_value;
  }
}

static int compare_values(const void *pa, const void *pb) {
  Value a = *(Value *) pa;
  Value b = *(Value *) pb;
  if (a.type != b.type) {
    return a.type - b.type;
  }
  switch (a.type) {
    case V_NIL:
      return 0;
    case V_TRUE:
      return 0;
    case V_INT:
      return a.int_value - b.int_value;
    case V_FLOAT:
      if (a.float_value < b.float_value) {
        return -1;
      } else if (a.float_value > b.float_value) {
        return 1;
      } else {
        return 0;
      }
    case V_SYMBOL:
      return strcmp(a.symbol_value, b.symbol_value);
    case V_STRING:
    case V_ARRAY:
    case V_OBJECT:
    case V_TIME:
    case V_FUNCTION:
    case V_CLOSURE:
      return 0; //TODO
  }
  return 0;
}

static Value sort(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value src = args->values[0];
  if (src.type != V_ARRAY) {
    arg_type_error(0, V_ARRAY, args, env);
    return nil_value;
  }
  Value dest = create_array(src.array_value->size, env->arena);
  memcpy(dest.array_value->cells, src.array_value->cells, src.array_value->size * sizeof(Value));
  dest.array_value->size = src.array_value->size;
  qsort(dest.array_value->cells, dest.array_value->size, sizeof(Value), compare_values);
  return dest;
}

void import_collections(Env *env) {
  env_def_fn("length", length, env);
  env_def_fn("keys", keys, env);
  env_def_fn("values", values, env);
  env_def_fn("map", map, env);
  env_def_fn("map_keys", map_keys, env);
  env_def_fn("filter", filter, env);
  env_def_fn("exclude", exclude, env);
  env_def_fn("sort", sort, env);
}

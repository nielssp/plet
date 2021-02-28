/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "collections.h"

#include "interpreter.h"
#include "util/sort_r.h"

#include <alloca.h>
#include <stdlib.h>
#include <string.h>

#ifdef WITH_UNICODE
#include <unicode/ucol.h>
#endif

typedef struct {
  Value func;
  Env *env;
  int reverse;
} CompareContext;

static Value length(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value arg = args->values[0];
  switch (arg.type) {
    case V_ARRAY:
      return create_int(arg.array_value->size);
    case V_OBJECT:
      return create_int(object_size(arg.object_value));
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
  Value array = create_array(object_size(arg.object_value), env->arena);
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
  Value array = create_array(object_size(arg.object_value), env->arena);
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
    Value dest = create_object(object_size(src.object_value), env->arena);
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
    Value dest = create_object(object_size(src.object_value), env->arena);
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

static Value flat_map(const Tuple *args, Env *env) {
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
      if (value.type == V_ARRAY) {
        for (size_t j = 0; j < value.array_value->size; j++) {
          array_push(dest.array_value, value.array_value->cells[j], env->arena);
        }
      } else {
        env_error(env, 1, "invalid return value of type %s", value_name(value.type));
      }
    }
    return dest;
  } else {
    arg_type_error(0, V_ARRAY, args, env);
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

static int compare_values(const void *pa, const void *pb, void *pc) {
  Value a = *(Value *) pa;
  Value b = *(Value *) pb;
  CompareContext *context = (CompareContext *) pc;
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
    case V_STRING: {
#ifdef WITH_UNICODE
      UErrorCode status = U_ZERO_ERROR;
      char *locale = NULL; // TODO
      UCollator *coll = ucol_open(locale, &status);
      UCollationResult result = UCOL_EQUAL;
      if (U_SUCCESS(status)) {
        result = ucol_strcollUTF8(coll, (char *) a.string_value->bytes, a.string_value->size,
            (char *) b.string_value->bytes, b.string_value->size, &status);
      }
      ucol_close(coll);
      if (U_FAILURE(status)) {
        env_error(context->env, -1, "collator error: %s", u_errorName(status));
        return 0;
      }
      return result;
#else
      size_t length = a.string_value->size > b.string_value->size ? a.string_value->size : b.string_value->size;
      for (size_t i = 0; i < length; i++) {
        if (a.string_value->bytes[i] < b.string_value->bytes[i]) {
          return -1;
        } else if (a.string_value->bytes[i] > b.string_value->bytes[i]) {
          return 1;
        }
      }
      if (a.string_value->size > b.string_value->size) {
        return 1;
      }
      if (a.string_value->size < b.string_value->size) {
        return -1;
      }
      return 0;
#endif
    }
    case V_ARRAY: {
      size_t length = a.array_value->size > b.array_value->size ? a.array_value->size : b.array_value->size;
      for (size_t i = 0; i < length; i++) {
        int subresult = compare_values(&a.array_value->cells[i], &b.array_value->cells[i], pc);
        if (subresult) {
          return subresult;
        }
      }
      if (a.array_value->size > b.array_value->size) {
        return 1;
      }
      if (a.array_value->size < b.array_value->size) {
        return -1;
      }
      return 0;
    }
    case V_OBJECT:
      return 0;
    case V_TIME:
      if (a.time_value < b.time_value) {
        return -1;
      } else if (a.time_value > b.time_value) {
        return 1;
      } else {
        return 0;
      }
    case V_FUNCTION:
    case V_CLOSURE:
      return 0;
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
  CompareContext context = {nil_value, env, 0};
  sort_r(dest.array_value->cells, dest.array_value->size, sizeof(Value), compare_values, &context);
  return dest;
}

static int sort_with_compare(const void *pa, const void *pb, void *pc) {
  Value a = *(Value *) pa;
  Value b = *(Value *) pb;
  CompareContext *context = (CompareContext *) pc;
  Tuple *func_args = alloca(sizeof(Tuple) + 2 * sizeof(Value));
  func_args->size = 2;
  func_args->values[0] = a;
  func_args->values[1] = b;
  Value result;
  if (!apply(context->func, func_args, &result, context->env)) {
    return 0;
  }
  if (result.type == V_INT) {
    if (result.int_value < 0) {
      return -1;
    } else if (result.int_value > 0) {
      return 1;
    } else {
      return 0;
    }
  } else if (result.type == V_FLOAT) {
    if (result.float_value < 0) {
      return -1;
    } else if (result.float_value > 0) {
      return 1;
    } else {
      return 0;
    }
  } else {
    env_error(context->env, -1, "invalid comparator return value of type %s", value_name(result.type));
    return 0;
  }
}

static Value sort_with(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  if (src.type != V_ARRAY) {
    arg_type_error(0, V_ARRAY, args, env);
    return nil_value;
  }
  Value func = args->values[1];
  if (func.type != V_FUNCTION && func.type != V_CLOSURE) {
    arg_type_error(1, V_FUNCTION, args, env);
    return nil_value;
  }
  Value dest = create_array(src.array_value->size, env->arena);
  memcpy(dest.array_value->cells, src.array_value->cells, src.array_value->size * sizeof(Value));
  dest.array_value->size = src.array_value->size;
  CompareContext context = {func, env, 0};
  sort_r(dest.array_value->cells, dest.array_value->size, sizeof(Value), sort_with_compare, &context);
  if (env->error) {
    env->error_arg = 1;
    return nil_value;
  }
  return dest;
}

static int sort_by_compare(const void *pa, const void *pb, void *pc) {
  Value a = *(Value *) pa;
  Value b = *(Value *) pb;
  CompareContext *context = (CompareContext *) pc;
  Tuple *func_args = alloca(sizeof(Tuple) + 2 * sizeof(Value));
  func_args->size = 1;
  func_args->values[0] = a;
  Value result_a, result_b;
  // TODO: don't apply function to the same element multiple times, maybe start
  // by applying function to all elements, then sort by results aftwerwards?
  if (!apply(context->func, func_args, &result_a, context->env)) {
    return 0;
  }
  func_args->values[0] = b;
  if (!apply(context->func, func_args, &result_b, context->env)) {
    return 0;
  }
  if (context->reverse) {
    return -compare_values(&result_a, &result_b, pc);
  }
  return compare_values(&result_a, &result_b, pc);
}

static Value sort_by(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  if (src.type != V_ARRAY) {
    arg_type_error(0, V_ARRAY, args, env);
    return nil_value;
  }
  Value func = args->values[1];
  if (func.type != V_FUNCTION && func.type != V_CLOSURE) {
    arg_type_error(1, V_FUNCTION, args, env);
    return nil_value;
  }
  Value dest = create_array(src.array_value->size, env->arena);
  memcpy(dest.array_value->cells, src.array_value->cells, src.array_value->size * sizeof(Value));
  dest.array_value->size = src.array_value->size;
  CompareContext context = {func, env, 0};
  sort_r(dest.array_value->cells, dest.array_value->size, sizeof(Value), sort_by_compare, &context);
  if (env->error) {
    env->error_arg = 1;
    return nil_value;
  }
  return dest;
}

static Value sort_by_desc(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  if (src.type != V_ARRAY) {
    arg_type_error(0, V_ARRAY, args, env);
    return nil_value;
  }
  Value func = args->values[1];
  if (func.type != V_FUNCTION && func.type != V_CLOSURE) {
    arg_type_error(1, V_FUNCTION, args, env);
    return nil_value;
  }
  Value dest = create_array(src.array_value->size, env->arena);
  memcpy(dest.array_value->cells, src.array_value->cells, src.array_value->size * sizeof(Value));
  dest.array_value->size = src.array_value->size;
  CompareContext context = {func, env, 1};
  sort_r(dest.array_value->cells, dest.array_value->size, sizeof(Value), sort_by_compare, &context);
  if (env->error) {
    env->error_arg = 1;
    return nil_value;
  }
  return dest;
}

static Value group_by(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  if (src.type != V_ARRAY) {
    arg_type_error(0, V_ARRAY, args, env);
    return nil_value;
  }
  Value func = args->values[1];
  if (func.type != V_FUNCTION && func.type != V_CLOSURE) {
    arg_type_error(1, V_FUNCTION, args, env);
    return nil_value;
  }
  Value dest = create_array(0, env->arena);
  Tuple *func_args = alloca(sizeof(Tuple) + 2 * sizeof(Value));
  for (size_t i = 0; i < src.array_value->size; i++) {
    func_args->size = 1;
    func_args->values[0] = src.array_value->cells[i];
    Value result_a, result_b;
    // TODO: don't apply function to the same element multiple times, maybe start
    // by applying function to all elements, then group by results aftwerwards?
    if (!apply(func, func_args, &result_a, env)) {
      env->error_arg = 1;
      return nil_value;
    }
    size_t j;
    for (j = 0; j < dest.array_value->size; j++) {
      func_args->values[0] = dest.array_value->cells[j].array_value->cells[0];
      if (!apply(func, func_args, &result_b, env)) {
        env->error_arg = 1;
        return nil_value;
      }
      if (equals(result_a, result_b)) {
        array_push(dest.array_value->cells[j].array_value, src.array_value->cells[i], env->arena);
        break;
      }
    }
    if (j >= dest.array_value->size) {
      Value new_array = create_array(0, env->arena);
      array_push(new_array.array_value, src.array_value->cells[i], env->arena);
      array_push(dest.array_value, new_array, env->arena);
    }
  }
  return dest;
}

static Value take(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  if (src.type != V_ARRAY) {
    arg_type_error(0, V_ARRAY, args, env);
    return nil_value;
  }
  Value n = args->values[1];
  if (n.type != V_INT) {
    arg_type_error(1, V_INT, args, env);
    return nil_value;
  }
  if (n.int_value > src.array_value->size) {
    n.int_value = src.array_value->size;
  }
  Value dest = create_array(n.int_value, env->arena);
  if (n.int_value > 0) {
    dest.array_value->size = n.int_value;
    memcpy(dest.array_value->cells, src.array_value->cells, dest.array_value->size * sizeof(Value));
  }
  return dest;
}

static Value drop(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  if (src.type != V_ARRAY) {
    arg_type_error(0, V_ARRAY, args, env);
    return nil_value;
  }
  Value n = args->values[1];
  if (n.type != V_INT) {
    arg_type_error(1, V_INT, args, env);
    return nil_value;
  }
  if (n.int_value > src.array_value->size) {
    n.int_value = src.array_value->size;
  }
  Value dest = create_array(src.array_value->size - n.int_value, env->arena);
  if (n.int_value < src.array_value->size) {
    dest.array_value->size = src.array_value->size - n.int_value;
    memcpy(dest.array_value->cells, src.array_value->cells + n.int_value, dest.array_value->size * sizeof(Value));
  }
  return dest;
}

static Value pop(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value src = args->values[0];
  if (src.type != V_ARRAY) {
    arg_type_error(0, V_ARRAY, args, env);
    return nil_value;
  }
  Value element;
  if (array_pop(src.array_value, &element)) {
    return element;
  }
  return nil_value;
}

static Value push(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  if (src.type != V_ARRAY) {
    arg_type_error(0, V_ARRAY, args, env);
    return nil_value;
  }
  array_push(src.array_value, args->values[1], env->arena);
  return src;
}

static Value push_all(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  if (src.type != V_ARRAY) {
    arg_type_error(0, V_ARRAY, args, env);
    return nil_value;
  }
  Value elements = args->values[1];
  if (src.type != V_ARRAY) {
    arg_type_error(1, V_ARRAY, args, env);
    return nil_value;
  }
  for (size_t i = 0; i < elements.array_value->size; i++) {
    array_push(src.array_value, elements.array_value->cells[i], env->arena);
  }
  return src;
}

static Value shift(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value src = args->values[0];
  if (src.type != V_ARRAY) {
    arg_type_error(0, V_ARRAY, args, env);
    return nil_value;
  }
  Value element;
  if (array_shift(src.array_value, &element)) {
    return element;
  }
  return nil_value;
}

static Value unshift(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  if (src.type != V_ARRAY) {
    arg_type_error(0, V_ARRAY, args, env);
    return nil_value;
  }
  array_unshift(src.array_value, args->values[1], env->arena);
  return src;
}

static Value contains(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  Value query = args->values[1];
  if (src.type == V_ARRAY) {
    for (size_t i = 0; i < src.array_value->size; i++) {
      if (equals(src.array_value->cells[i], query)) {
        return true_value;
      }
    }
  } else if (src.type == V_OBJECT) {
    if (object_get(src.object_value, query, NULL)) {
      return true_value;
    }
  } else {
    arg_error(0, "array|object", args, env);
  }
  return nil_value;
}

static Value delete(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value src = args->values[0];
  if (src.type != V_OBJECT) {
    arg_type_error(0, V_OBJECT, args, env);
    return nil_value;
  }
  if (object_remove(src.object_value, args->values[1], NULL)) {
    return true_value;
  }
  return nil_value;
}

void import_collections(Env *env) {
  env_def_fn("length", length, env);
  env_def_fn("keys", keys, env);
  env_def_fn("values", values, env);
  env_def_fn("map", map, env);
  env_def_fn("map_keys", map_keys, env);
  env_def_fn("flat_map", flat_map, env);
  env_def_fn("filter", filter, env);
  env_def_fn("exclude", exclude, env);
  env_def_fn("sort", sort, env);
  env_def_fn("sort_with", sort_with, env);
  env_def_fn("sort_by", sort_by, env);
  env_def_fn("sort_by_desc", sort_by_desc, env);
  env_def_fn("group_by", group_by, env);
  env_def_fn("take", take, env);
  env_def_fn("drop", drop, env);
  env_def_fn("pop", pop, env);
  env_def_fn("push", push, env);
  env_def_fn("push_all", push_all, env);
  env_def_fn("shift", shift, env);
  env_def_fn("unshift", unshift, env);
  env_def_fn("contains", contains, env);
  env_def_fn("delete", delete, env);
}

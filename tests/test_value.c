/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "../src/module.h"
#include "../src/value.h"

#include "test.h"

static void test_env(void) {
  Arena *arena = create_arena();
  ModuleMap *modules = create_module_map();
  SymbolMap *symbol_map = create_symbol_map();
  Env *env = create_env(arena, modules, symbol_map);
  Value value;
  Symbol sym1 = "foo";
  assert(env_get(sym1, &value, env) == 0);
  Value value1 = create_int(42);
  env_put(sym1, value1, env);
  assert(env_get(sym1, &value, env));
  assert(equals(value, value1));
  delete_arena(arena);
  delete_module_map(modules);
  delete_symbol_map(symbol_map);
}

static void test_array_push(void) {
  Arena *arena = create_arena();
  Value array = create_array(0, arena);
  for (size_t i = 0; i < 1000; i++) {
    array_push(array.array_value, create_int(i), arena);
  }
  assert(array.array_value->size == 1000);
  for (size_t i = 0; i < 1000; i++) {
    Value elem = array.array_value->cells[i];
    assert(elem.type == V_INT);
    assert(elem.int_value == i);
  }
  delete_arena(arena);
}

static void test_array_remove(void) {
  Arena *arena = create_arena();
  Value array = create_array(0, arena);
  assert(array_remove(array.array_value, 0) == 0);
  for (size_t i = 0; i < 1000; i++) {
    array_push(array.array_value, create_int(i), arena);
  }
  assert(array_remove(array.array_value, 0) == 1);
  assert(array_remove(array.array_value, 998) == 1);
  assert(array_remove(array.array_value, 998) == 0);
  assert(array.array_value->size == 998);
  for (size_t i = 0; i < 998; i++) {
    Value elem = array.array_value->cells[i];
    assert(elem.type == V_INT);
    assert(elem.int_value == i + 1);
  }
  for (size_t i = 0; i < 998; i++) {
    assert(array_remove(array.array_value, array.array_value->size - 1) == 1);
  }
  assert(array.array_value->size == 0);
  delete_arena(arena);
}

static void test_allocate_string(void) {
  Arena *arena = create_arena();
  Value string = allocate_string(1000, arena);
  for (size_t i = 0; i < 1000; i++) {
    string.string_value->bytes[i] = i % 256;
  }
  assert(string.string_value->size == 1000);
  for (size_t i = 0; i < 1000; i++) {
    assert(string.string_value->bytes[i] == (i % 256));
  }
  delete_arena(arena);
}

static void test_reallocate_string(void) {
  Arena *arena = create_arena();
  Value string = allocate_string(1000, arena);
  for (size_t i = 0; i < 1000; i++) {
    string.string_value->bytes[i] = i % 256;
  }
  string = reallocate_string(string.string_value, 2000, arena);
  for (size_t i = 1000; i < 2000; i++) {
    string.string_value->bytes[i] = i % 256;
  }
  assert(string.string_value->size == 2000);
  for (size_t i = 0; i < 2000; i++) {
    assert(string.string_value->bytes[i] == (i % 256));
  }
  delete_arena(arena);
}

void test_value(void) {
  run_test(test_env);
  run_test(test_array_push);
  run_test(test_array_remove);
  run_test(test_allocate_string);
  run_test(test_reallocate_string);
}


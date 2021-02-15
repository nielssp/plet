/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "../src/value.h"

#include "test.h"

static void test_env(void) {
  Arena *arena = create_arena();
  ModuleMap *modules = create_module_map();
  Env *env = create_env(arena, modules);
  Value value;
  Symbol sym1 = "foo";
  assert(env_get(sym1, &value, env) == 0);
  Value value1 = create_int(42);
  env_put(sym1, value1, env);
  assert(env_get(sym1, &value, env));
  assert(equals(value, value1));
  delete_arena(arena);
  delete_module_map(modules);
}

void test_value(void) {
  run_test(test_env);
}


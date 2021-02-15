/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "core.h"

static Value import(const Tuple *args, Env *env) {
  return nil_value;
}

static Value type(const Tuple *args, Env *env) {
  return nil_value;
}

static Value string(const Tuple *args, Env *env) {
  return nil_value;
}

void import_core(Env *env, SymbolMap *symbol_map) {
  env_def("nil", nil_value, env, symbol_map);
  env_def("false", nil_value, env, symbol_map);
  env_def("true", true_value, env, symbol_map);
  env_def_fn("import", import, env, symbol_map);
  env_def_fn("type", type, env, symbol_map);
  env_def_fn("string", string, env, symbol_map);
}

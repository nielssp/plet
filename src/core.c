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

void import_core(Env *env) {
  env_def("nil", nil_value, env);
  env_def("false", nil_value, env);
  env_def("true", true_value, env);
  env_def_fn("import", import, env);
  env_def_fn("type", type, env);
  env_def_fn("string", string, env);
}

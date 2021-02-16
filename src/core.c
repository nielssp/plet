/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "core.h"

#include <string.h>

static Value import(const Tuple *args, Env *env) {
  check_args(1, args, env);
  return nil_value;
}

static Value type(const Tuple *args, Env *env) {
  check_args(1, args, env);
  const char *type_name = value_name(args->values[0].type);
  return create_string((uint8_t *) type_name, strlen(type_name), env->arena);
}

static Value string(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Buffer buffer = create_buffer(0);
  value_to_string(args->values[0], &buffer);
  Value string = create_string(buffer.data, buffer.size, env->arena);
  delete_buffer(buffer);
  return string;
}

void import_core(Env *env) {
  env_def("nil", nil_value, env);
  env_def("false", nil_value, env);
  env_def("true", true_value, env);
  env_def_fn("import", import, env);
  env_def_fn("type", type, env);
  env_def_fn("string", string, env);
}

/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "core.h"

#include <alloca.h>
#include <string.h>

static Value import(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value name = args->values[0];
  if (name.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
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

static Value error(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value message = args->values[0];
  if (message.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  char *message_string = alloca(message.string_value->size + 1);
  memcpy(message_string, message.string_value->bytes, message.string_value->size);
  message_string[message.string_value->size] = '\0';
  env_error(env, ENV_ARG_NONE, "%s", message_string);
  return nil_value;
}

static Value warning(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value message = args->values[0];
  if (message.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  char *message_string = alloca(message.string_value->size + 1);
  memcpy(message_string, message.string_value->bytes, message.string_value->size);
  message_string[message.string_value->size] = '\0';
  env_warn(env, ENV_ARG_NONE, "%s", message_string);
  return nil_value;
}

static Value info(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value message = args->values[0];
  if (message.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  char *message_string = alloca(message.string_value->size + 1);
  memcpy(message_string, message.string_value->bytes, message.string_value->size);
  message_string[message.string_value->size] = '\0';
  env_info(env, ENV_ARG_NONE, "%s", message_string);
  return nil_value;
}

void import_core(Env *env) {
  env_def("nil", nil_value, env);
  env_def("false", nil_value, env);
  env_def("true", true_value, env);
  env_def_fn("import", import, env);
  env_def_fn("type", type, env);
  env_def_fn("string", string, env);
  env_def_fn("error", error, env);
  env_def_fn("warning", warning, env);
  env_def_fn("info", info, env);
}

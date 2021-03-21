/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "core.h"

#include "build.h"
#include "parser.h"
#include "reader.h"
#include "strings.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static Value import(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value name_value = args->values[0];
  if (name_value.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  Path *name = string_to_path(name_value.string_value);
  Module *m = load_module(name, env);
  Value result = nil_value;
  delete_path(name);
  if (!m) {
    env_error(env, -1, "unable to load module");
    return nil_value;
  }
  switch (m->type) {
    case M_SYSTEM:
      m->system_value.import_func(env);
      break;
    case M_USER:
      break;
    case M_DATA:
      break;
    case M_ASSET:
      result = path_to_string(m->file_name, env->arena);
      break;
  }
  return result;
}

static Value copy(const Tuple *args, Env *env) {
  check_args(1, args, env);
  return copy_value(args->values[0], env->arena);
}

static Value type(const Tuple *args, Env *env) {
  check_args(1, args, env);
  const char *type_name = value_name(args->values[0].type);
  return create_string((uint8_t *) type_name, strlen(type_name), env->arena);
}

static Value string(const Tuple *args, Env *env) {
  check_args(1, args, env);
  StringBuffer buffer = create_string_buffer(0, env->arena);
  string_buffer_append_value(&buffer, args->values[0]);
  return finalize_string_buffer(buffer);
}

static Value bool_(const Tuple *args, Env *env) {
  check_args(1, args, env);
  if (is_truthy(args->values[0])) {
    return true_value;
  }
  return false_value;
}

static Value error(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value message = args->values[0];
  if (message.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  char *message_string = allocate(message.string_value->size + 1);
  memcpy(message_string, message.string_value->bytes, message.string_value->size);
  message_string[message.string_value->size] = '\0';
  env_error(env, ENV_ARG_NONE, "%s", message_string);
  free(message_string);
  return nil_value;
}

static Value warning(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value message = args->values[0];
  if (message.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  char *message_string = allocate(message.string_value->size + 1);
  memcpy(message_string, message.string_value->bytes, message.string_value->size);
  message_string[message.string_value->size] = '\0';
  env_warn(env, ENV_ARG_NONE, "%s", message_string);
  free(message_string);
  return nil_value;
}

static Value info(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value message = args->values[0];
  if (message.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  char *message_string = allocate(message.string_value->size + 1);
  memcpy(message_string, message.string_value->bytes, message.string_value->size);
  message_string[message.string_value->size] = '\0';
  env_info(env, ENV_ARG_NONE, "%s", message_string);
  free(message_string);
  return nil_value;
}

void import_core(Env *env) {
  env_def("nil", nil_value, env);
  env_def("false", false_value, env);
  env_def("true", true_value, env);
  env_def_fn("import", import, env);
  env_def_fn("copy", copy, env);
  env_def_fn("type", type, env);
  env_def_fn("string", string, env);
  env_def_fn("bool", bool_, env);
  env_def_fn("error", error, env);
  env_def_fn("warning", warning, env);
  env_def_fn("info", info, env);
}

/* Plet
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#define _GNU_SOURCE
#include "exec.h"

#include "strings.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void shell_encode_byte(StringBuffer *buffer, uint8_t byte) {
  switch (byte) {
    case '\0':
      break;
    case '\'':
      string_buffer_printf(buffer, "'\\''");
      break;
    default:
      string_buffer_put(buffer, byte);
      break;
  }
}

static void shell_encode_value(StringBuffer *buffer, Value value) {
  string_buffer_put(buffer, '\'');
  switch (value.type) {
    case V_SYMBOL:
      while (*value.symbol_value) {
        shell_encode_byte(buffer, *value.symbol_value);
        value.symbol_value++;
      }
      break;
    case V_STRING:
      for (size_t i = 0; i < value.string_value->size; i++) {
        shell_encode_byte(buffer, value.string_value->bytes[i]);
      }
      break;
    default:
      string_buffer_append_value(buffer, value);
      break;
  }
  string_buffer_put(buffer, '\'');
}

static Value shell_escape(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value value = args->values[0];
  StringBuffer buffer = create_string_buffer(0, env->arena);
  shell_encode_value(&buffer, value);
  return finalize_string_buffer(buffer);
}

static Value plet_exec(const Tuple *args, Env *env) {
  check_args_min(1, args, env);
  Value command_value = args->values[0];
  if (command_value.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  StringBuffer command = create_string_buffer(command_value.string_value->size, env->arena);
  string_buffer_append(&command, command_value.string_value);
  for (size_t i = 1; i < args->size; i++) {
    string_buffer_put(&command, ' ');
    shell_encode_value(&command, args->values[i]);
  }
  string_buffer_put(&command, '\0');
  FILE *p = popen((char *) command.string->bytes, "r");
  if (!p) {
    env_error(env, -1, "unable to fork: %s", strerror(errno));
    return nil_value;
  }
  Buffer buffer = create_buffer(8192);
  size_t n;
  do {
    if (buffer.size) {
      buffer.capacity += 8192;
      buffer.data = reallocate(buffer.data, buffer.capacity);
    }
    n = fread(buffer.data + buffer.size, 1, 8192, p);
    buffer.size += n;
  } while (n == 8192);
  if (!feof(p)) {
    env_error(env, -1, "read error: %s", strerror(errno));
  }
  pclose(p);
  Value output = create_string(buffer.data, buffer.size, env->arena);
  delete_buffer(buffer);
  return output;
}

void import_exec(Env *env) {
  env_def_fn("shell_escape", shell_escape, env);
  env_def_fn("exec", plet_exec, env);
}

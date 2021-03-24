/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "exec.h"

#include <errno.h>
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Value tsc_exec(const Tuple *args, Env *env) {
  check_args(1, args, env);
  // TODO: varargs + escape
  Value command_value = args->values[0];
  if (command_value.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  char *command = string_to_c_string(command_value.string_value);
  FILE *p = popen(command, "r");
  free(command);
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
  env_def_fn("exec", tsc_exec, env);
}

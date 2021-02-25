/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef BUILD_H
#define BUILD_H

#include "value.h"

typedef struct {
  char *program_name;
  char *command_name;
  int argc;
  char **argv;
} GlobalArgs;

Module *get_template(const char *name, Env *env);
Env *create_template_env(Value data, Env *parent);
void delete_template_env(Env *env);
Value eval_template(Module *module, Value data, Env *env);

int build(GlobalArgs args);

char *get_src_path(String *path, Env *env);
char *get_dist_path(String *path, Env *env);

#endif

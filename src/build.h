/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef BUILD_H
#define BUILD_H

typedef struct {
  char *program_name;
  char *command_name;
  int argc;
  char **argv;
} GlobalArgs;

int build(GlobalArgs args);

#endif
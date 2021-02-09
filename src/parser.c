/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "parser.h"

#include "util.h"

static Node parse_template(Token **tokens, Module *m) {
}

Module *parse(Token *tokens, const char *file_name) {
  Module *m = create_module(file_name);
  m->root = allocate(sizeof(Node));
  *m->root = parse_template(&tokens, m);
  return m;
}

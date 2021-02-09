/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

#define SGR_RESET "\001\033[0m\002"
#define SGR_RED "\001\033[31m\002"
#define SGR_BOLD "\001\033[1m\002"
#define ERROR_LABEL SGR_BOLD SGR_RED "error: " SGR_RESET SGR_BOLD

typedef struct {
  int line;
  int column;
} Pos;

void *allocate(size_t size);

void *reallocate(void *old, size_t size);

char *copy_string(const char *src);

#endif

/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "util.h"

#include <stdio.h>

void *allocate(size_t size) {
  void *p = malloc(size);
  if (!p) {
    fprintf(stderr, "memory allocation failed!\n");
    exit(-1);
  }
  return p;
}

void *reallocate(void *old, size_t size) {
  void *new = realloc(old, size);
  if (!new) {
    fprintf(stderr, "memory allocation failed!\n");
    exit(-1);
  }
  return new;
}

/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

void *allocate(size_t size);

void *reallocate(void *old, size_t size);

#endif

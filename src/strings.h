/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef STRINGS_H
#define STRINGS_H

#include "value.h"

void import_strings(Env *env);

int string_equals(const char *c_string, String *string);
int string_starts_with(const char *prefix, String *string);
int string_ends_with(const char *prefix, String *string);

#endif

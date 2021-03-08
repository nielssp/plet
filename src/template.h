/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef TEMPLATE_H
#define TEMPLATE_H

#include "value.h"

void import_template(Env *env);

int path_is_current(String *path, Env *env);

#endif

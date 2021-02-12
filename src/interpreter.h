/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "value.h"

Value interpret(Node node, Env *env);

#endif

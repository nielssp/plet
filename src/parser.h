/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "token.h"

Module *parse(TokenStream tokens, const char *file_name);

#endif


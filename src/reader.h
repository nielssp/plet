/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef READER_H
#define READER_H

#include "token.h"

#include <stdint.h>
#include <stdio.h>

typedef struct Reader Reader;

Reader *open_reader(FILE *file, const char *file_name, SymbolMap *symbol_map);
void close_reader(Reader *r);
int reader_errors(Reader *r);

TokenStream read_all(Reader *r, int template);

#endif

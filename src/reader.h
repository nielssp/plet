/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef READER_H
#define READER_H

#include <stdint.h>
#include <stdio.h>

typedef struct Token Token;
typedef struct Reader Reader;

typedef enum {
  T_ERROR,
  T_NAME,
  T_KEYWORD,
  T_OPERATOR,
  T_STRING,
  T_INT,
  T_FLOAT,
  T_TEXT,
  T_LF,
  T_END_QUOTE,
  T_START_QUOTE,
  T_PUNCT,
  T_EOF
} TokenType;

struct Token {
  union {
    int64_t int_value;
    double float_value;
    uint8_t *string_value;
  };
  Token *next;
  int line;
  int column;
  uint32_t size;
  TokenType type;
};

Reader *open_reader(FILE *file);
void close_reader(Reader r);

void delete_token(Token *t);

Token *read_all(Reader *r, int template);

#endif

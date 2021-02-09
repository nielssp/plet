/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef READER_H
#define READER_H

#include "util.h"

#include <stdint.h>
#include <stdio.h>

typedef struct Token Token;
typedef struct Reader Reader;

typedef enum {
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
    char *name_value;
    uint8_t *string_value;
    char operator_value[3];
    char punct_value;
  };
  Token *next;
  size_t size;
  Pos start;
  Pos end;
  int error;
  TokenType type;
};

Reader *open_reader(FILE *file, const char *file_name);
void close_reader(Reader *r);
int reader_errors(Reader *r);

const char *token_name(TokenType type);
void delete_token(Token *t);
void delete_tokens(Token *t);

Token *read_all(Reader *r, int template);

#endif

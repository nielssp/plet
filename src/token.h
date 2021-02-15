/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef TOKEN_H
#define TOKEN_H

#include "util.h"

#include <stddef.h>
#include <stdint.h>

typedef const char *Symbol;

typedef struct SymbolMap SymbolMap;

typedef struct Token Token;

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
    Symbol name_value;
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

typedef Token *(*TokenStreamPeek)(void *);
typedef Token *(*TokenStreamPop)(void *);

typedef struct {
  TokenStreamPeek peek;
  TokenStreamPop pop;
  void *context;
} TokenStream;

SymbolMap *create_symbol_map(void);
void delete_symbol_map(SymbolMap *symbol_map);

Symbol get_symbol(const char *name, SymbolMap *symbol_map);

uint8_t *copy_string_token(Token *token);

const char *token_name(TokenType type);
void delete_token(Token *t);
void delete_tokens(Token *t);

Token *peek_token(TokenStream tokens);
Token *pop_token(TokenStream tokens);

#endif

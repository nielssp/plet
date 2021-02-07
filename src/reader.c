/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "reader.h"

#include <stdlib.h>

struct Reader {
  FILE *file;
  int line;
  int column;
  int la;
  uint8_t buffer[3];
}

Reader *open_reader(FILE *file) {
  Reader *r = malloc(sizeof(Reader));
  r->file = file;
  r->line = 1;
  r->column = 1;
  r->la = 0;
  return r;
}

void close_reader(Reader *r) {
  free(r);
}

static int peek_n(uint8_t n, Reader *r) {
  while (r->la < n) {
    int c = fgetc(r->file);
    if (c == EOF) {
      return EOF;
    }
    r->buffer[r->la] = (uint8_t)c;
    r->la++;
  }
  return r->buffer[n - 1];
}

static int peek(Reader *r) {
  return peek_n(1, r);
}

static int pop(Reader *r) {
  int c;
  if (r->la > 0) {
    c = r->buffer[0];
    r->la--;
    for (int i = 0; i < r->la; i++) {
      r->buffer[i] = r->buffer[i + 1];
    }
  } else {
    c = fgetc(r->file);
  }
  if (c == '\n') {
    r->line++;
    r->column = 1;
  } else {
    r->column++;
  }
  return c;
}

static Token *create_token(TokenType type, Reader *r) {
  Token *t = malloc(sizeof(Token));
  t->string_value = NULL;
  t->next = NULL;
  t->line = r->line;
  t->column = r->column;
  t->size = 0;
  t->type = type;
  return t;
}

void delete_token(Token *t) {
  switch (t->type) {
    case T_NAME:
    case T_KEYWORD:
    case T_OPERATOR:
    case T_STRING:
      if (t->string_value) {
        free(t->string_value);
      }
      break;
    default:
      break;
  }
  free(t);
}

static Token *read_name(Reader *r) {
  Token *token = create_token(T_NAME, r);
}

static Token *read_operator(Reader *r) {
}

static char *utf8_encode(uint32_t code_point) {
}

static char *read_escape_sequence(Reader *r, int double_quote) {
} 

static Token *read_verbatim(Reader *r) {
}

static Token *read_string(Reader *r) {
}

static Token *read_number(Reader *r) {
}

static Token *read_next_token(Reader *r) {
}

Token *read_all(Reader *r, int template) {
}

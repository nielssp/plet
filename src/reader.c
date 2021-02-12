/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "reader.h"

#include "hashmap.h"
#include "util.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct ParenStack ParenStack;

struct ParenStack {
  ParenStack *next;
  uint8_t paren;
};

struct SymbolMap {
  GenericHashMap map;
};

struct Reader {
  FILE *file;
  char *file_name;
  SymbolMap *symbol_map;
  ParenStack *parens;
  Pos pos;
  int errors;
  int la;
  uint8_t buffer[3];
};

const char *keywords[] = {
  "if", "else", "for", "in", "switch", "case", "default", "end", "fn", "and", "or", "not", "do", NULL
};

static Hash symbol_hash(const void *p) {
  Symbol name = *(Symbol *) p;
  Hash h = INIT_HASH;
  while (*name) {
    h = HASH_ADD_BYTE(*name, h);
    name++;
  }
  return h;
}

static int symbol_equals(const void *a, const void *b) {
  return strcmp(*(const Symbol *) a, *(const Symbol *) b) == 0;
}

SymbolMap *create_symbol_map() {
  SymbolMap *symbol_map = allocate(sizeof(SymbolMap));
  init_generic_hash_map(&symbol_map->map, sizeof(Symbol), 0, symbol_hash, symbol_equals, NULL);
  return symbol_map;
}

void delete_symbol_map(SymbolMap *symbol_map) {
  char *symbol;
  HashMapIterator it = generic_hash_map_iterate(&symbol_map->map);
  while (generic_hash_map_next(&it, &symbol)) {
    free(symbol);
  }
  delete_generic_hash_map(&symbol_map->map);
  free(symbol_map);
}

Symbol get_symbol(const char *name, SymbolMap *symbol_map) {
  Symbol symbol;
  if (generic_hash_map_get(&symbol_map->map, &name, &symbol)) {
    return symbol;
  }
  symbol = copy_string(name);
  generic_hash_map_add(&symbol_map->map, &symbol);
  return symbol;
}

Reader *open_reader(FILE *file, const char *file_name, SymbolMap *symbol_map) {
  Reader *r = allocate(sizeof(Reader));
  r->file = file;
  r->file_name = copy_string(file_name);
  r->symbol_map = symbol_map;
  r->parens = NULL;
  r->pos.line = 1;
  r->pos.column = 1;
  r->la = 0;
  return r;
}

int reader_errors(Reader *r) {
  return r->errors;
}

static uint8_t get_top_paren(Reader *r) {
  if (r->parens) {
    return r->parens->paren;
  }
  return 0;
}

static uint8_t pop_paren(Reader *r) {
  uint8_t top = 0;
  if (r->parens) {
    ParenStack *p = r->parens;
    r->parens = p->next;
    top = p->paren;
    free(p);
  }
  return top;
}

static void push_paren(Reader *r, uint8_t paren) {
  ParenStack *p = allocate(sizeof(ParenStack));
  p->next = r->parens;
  p->paren = paren;
  r->parens = p;
}

static void clear_parens(Reader *r) {
  while (r->parens) {
    pop_paren(r);
  }
}

void close_reader(Reader *r) {
  clear_parens(r);
  free(r->file_name);
  free(r);
}

static void reader_error(Reader *r, const char *format, ...) {
  va_list va;
  fprintf(stderr, SGR_BOLD "%s:%d:%d: " ERROR_LABEL, r->file_name, r->pos.line, r->pos.column);
  va_start(va, format);
  vfprintf(stderr, format, va);
  va_end(va);
  fprintf(stderr, "\n" SGR_RESET);
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
    r->pos.line++;
    r->pos.column = 1;
  } else {
    r->pos.column++;
  }
  return c;
}

static Token *create_token(TokenType type, Reader *r) {
  Token *t = allocate(sizeof(Token));
  t->string_value = NULL;
  t->next = NULL;
  t->start = r->pos;
  t->end = r->pos;
  t->size = 0;
  t->type = type;
  t->error = 0;
  return t;
}

const char *token_name(TokenType type) {
  switch (type) {
    case T_NAME: return "name";
    case T_KEYWORD: return "keyword";
    case T_OPERATOR: return "operator";
    case T_STRING: return "string";
    case T_INT: return "integer";
    case T_FLOAT: return "float";
    case T_TEXT: return "text";
    case T_LF: return "newline";
    case T_END_QUOTE: return "end quote";
    case T_START_QUOTE: return "start quote";
    case T_PUNCT: return "punctuation";
    case T_EOF: return "eof";
    default: return "unknwon";
  }
}

void delete_token(Token *t) {
  switch (t->type) {
    case T_NAME:
    case T_KEYWORD:
      break;
    case T_STRING:
    case T_TEXT:
      if (t->string_value) {
        free(t->string_value);
      }
      break;
    default:
      break;
  }
  free(t);
}

void delete_tokens(Token *t) {
  if (t->next) {
    delete_tokens(t->next);
  }
  delete_token(t);
}

uint8_t *copy_string_token(Token *token) {
  uint8_t *copy = allocate(token->size + 1);
  memcpy(copy, token->string_value, token->size + 1);
  return copy;
}

static int is_valid_name_char(int c) {
  return c == '_' || isalnum(c);
}

static Token *read_name(Reader *r) {
  Token *token = create_token(T_NAME, r);
  Buffer buffer = create_buffer();
  while (1) {
    int c = peek(r);
    if (c == EOF || !is_valid_name_char(c)) {
      if (!buffer.size) {
        pop(r);
        reader_error(r, "unexpected '%c'", c);
        token->error = 1;
      }
      break;
    }
    buffer_put(&buffer, c);
    pop(r);
  }
  token->size = buffer.size;
  buffer_put(&buffer, '\0');
  token->name_value = get_symbol((char *) buffer.data, r->symbol_map);
  delete_buffer(buffer);
  for (const char **keyword = keywords; *keyword; keyword++) {
    if (strcmp(*keyword, token->name_value) == 0) {
      token->type = T_KEYWORD;
      break;
    }
  }
  token->end = r->pos;
  return token;
}

static int is_operator_char(int c) {
  return !!strchr("+-*/%!<>=|.,:", c);
}

static Token *read_operator(Reader *r) {
  Token *token = create_token(T_OPERATOR, r);
  token->operator_value[0] = (uint8_t) pop(r);
  token->operator_value[1] = '\0';
  switch (token->operator_value[0]) {
    case '-':
      if (peek(r) == '=' || peek(r) == '>') {
        token->operator_value[1] = (uint8_t) pop(r);
        token->operator_value[2] = '\0';
      }
      break;
    case '+':
    case '*':
    case '/':
    case '<':
    case '>':
    case '=':
    case '!':
      if (peek(r) == '=') {
        token->operator_value[1] = (uint8_t) pop(r);
        token->operator_value[2] = '\0';
      }
      break;
  }
  token->end = r->pos;
  return token;
}

static int utf8_encode(uint32_t code_point, Buffer *buffer, Reader *r) {
  if (code_point < 0x80) {
    buffer_put(buffer, code_point);
    return 1;
  }
  if (code_point > 0x10FFFFFF) {
    reader_error(r, "unicode code point out of range: 0x%x", code_point);
    return 0;
  }
  uint8_t bytes[3];
  bytes[0] = 0x80 | (code_point & 0x3F);
  code_point >>= 6;
  if (code_point < 0x20) {
    buffer_put(buffer, 0xC0 | code_point);
  } else {
    bytes[1] = 0x80 | (code_point & 0x3F);
    code_point >>= 6;
    if (code_point < 0x10) {
      buffer_put(buffer, 0xE0 | code_point);
    } else {
      bytes[2] = 0x80 | (code_point & 0x3F);
      code_point >>= 6;
      buffer_put(buffer, 0xF0 | code_point);
      buffer_put(buffer, bytes[2]);
    }
    buffer_put(buffer, bytes[1]);
  }
  buffer_put(buffer, bytes[0]);
  return 1;
}

static uint8_t hex_to_dec(int c) {
  if (c >= 'a') {
    return 10 + (c - 'a');
  } else if (c >= 'A') {
    return 10 + (c - 'A');
  } else {
    return (c - '0');
  }
}

static int read_hex_code_point(Reader *r, uint32_t *code_point, int length) {
  *code_point = 0;
  for (int i = 0; i < length; i++) {
    *code_point <<= 4;
    int c = peek(r);
    if (!isxdigit(c)) {
      reader_error(r, "invalid hexadecimal escape sequence");
      return 0;
    }
    *code_point |= hex_to_dec(pop(r));
  }
  return 1;
}

static int read_escape_sequence(Reader *r, Buffer *buffer, int double_quote) {
  uint32_t code_point;
  int c = pop(r);
  if (c == EOF) {
    reader_error(r, "unexpected end of input");
    return 0;
  }
  if (double_quote && (c == '{' || c == '}')) {
    buffer_put(buffer, pop(r));
    return 1;
  }
  switch (c) {
    case '"':
    case '\'':
    case '\\':
    case '/':
      buffer_put(buffer, c);
      break;
    case 'b':
      buffer_put(buffer, '\x08');
      break;
    case 'f':
      buffer_put(buffer, '\f');
      break;
    case 'n':
      buffer_put(buffer, '\n');
      break;
    case 'r':
      buffer_put(buffer, '\r');
      break;
    case 't':
      buffer_put(buffer, '\t');
      break;
    case 'x':
      if (!read_hex_code_point(r, &code_point, 2)) {
        return 0;
      }
      buffer_put(buffer, code_point);
      break;
    case 'u':
      if (!read_hex_code_point(r, &code_point, 4)) {
        return 0;
      }
      if (!utf8_encode(code_point, buffer, r)) {
        return 0;
      }
      break;
    case 'U':
      if (!read_hex_code_point(r, &code_point, 8)) {
        return 0;
      }
      if (!utf8_encode(code_point, buffer, r)) {
        return 0;
      }
      break;
    default:
      reader_error(r, "undefined escape sequence: '\\%c'", c);
      return 0;
  }
  return 1;
} 

static Token *read_string(Reader *r) {
  Token *token = create_token(T_STRING, r);
  Buffer buffer = create_buffer();
  pop(r);
  while (1) {
    int c = peek(r);
    if (c == EOF) {
      reader_error(r, "missing end of string literal, string literal started on line %d:%d", token->start.line, token->start.column);
      token->error = 1;
      break;
    } else if (c == '\'') {
      pop(r);
      break;
    } else if (c == '\\') {
      pop(r);
      if (!read_escape_sequence(r, &buffer, 0)) {
        token->error = 1;
      }
    } else {
      buffer_put(&buffer, pop(r));
    }
  }
  token->size = buffer.size;
  buffer_put(&buffer, '\0');
  token->string_value = buffer.data;
  token->end = r->pos;
  return token;
}

static Token *read_verbatim(Reader *r) {
  Token *token = create_token(T_STRING, r);
  Buffer buffer = create_buffer();
  pop(r);
  pop(r);
  pop(r);
  while (1) {
    int c = peek(r);
    if (c == EOF) {
      reader_error(r, "missing end of string literal, string literal started on line %d:%d", token->start.line, token->start.column);
      token->error = 1;
      break;
    } else if (c == '"' && peek_n(2, r) == '"' && peek_n(3, r) == '"') {
      pop(r);
      pop(r);
      pop(r);
      break;
    } else {
      buffer_put(&buffer, pop(r));
    }
  }
  token->size = buffer.size;
  buffer_put(&buffer, '\0');
  token->string_value = buffer.data;
  token->end = r->pos;
  return token;
}

static Token *read_number(Reader *r) {
  int c;
  Token *token = create_token(T_INT, r);
  Buffer buffer = create_buffer();
  while (1) {
    c = peek(r);
    if (c == EOF || !isdigit(c)) {
      break;
    }
    buffer_put(&buffer, pop(r));
  }
  c = peek(r);
  if (c == '.' || c == 'e' || c == 'E') {
    token->type = T_FLOAT;
    if (c == '.') {
      buffer_put(&buffer, pop(r));
      while (1) {
        c = peek(r);
        if (c == EOF || !isdigit(c)) {
          break;
        }
        buffer_put(&buffer, pop(r));
      }
    }
    if (c == 'e' || c == 'E') {
      buffer_put(&buffer, pop(r));
      c = peek(r);
      if (c == '+' || c == '-') {
        buffer_put(&buffer, pop(r));
      }
      while (1) {
        c = peek(r);
        if (c == EOF || !isdigit(c)) {
          break;
        }
        buffer_put(&buffer, pop(r));
      }
    }
    buffer_put(&buffer, '\0');
    token->float_value = atof((char *) buffer.data);
  } else {
    buffer_put(&buffer, '\0');
    token->int_value = atol((char *) buffer.data);
  }
  delete_buffer(buffer);
  token->end = r->pos;
  return token;
}

static void skip_ws(Reader *r, int skip_lf) {
  int c = peek(r);
  while (c == ' ' || c == '\t' || c == '\r' || (skip_lf && c == '\n')) {
    pop(r);
    c = peek(r);
  }
}

static Token *read_next_token(Reader *r) {
  if (peek(r) == EOF) {
    return create_token(T_EOF, r);
  }
  uint8_t top_paren = get_top_paren(r);
  if (!top_paren || top_paren == '"') {
    Token *token = create_token(T_TEXT, r);
    Buffer buffer = create_buffer();
    while (1) {
      int c = peek(r);
      if (c == EOF) {
        break;
      } else if (c == '{') {
        pop(r);
        if (peek(r) == '#') {
          // Comment
          pop(r);
          while (peek(r) != EOF) {
            if (pop(r) == '#') {
              if (peek(r) == '}') {
                pop(r);
                break;
              }
            }
          }
        } else {
          push_paren(r, '{');
        }
        break;
      } else if (top_paren == '"' && c == '\\') {
        pop(r);
        if (!read_escape_sequence(r, &buffer, 1)) {
          token->error = 1;
        }
      } else if (top_paren == '"' && c == '"') {
        // Don't pop char, will be picked up as T_END_QUOTE on next call
        pop_paren(r);
        push_paren(r, '$'); // end quote
        break;
      } else {
        buffer_put(&buffer, pop(r));
      }
    }
    token->size = buffer.size;
    buffer_put(&buffer, '\0');
    token->string_value = buffer.data;
    token->end = r->pos;
    return token;
  } else {
    int is_command = (top_paren == '{' && (!r->parens->next || (r->parens->next && r->parens->next->paren == '"')));
    skip_ws(r, !!r->parens->next);
    int c = peek(r);
    if (c == '\n') {
      Token *token = create_token(T_LF, r);
      pop(r);
      token->end = r->pos;
      return token;
    } else if (c == '}' && is_command) {
      pop(r);
      pop_paren(r);
      return read_next_token(r);
    } else if (c == '\'') {
      return read_string(r);
    } else if (c == '"' && top_paren == '$') {
      Token *token = create_token(T_END_QUOTE, r);
      pop(r);
      pop_paren(r);
      token->end = r->pos;
      return token;
    } else if (c == '"') {
      if (peek_n(2, r) == '"' && peek_n(3, r) == '"') {
        return read_verbatim(r);
      } else {
        Token *token = create_token(T_START_QUOTE, r);
        pop(r);
        push_paren(r, '"');
        token->end = r->pos;
        return token;
      }
    } else if (c == '(' || c == '[' || c == '{') {
      Token *token = create_token(T_PUNCT, r);
      token->punct_value = pop(r);
      if (c == '{' && peek(r) == '#') {
        // Comment
        pop(r);
        while (peek(r) != EOF) {
          if (pop(r) == '#') {
            if (peek(r) == '}') {
              pop(r);
              break;
            }
          }
        }
        return read_next_token(r);
      }
      push_paren(r, token->punct_value);
      token->end = r->pos;
      return token;
    } else if (c == ')' || c == ']' || c == '}') {
      Token *token = create_token(T_PUNCT, r);
      token->punct_value = pop(r);
      if (!top_paren) {
        reader_error(r, "unexpected '%c'", c);
        token->error = 1;
      } else {
        uint8_t expected = pop_paren(r);
        switch (expected) {
          case '(': expected = ')'; break;
          case '[': expected = ']'; break;
          case '{': expected = '}'; break;
        }
        if (c != expected) {
          reader_error(r, "unexpected '%c', expected '%c'", c, expected);
          token->error = 1;
        }
      }
      token->end = r->pos;
      return token;
    } else if (is_operator_char(c)) {
      return read_operator(r);
    } else if (isdigit(c)) {
      return read_number(r);
    } else {
      return read_name(r);
    }
  }
}

Token *read_all(Reader *r, int template) {
  Token *first = NULL, *last = NULL;
  r->errors = 0;
  clear_parens(r);
  if (!template) {
    push_paren(r, '{');
  }
  while (1) {
    Token *token = read_next_token(r);
    if (!first) {
      first = token;
    } else {
      last->next = token;
    }
    last = token;
    if (token->error) {
      r->errors++;
      if (r->errors > 20) {
        reader_error(r, "too many errors");
        break;
      }
    }
    if (token->type == T_EOF) {
      break;
    }
  }
  return first;
}

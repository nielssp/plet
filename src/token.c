/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "token.h"

#include "hashmap.h"

#include <stdlib.h>
#include <string.h>

struct SymbolMap {
  GenericHashMap map;
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

SymbolMap *create_symbol_map(void) {
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
  if (t) {
    delete_tokens(t->next);
    delete_token(t);
  }
}

Token *peek_token(TokenStream tokens) {
  return tokens.peek(tokens.context);
}

Token *pop_token(TokenStream tokens) {
  return tokens.pop(tokens.context);
}


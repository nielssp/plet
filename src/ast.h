/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef AST_H
#define AST_H

#include "util.h"

#include <stdint.h>
#include <stdlib.h>

#define LL_APPEND(TYPE, HEAD, LAST, ELEM) \
  do {\
    TYPE *_node = allocate(sizeof(TYPE));\
    _node->tail = NULL;\
    _node->head = (ELEM);\
    if (LAST) {\
      LAST->tail = _node;\
      LAST = _node;\
    } else {\
      HEAD = LAST = _node;\
    }\
  } while (0);

#define LL_DELETE(TYPE, HEAD, DELETE_ELEM) \
  do {\
    TYPE *_list = HEAD;\
    while (_list) {\
      TYPE *_head = _list;\
      DELETE_ELEM(_head->head);\
      _list = _head->tail;\
      free(_head);\
    }\
  } while (0);

typedef enum {
  N_NAME,
  N_INT,
  N_FLOAT,
  N_STRING,
  N_LIST,
  N_OBJECT,
  N_APPLY,
  N_SUBSCRIPT,
  N_DOT,
  N_PREFIX,
  N_INFIX,
  N_FN,
  N_IF,
  N_FOR,
  N_SWITCH,
  N_ASSIGN,
  N_BLOCK
} NodeType;

typedef struct Module Module;
typedef struct Node Node;
typedef struct NodeList NodeList;
typedef struct PropertyList PropertyList;
typedef struct NameList NameList;

struct Module {
  char *file_name;
  Node *root;
};

struct Node {
  Module *module;
  Pos start;
  Pos end;
  NodeType type;
  union {
    char *name_value;
    int64_t int_value;
    double float_value;
    struct {
      uint8_t *bytes;
      size_t size;
    } string_value;
    NodeList *list_value;
    PropertyList *object_value;
    struct {
      Node *callee;
      NodeList *args;
    } apply_value;
    struct {
      Node *list;
      Node *index;
    } subscript_value;
    struct {
      Node *object;
      uint8_t *name;
    } dot_value;
    struct {
      Node *operand;
      uint8_t operator[3];
    } prefix_value;
    struct {
      Node *left;
      Node *right;
      uint8_t operator[3];
    } infix_value;
    struct {
      NameList *params;
      Node *body;
    } fn_value;
    struct {
      Node *cond;
      Node *cons;
      Node *alt;
    } if_value;
    struct {
      uint8_t *key;
      uint8_t *value;
      Node *collection;
      Node *body;
      Node *alt;
    } for_value;
    struct {
      Node *expr;
      PropertyList *cases;
      Node *default_case;
    } switch_value;
    struct {
      Node *left;
      Node *right;
    } assign_value;
    NodeList *block_value;
  };
};

struct NodeList {
  NodeList *tail;
  Node head;
};

struct PropertyList {
  PropertyList *tail;
  Node key;
  Node value;
};

struct NameList {
  NameList *tail;
  uint8_t *head;
};

Module *create_module(const char *file_name);
void delete_module(Module *module);

#endif


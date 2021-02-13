/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef AST_H
#define AST_H

#include "reader.h"
#include "util.h"

#include <stddef.h>
#include <stdint.h>

#define LL_SIZE(HEAD) ((HEAD) ? (HEAD)->size : 0)

#define LL_APPEND(TYPE, HEAD, LAST, ELEM) \
  do {\
    TYPE *_node = allocate(sizeof(TYPE));\
    _node->tail = NULL;\
    _node->head = (ELEM);\
    if (LAST) {\
      LAST->tail = _node;\
      LAST = _node;\
      HEAD->size++;\
    } else {\
      HEAD = LAST = _node;\
      HEAD->size = 1;\
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

typedef enum {
  P_NOT,
  P_NEG
} PrefixOperator;

typedef enum {
  I_ADD,
  I_SUB,
  I_MUL,
  I_DIV,
  I_MOD,
  I_LT,
  I_LEQ,
  I_GT,
  I_GEQ,
  I_EQ,
  I_NEQ,
  I_AND,
  I_OR
} InfixOperator;

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
    Symbol name_value;
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
      Symbol name;
    } dot_value;
    struct {
      Node *operand;
      PrefixOperator operator;
    } prefix_value;
    struct {
      Node *left;
      Node *right;
      InfixOperator operator;
    } infix_value;
    struct {
      NameList *params;
      NameList *free_variables;
      Node *body;
    } fn_value;
    struct {
      Node *cond;
      Node *cons;
      Node *alt;
    } if_value;
    struct {
      Symbol key;
      Symbol value;
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
      InfixOperator operator;
    } assign_value;
    NodeList *block_value;
  };
};

struct NodeList {
  size_t size;
  NodeList *tail;
  Node head;
};

struct PropertyList {
  size_t size;
  PropertyList *tail;
  Node key;
  Node value;
};

struct NameList {
  size_t size;
  NameList *tail;
  Symbol head;
};

Module *create_module(const char *file_name);
void delete_module(Module *module);

void delete_node(Node node);

NameList *copy_name_list(NameList *list);
NameList *name_list_put(Symbol name, NameList *list);
NameList *name_list_remove(Symbol name, NameList *list);
void delete_name_list(NameList *list);

#endif


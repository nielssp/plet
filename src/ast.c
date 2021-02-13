/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "ast.h"

#include "util.h"

#include <stdlib.h>
#include <string.h>

#define DELETE_NODE(VALUE) \
  if (VALUE) {\
    delete_node(*(VALUE));\
    free(VALUE);\
  }

Module *create_module(const char *file_name) {
  Module *module = allocate(sizeof(Module));
  module->file_name = copy_string(file_name);
  module->root = NULL;
  return module;
}

void delete_node(Node node) {
  switch (node.type) {
    case N_NAME:
    case N_INT:
    case N_FLOAT:
      break;
    case N_STRING:
      free(node.string_value.bytes);
      break;
    case N_LIST:
      LL_DELETE(NodeList, node.list_value, delete_node);
      break;
    case N_OBJECT: {
      PropertyList *list = node.object_value;
      while (list) {
        PropertyList *head = list;
        delete_node(head->key);
        delete_node(head->value);
        list = head->tail;
        free(head);
      }
      break;
    }
    case N_APPLY:
      DELETE_NODE(node.apply_value.callee);
      LL_DELETE(NodeList, node.apply_value.args, delete_node);
      break;
    case N_SUBSCRIPT:
      DELETE_NODE(node.subscript_value.list);
      DELETE_NODE(node.subscript_value.index);
      break;
    case N_DOT:
      DELETE_NODE(node.dot_value.object);
      break;
    case N_PREFIX:
      DELETE_NODE(node.prefix_value.operand);
      break;
    case N_INFIX:
      DELETE_NODE(node.infix_value.left);
      DELETE_NODE(node.infix_value.right);
      break;
    case N_FN:
      delete_name_list(node.fn_value.params);
      delete_name_list(node.fn_value.free_variables);
      DELETE_NODE(node.fn_value.body);
      break;
    case N_IF:
      DELETE_NODE(node.if_value.cond);
      DELETE_NODE(node.if_value.cons);
      DELETE_NODE(node.if_value.alt);
      break;
    case N_FOR:
      DELETE_NODE(node.for_value.collection);
      DELETE_NODE(node.for_value.body);
      DELETE_NODE(node.for_value.alt);
      break;
    case N_SWITCH: {
      DELETE_NODE(node.switch_value.expr);
      DELETE_NODE(node.switch_value.default_case);
      PropertyList *list = node.switch_value.cases;
      while (list) {
        PropertyList *head = list;
        delete_node(head->key);
        delete_node(head->value);
        list = head->tail;
        free(head);
      }
      break;
    }
    case N_ASSIGN:
      DELETE_NODE(node.assign_value.left);
      DELETE_NODE(node.assign_value.right);
      break;
    case N_BLOCK:
      LL_DELETE(NodeList, node.block_value, delete_node);
      break;
  }
}

void delete_module(Module *module) {
  DELETE_NODE(module->root);
  free(module->file_name);
  free(module);
}

NameList *copy_name_list(NameList *list) {
  if (!list) {
    return NULL;
  }
  NameList *copy = allocate(sizeof(NameList));
  copy->size = list->size;
  copy->tail = copy_name_list(list->tail);
  copy->head = list->head;
  return copy;
}

NameList *name_list_put(Symbol name, NameList *list) {
  for (NameList *elem = list; elem; elem = elem->tail) {
    if (strcmp(elem->head, name) == 0) {
      return list;
    }
  }
  NameList *new = allocate(sizeof(NameList));
  new->tail = list;
  new->head = name;
  new->size = list ? list->size + 1 : 0;
  return new;
}

NameList *name_list_remove(Symbol name, NameList *list) {
  if (!list) {
    return NULL;
  }
  if (strcmp(list->head, name) == 0) {
    NameList *tail = list->tail;
    tail->size = list->size - 1;
    free(list);
    return tail;
  }
  for (NameList *elem = list; elem && elem->tail; elem = elem->tail) {
    if (strcmp(elem->tail->head, name) == 0) {
      NameList *delete = elem->tail;
      elem->tail = delete->tail;
      free(delete);
      list->size--;
    }
  }
  return list;
}

void delete_name_list(NameList *list) {
  while (list) {
    NameList *tail = list->tail;
    free(list);
    list = tail;
  }
}

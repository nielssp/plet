/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "ast.h"

#include "util.h"

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
      free(node.name_value);
      break;
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
      free(node.dot_value.name);
      break;
    case N_PREFIX:
      DELETE_NODE(node.prefix_value.operand);
      break;
    case N_INFIX:
      DELETE_NODE(node.infix_value.left);
      DELETE_NODE(node.infix_value.right);
      break;
    case N_FN:
      LL_DELETE(NameList, node.fn_value.params, free);
      DELETE_NODE(node.fn_value.body);
      break;
    case N_IF:
      DELETE_NODE(node.if_value.cond);
      DELETE_NODE(node.if_value.cons);
      DELETE_NODE(node.if_value.alt);
      break;
    case N_FOR:
      free(node.for_value.key);
      free(node.for_value.value);
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

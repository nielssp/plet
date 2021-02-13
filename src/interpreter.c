/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "interpreter.h"

#include <alloca.h>
#include <stdarg.h>
#include <string.h>

static void eval_error(Node node, const char *format, ...) {
  va_list va;
  fprintf(stderr, SGR_BOLD "%s:%d:%d: " ERROR_LABEL, node.module->file_name, node.start.line, node.start.column);
  va_start(va, format);
  vfprintf(stderr, format, va);
  va_end(va);
  fprintf(stderr, "\n" SGR_RESET);
}

static Value eval_apply(Node node, Env *env) {
  Value callee = interpret(*node.apply_value.callee, env);
  Tuple *args = alloca(sizeof(Tuple) + LL_SIZE(node.apply_value.args) * sizeof(Value));
  args->size = LL_SIZE(node.apply_value.args);
  for (int i = 0; i < args->size; i++) {
    args->values[i] = interpret(node.apply_value.args->head, env);
    node.apply_value.args = node.apply_value.args->tail;
  }
  if (callee.type == V_FUNCTION) {
    return callee.function_value(args, env);
  } else if (callee.type == V_CLOSURE) {
    Env *closure_env = create_env(env->arena);
    ObjectIterator it = iterate_object(callee.closure_value->env);
    Value key, value;
    while (object_iterator_next(&it, &key, &value)) {
      if (key.type == V_SYMBOL) {
        env_put(key.symbol_value, value, closure_env);
      }
      int i = 0;
      while (callee.closure_value->params) {
        Value arg;
        if (i < args->size) {
          arg = args->values[i];
        } else {
          arg = nil_value;
        }
        env_put(callee.closure_value->params->head, arg, closure_env);
        callee.closure_value->params = callee.closure_value->params->tail;
      }
    }
    return interpret(callee.closure_value->body, closure_env);
  } else {
    eval_error(*node.apply_value.callee, "not a function");
    return nil_value;
  }
}

static Value eval_subscript(Node node, Env *env) {
  Value object = interpret(*node.subscript_value.list, env);
  Value index = interpret(*node.subscript_value.index, env);
  if (object.type == V_OBJECT) {
    Value value;
    if (object_get(object.object_value, index, &value)) {
      return value;
    }
    return nil_value;
  }
  if (index.type != V_INT) {
    eval_error(*node.subscript_value.index, "expected an integer");
    return nil_value;
  }
  if (object.type == V_ARRAY) {
    if (index.int_value < 0 || index.int_value >= object.array_value->size) {
      eval_error(*node.subscript_value.list, "array index out of range");
      return nil_value;
    }
    return object.array_value->cells[index.int_value];
  } else if (object.type == V_STRING) {
    if (index.int_value < 0 || index.int_value >= object.string_value->size) {
      eval_error(*node.subscript_value.list, "string index out of range");
      return nil_value;
    }
    return create_int(object.string_value->bytes[index.int_value]);
  } else {
    eval_error(*node.subscript_value.list, "value is not indexable");
    return nil_value;
  }
}

static Value eval_dot(Node node, Env *env) {
  Value object = interpret(*node.dot_value.object, env);
  if (object.type != V_OBJECT) {
    eval_error(*node.dot_value.object, "value is not an object");
    return nil_value;
  }
  size_t symbol_length = strlen(node.dot_value.name);
  Value key = create_string((uint8_t *) node.dot_value.name, symbol_length, env->arena);
  Value value;
  if (object_get(object.object_value, key, &value)) {
    return value;
  }
  eval_error(node, "object does not have property: %s", node.dot_value.name);
  return nil_value;
}

static Value eval_prefix(Node node, Env *env) {
  Value operand = interpret(*node.prefix_value.operand, env);
  switch (node.prefix_value.operator) {
    case P_NOT:
      if (is_truthy(operand)) {
        return nil_value;
      } else {
        return true_value;
      }
    case P_NEG:
      if (operand.type == V_INT) {
        return create_int(-operand.int_value);
      } else if (operand.type == V_FLOAT) {
        return create_float(-operand.float_value);
      } else {
        eval_error(*node.prefix_value.operand, "value is not a number");
        return nil_value;
      }
  }
  return nil_value;
}

static Value eval_infix(Node node, Env *env) {
  Value left = interpret(*node.infix_value.left, env);
  switch (node.infix_value.operator) {
    case I_ADD:
    case I_SUB:
    case I_MUL:
    case I_DIV:
    case I_MOD:
    case I_LT:
    case I_LEQ:
    case I_GT:
    case I_GEQ:
      eval_error(node, "not implemented");
      return nil_value;
    case I_EQ:
      if (equals(left, interpret(*node.infix_value.right, env))) {
        return true_value;
      }
      return nil_value;
    case I_NEQ:
      if (equals(left, interpret(*node.infix_value.right, env))) {
        return nil_value;
      }
      return true_value;
    case I_AND:
      if (is_truthy(left)) {
        return interpret(*node.infix_value.right, env);
      }
      return nil_value;
    case I_OR:
      if (is_truthy(left)) {
        return left;
      }
      return interpret(*node.infix_value.right, env);
  }
  return nil_value;
}

Value interpret(Node node, Env *env) {
  switch (node.type) {
    case N_NAME: {
      Value value;
      if (env_get(node.name_value, &value, env)) {
        return value;
      }
      eval_error(node, "undefined variable: %s", node.name_value);
      return nil_value;
    }
    case N_INT:
      return create_int(node.int_value);
    case N_FLOAT:
      return create_float(node.float_value);
    case N_STRING:
      return create_string(node.string_value.bytes, node.string_value.size, env->arena);
    case N_LIST: {
      Value array = create_array(LL_SIZE(node.list_value), env->arena);
      while (node.list_value) {
        array_push(array.array_value, interpret(node.list_value->head, env), env->arena);
        node.list_value = node.list_value->tail;
      }
      return array;
    }
    case N_OBJECT: {
      Value object = create_object(LL_SIZE(node.object_value), env->arena);
      while (node.object_value) {
        object_put(object.object_value, interpret(node.object_value->key, env),
            interpret(node.object_value->value, env), env->arena);
        node.object_value = node.object_value->tail;
      }
      return object;
    }
    case N_APPLY:
      return eval_apply(node, env);
    case N_SUBSCRIPT:
      return eval_subscript(node, env);
    case N_DOT:
      return eval_dot(node, env);
    case N_PREFIX:
      return eval_prefix(node, env);
    case N_INFIX:
      return eval_infix(node, env);
    case N_FN:
      return create_closure(node.fn_value.params, node.fn_value.free_variables, *node.fn_value.body, env, env->arena);
    case N_IF:
    case N_FOR:
    case N_SWITCH:
    case N_ASSIGN:
      eval_error(node, "not implemented");
      return nil_value;
    case N_BLOCK: {
      Value value = nil_value;
      // TODO: return concatenated string
      while (node.block_value) {
        value = interpret(node.block_value->head, env);
        node.block_value = node.block_value->tail;
      }
      return value;
    }
  }
  return nil_value;
}

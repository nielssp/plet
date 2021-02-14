/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "interpreter.h"

#include <alloca.h>
#include <inttypes.h>
#include <stdarg.h>
#include <string.h>

typedef enum {
  C_ERROR,
  C_GT,
  C_LT,
  C_EQ
} Comparison;

static void eval_error(Node node, const char *format, ...) {
  va_list va;
  fprintf(stderr, SGR_BOLD "%s:%d:%d: " ERROR_LABEL, node.module->file_name, node.start.line, node.start.column);
  va_start(va, format);
  vfprintf(stderr, format, va);
  va_end(va);
  fprintf(stderr, SGR_RESET "\n");
  if (node.module->file_name) {
    print_error_line(node.module->file_name, node.start, node.end);
  }
}

static Value eval_apply(Node node, Env *env) {
  Tuple *args = alloca(sizeof(Tuple) + LL_SIZE(node.apply_value.args) * sizeof(Value));
  args->size = LL_SIZE(node.apply_value.args);
  for (int i = 0; i < args->size; i++) {
    args->values[i] = interpret(node.apply_value.args->head, env);
    node.apply_value.args = node.apply_value.args->tail;
  }
  Value callee;
  if (node.apply_value.callee->type == N_NAME) {
    if (!env_get(node.apply_value.callee->name_value, &callee, env)) {
      eval_error(*node.apply_value.callee, "undefined function: %s", node.apply_value.callee->name_value);
      return nil_value;
    }
  } else {
    callee = interpret(*node.apply_value.callee, env);
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
      i++;
    }
    return interpret(callee.closure_value->body, closure_env);
  } else {
    eval_error(*node.apply_value.callee, "value of type %s is not a function", value_name(callee.type));
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
    eval_error(*node.subscript_value.index, "value of type %s is not a valid array index", value_name(index.type));
    return nil_value;
  }
  if (object.type == V_ARRAY) {
    if (index.int_value < 0 || index.int_value >= object.array_value->size) {
      eval_error(*node.subscript_value.index, "array index out of range: %" PRId64, index.int_value);
      return nil_value;
    }
    return object.array_value->cells[index.int_value];
  } else if (object.type == V_STRING) {
    if (index.int_value < 0 || index.int_value >= object.string_value->size) {
      eval_error(*node.subscript_value.index, "string index out of range: %" PRId64, index.int_value);
      return nil_value;
    }
    return create_int(object.string_value->bytes[index.int_value]);
  } else {
    eval_error(*node.subscript_value.list, "value of type %s is not indexable", value_name(object.type));
    return nil_value;
  }
}

static Value eval_dot(Node node, Env *env) {
  Value object = interpret(*node.dot_value.object, env);
  if (object.type != V_OBJECT) {
    eval_error(*node.dot_value.object, "value of type %s is not an object", value_name(object.type));
    return nil_value;
  }
  Value key = create_symbol(node.dot_value.name);
  Value value;
  if (object_get(object.object_value, key, &value)) {
    return value;
  }
  eval_error(node, "undefined object property: %s", node.dot_value.name);
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
        eval_error(*node.prefix_value.operand, "value of type is not a number", value_name(operand.type));
        return nil_value;
      }
  }
  return nil_value;
}

static Value concatenate_strings(Value left, Value right, Env *env) {
  Buffer buffer = create_buffer(0);
  value_to_string(left, &buffer);
  value_to_string(right, &buffer);
  Value result = create_string(buffer.data, buffer.size, env->arena);
  delete_buffer(buffer);
  return result;
}

static Value eval_add(Node node, Value left, Value right, Env *env) {
  if (left.type == V_STRING || right.type == V_STRING) {
    return concatenate_strings(left, right, env);
  } else if (left.type == V_ARRAY && right.type == V_ARRAY) {
    Value result = create_array(left.array_value->size + right.array_value->size, env->arena);
    for (size_t i = 0; i < left.array_value->size; i++) {
      array_push(result.array_value, left.array_value->cells[i], env->arena);
    }
    for (size_t i = 0; i < right.array_value->size; i++) {
      array_push(result.array_value, right.array_value->cells[i], env->arena);
    }
    return result;
  } else if (left.type == V_OBJECT && right.type == V_OBJECT) {
    Value result = create_object(left.object_value->size + right.object_value->size, env->arena);
    Value key, value;
    ObjectIterator it = iterate_object(left.object_value);
    while (object_iterator_next(&it, &key, &value)) {
      object_put(result.object_value, key, value, env->arena);
    }
    it = iterate_object(right.object_value);
    while (object_iterator_next(&it, &key, &value)) {
      object_put(result.object_value, key, value, env->arena);
    }
    return result;
  } else if (left.type == V_INT) {
    if (right.type == V_INT) {
      return create_int(left.int_value + right.int_value);
    } else if (right.type == V_FLOAT) {
      return create_float(left.int_value + right.float_value);
    }
  } else if (left.type == V_FLOAT) {
    if (right.type == V_INT) {
      return create_float(left.float_value + right.int_value);
    } else if (right.type == V_FLOAT) {
      return create_float(left.float_value + right.float_value);
    }
  }
  eval_error(node, "'+'-operator undefined for types %s and %s", value_name(left.type),
      value_name(right.type));
  return nil_value;
}

static Value eval_sub(Node node, Value left, Value right, Env *env) {
  if (left.type == V_INT) {
    if (right.type == V_INT) {
      return create_int(left.int_value - right.int_value);
    } else if (right.type == V_FLOAT) {
      return create_float(left.int_value - right.float_value);
    }
  } else if (left.type == V_FLOAT) {
    if (right.type == V_INT) {
      return create_float(left.float_value - right.int_value);
    } else if (right.type == V_FLOAT) {
      return create_float(left.float_value - right.float_value);
    }
  }
  eval_error(node, "'-'-operator undefined for types %s and %s", value_name(left.type),
      value_name(right.type));
  return nil_value;
}

static Value eval_mul(Node node, Value left, Value right, Env *env) {
  if (left.type == V_INT) {
    if (right.type == V_INT) {
      return create_int(left.int_value * right.int_value);
    } else if (right.type == V_FLOAT) {
      return create_float(left.int_value * right.float_value);
    }
  } else if (left.type == V_FLOAT) {
    if (right.type == V_INT) {
      return create_float(left.float_value * right.int_value);
    } else if (right.type == V_FLOAT) {
      return create_float(left.float_value * right.float_value);
    }
  }
  eval_error(node, "'*'-operator undefined for types %s and %s", value_name(left.type),
      value_name(right.type));
  return nil_value;
}

static Value eval_div(Node node, Value left, Value right, Env *env) {
  if (left.type == V_INT) {
    if (right.type == V_INT) {
      return create_int(left.int_value / right.int_value);
    } else if (right.type == V_FLOAT) {
      return create_float(left.int_value / right.float_value);
    }
  } else if (left.type == V_FLOAT) {
    if (right.type == V_INT) {
      return create_float(left.float_value / right.int_value);
    } else if (right.type == V_FLOAT) {
      return create_float(left.float_value / right.float_value);
    }
  }
  eval_error(node, "'/'-operator undefined for types %s and %s", value_name(left.type),
      value_name(right.type));
  return nil_value;
}

static Value eval_mod(Node node, Value left, Value right, Env *env) {
  if (left.type == V_INT && right.type == V_INT) {
    return create_int(left.int_value % right.int_value);
  }
  eval_error(node, "'%%'-operator undefined for types %s and %s", value_name(left.type),
      value_name(right.type));
  return nil_value;
}

static Comparison compare_values(Node node, Value left, Value right, const char *op, Env *env) {
  if (left.type == V_INT) {
    if (right.type == V_INT) {
      if (left.int_value < right.int_value) {
        return C_LT;
      } else if (left.int_value > right.int_value) {
        return C_GT;
      } else {
        return C_EQ;
      }
    } else if (right.type == V_FLOAT) {
      if (left.int_value < right.float_value) {
        return C_LT;
      } else if (left.int_value > right.float_value) {
        return C_GT;
      } else {
        return C_EQ;
      }
    }
  } else if (left.type == V_FLOAT) {
    if (right.type == V_INT) {
      if (left.float_value < right.int_value) {
        return C_LT;
      } else if (left.float_value > right.int_value) {
        return C_GT;
      } else {
        return C_EQ;
      }
    } else if (right.type == V_FLOAT) {
      if (left.float_value < right.float_value) {
        return C_LT;
      } else if (left.float_value > right.float_value) {
        return C_GT;
      } else {
        return C_EQ;
      }
    }
  }
  eval_error(node, "'%s'-operator undefined for types %s and %s", op, value_name(left.type),
      value_name(right.type));
  return C_ERROR;
}

static Value eval_infix(Node node, Env *env) {
  Value left = interpret(*node.infix_value.left, env);
  switch (node.infix_value.operator) {
    case I_NONE:
      return nil_value;
    case I_ADD:
      return eval_add(node, left, interpret(*node.infix_value.right, env), env);
    case I_SUB:
      return eval_sub(node, left, interpret(*node.infix_value.right, env), env);
    case I_MUL:
      return eval_mul(node, left, interpret(*node.infix_value.right, env), env);
    case I_DIV:
      return eval_div(node, left, interpret(*node.infix_value.right, env), env);
    case I_MOD:
      return eval_mod(node, left, interpret(*node.infix_value.right, env), env);
    case I_LT:
      return compare_values(node, left, interpret(*node.infix_value.right, env), "<", env) == C_LT
        ? true_value
        : nil_value;
    case I_LEQ: {
      Comparison c = compare_values(node, left, interpret(*node.infix_value.right, env), "<", env);
      return c == C_LT || c == C_EQ
        ? true_value
        : nil_value;
    }
    case I_GT:
      return compare_values(node, left, interpret(*node.infix_value.right, env), ">", env) == C_GT
        ? true_value
        : nil_value;
    case I_GEQ: {
      Comparison c = compare_values(node, left, interpret(*node.infix_value.right, env), "<", env);
      return c == C_GT || c == C_EQ
        ? true_value
        : nil_value;
    }
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

static Value eval_if(Node node, Env *env) {
  Value cond = interpret(*node.if_value.cond, env);
  if (is_truthy(cond)) {
    return interpret(*node.if_value.cons, env);
  }
  return interpret(*node.if_value.alt, env);
}

static Value eval_for(Node node, Env *env) {
  Value collection = interpret(*node.for_value.collection, env);
  if (collection.type == V_ARRAY) {
    if (collection.array_value->size == 0) {
      if (node.for_value.alt) {
        return interpret(*node.for_value.alt, env);
      }
      return nil_value;
    }
    Buffer buffer = create_buffer(0);
    for (size_t i = 0; i < collection.array_value->size; i++) {
      if (node.for_value.key) {
        env_put(node.for_value.key, create_int(i), env);
      }
      env_put(node.for_value.value, collection.array_value->cells[i], env);
      value_to_string(interpret(*node.for_value.body, env), &buffer);
    }
    Value result = create_string(buffer.data, buffer.size, env->arena);
    delete_buffer(buffer);
    return result;
  } else if (collection.type == V_OBJECT) {
    if (collection.object_value->size == 0) {
      if (node.for_value.alt) {
        return interpret(*node.for_value.alt, env);
      }
      return nil_value;
    }
    Buffer buffer = create_buffer(0);
    Value key, value;
    ObjectIterator it = iterate_object(collection.object_value);
    while (object_iterator_next(&it, &key, &value)) {
      if (node.for_value.key) {
        env_put(node.for_value.key, key, env);
      }
      env_put(node.for_value.value, value, env);
      value_to_string(interpret(*node.for_value.body, env), &buffer);
    }
    Value result = create_string(buffer.data, buffer.size, env->arena);
    delete_buffer(buffer);
    return result;
  } else if (collection.type == V_STRING) {
    if (collection.string_value->size == 0) {
      if (node.for_value.alt) {
        return interpret(*node.for_value.alt, env);
      }
      return nil_value;
    }
    Buffer buffer = create_buffer(0);
    for (size_t i = 0; i < collection.string_value->size; i++) {
      if (node.for_value.key) {
        env_put(node.for_value.key, create_int(i), env);
      }
      env_put(node.for_value.value, create_int(collection.string_value->bytes[i]), env);
      value_to_string(interpret(*node.for_value.body, env), &buffer);
    }
    Value result = create_string(buffer.data, buffer.size, env->arena);
    delete_buffer(buffer);
    return result;
  } else {
    eval_error(*node.for_value.collection, "value of type %s is not iterable", value_name(collection.type));
    if (node.for_value.alt) {
      return interpret(*node.for_value.alt, env);
    }
    return nil_value;
  }
}

static Value eval_switch(Node node, Env *env) {
  Value a = interpret(*node.switch_value.expr, env);
  while (node.switch_value.cases) {
    Value b = interpret(node.switch_value.cases->key, env);
    if (equals(a, b)) {
      return interpret(node.switch_value.cases->value, env);
    }
    node.switch_value.cases = node.switch_value.cases->tail;
  }
  if (node.switch_value.default_case) {
    return interpret(*node.switch_value.default_case, env);
  }
  return nil_value;
}

static Value eval_assign_operator(Node node, Value existing, Value value, Env *env) {
  switch (node.assign_value.operator) {
    case I_ADD:
      return eval_add(node, existing, value, env);
    case I_SUB:
      return eval_sub(node, existing, value, env);
    case I_MUL:
      return eval_mul(node, existing, value, env);
    case I_DIV:
      return eval_div(node, existing, value, env);
    default:
      return value;
  }
}

static Value eval_assign(Node node, Env *env) {
  Value value = interpret(*node.assign_value.right, env);
  switch (node.assign_value.left->type) {
    case N_NAME: {
      if (node.assign_value.operator != I_NONE) {
        Value existing;
        if (!env_get(node.assign_value.left->name_value, &existing, env)) {
          eval_error(*node.assign_value.left, "undefined variable: %s",
              node.assign_value.left->name_value);
          return nil_value;
        }
        value = eval_assign_operator(node, existing, value, env);
      }
      env_put(node.assign_value.left->name_value, value, env);
      break;
    }
    case N_SUBSCRIPT: {
      Value object = interpret(*node.assign_value.left->subscript_value.list, env);
      Value index = interpret(*node.assign_value.left->subscript_value.index, env);
      if (object.type == V_OBJECT) {
        if (node.assign_value.operator != I_NONE) {
          Value existing;
          if (!object_get(object.object_value, index, &existing)) {
            eval_error(*node.assign_value.left, "undefined object property");
            return nil_value;
          }
          value = eval_assign_operator(node, existing, value, env);
        }
        object_put(object.object_value, index, value, env->arena);
      } else if (object.type == V_ARRAY) {
        if (index.type != V_INT) {
          eval_error(*node.assign_value.left->subscript_value.index,
              "value of type %s is not a valid array index", value_name(index.type));
        } else if (index.int_value < 0 || index.int_value >= object.array_value->size) {
          eval_error(*node.assign_value.left->subscript_value.index,
              "array index out of range: %" PRId64, index.int_value);
        } else {
          if (node.assign_value.operator != I_NONE) {
            value = eval_assign_operator(node, object.array_value->cells[index.int_value],
                value, env);
          }
          object.array_value->cells[index.int_value] = value;
        }
      } else {
        eval_error(*node.assign_value.left->subscript_value.list, "value of type %s is not indexable",
            value_name(object.type));
      }
      break;
    }
    case N_DOT: {
      Value object = interpret(*node.assign_value.left->dot_value.object, env);
      if (object.type == V_OBJECT) {
        Value key = create_symbol(node.assign_value.left->dot_value.name);
        if (node.assign_value.operator != I_NONE) {
          Value existing;
          if (!object_get(object.object_value, key, &existing)) {
            eval_error(*node.assign_value.left, "undefined object property: %s",
                key.symbol_value);
            return nil_value;
          }
          value = eval_assign_operator(node, existing, value, env);
        }
        object_put(object.object_value, key, value, env->arena);
      } else {
        eval_error(*node.assign_value.left->dot_value.object, "value of type %s is not an object",
            value_name(object.type));
      }
      break;
    }
    default:
      eval_error(*node.assign_value.left, "left side of assignment is invalid");
      break;
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
        if (node.object_value->key.type == N_NAME) {
          object_put(object.object_value, create_symbol(node.object_value->key.name_value),
              interpret(node.object_value->value, env), env->arena);
        } else {
          object_put(object.object_value, interpret(node.object_value->key, env),
              interpret(node.object_value->value, env), env->arena);
        }
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
      return eval_if(node, env);
    case N_FOR:
      return eval_for(node, env);
    case N_SWITCH:
      return eval_switch(node, env);
    case N_ASSIGN:
      return eval_assign(node, env);
    case N_BLOCK: {
      Buffer buffer = create_buffer(0);
      while (node.block_value) {
        value_to_string(interpret(node.block_value->head, env), &buffer);
        node.block_value = node.block_value->tail;
      }
      Value result = create_string(buffer.data, buffer.size, env->arena);
      delete_buffer(buffer);
      return result;
    }
  }
  return nil_value;
}

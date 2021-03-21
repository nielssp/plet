/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "interpreter.h"

#include "strings.h"

#include <alloca.h>
#include <inttypes.h>
#include <stdarg.h>
#include <string.h>

#define RESULT_VALUE(VALUE) ((InterpreterResult) { IR_VALUE, (VALUE) })

typedef enum {
  C_ERROR,
  C_GT,
  C_LT,
  C_EQ
} Comparison;

static void eval_error(Node node, const char *format, ...) {
  va_list va;
  fprintf(stderr, SGR_BOLD "%s:%d:%d: " ERROR_LABEL, node.module.file_name->path, node.start.line, node.start.column);
  va_start(va, format);
  vfprintf(stderr, format, va);
  va_end(va);
  fprintf(stderr, SGR_RESET "\n");
  print_error_line(node.module.file_name->path, node.start, node.end);
}

int apply(Value func, const Tuple *args, Value *return_value, Env *env) {
  if (func.type == V_FUNCTION) {
    env_clear_error(env);
    *return_value = func.function_value(args, env);
    return !env->error;
  } else if (func.type == V_CLOSURE) {
    int i = 0;
    NameList *params = func.closure_value->params;
    while (params) {
      Value arg;
      if (i < args->size) {
        arg = args->values[i];
      } else {
        arg = nil_value;
      }
      env_put(params->head, arg, func.closure_value->env);
      params = params->tail;
      i++;
    }
    *return_value = interpret(func.closure_value->body, func.closure_value->env).value;
    return 1;
  } else {
    env_error(env, -1, "value of type %s is not a function", value_name(func.type));
    return 0;
  }
}

static InterpreterResult eval_apply(Node node, Env *env) {
  InterpreterResult result;
  int suppress = node.apply_value.callee->type == N_SUPPRESS;
  Tuple *args = alloca(sizeof(Tuple) + LL_SIZE(node.apply_value.args) * sizeof(Value));
  args->size = LL_SIZE(node.apply_value.args);
  NodeList *arg_nodes = node.apply_value.args;
  for (int i = 0; i < args->size; i++) {
    result = interpret(arg_nodes->head, env);
    if (result.type != IR_VALUE) {
      return result;
    }
    args->values[i] = result.value;
    arg_nodes = arg_nodes->tail;
  }
  Value callee;
  if (node.apply_value.callee->type == N_NAME) {
    if (!env_get(node.apply_value.callee->name_value, &callee, env)) {
      eval_error(*node.apply_value.callee, "undefined function: %s", node.apply_value.callee->name_value);
      return RESULT_VALUE(nil_value);
    }
  } else {
    result = interpret(*node.apply_value.callee, env);
    if (result.type != IR_VALUE) {
      return result;
    }
    callee = result.value;
  }
  if (callee.type == V_FUNCTION) {
    env_clear_error(env);
    env->calling_node = &node;
    Value return_value = callee.function_value(args, env);
    if (env->error) {
      if (env->error_arg < 0 || env->error_arg >= args->size) {
        display_env_error(node, env->error_level, env->error_arg != ENV_ARG_NONE, "%s", env->error);
      } else {
        arg_nodes = node.apply_value.args;
        while (env->error_arg > 0) {
          arg_nodes = arg_nodes->tail;
          env->error_arg--;
        }
        display_env_error(arg_nodes->head, env->error_level, 1, "%s", env->error);
      }
      env_clear_error(env);
    }
    return RESULT_VALUE(return_value);
  } else if (callee.type == V_CLOSURE) {
    int i = 0;
    NameList *params = callee.closure_value->params;
    while (params) {
      Value arg;
      if (i < args->size) {
        arg = args->values[i];
      } else {
        arg = nil_value;
      }
      env_put(params->head, arg, callee.closure_value->env);
      params = params->tail;
      i++;
    }
    result = interpret(callee.closure_value->body, callee.closure_value->env);
    return RESULT_VALUE(result.value);
  } else {
    if (!suppress || callee.type != V_NIL) {
      eval_error(*node.apply_value.callee, "value of type %s is not a function", value_name(callee.type));
    }
    return RESULT_VALUE(nil_value);
  }
}

static InterpreterResult eval_subscript(Node node, Env *env, int suppress_name_error) {
  int suppress_type_error = node.subscript_value.list->type == N_SUPPRESS;
  InterpreterResult result = interpret(*node.subscript_value.list, env);
  if (result.type != IR_VALUE) {
    return result;
  }
  Value object = result.value;
  result = interpret(*node.subscript_value.index, env);
  if (result.type != IR_VALUE) {
    return result;
  }
  Value index = result.value;
  if (object.type == V_OBJECT) {
    Value value;
    if (object_get(object.object_value, index, &value)) {
      return RESULT_VALUE(value);
    }
    return RESULT_VALUE(nil_value);
  }
  if (index.type != V_INT) {
    if (!suppress_name_error) {
      eval_error(*node.subscript_value.index, "value of type %s is not a valid array index", value_name(index.type));
    }
    return RESULT_VALUE(nil_value);
  }
  if (object.type == V_ARRAY) {
    if (index.int_value < 0 || index.int_value >= object.array_value->size) {
      if (!suppress_name_error) {
        eval_error(*node.subscript_value.index, "array index out of range: %" PRId64, index.int_value);
      }
      return RESULT_VALUE(nil_value);
    }
    return RESULT_VALUE(object.array_value->cells[index.int_value]);
  } else if (object.type == V_STRING) {
    if (index.int_value < 0 || index.int_value >= object.string_value->size) {
      if (!suppress_name_error) {
        eval_error(*node.subscript_value.index, "string index out of range: %" PRId64, index.int_value);
      }
      return RESULT_VALUE(nil_value);
    }
    return RESULT_VALUE(create_int(object.string_value->bytes[index.int_value]));
  } else {
    if (!suppress_type_error || object.type != V_NIL) {
      eval_error(*node.subscript_value.list, "value of type %s is not indexable", value_name(object.type));
    }
    return RESULT_VALUE(nil_value);
  }
}

static InterpreterResult eval_dot(Node node, Env *env, int suppress_name_error) {
  int suppress_type_error = node.dot_value.object->type == N_SUPPRESS;
  InterpreterResult result = interpret(*node.dot_value.object, env);
  if (result.type != IR_VALUE) {
    return result;
  }
  Value object = result.value;
  if (object.type != V_OBJECT) {
    if (!suppress_type_error || object.type != V_NIL) {
      eval_error(*node.dot_value.object, "value of type %s is not an object", value_name(object.type));
    }
    return RESULT_VALUE(nil_value);
  }
  Value key = create_symbol(node.dot_value.name);
  Value value;
  if (object_get(object.object_value, key, &value)) {
    return RESULT_VALUE(value);
  }
  if (!suppress_name_error) {
    eval_error(node, "undefined object property: %s", node.dot_value.name);
  }
  return RESULT_VALUE(nil_value);
}

static InterpreterResult eval_prefix(Node node, Env *env) {
  InterpreterResult operand = interpret(*node.prefix_value.operand, env);
  if (operand.type != IR_VALUE) {
    return operand;
  }
  switch (node.prefix_value.operator) {
    case P_NOT:
      if (is_truthy(operand.value)) {
        return RESULT_VALUE(nil_value);
      } else {
        return RESULT_VALUE(true_value);
      }
    case P_NEG:
      if (operand.value.type == V_INT) {
        return RESULT_VALUE(create_int(-operand.value.int_value));
      } else if (operand.value.type == V_FLOAT) {
        return RESULT_VALUE(create_float(-operand.value.float_value));
      } else {
        eval_error(*node.prefix_value.operand, "value of type is not a number", value_name(operand.value.type));
        return RESULT_VALUE(nil_value);
      }
  }
  return RESULT_VALUE(nil_value);
}

static Value concatenate_strings(Value left, Value right, Env *env) {
  StringBuffer buffer = create_string_buffer(0, env->arena);
  string_buffer_append_value(&buffer, left);
  string_buffer_append_value(&buffer, right);
  return finalize_string_buffer(buffer);
}

static InterpreterResult eval_add(Node node, Value left, Value right, Env *env) {
  if (left.type == V_STRING || right.type == V_STRING) {
    return RESULT_VALUE(concatenate_strings(left, right, env));
  } else if (left.type == V_ARRAY && right.type == V_ARRAY) {
    Value result = create_array(left.array_value->size + right.array_value->size, env->arena);
    for (size_t i = 0; i < left.array_value->size; i++) {
      array_push(result.array_value, left.array_value->cells[i], env->arena);
    }
    for (size_t i = 0; i < right.array_value->size; i++) {
      array_push(result.array_value, right.array_value->cells[i], env->arena);
    }
    return RESULT_VALUE(result);
  } else if (left.type == V_OBJECT && right.type == V_OBJECT) {
    Value result = create_object(object_size(left.object_value) + object_size(right.object_value), env->arena);
    Value key, value;
    ObjectIterator it = iterate_object(left.object_value);
    while (object_iterator_next(&it, &key, &value)) {
      object_put(result.object_value, key, value, env->arena);
    }
    it = iterate_object(right.object_value);
    while (object_iterator_next(&it, &key, &value)) {
      object_put(result.object_value, key, value, env->arena);
    }
    return RESULT_VALUE(result);
  } else if (left.type == V_INT) {
    if (right.type == V_INT) {
      return RESULT_VALUE(create_int(left.int_value + right.int_value));
    } else if (right.type == V_FLOAT) {
      return RESULT_VALUE(create_float(left.int_value + right.float_value));
    }
  } else if (left.type == V_FLOAT) {
    if (right.type == V_INT) {
      return RESULT_VALUE(create_float(left.float_value + right.int_value));
    } else if (right.type == V_FLOAT) {
      return RESULT_VALUE(create_float(left.float_value + right.float_value));
    }
  }
  eval_error(node, "'+'-operator undefined for types %s and %s", value_name(left.type),
      value_name(right.type));
  return RESULT_VALUE(nil_value);
}

static InterpreterResult eval_sub(Node node, Value left, Value right, Env *env) {
  if (left.type == V_INT) {
    if (right.type == V_INT) {
      return RESULT_VALUE(create_int(left.int_value - right.int_value));
    } else if (right.type == V_FLOAT) {
      return RESULT_VALUE(create_float(left.int_value - right.float_value));
    }
  } else if (left.type == V_FLOAT) {
    if (right.type == V_INT) {
      return RESULT_VALUE(create_float(left.float_value - right.int_value));
    } else if (right.type == V_FLOAT) {
      return RESULT_VALUE(create_float(left.float_value - right.float_value));
    }
  }
  eval_error(node, "'-'-operator undefined for types %s and %s", value_name(left.type),
      value_name(right.type));
  return RESULT_VALUE(nil_value);
}

static InterpreterResult eval_mul(Node node, Value left, Value right, Env *env) {
  if (left.type == V_INT) {
    if (right.type == V_INT) {
      return RESULT_VALUE(create_int(left.int_value * right.int_value));
    } else if (right.type == V_FLOAT) {
      return RESULT_VALUE(create_float(left.int_value * right.float_value));
    }
  } else if (left.type == V_FLOAT) {
    if (right.type == V_INT) {
      return RESULT_VALUE(create_float(left.float_value * right.int_value));
    } else if (right.type == V_FLOAT) {
      return RESULT_VALUE(create_float(left.float_value * right.float_value));
    }
  }
  eval_error(node, "'*'-operator undefined for types %s and %s", value_name(left.type),
      value_name(right.type));
  return RESULT_VALUE(nil_value);
}

static InterpreterResult eval_div(Node node, Value left, Value right, Env *env) {
  if (left.type == V_INT) {
    if (right.type == V_INT) {
      return RESULT_VALUE(create_int(left.int_value / right.int_value));
    } else if (right.type == V_FLOAT) {
      return RESULT_VALUE(create_float(left.int_value / right.float_value));
    }
  } else if (left.type == V_FLOAT) {
    if (right.type == V_INT) {
      return RESULT_VALUE(create_float(left.float_value / right.int_value));
    } else if (right.type == V_FLOAT) {
      return RESULT_VALUE(create_float(left.float_value / right.float_value));
    }
  }
  eval_error(node, "'/'-operator undefined for types %s and %s", value_name(left.type),
      value_name(right.type));
  return RESULT_VALUE(nil_value);
}

static InterpreterResult eval_mod(Node node, Value left, Value right, Env *env) {
  if (left.type == V_INT && right.type == V_INT) {
    return RESULT_VALUE(create_int(left.int_value % right.int_value));
  }
  eval_error(node, "'%%'-operator undefined for types %s and %s", value_name(left.type),
      value_name(right.type));
  return RESULT_VALUE(nil_value);
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

static InterpreterResult eval_infix(Node node, Env *env) {
  InterpreterResult right, left = interpret(*node.infix_value.left, env);
  if (left.type != IR_VALUE) {
    return left;
  }
  if (node.infix_value.operator != I_AND && node.infix_value.operator != I_OR) {
    right = interpret(*node.infix_value.right, env);
    if (right.type != IR_VALUE) {
      return right;
    }
  }
  switch (node.infix_value.operator) {
    case I_NONE:
      return RESULT_VALUE(nil_value);
    case I_ADD:
      return eval_add(node, left.value, right.value, env);
    case I_SUB:
      return eval_sub(node, left.value, right.value, env);
    case I_MUL:
      return eval_mul(node, left.value, right.value, env);
    case I_DIV:
      return eval_div(node, left.value, right.value, env);
    case I_MOD:
      return eval_mod(node, left.value, right.value, env);
    case I_LT:
      return compare_values(node, left.value, right.value, "<", env) == C_LT
        ? RESULT_VALUE(true_value)
        : RESULT_VALUE(nil_value);
    case I_LEQ: {
      Comparison c = compare_values(node, left.value, right.value, "<", env);
      return c == C_LT || c == C_EQ
        ? RESULT_VALUE(true_value)
        : RESULT_VALUE(nil_value);
    }
    case I_GT:
      return compare_values(node, left.value, right.value, ">", env) == C_GT
        ? RESULT_VALUE(true_value)
        : RESULT_VALUE(nil_value);
    case I_GEQ: {
      Comparison c = compare_values(node, left.value, right.value, "<", env);
      return c == C_GT || c == C_EQ
        ? RESULT_VALUE(true_value)
        : RESULT_VALUE(nil_value);
    }
    case I_EQ:
      if (equals(left.value, right.value)) {
        return RESULT_VALUE(true_value);
      }
      return RESULT_VALUE(nil_value);
    case I_NEQ:
      if (equals(left.value, right.value)) {
        return RESULT_VALUE(nil_value);
      }
      return RESULT_VALUE(true_value);
    case I_AND:
      if (is_truthy(left.value)) {
        return interpret(*node.infix_value.right, env);
      }
      return RESULT_VALUE(nil_value);
    case I_OR:
      if (is_truthy(left.value)) {
        return left;
      }
      return interpret(*node.infix_value.right, env);
  }
  return RESULT_VALUE(nil_value);
}

static InterpreterResult eval_if(Node node, Env *env) {
  InterpreterResult cond = interpret(*node.if_value.cond, env);
  if (cond.type != IR_VALUE) {
    return cond;
  }
  if (is_truthy(cond.value)) {
    return interpret(*node.if_value.cons, env);
  }
  if (node.if_value.alt) {
    return interpret(*node.if_value.alt, env);
  }
  return RESULT_VALUE(nil_value);
}

static InterpreterResult eval_for(Node node, Env *env) {
  InterpreterResult result = interpret(*node.for_value.collection, env);
  if (result.type != IR_VALUE) {
    return result;
  }
  Value collection = result.value;
  if (collection.type == V_ARRAY) {
    if (collection.array_value->size == 0) {
      if (node.for_value.alt) {
        return interpret(*node.for_value.alt, env);
      }
      return RESULT_VALUE(nil_value);
    }
    Buffer buffer = create_buffer(0);
    for (size_t i = 0; i < collection.array_value->size; i++) {
      if (node.for_value.key) {
        env_put(node.for_value.key, create_int(i), env);
      }
      env_put(node.for_value.value, collection.array_value->cells[i], env);
      int64_t loops = env->loops;
      env->loops = loops + 1;
      result = interpret(*node.for_value.body, env);
      env->loops = loops;
      if (result.type != IR_RETURN) {
        value_to_string(result.value, &buffer);
      }
      if (result.type == IR_CONTINUE && result.level <= 1) {
        continue;
      } else if (result.type != IR_VALUE) {
        break;
      }
    }
    if (result.type == IR_RETURN) {
      delete_buffer(buffer);
      return result;
    } 
    Value string = create_string(buffer.data, buffer.size, env->arena);
    delete_buffer(buffer);
    if ((result.type == IR_CONTINUE || result.type == IR_BREAK) && result.level > 1) {
      result.value = string;
      result.level--;
      return result;
    }
    return RESULT_VALUE(string);
  } else if (collection.type == V_OBJECT) {
    if (!object_size(collection.object_value)) {
      if (node.for_value.alt) {
        return interpret(*node.for_value.alt, env);
      }
      return RESULT_VALUE(nil_value);
    }
    Buffer buffer = create_buffer(0);
    Value key, value;
    ObjectIterator it = iterate_object(collection.object_value);
    while (object_iterator_next(&it, &key, &value)) {
      if (node.for_value.key) {
        env_put(node.for_value.key, key, env);
      }
      env_put(node.for_value.value, value, env);
      int64_t loops = env->loops;
      env->loops = loops + 1;
      result = interpret(*node.for_value.body, env);
      env->loops = loops;
      if (result.type != IR_RETURN) {
        value_to_string(result.value, &buffer);
      }
      if (result.type == IR_CONTINUE && result.level <= 1) {
        continue;
      } else if (result.type != IR_VALUE) {
        break;
      }
    }
    if (result.type == IR_RETURN) {
      delete_buffer(buffer);
      return result;
    } 
    Value string = create_string(buffer.data, buffer.size, env->arena);
    delete_buffer(buffer);
    if ((result.type == IR_CONTINUE || result.type == IR_BREAK) && result.level > 1) {
      result.value = string;
      result.level--;
      return result;
    }
    return RESULT_VALUE(string);
  } else if (collection.type == V_STRING) {
    if (collection.string_value->size == 0) {
      if (node.for_value.alt) {
        return interpret(*node.for_value.alt, env);
      }
      return RESULT_VALUE(nil_value);
    }
    Buffer buffer = create_buffer(0);
    for (size_t i = 0; i < collection.string_value->size; i++) {
      if (node.for_value.key) {
        env_put(node.for_value.key, create_int(i), env);
      }
      env_put(node.for_value.value, create_int(collection.string_value->bytes[i]), env);
      int64_t loops = env->loops;
      env->loops = loops + 1;
      result = interpret(*node.for_value.body, env);
      env->loops = loops;
      if (result.type != IR_RETURN) {
        value_to_string(result.value, &buffer);
      }
      if (result.type == IR_CONTINUE && result.level <= 1) {
        continue;
      } else if (result.type != IR_VALUE) {
        break;
      }
    }
    if (result.type == IR_RETURN) {
      delete_buffer(buffer);
      return result;
    } 
    Value string = create_string(buffer.data, buffer.size, env->arena);
    delete_buffer(buffer);
    if ((result.type == IR_CONTINUE || result.type == IR_BREAK) && result.level > 1) {
      result.value = string;
      result.level--;
      return result;
    }
    return RESULT_VALUE(string);
  } else {
    eval_error(*node.for_value.collection, "value of type %s is not iterable", value_name(collection.type));
    if (node.for_value.alt) {
      return interpret(*node.for_value.alt, env);
    }
    return RESULT_VALUE(nil_value);
  }
}

static InterpreterResult eval_switch(Node node, Env *env) {
  InterpreterResult a = interpret(*node.switch_value.expr, env);
  if (a.type != IR_VALUE) {
    return a;
  }
  while (node.switch_value.cases) {
    InterpreterResult b = interpret(node.switch_value.cases->key, env);
    if (b.type != IR_VALUE) {
      return b;
    }
    if (equals(a.value, b.value)) {
      return interpret(node.switch_value.cases->value, env);
    }
    node.switch_value.cases = node.switch_value.cases->tail;
  }
  if (node.switch_value.default_case) {
    return interpret(*node.switch_value.default_case, env);
  }
  return RESULT_VALUE(nil_value);
}

static InterpreterResult eval_assign_operator(Node node, Value existing, Value value, Env *env) {
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
      return RESULT_VALUE(value);
  }
}

static InterpreterResult eval_assign(Node node, Env *env) {
  InterpreterResult result = interpret(*node.assign_value.right, env);
  if (result.type != IR_VALUE) {
    return result;
  }
  Value value = result.value;
  switch (node.assign_value.left->type) {
    case N_NAME: {
      if (node.assign_value.operator != I_NONE) {
        Value existing;
        if (!env_get(node.assign_value.left->name_value, &existing, env)) {
          eval_error(*node.assign_value.left, "undefined variable: %s",
              node.assign_value.left->name_value);
          return RESULT_VALUE(nil_value);
        }
        result = eval_assign_operator(node, existing, value, env);
        if (result.type != IR_VALUE) {
          return result;
        }
        value = result.value;
      }
      env_put(node.assign_value.left->name_value, value, env);
      break;
    }
    case N_SUBSCRIPT: {
      result = interpret(*node.assign_value.left->subscript_value.list, env);
      if (result.type != IR_VALUE) {
        return result;
      }
      Value object = result.value;
      result = interpret(*node.assign_value.left->subscript_value.index, env);
      if (result.type != IR_VALUE) {
        return result;
      }
      Value index = result.value;
      if (object.type == V_OBJECT) {
        if (node.assign_value.operator != I_NONE) {
          Value existing;
          if (!object_get(object.object_value, index, &existing)) {
            eval_error(*node.assign_value.left, "undefined object property");
            return RESULT_VALUE(nil_value);
          }
          result = eval_assign_operator(node, existing, value, env);
          if (result.type != IR_VALUE) {
            return result;
          }
          value = result.value;
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
            result = eval_assign_operator(node, object.array_value->cells[index.int_value], value, env);
            if (result.type != IR_VALUE) {
              return result;
            }
            value = result.value;
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
      result = interpret(*node.assign_value.left->dot_value.object, env);
      if (result.type != IR_VALUE) {
        return result;
      }
      Value object = result.value;
      if (object.type == V_OBJECT) {
        Value key = create_symbol(node.assign_value.left->dot_value.name);
        if (node.assign_value.operator != I_NONE) {
          Value existing;
          if (!object_get(object.object_value, key, &existing)) {
            eval_error(*node.assign_value.left, "undefined object property: %s",
                key.symbol_value);
            return RESULT_VALUE(nil_value);
          }
          result = eval_assign_operator(node, existing, value, env);
          if (result.type != IR_VALUE) {
            return result;
          }
          value = result.value;
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
  return RESULT_VALUE(nil_value);
}

InterpreterResult interpret(Node node, Env *env) {
  switch (node.type) {
    case N_NAME: {
      Value value;
      if (env_get(node.name_value, &value, env)) {
        return RESULT_VALUE(value);
      }
      eval_error(node, "undefined variable: %s", node.name_value);
      return RESULT_VALUE(nil_value);
    }
    case N_INT:
      return RESULT_VALUE(create_int(node.int_value));
    case N_FLOAT:
      return RESULT_VALUE(create_float(node.float_value));
    case N_STRING:
      return RESULT_VALUE(create_string(node.string_value.bytes, node.string_value.size, env->arena));
    case N_LIST: {
      Value array = create_array(LL_SIZE(node.list_value), env->arena);
      while (node.list_value) {
        InterpreterResult result = interpret(node.list_value->head, env);
        if (result.type != IR_VALUE) {
          return result;
        }
        array_push(array.array_value, result.value, env->arena);
        node.list_value = node.list_value->tail;
      }
      return RESULT_VALUE(array);
    }
    case N_OBJECT: {
      Value object = create_object(LL_SIZE(node.object_value), env->arena);
      while (node.object_value) {
        if (node.object_value->key.type == N_NAME) {
          InterpreterResult result = interpret(node.object_value->value, env);
          if (result.type != IR_VALUE) {
            return result;
          }
          object_put(object.object_value, create_symbol(node.object_value->key.name_value),
              result.value, env->arena);
        } else {
          InterpreterResult key_result = interpret(node.object_value->key, env);
          if (key_result.type != IR_VALUE) {
            return key_result;
          }
          InterpreterResult value_result = interpret(node.object_value->value, env);
          if (value_result.type != IR_VALUE) {
            return value_result;
          }
          object_put(object.object_value, key_result.value, value_result.value, env->arena);
        }
        node.object_value = node.object_value->tail;
      }
      return RESULT_VALUE(object);
    }
    case N_APPLY:
      return eval_apply(node, env);
    case N_SUBSCRIPT:
      return eval_subscript(node, env, 0);
    case N_DOT:
      return eval_dot(node, env, 0);
    case N_PREFIX:
      return eval_prefix(node, env);
    case N_INFIX:
      return eval_infix(node, env);
    case N_TUPLE:
      eval_error(node, "unexpected tuple");
      return RESULT_VALUE(nil_value);
    case N_FN:
      return RESULT_VALUE(create_closure(node.fn_value.params, node.fn_value.free_variables,
            *node.fn_value.body, env, env->arena));
    case N_IF:
      return eval_if(node, env);
    case N_FOR:
      return eval_for(node, env);
    case N_SWITCH:
      return eval_switch(node, env);
    case N_EXPORT: {
      InterpreterResult result = interpret(*node.export_value.right, env);
      if (result.type != IR_VALUE) {
        return result;
      }
      env_put(node.export_value.left, result.value, env);
      array_push(env->exports, create_symbol(node.export_value.left), env->arena);
      return RESULT_VALUE(nil_value);
    }
    case N_ASSIGN:
      return eval_assign(node, env);
    case N_BLOCK: {
      Buffer buffer = create_buffer(0);
      while (node.block_value) {
        InterpreterResult result = interpret(node.block_value->head, env);
        if (result.type != IR_VALUE) {
          if (result.type != IR_RETURN) {
            value_to_string(result.value, &buffer);
            result.value = create_string(buffer.data, buffer.size, env->arena);
          }
          delete_buffer(buffer);
          return result;
        }
        value_to_string(result.value, &buffer);
        node.block_value = node.block_value->tail;
      }
      Value result = create_string(buffer.data, buffer.size, env->arena);
      delete_buffer(buffer);
      return RESULT_VALUE(result);
    }
    case N_SUPPRESS:
      switch (node.suppress_value->type) {
        case N_NAME: {
          Value value;
          if (env_get(node.suppress_value->name_value, &value, env)) {
            return RESULT_VALUE(value);
          }
          return RESULT_VALUE(nil_value);
        }
        case N_SUBSCRIPT:
          return eval_subscript(*node.suppress_value, env, 1);
        case N_DOT:
          return eval_dot(*node.suppress_value, env, 1);
        default:
          return interpret(*node.suppress_value, env);
      }
    case N_RETURN: {
      if (node.return_value) {
        InterpreterResult result = interpret(*node.return_value, env);
        if (result.type != IR_VALUE) {
          return result;
        }
        return (InterpreterResult) { IR_RETURN, result.value };
      }
      return (InterpreterResult) { IR_RETURN, nil_value };
    }
    case N_BREAK:
      if (!env->loops) {
        eval_error(node, "unexpected break outside of loop");
        return RESULT_VALUE(nil_value);
      }
      if (node.break_value < 1 || node.break_value > env->loops) {
        eval_error(node, "invalid numeric argument for break, expected an integer between 1 and %" PRId64,
            env->loops);
        node.break_value = node.break_value < 1 ? 1 : env->loops;
      }
      return (InterpreterResult) { IR_BREAK, nil_value, node.break_value };
    case N_CONTINUE:
      if (!env->loops) {
        eval_error(node, "unexpected continue outside of loop");
        return RESULT_VALUE(nil_value);
      }
      if (node.continue_value < 1 || node.continue_value > env->loops) {
        eval_error(node, "invalid numeric argument for continue, expected an integer between 1 and %" PRId64,
            env->loops);
        node.continue_value = node.continue_value < 1 ? 1 : env->loops;
      }
      return (InterpreterResult) { IR_CONTINUE, nil_value, node.continue_value };
  }
  return RESULT_VALUE(nil_value);
}

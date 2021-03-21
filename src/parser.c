/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "parser.h"

#include "util.h"

#include <string.h>
#include <stdarg.h>
#include <assert.h>

#define ASSIGN_NODE(DEST, SRC) \
  assert(!(DEST));\
  (DEST) = allocate(sizeof(Node));\
  *(DEST) = (SRC);

typedef struct {
  TokenStream tokens;
  Module *module;
  NameList *free_variables;
  int errors;
  Pos end;
  int ignore_lf;
  int object_notation;
} Parser;

static Node parse_expression(Parser *parser);
static Node parse_statement(Parser *parser);
static Node parse_block(Parser *parser);
static Node parse_template(Parser *parser);

static Node create_node(NodeType type, Parser *parser) {
  Node node;
  node.type = type;
  node.module.file_name = parser->module->file_name;
  node.start = peek_token(parser->tokens)->start;
  node.end = node.start;
  switch (type) {
    case N_NAME:
      node.name_value = NULL;
      break;
    case N_INT:
      node.int_value = 0;
      break;
    case N_FLOAT:
      node.float_value = 0;
      break;
    case N_STRING:
      node.string_value.bytes = NULL;
      node.string_value.size = 0;
      break;
    case N_LIST:
      node.list_value = NULL;
      break;
    case N_OBJECT:
      node.object_value = NULL;
      break;
    case N_APPLY:
      node.apply_value.callee = NULL;
      node.apply_value.args = NULL;
      break;
    case N_SUBSCRIPT:
      node.subscript_value.list = NULL;
      node.subscript_value.index = NULL;
      break;
    case N_DOT:
      node.dot_value.object = NULL;
      node.dot_value.name = NULL;
      break;
    case N_PREFIX:
      node.prefix_value.operand = NULL;
      break;
    case N_INFIX:
      node.infix_value.left = NULL;
      node.infix_value.right = NULL;
      break;
    case N_TUPLE:
      node.tuple_value = NULL;
      break;
    case N_FN:
      node.fn_value.params = NULL;
      node.fn_value.free_variables = NULL;
      node.fn_value.body = NULL;
      break;
    case N_IF:
      node.if_value.cond = NULL;
      node.if_value.cons = NULL;
      node.if_value.alt = NULL;
      break;
    case N_FOR:
      node.for_value.key = NULL;
      node.for_value.value = NULL;
      node.for_value.collection = NULL;
      node.for_value.body = NULL;
      node.for_value.alt = NULL;
      break;
    case N_SWITCH:
      node.switch_value.expr = NULL;
      node.switch_value.default_case = NULL;
      node.switch_value.cases = NULL;
      break;
    case N_EXPORT:
      node.export_value.left = NULL;
      node.export_value.right = NULL;
      break;
    case N_ASSIGN:
      node.assign_value.left = NULL;
      node.assign_value.right = NULL;
      node.assign_value.operator = I_NONE;
      break;
    case N_BLOCK:
      node.block_value = NULL;
      break;
    case N_SUPPRESS:
      node.suppress_value = NULL;
      break;
    case N_RETURN:
      node.return_value = NULL;
      break;
    case N_BREAK:
      node.break_value = 1;
      break;
    case N_CONTINUE:
      node.continue_value = 1;
      break;
  }
  return node;
}

static void parser_error(Parser *parser, Token *token, const char *format, ...) {
  va_list va;
  if (token) {
    fprintf(stderr, SGR_BOLD "%s:%d:%d: " ERROR_LABEL, parser->module->file_name->path, token->start.line, token->start.column);
  } else {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL, parser->module->file_name->path);
  }
  va_start(va, format);
  vfprintf(stderr, format, va);
  va_end(va);
  fprintf(stderr, SGR_RESET "\n");
  parser->errors++;
  if (parser->module->file_name && token) {
    print_error_line(parser->module->file_name->path, token->start, token->end);
  }
}

static Token *pop(Parser *parser) {
  Token *t = pop_token(parser->tokens);
  parser->end = t->end;
  return t;
}

static void skip_lf_if_ignored(Parser *parser) {
  if (parser->ignore_lf) {
    while (peek_token(parser->tokens)->type == T_LF) {
      pop(parser);
    }
  }
}

static int peek_type(TokenType type, Parser *parser) {
  skip_lf_if_ignored(parser);
  return peek_token(parser->tokens)->type == type;
}

static int peek_keyword(const char *keyword, Parser *parser) {
  skip_lf_if_ignored(parser);
  Token *t = peek_token(parser->tokens);
  return t->type == T_KEYWORD && strcmp(t->name_value, keyword) == 0;
}

static int peek_operator(const char *operator, Parser *parser) {
  skip_lf_if_ignored(parser);
  Token *t = peek_token(parser->tokens);
  return t->type == T_OPERATOR && strcmp(t->operator_value, operator) == 0;
}

static int peek_punct(const char punct, Parser *parser) {
  skip_lf_if_ignored(parser);
  Token *t = peek_token(parser->tokens);
  return t->type == T_PUNCT && t->punct_value == punct;
}

static Token *expect_type(TokenType type, Parser *parser) {
  if (peek_type(type, parser)) {
    return pop(parser);
  }
  Token *t = peek_token(parser->tokens);
  parser_error(parser, t, "unexpected %s, expected %s", token_name(t->type), token_name(type));
  return NULL;
}

static Token *expect_keyword(const char *keyword, Parser *parser) {
  if (peek_keyword(keyword, parser)) {
    return pop(parser);
  }
  Token *t = peek_token(parser->tokens);
  if (t->type == T_KEYWORD) {
    parser_error(parser, t, "unexpected \"%s\", expected \"%s\"", t->name_value, keyword);
  } else {
    parser_error(parser, t, "unexpected %s, expected \"%s\"", token_name(t->type), keyword);
  }
  return NULL;
}

static Token *expect_operator(const char *operator, Parser *parser) {
  if (peek_operator(operator, parser)) {
    return pop(parser);
  }
  Token *t = peek_token(parser->tokens);
  if (t->type == T_OPERATOR) {
    parser_error(parser, t, "unexpected \"%s\", expected \"%s\"", t->operator_value, operator);
  } else {
    parser_error(parser, t, "unexpected %s, expected \"%s\"", token_name(t->type), operator);
  }
  return NULL;
}

static Token *expect_punct(const char punct, Parser *parser) {
  if (peek_punct(punct, parser)) {
    return pop(parser);
  }
  Token *t = peek_token(parser->tokens);
  if (t->type == T_OPERATOR) {
    parser_error(parser, t, "unexpected '%c', expected '%c'", t->punct_value, punct);
  } else {
    parser_error(parser, t, "unexpected %s, expected '%c'", token_name(t->type), punct);
  }
  return NULL;
}

static int expect_end(const char *keyword, Parser *parser) {
  if (peek_keyword("end", parser)) {
    pop(parser);
    return !!expect_keyword(keyword, parser);
  }
  Token *t = peek_token(parser->tokens);
  if (t->type == T_KEYWORD) {
    parser_error(parser, t, "unexpected \"%s\", expected \"end %s\"", t->name_value, keyword);
  } else {
    parser_error(parser, t, "unexpected %s, expected \"end %s\"", token_name(t->type), keyword);
  }
  return 0;
}

static void skip_lf(Parser *parser) {
  while (peek_type(T_LF, parser)) {
    pop(parser);
  }
}

static void skip_lf_and_text(Parser *parser) {
  skip_lf(parser);
  if (peek_punct('}', parser)) {
    pop(parser);
    while (peek_type(T_TEXT, parser)) {
      pop(parser);
    }
    expect_punct('{', parser);
    skip_lf(parser);
  }
}

static Symbol parse_name(Parser *parser) {
  Token *t = expect_type(T_NAME, parser);
  if (t) {
    return t->name_value;
  }
  return "";
}

static Node parse_string(Parser *parser) {
  Node node = create_node(N_STRING, parser);
  Token *token = pop(parser);
  node.string_value.size = token->size;
  node.string_value.bytes = copy_string_token(token);
  node.end = parser->end;
  return node;
}

static Node parse_atom(Parser *parser) {
  if (peek_type(T_INT, parser)) {
    Node node = create_node(N_INT, parser);
    node.int_value = pop(parser)->int_value;
    node.end = parser->end;
    return node;
  } else if (peek_type(T_FLOAT, parser)) {
    Node node = create_node(N_FLOAT, parser);
    node.float_value = pop(parser)->float_value;
    node.end = parser->end;
    return node;
  } else if (peek_type(T_STRING, parser)) {
    return parse_string(parser);
  } else if (peek_type(T_NAME, parser)) {
    Node node = create_node(N_NAME, parser);
    node.name_value = parse_name(parser);
    if (peek_operator("=>", parser)) {
      Node tuple = create_node(N_TUPLE, parser);
      tuple.start = node.start;
      tuple.end = node.end;
      NameList *last_param = NULL;
      LL_APPEND(NameList, tuple.tuple_value, last_param, node.name_value);
      return tuple;
    }
    parser->free_variables = name_list_put(node.name_value, parser->free_variables);
    node.end = parser->end;
    if (peek_operator("?", parser)) {
      Node suppress = create_node(N_SUPPRESS, parser);
      suppress.start = node.start;
      pop(parser);
      ASSIGN_NODE(suppress.suppress_value, node);
      suppress.end = parser->end;
      node = suppress;
    }
    return node;
  } else {
    Token *t = peek_token(parser->tokens);
    parser_error(parser, t, "unexpected %s, expected an expression", token_name(t->type));
    pop(parser);
    return create_node(N_INT, parser);
  }
}

static Node parse_delimited(Parser *parser) {
  if (peek_punct('[', parser)) {
    Node list = create_node(N_LIST, parser);
    pop(parser);
    int ignore_lf = parser->ignore_lf;
    parser->ignore_lf = 1;
    if (!peek_punct(']', parser)) {
      NodeList *last_item = NULL;
      while (1) {
        Node item = parse_expression(parser);
        LL_APPEND(NodeList, list.list_value, last_item, item);
        if (!peek_operator(",", parser)) {
          break;
        }
        pop(parser);
        // Allow comma after last element
        if (peek_punct(']', parser)) {
          break;
        }
      }
    }
    expect_punct(']', parser);
    parser->ignore_lf = ignore_lf;
    list.end = parser->end;
    return list;
  } else if (peek_punct('(', parser)) {
    Token *start_paren = pop(parser);
    int ignore_lf = parser->ignore_lf;
    parser->ignore_lf = 1;
    if (peek_punct(')', parser)) {
      pop(parser);
      parser->ignore_lf = ignore_lf;
      Node tuple = create_node(N_TUPLE, parser);
      tuple.start = start_paren->start;
      tuple.end = parser->end;
      if (!peek_operator("=>", parser)) {
        expect_operator("=>", parser);
      }
      return tuple;
    }
    Node expr = parse_expression(parser);
    if (expr.type == N_NAME && peek_operator(",", parser)) {
      Node tuple = create_node(N_TUPLE, parser);
      tuple.start = start_paren->start;
      NameList *last_param = NULL;
      LL_APPEND(NameList, tuple.tuple_value, last_param, expr.name_value);
      parser->free_variables = name_list_remove(expr.name_value, parser->free_variables);
      if (!peek_punct(')', parser)) {
        while (peek_operator(",", parser)) {
          pop(parser);
          if (peek_punct(')', parser)) {
            break;
          }
          LL_APPEND(NameList, tuple.tuple_value, last_param, parse_name(parser));
        }
      }
      expect_punct(')', parser);
      parser->ignore_lf = ignore_lf;
      tuple.end = parser->end;
      if (!peek_operator("=>", parser)) {
        expect_operator("=>", parser);
      }
      return tuple;
    }
    expect_punct(')', parser);
    parser->ignore_lf = ignore_lf;
    expr.end = parser->end;
    if (expr.type == N_NAME && peek_operator("=>", parser)) {
      Node tuple = create_node(N_TUPLE, parser);
      tuple.start = expr.start;
      tuple.end = expr.end;
      NameList *last_param = NULL;
      LL_APPEND(NameList, tuple.tuple_value, last_param, expr.name_value);
      return tuple;
    }
    return expr;
  } else if (peek_punct('{', parser)) {
    Node object = create_node(N_OBJECT, parser);
    pop(parser);
    int ignore_lf = parser->ignore_lf;
    parser->ignore_lf = 1;
    if (!peek_punct('}', parser)) {
      PropertyList *last_property = NULL;
      while (1) {
        Node key = parse_atom(parser);
        expect_operator(":", parser);
        Node value = parse_expression(parser);
        PropertyList *property = allocate(sizeof(PropertyList));
        property->tail = NULL;
        property->key = key;
        property->value = value;
        if (last_property) {
          last_property->tail = property;
          last_property = property;
          object.object_value->size++;
        } else {
          object.object_value = last_property = property;
          object.object_value->size = 1;
        }
        if (!peek_operator(",", parser)) {
          break;
        }
        pop(parser);
        // Allow comma after last element
        if (peek_punct('}', parser)) {
          break;
        }
      }
    }
    expect_punct('}', parser);
    parser->ignore_lf = ignore_lf;
    object.end = parser->end;
    return object;
  } else if (peek_keyword("do", parser)) {
    Pos start = pop(parser)->start;
    int ignore_lf = parser->ignore_lf;
    parser->ignore_lf = 0;
    Node block = parse_block(parser);
    expect_end("do", parser);
    parser->ignore_lf = ignore_lf;
    block.start = start;
    block.end = parser->end;
    return block;
  } else if (peek_type(T_START_QUOTE, parser)) {
    Pos start = pop(parser)->start;
    Node template = parse_template(parser);
    expect_type(T_END_QUOTE, parser);
    template.start = start;
    template.end = parser->end;
    return template;
  } else {
    return parse_atom(parser);
  }
}

static Node parse_apply_dot(Parser *parser) {
  Node expr = parse_delimited(parser);
  while (1) {
    if (peek_punct('(', parser)) {
      Node apply = create_node(N_APPLY, parser);
      apply.start = expr.start;
      pop(parser);
      ASSIGN_NODE(apply.apply_value.callee, expr);
      if (!peek_punct(')', parser)) {
        NodeList *last_arg = NULL;
        while (1) {
          Node arg = parse_expression(parser);
          LL_APPEND(NodeList, apply.apply_value.args, last_arg, arg);
          if (!peek_operator(",", parser)) {
            break;
          }
          pop(parser);
          // Allow comma after last element
          if (peek_punct(')', parser)) {
            break;
          }
        }
      }
      expect_punct(')', parser);
      apply.end = parser->end;
      expr = apply;
    } else if (peek_punct('[', parser)) {
      Node subscript = create_node(N_SUBSCRIPT, parser);
      subscript.start = expr.start;
      pop(parser);
      ASSIGN_NODE(subscript.subscript_value.list, expr);
      ASSIGN_NODE(subscript.subscript_value.index, parse_expression(parser));
      expect_punct(']', parser);
      subscript.end = parser->end;
      expr = subscript;
      if (peek_operator("?", parser)) {
        Node suppress = create_node(N_SUPPRESS, parser);
        suppress.start = expr.start;
        pop(parser);
        ASSIGN_NODE(suppress.suppress_value, expr);
        suppress.end = parser->end;
        expr = suppress;
      }
    } else if (peek_operator(".", parser)) {
      Node dot = create_node(N_DOT, parser);
      dot.start = expr.start;
      pop(parser);
      ASSIGN_NODE(dot.dot_value.object, expr);
      dot.dot_value.name = parse_name(parser);
      dot.end = parser->end;
      expr = dot;
      if (peek_operator("?", parser)) {
        Node suppress = create_node(N_SUPPRESS, parser);
        suppress.start = expr.start;
        pop(parser);
        ASSIGN_NODE(suppress.suppress_value, expr);
        suppress.end = parser->end;
        expr = suppress;
      }
    } else {
      break;
    }
  }
  return expr;
}

static Node parse_negate(Parser *parser) {
  if (peek_operator("-", parser)) {
    Node prefix = create_node(N_PREFIX, parser);
    prefix.prefix_value.operator = P_NEG;
    pop(parser);
    ASSIGN_NODE(prefix.prefix_value.operand, parse_negate(parser));
    prefix.end = parser->end;
    return prefix;
  }
  return parse_apply_dot(parser);
}

static Node parse_add_sub(Parser *parser) {
  Node expr = parse_negate(parser);
  while (peek_operator("+", parser) || peek_operator("-", parser)) {
    Node infix = create_node(N_INFIX, parser);
    infix.start = expr.start;
    char *op = pop(parser)->operator_value;
    if (op[0] == '+') {
      infix.infix_value.operator = I_ADD;
    } else {
      infix.infix_value.operator = I_SUB;
    }
    ASSIGN_NODE(infix.infix_value.left, expr);
    ASSIGN_NODE(infix.infix_value.right, parse_negate(parser));
    infix.end = parser->end;
    expr = infix;
  }
  return expr;
}

static Node parse_mul_div(Parser *parser) {
  Node expr = parse_add_sub(parser);
  while (peek_operator("*", parser) || peek_operator("/", parser) || peek_operator("%", parser)) {
    Node infix = create_node(N_INFIX, parser);
    infix.start = expr.start;
    char *op = pop(parser)->operator_value;
    if (op[0] == '*') {
      infix.infix_value.operator = I_MUL;
    } else if (op[0] == '/') {
      infix.infix_value.operator = I_DIV;
    } else {
      infix.infix_value.operator = I_MOD;
    }
    ASSIGN_NODE(infix.infix_value.left, expr);
    ASSIGN_NODE(infix.infix_value.right, parse_add_sub(parser));
    infix.end = parser->end;
    expr = infix;
  }
  return expr;
}

static Node parse_comparison(Parser *parser) {
  Node expr = parse_mul_div(parser);
  while (peek_operator("<", parser) || peek_operator(">", parser) || peek_operator("<=", parser)
      || peek_operator(">=", parser) || peek_operator("==", parser) || peek_operator("!=", parser)) {
    Node infix = create_node(N_INFIX, parser);
    infix.start = expr.start;
    char *op = pop(parser)->operator_value;
    if (op[0] == '<') {
      if (op[1] == '=') {
        infix.infix_value.operator = I_LEQ;
      } else {
        infix.infix_value.operator = I_LT;
      }
    } else if (op[0] == '>') {
      if (op[1] == '=') {
        infix.infix_value.operator = I_GEQ;
      } else {
        infix.infix_value.operator = I_GT;
      }
    } else if (op[0] == '=') {
      infix.infix_value.operator = I_EQ;
    } else {
      infix.infix_value.operator = I_NEQ;
    }
    ASSIGN_NODE(infix.infix_value.left, expr);
    ASSIGN_NODE(infix.infix_value.right, parse_mul_div(parser));
    infix.end = parser->end;
    expr = infix;
  }
  return expr;
}

static Node parse_logical_not(Parser *parser) {
  if (peek_keyword("not", parser)) {
    Node prefix = create_node(N_PREFIX, parser);
    prefix.prefix_value.operator = P_NOT;
    pop(parser);
    ASSIGN_NODE(prefix.prefix_value.operand, parse_logical_not(parser));
    prefix.end = parser->end;
    return prefix;
  }
  return parse_comparison(parser);
}

static Node parse_logical_and(Parser *parser) {
  Node expr = parse_logical_not(parser);
  while (peek_keyword("and", parser)) {
    Node infix = create_node(N_INFIX, parser);
    infix.start = expr.start;
    pop(parser);
    infix.infix_value.operator = I_AND;
    ASSIGN_NODE(infix.infix_value.left, expr);
    ASSIGN_NODE(infix.infix_value.right, parse_logical_not(parser));
    infix.end = parser->end;
    expr = infix;
  }
  return expr;
}

static Node parse_logical_or(Parser *parser) {
  Node expr = parse_logical_and(parser);
  while (peek_keyword("or", parser)) {
    Node infix = create_node(N_INFIX, parser);
    infix.start = expr.start;
    pop(parser);
    infix.infix_value.operator = I_OR;
    ASSIGN_NODE(infix.infix_value.left, expr);
    ASSIGN_NODE(infix.infix_value.right, parse_logical_and(parser));
    infix.end = parser->end;
    expr = infix;
  }
  return expr;
}

static Node parse_pipe_line(Parser *parser) {
  Node expr = parse_logical_or(parser);
  while (peek_operator("|", parser)) {
    Node apply = create_node(N_APPLY, parser);
    apply.start = expr.start;
    pop(parser);
    Node name = create_node(N_NAME, parser);
    name.name_value = parse_name(parser);
    name.end = parser->end;
    ASSIGN_NODE(apply.apply_value.callee, name);
    NodeList *last_arg = NULL;
    LL_APPEND(NodeList, apply.apply_value.args, last_arg, expr);
    if (peek_punct('(', parser)) {
      pop(parser);
      if (!peek_punct(')', parser)) {
        while (1) {
          Node arg = parse_expression(parser);
          LL_APPEND(NodeList, apply.apply_value.args, last_arg, arg);
          if (!peek_operator(",", parser)) {
            break;
          }
          pop(parser);
          // Allow comma after last element
          if (peek_punct(')', parser)) {
            break;
          }
        }
      }
      expect_punct(')', parser);
    }
    apply.end = parser->end;
    expr = apply;
  }
  return expr;
}

static Node parse_fat_arrow(Parser *parser) {
  Node expr = parse_pipe_line(parser);
  if (expr.type != N_TUPLE) {
    return expr;
  }
  Node fn = create_node(N_FN, parser);
  fn.start = expr.start;
  fn.fn_value.params = expr.tuple_value;
  expect_operator("=>", parser);
  NameList *previous_name_list = parser->free_variables;
  parser->free_variables = NULL;
  ASSIGN_NODE(fn.fn_value.body, parse_statement(parser));
  for (NameList *name = fn.fn_value.params; name; name = name->tail) {
    parser->free_variables = name_list_remove(name->head, parser->free_variables);
  }
  fn.fn_value.free_variables = parser->free_variables;
  parser->free_variables = previous_name_list;
  for (NameList *name = fn.fn_value.free_variables; name; name = name->tail) {
    parser->free_variables = name_list_put(name->head, parser->free_variables);
  }
  fn.end = parser->end;
  return fn;
}

static Node parse_partial_dot(Parser *parser) {
  Node fn = create_node(N_FN, parser);
  fn.fn_value.params = allocate(sizeof(NameList));
  fn.fn_value.params->tail = NULL;
  fn.fn_value.params->head = "o";
  Node expr = create_node(N_NAME, parser);
  expr.name_value = "o";
  while (peek_operator(".", parser)) {
    Node dot = create_node(N_DOT, parser);
    pop(parser);
    ASSIGN_NODE(dot.dot_value.object, expr);
    dot.dot_value.name = parse_name(parser);
    expr = dot;
  }
  ASSIGN_NODE(fn.fn_value.body, expr);
  fn.end = parser->end;
  return fn;
}

static Node parse_expression(Parser *parser) {
  if (peek_operator(".", parser)) {
    return parse_partial_dot(parser);
  } else {
    return parse_fat_arrow(parser);
  }
}

static Node parse_if(Parser *parser) {
  Node stmt = create_node(N_IF, parser);
  expect_keyword("if", parser);
  ASSIGN_NODE(stmt.if_value.cond, parse_expression(parser));
  if (peek_keyword("then", parser)) {
    pop(parser);
    ASSIGN_NODE(stmt.if_value.cons, parse_expression(parser));
    expect_keyword("else", parser);
    ASSIGN_NODE(stmt.if_value.alt, parse_statement(parser));
  } else {
    ASSIGN_NODE(stmt.if_value.cons, parse_block(parser));
    Node *parent = &stmt;
    while (peek_keyword("else", parser)) {
      pop(parser);
      if (peek_keyword("if", parser)) {
        pop(parser);
        Node nested = create_node(N_IF, parser);
        ASSIGN_NODE(parent->if_value.alt, nested);
        parent = parent->if_value.alt;
        ASSIGN_NODE(parent->if_value.cond, parse_expression(parser));
        ASSIGN_NODE(parent->if_value.cons, parse_block(parser));
      } else {
        ASSIGN_NODE(parent->if_value.alt, parse_block(parser));
        break;
      }
    }
    expect_end("if", parser);
  }
  stmt.end = parser->end;
  return stmt;
}

static Node parse_for(Parser *parser) {
  Node stmt = create_node(N_FOR, parser);
  expect_keyword("for", parser);
  Symbol name = parse_name(parser);
  if (peek_operator(":", parser)) {
    stmt.for_value.key = name;
    pop(parser);
    stmt.for_value.value = parse_name(parser);
  } else {
    stmt.for_value.value = name;
  }
  expect_keyword("in", parser);
  ASSIGN_NODE(stmt.for_value.collection, parse_expression(parser));
  ASSIGN_NODE(stmt.for_value.body, parse_block(parser));
  if (peek_keyword("else", parser)) {
    pop(parser);
    ASSIGN_NODE(stmt.for_value.alt, parse_block(parser));
  }
  expect_end("for", parser);
  stmt.end = parser->end;
  return stmt;
}

static Node parse_switch(Parser *parser) {
  Node stmt = create_node(N_SWITCH, parser);
  expect_keyword("switch", parser);
  ASSIGN_NODE(stmt.switch_value.expr, parse_expression(parser));
  if (!peek_type(T_TEXT, parser)) {
    expect_type(T_LF, parser);
  }
  skip_lf_and_text(parser);
  PropertyList *last_case = NULL;
  while (peek_keyword("case", parser)) {
    pop(parser);
    PropertyList *case_item = allocate(sizeof(PropertyList));
    case_item->tail = NULL;
    case_item->key = parse_expression(parser);
    case_item->value = parse_block(parser);
    if (last_case) {
      last_case->tail = case_item;
      last_case = case_item;
    } else {
      stmt.switch_value.cases = last_case = case_item;
    }
  }
  if (peek_keyword("default", parser)) {
    pop(parser);
    ASSIGN_NODE(stmt.switch_value.default_case, parse_block(parser));
  }
  expect_end("switch", parser);
  stmt.end = parser->end;
  return stmt;
}

static Node parse_export(Parser *parser) {
  Node export = create_node(N_EXPORT, parser);
  expect_keyword("export", parser);
  export.export_value.left = parse_name(parser);
  if (peek_operator("=", parser)) {
    pop(parser);
    ASSIGN_NODE(export.export_value.right, parse_expression(parser));
  }
  export.end = parser->end;
  return export;
}

static Node parse_assign(Parser *parser) {
  Node expr = parse_expression(parser);
  if (peek_operator("=", parser) || peek_operator("+=", parser) || peek_operator("-=", parser)
      || peek_operator("*=", parser) || peek_operator("/=", parser)) {
    Node assign = create_node(N_ASSIGN, parser);
    assign.start = expr.start;
    char *op = pop(parser)->operator_value;
    ASSIGN_NODE(assign.assign_value.left, expr);
    ASSIGN_NODE(assign.assign_value.right, parse_expression(parser));
    if (op[1]) {
      switch (op[0]) {
        case '+':
          assign.assign_value.operator = I_ADD;
          break;
        case '-':
          assign.assign_value.operator = I_SUB;
          break;
        case '*':
          assign.assign_value.operator = I_MUL;
          break;
        case '/':
          assign.assign_value.operator = I_DIV;
          break;
        default:
          break;
      }
    }
    assign.end = parser->end;
    expr = assign;
  }
  return expr;
}

static Node parse_return(Parser *parser) {
  Node node = create_node(N_RETURN, parser);
  expect_keyword("return", parser);
  if (!peek_type(T_LF, parser) && !peek_type(T_EOF, parser) && !peek_type(T_TEXT, parser)) {
    ASSIGN_NODE(node.return_value, parse_expression(parser));
  }
  node.end = parser->end;
  return node;
}

static Node parse_break(Parser *parser) {
  Node node = create_node(N_BREAK, parser);
  expect_keyword("break", parser);
  if (peek_type(T_INT, parser)) {
    node.break_value = pop(parser)->int_value;
  }
  node.end = parser->end;
  return node;
}

static Node parse_continue(Parser *parser) {
  Node node = create_node(N_CONTINUE, parser);
  expect_keyword("continue", parser);
  if (peek_type(T_INT, parser)) {
    node.continue_value = pop(parser)->int_value;
  }
  node.end = parser->end;
  return node;
}

static Node parse_statement(Parser *parser) {
  if (peek_keyword("if", parser)) {
    return parse_if(parser);
  } else if (peek_keyword("for", parser)) {
    return parse_for(parser);
  } else if (peek_keyword("switch", parser)) {
    return parse_switch(parser);
  } else if (peek_keyword("export", parser)) {
    return parse_export(parser);
  } else if (peek_keyword("return", parser)) {
    return parse_return(parser);
  } else if (peek_keyword("break", parser)) {
    return parse_break(parser);
  } else if (peek_keyword("continue", parser)) {
    return parse_continue(parser);
  } else {
    return parse_assign(parser);
  }
}

static Node parse_block(Parser *parser) {
  Node block = create_node(N_BLOCK, parser);
  if (peek_type(T_TEXT, parser)) {
    Node string = parse_string(parser);
    block.block_value = allocate(sizeof(NodeList));
    block.block_value->tail = NULL;
    block.block_value->head = string;
  } else {
    expect_type(T_LF, parser);
  }
  Node template = parse_template(parser);
  if (block.block_value) {
    block.block_value->tail = template.block_value;
  } else {
    block.block_value = template.block_value;
  }
  block.end = parser->end;
  return block;
}

static Node parse_template(Parser *parser) {
  Node block = create_node(N_BLOCK, parser);
  NodeList *last_item = NULL;
  while (1) {
    if (peek_type(T_TEXT, parser)) {
      LL_APPEND(NodeList, block.block_value, last_item, parse_string(parser));
    } else if (peek_keyword("end", parser) || peek_keyword("else", parser) || peek_keyword("case", parser)
        || peek_keyword("default", parser)) {
      break;
    } else if (peek_type(T_EOF, parser) || peek_type(T_END_QUOTE, parser)) {
      break;
    } else if (!peek_type(T_LF, parser)) {
      LL_APPEND(NodeList, block.block_value, last_item, parse_statement(parser));
      if (!peek_type(T_TEXT, parser) && !peek_type(T_EOF, parser)) {
        expect_type(T_LF, parser);
      }
    } else {
      pop(parser);
    }
  }
  return block;
}

Module *parse(TokenStream tokens, const Path *file_name) {
  Module *m = create_module(file_name, M_USER);
  Parser parser = (Parser) { .tokens = tokens, .module = m, .free_variables = NULL, .errors = 0,
    .end.line = 1, .end.column = 1, .ignore_lf = 0, .object_notation = 0 };
  ASSIGN_NODE(m->user_value.root, parse_template(&parser));
  expect_type(T_EOF, &parser);
  delete_name_list(parser.free_variables);
  m->user_value.parse_error = parser.errors;
  return m;
}

Module *parse_object_notation(TokenStream tokens, const Path *file_name, int expect_eof) {
  Module *m = create_module(file_name, M_DATA);
  Parser parser = (Parser) { .tokens = tokens, .module = m, .free_variables = NULL, .errors = 0,
    .end.line = 1, .end.column = 1, .ignore_lf = 0, .object_notation = 1 };
  ASSIGN_NODE(m->data_value.root, parse_delimited(&parser));
  if (expect_eof) {
    expect_type(T_EOF, &parser);
  }
  delete_name_list(parser.free_variables);
  m->data_value.parse_error = parser.errors;
  return m;
}

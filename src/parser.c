/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "parser.h"

#include "util.h"

#include <string.h>
#include <stdarg.h>

typedef struct {
  Token *tokens;
  Module *module;
  int errors;
  Pos end;
} Parser;

struct Node parse_expression(Parser *parser);
static Node parse_block(Parser *parser);
static Node parse_template(Parser *parser);

static Node create_node(NodeType type, Parser *parser) {
  Node node;
  node.type = type;
  node.module = parser->module;
  if (parser->tokens) {
    node.start = parser->tokens->start;
  } else {
    node.start = parser->end;
  }
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
      node.prefix_value.operator[0] = '\0';
      break;
    case N_INFIX:
      node.infix_value.left = NULL;
      node.infix_value.right = NULL;
      node.infix_value.operator[0] = '\0';
      break;
    case N_FN:
      node.fn_value.params = NULL;
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
    case N_SWITCH: {
      node.switch_value.expr = NULL;
      node.switch_value.default_case = NULL;
      node.switch_value.cases = NULL;
      break;
    }
    case N_ASSIGN:
      node.assign_value.left = NULL;
      node.assign_value.right = NULL;
      break;
    case N_BLOCK:
      node.block_value = NULL;
      break;
  }
  return node;
}

static void parser_error(Parser *parser, Token *token, const char *format, ...) {
  va_list va;
  if (token) {
    fprintf(stderr, SGR_BOLD "%s:%d:%d: " ERROR_LABEL, parser->module->file_name, token->start.line, token->start.column);
  } else {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL, parser->module->file_name);
  }
  va_start(va, format);
  vfprintf(stderr, format, va);
  va_end(va);
  fprintf(stderr, "\n" SGR_RESET);
  parser->errors++;
}

static Token *peek(Parser *parser) {
  return parser->tokens;
}

static int peek_type(TokenType type, Parser *parser) {
  return parser->tokens && parser->tokens->type == type;
}

static int peek_keyword(const char *keyword, Parser *parser) {
  return parser->tokens && parser->tokens->type == T_KEYWORD && strcmp(parser->tokens->name_value, keyword) == 0;
}

static int peek_operator(const char *operator, Parser *parser) {
  return parser->tokens && parser->tokens->type == T_OPERATOR && strcmp(parser->tokens->operator_value, operator) == 0;
}

static int peek_punct(const char punct, Parser *parser) {
  return parser->tokens && parser->tokens->type == T_PUNCT && parser->tokens->punct_value == punct;
}

static Token *pop(Parser *parser) {
  Token *t = parser->tokens;
  if (t) {
    parser->end = t->end;
    parser->tokens = t->next;
  }
  return t;
}

static Token *expect_type(TokenType type, Parser *parser) {
  if (peek_type(type, parser)) {
    return pop(parser);
  }
  if (!parser->tokens) {
    parser_error(parser, NULL, "unexpected end of token stream, expected %s", token_name(type));
  } else {
    parser_error(parser, parser->tokens, "unexpected %s, expected %s", token_name(parser->tokens->type), token_name(type));
  }
  return NULL;
}

static Token *expect_keyword(const char *keyword, Parser *parser) {
  if (peek_keyword(keyword, parser)) {
    return pop(parser);
  }
  if (!parser->tokens) {
    parser_error(parser, NULL, "unexpected end of token stream, expected \"%s\"", keyword);
  } else if (parser->tokens->type == T_KEYWORD) {
    parser_error(parser, parser->tokens, "unexpected \"%s\", expected \"%s\"", parser->tokens->name_value, keyword);
  } else {
    parser_error(parser, parser->tokens, "unexpected %s, expected \"%s\"", token_name(parser->tokens->type), keyword);
  }
  return NULL;
}

static Token *expect_operator(const char *operator, Parser *parser) {
  if (peek_operator(operator, parser)) {
    return pop(parser);
  }
  if (!parser->tokens) {
    parser_error(parser, NULL, "unexpected end of token stream, expected \"%s\"", operator);
  } else if (parser->tokens->type == T_OPERATOR) {
    parser_error(parser, parser->tokens, "unexpected \"%s\", expected \"%s\"", parser->tokens->operator_value, operator);
  } else {
    parser_error(parser, parser->tokens, "unexpected %s, expected \"%s\"", token_name(parser->tokens->type), operator);
  }
  return NULL;
}

static Token *expect_punct(const char punct, Parser *parser) {
  if (peek_punct(punct, parser)) {
    return pop(parser);
  }
  if (!parser->tokens) {
    parser_error(parser, NULL, "unexpected end of token stream, expected '%c'", punct);
  } else if (parser->tokens->type == T_OPERATOR) {
    parser_error(parser, parser->tokens, "unexpected '%c', expected '%c'", parser->tokens->punct_value, punct);
  } else {
    parser_error(parser, parser->tokens, "unexpected %s, expected '%c'", token_name(parser->tokens->type), punct);
  }
  return NULL;
}

static int expect_end(const char *keyword, Parser *parser) {
  if (peek_keyword("end", parser)) {
    pop(parser);
    return !!expect_keyword(keyword, parser);
  }
  if (!parser->tokens) {
    parser_error(parser, NULL, "unexpected end of token stream, expected \"end %s\"", keyword);
  } else if (parser->tokens->type == T_KEYWORD) {
    parser_error(parser, NULL, "unexpected \"%s\", expected \"end %s\"", parser->tokens->name_value, keyword);
  } else {
    parser_error(parser, parser->tokens, "unexpected %s, expected \"end %s\"", token_name(parser->tokens->type), keyword);
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

static char *parse_name(Parser *parser) {
  Token *t = expect_type(T_NAME, parser);
  if (t) {
    return copy_string(t->name_value);
  }
  return copy_string("");
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
    Node node = create_node(N_STRING, parser);
    node.string_value.size = peek(parser)->size;
    memcpy(node.string_value.bytes, pop(parser)->string_value, node.string_value.size);;
    node.end = parser->end;
    return node;
  } else if (peek_type(T_NAME, parser)) {
    Node node = create_node(N_NAME, parser);
    node.name_value = copy_string(pop(parser)->name_value);
    node.end = parser->end;
    return node;
  } else if (parser->tokens) {
    parser_error(parser, parser->tokens, "unexpected %s, expected an atom", token_name(parser->tokens->type));
    return create_node(N_INT, parser);
  } else {
    parser_error(parser, NULL, "unexpected end of token stream, expected an atom");
    return create_node(N_INT, parser);
  }
}

static Node parse_delimited(Parser *parser) {
  if (peek_punct('[', parser)) {
    Node list = create_node(N_LIST, parser);
    pop(parser);
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
    list.end = parser->end;
    return list;
  } else if (peek_punct('(', parser)) {
    pop(parser);
    Node expr = parse_expression(parser);
    expect_punct(')', parser);
    return expr;
  } else if (peek_punct('{', parser)) {
    Node object = create_node(N_OBJECT, parser);
    pop(parser);
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
        } else {
          object.object_value = last_property = property;
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
    object.end = parser->end;
    return object;
  } else if (peek_keyword("do", parser)) {
    Pos start = pop(parser)->start;
    Node block = parse_block(parser);
    expect_end("do", parser);
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

static Node parse_template(Parser *parser) {
  Node block = create_node(N_BLOCK, parser);
  while (1) {
  }
}

Module *parse(Token *tokens, const char *file_name) {
  Module *m = create_module(file_name);
  Parser parser = (Parser) { .tokens = tokens, .module = m, .errors = 0, .end.line = 1, .end.column = 1 };
  m->root = allocate(sizeof(Node));
  *m->root = parse_template(&parser);
  return m;
}

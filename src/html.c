/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "html.h"

#include "strings.h"

#ifdef WITH_GUMBO
#include <gumbo.h>
#endif

static void html_encode_byte(Buffer *buffer, uint8_t byte) {
  switch (byte) {
    case '&':
      buffer_printf(buffer, "&amp;");
      break;
    case '"':
      buffer_printf(buffer, "&quot;");
      break;
    case '\'':
      buffer_printf(buffer, "&#39;");
      break;
    case '<':
      buffer_printf(buffer, "&lt;");
      break;
    case '>':
      buffer_printf(buffer, "&gt;");
      break;
    default:
      buffer_put(buffer, byte);
      break;
  }
}

static Value h(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value value = args->values[0];
  Buffer buffer = create_buffer(0);
  switch (value.type) {
    case V_SYMBOL:
      while (*value.symbol_value) {
        html_encode_byte(&buffer, *value.symbol_value);
        value.symbol_value++;
      }
      break;
    case V_STRING:
      for (size_t i = 0; i < value.string_value->size; i++) {
        html_encode_byte(&buffer, value.string_value->bytes[i]);
      }
      break;
    default:
      value_to_string(value, &buffer);
      break;
  }
  Value string = create_string(buffer.data, buffer.size, env->arena);
  delete_buffer(buffer);
  return string;
}

static Value href(const Tuple *args, Env *env) {
  check_args_between(0, 2, args, env);
  Value path;
  Value class = create_string(NULL, 0, env->arena);
  if (args->size > 0) {
    path = args->values[0];
    if (path.type != V_STRING) {
      arg_type_error(0, V_STRING, args, env);
      return nil_value;
    }
    if (args->size > 1) {
      class = args->values[1];
      if (class.type != V_STRING) {
        arg_type_error(1, V_STRING, args, env);
        return nil_value;
      }
    }
  } else if (!env_get(get_symbol("PATH", env->symbol_map), &path, env) || path.type != V_STRING) {
    env_error(env, -1, "PATH is not set or not a string");
    return nil_value;
  }
  if (string_equals("index.html", path.string_value)) {
    path = copy_c_string("", env->arena);
  } else if (string_ends_with("/index.html", path.string_value)) {
    path = create_string(path.string_value->bytes, path.string_value->size - 11, env->arena);
  }
  // TODO: class += 'current' if path == PATH
  Buffer buffer = create_buffer(0);
  buffer_printf(&buffer, " href=\"");
  for (size_t i = 0; i < path.string_value->size; i++) {
    html_encode_byte(&buffer, path.string_value->bytes[i]);
  }
  buffer_printf(&buffer, "\"");
  if (class.string_value->size) {
    buffer_printf(&buffer, " class=\"");
    for (size_t i = 0; i < class.string_value->size; i++) {
      html_encode_byte(&buffer, class.string_value->bytes[i]);
    }
    buffer_printf(&buffer, "\"");
  }
  Value string = create_string(buffer.data, buffer.size, env->arena);
  delete_buffer(buffer);
  return string;
}

#ifdef WITH_GUMBO
static Value parse_html(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value src = args->values[0];
  if (src.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  return html_parse(src.string_value, env);
}
#endif

void import_html(Env *env) {
  env_def_fn("h", h, env);
  env_def_fn("href", href, env);
#ifdef WITH_GUMBO
  env_def_fn("parse_html", parse_html, env);
#endif
}

#ifdef WITH_GUMBO

static Value convert_gumbo_node(GumboNode *node, Env *env) {
  switch (node->type) {
    case GUMBO_NODE_DOCUMENT: {
      Value obj = create_object(2, env->arena);
      object_def(obj.object_value, "type", copy_c_string("document", env->arena), env);
      Value children = create_array(node->v.element.children.length, env->arena);
      for (unsigned int i = 0; i < node->v.element.children.length; i++) {
        Value child = convert_gumbo_node(node->v.element.children.data[i], env);
        if (child.type != V_NIL) {
          array_push(children.array_value, child, env->arena);
        }
      }
      object_def(obj.object_value, "children", children, env);
      return obj;
    }
    case GUMBO_NODE_ELEMENT: {
      Value obj = create_object(4, env->arena);
      object_def(obj.object_value, "type", copy_c_string("element", env->arena), env);
      object_def(obj.object_value, "tag", copy_c_string(gumbo_normalized_tagname(node->v.element.tag),
            env->arena), env);
      Value attributes = create_object(node->v.element.attributes.length, env->arena);
      for (unsigned int i = 0; i < node->v.element.attributes.length; i++) {
        GumboAttribute *attribute = node->v.element.attributes.data[i];
        object_put(attributes.object_value, copy_c_string(attribute->name, env->arena),
            copy_c_string(attribute->value, env->arena), env->arena);
      }
      object_def(obj.object_value, "attributes", attributes, env);
      Value children = create_array(node->v.element.children.length, env->arena);
      for (unsigned int i = 0; i < node->v.element.children.length; i++) {
        Value child = convert_gumbo_node(node->v.element.children.data[i], env);
        if (child.type != V_NIL) {
          array_push(children.array_value, child, env->arena);
        }
      }
      object_def(obj.object_value, "children", children, env);
      return obj;
    }
    case GUMBO_NODE_TEXT:
    case GUMBO_NODE_CDATA:
      return copy_c_string(node->v.text.text, env->arena);
    case GUMBO_NODE_COMMENT:
      return nil_value;
    case GUMBO_NODE_WHITESPACE:
      return copy_c_string(node->v.text.text, env->arena);
    case GUMBO_NODE_TEMPLATE:
    default:
      return nil_value;
  }
}

Value html_parse(String *html, Env *env) {
  GumboOptions options = kGumboDefaultOptions;
  options.fragment_context = GUMBO_TAG_DIV;
  GumboOutput *output = gumbo_parse_with_options(&options, (char *) html->bytes, html->size);
  Value root = convert_gumbo_node(output->root, env);
  gumbo_destroy_output(&options, output);
  return root;
}

#else /* ifdef WITH_GUMBO */

Value html_parse(String *html, Env *env) {
  return nil_value;
}

#endif /* ifdef WITH_GUMBO */

/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "html.h"

#include "strings.h"

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

void import_html(Env *env) {
  env_def_fn("h", h, env);
  env_def_fn("href", href, env);
}

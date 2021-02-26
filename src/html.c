/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "html.h"

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

void import_html(Env *env) {
  env_def_fn("h", h, env);
}

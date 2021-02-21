/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "strings.h"

#include <alloca.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static Value lower(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value arg = args->values[0];
  if (arg.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
  }
  if (!arg.string_value->size) {
    return arg;
  }
  Value result = allocate_string(arg.string_value->size, env->arena);
  result.string_value->size = arg.string_value->size;
  for (size_t i = 0; i < result.string_value->size; i++) {
    uint8_t byte = arg.string_value->bytes[i];
    if (byte >= 'A' && byte <= 'Z') {
      byte += 32;
      // TODO: unicode support using ICU maybe?
    }
    result.string_value->bytes[i] = byte;
  }
  return result;
}

static Value upper(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value arg = args->values[0];
  if (arg.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
  }
  if (!arg.string_value->size) {
    return arg;
  }
  Value result = allocate_string(arg.string_value->size, env->arena);
  result.string_value->size = arg.string_value->size;
  for (size_t i = 0; i < result.string_value->size; i++) {
    uint8_t byte = arg.string_value->bytes[i];
    if (byte >= 'a' && byte <= 'z') {
      byte -= 32;
      // TODO: unicode support using ICU maybe?
    }
    result.string_value->bytes[i] = byte;
  }
  return result;
}

static Value starts_with(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value obj = args->values[0];
  if (obj.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
  }
  Value prefix = args->values[1];
  if (prefix.type != V_STRING) {
    arg_type_error(1, V_STRING, args, env);
  }
  if (!prefix.string_value->size) {
    return true_value;
  }
  if (obj.string_value->size < prefix.string_value->size) {
    return nil_value;
  }
  for (size_t i = 0; i < prefix.string_value->size; i++) {
    if (prefix.string_value->bytes[i] != obj.string_value->bytes[i]) {
      return nil_value;
    }
  }
  return true_value;
}

static Value ends_with(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value obj = args->values[0];
  if (obj.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
  }
  Value prefix = args->values[1];
  if (prefix.type != V_STRING) {
    arg_type_error(1, V_STRING, args, env);
  }
  if (!prefix.string_value->size) {
    return true_value;
  }
  if (obj.string_value->size < prefix.string_value->size) {
    return nil_value;
  }
  for (size_t i = 0; i < prefix.string_value->size; i++) {
    if (prefix.string_value->bytes[i] != obj.string_value->bytes[obj.string_value->size - prefix.string_value->size + i]) {
      return nil_value;
    }
  }
  return true_value;
}

static Value symbol(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value arg = args->values[0];
  if (arg.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
  }
  char *name = alloca(arg.string_value->size + 1);
  memcpy(name, arg.string_value->bytes, arg.string_value->size);
  name[arg.string_value->size] = '\0';
  return create_symbol(get_symbol(name, env->symbol_map));
}

static void json_encode_value(Value value, Buffer *buffer) {
  switch (value.type) {
    case V_NIL:
      buffer_printf(buffer, "null");
      break;
    case V_TRUE:
      buffer_printf(buffer, "true");
      break;
    case V_INT:
      buffer_printf(buffer, "%" PRId64, value.int_value);
      break;
    case V_FLOAT:
      buffer_printf(buffer, "%lg", value.float_value);
      break;
    case V_SYMBOL:
      buffer_printf(buffer, "\"%s\"", value.symbol_value);
      break;
    case V_STRING:
      buffer_printf(buffer, "\"");
      for (size_t i = 0; i < value.string_value->size; i++) {
        uint8_t byte = value.string_value->bytes[i];
        switch (byte) {
          case '"':
            buffer_printf(buffer, "\\\"");
            break;
          case '\\':
            buffer_printf(buffer, "\\\\");
            break;
          case '\x08':
            buffer_printf(buffer, "\\b");
            break;
          case '\f':
            buffer_printf(buffer, "\\f");
            break;
          case '\n':
            buffer_printf(buffer, "\\n");
            break;
          case '\r':
            buffer_printf(buffer, "\\r");
            break;
          case '\t':
            buffer_printf(buffer, "\\t");
            break;
          default:
            if (byte < 32 || byte > 126) {
              buffer_printf(buffer, "\\x%02x", byte);
            } else {
              buffer_put(buffer, byte);
            }
            break;
        }
      }
      buffer_printf(buffer, "\"");
      break;
    case V_ARRAY:
      buffer_printf(buffer, "[");
      for (size_t i = 0; i < value.array_value->size; i++) {
        if (i > 0) {
          buffer_printf(buffer, ",");
        }
        json_encode_value(value.array_value->cells[i], buffer);
      }
      buffer_printf(buffer, "]");
      break;
    case V_OBJECT:
      buffer_printf(buffer, "{");
      for (size_t i = 0; i < value.object_value->size; i++) {
        if (i > 0) {
          buffer_printf(buffer, ",");
        }
        Value key = value.object_value->entries[i].key;
        if (key.type == V_STRING || key.type == V_SYMBOL) {
          json_encode_value(key, buffer);
        } else {
          Buffer buffer2 = create_buffer(0);
          json_encode_value(key, &buffer2);
          String *string = allocate(sizeof(String) + buffer2.size);
          string->size = buffer2.size;
          memcpy(string->bytes, buffer2.data, buffer2.size);
          json_encode_value((Value) { .type = V_STRING, .string_value = string }, buffer);
          free(string);
          delete_buffer(buffer2);
        }
        buffer_printf(buffer, ":");
        json_encode_value(value.object_value->entries[i].value, buffer);
      }
      buffer_printf(buffer, "}");
      break;
    case V_TIME: {
      struct tm *utc = gmtime(&value.time_value);
      if (utc) {
        char date[26];
        if (strftime(date, sizeof(date), "%Y-%m-%dT%H:%M:%SZ", utc)) {
          buffer_printf(buffer, "\"%s\"", date);
        } else {
          buffer_printf(buffer, "\"(invalid time)\"");
        }
      } else {
        buffer_printf(buffer, "\"(invalid time)\"");
      }
      break;
    }
    case V_FUNCTION:
    case V_CLOSURE:
      buffer_printf(buffer, "\"(function)\"");
      break;
  }
}

static Value json(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Buffer buffer = create_buffer(0);
  json_encode_value(args->values[0], &buffer);
  Value result = create_string(buffer.data, buffer.size, env->arena);
  delete_buffer(buffer);
  return result;
}

void import_strings(Env *env) {
  env_def_fn("lower", lower, env);
  env_def_fn("upper", upper, env);
  env_def_fn("starts_with", starts_with, env);
  env_def_fn("ends_with", ends_with, env);
  env_def_fn("symbol", symbol, env);
  env_def_fn("json", json, env);
}

/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "strings.h"

#include <alloca.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#ifdef WITH_UNICODE
#include <unicode/ucasemap.h>
#endif

static Value lower(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value arg = args->values[0];
  if (arg.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  if (!arg.string_value->size) {
    return arg;
  }
  Value result = allocate_string(arg.string_value->size, env->arena);
  result.string_value->size = arg.string_value->size;
#ifdef WITH_UNICODE
  UErrorCode status = U_ZERO_ERROR;
  char *locale = NULL; // TODO
  UCaseMap *case_map = ucasemap_open(locale, U_FOLD_CASE_DEFAULT, &status);
  if (U_SUCCESS(status)) {
    int32_t result_len = ucasemap_utf8ToLower(case_map, (char *) result.string_value->bytes,
        result.string_value->size, (char *) arg.string_value->bytes, arg.string_value->size, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
      result = reallocate_string(result.string_value, result_len, env->arena);
      status = U_ZERO_ERROR;
      result_len = ucasemap_utf8ToLower(case_map, (char *) result.string_value->bytes, result.string_value->size,
          (char *) arg.string_value->bytes, arg.string_value->size, &status);
    }
    result.string_value->size = result_len;
  }
  ucasemap_close(case_map);
  if (U_FAILURE(status)) {
    env_error(env, -1, "case map error: %s", u_errorName(status));
    return nil_value;
  }
#else
  for (size_t i = 0; i < result.string_value->size; i++) {
    uint8_t byte = arg.string_value->bytes[i];
    if (byte >= 'A' && byte <= 'Z') {
      byte += 32;
    }
    result.string_value->bytes[i] = byte;
  }
#endif
  return result;
}

static Value upper(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value arg = args->values[0];
  if (arg.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  if (!arg.string_value->size) {
    return arg;
  }
  Value result = allocate_string(arg.string_value->size, env->arena);
  result.string_value->size = arg.string_value->size;
#ifdef WITH_UNICODE
  UErrorCode status = U_ZERO_ERROR;
  char *locale = NULL; // TODO
  UCaseMap *case_map = ucasemap_open(locale, U_FOLD_CASE_DEFAULT, &status);
  if (U_SUCCESS(status)) {
    int32_t result_len = ucasemap_utf8ToUpper(case_map, (char *) result.string_value->bytes,
        result.string_value->size, (char *) arg.string_value->bytes, arg.string_value->size, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
      result = reallocate_string(result.string_value, result_len, env->arena);
      status = U_ZERO_ERROR;
      result_len = ucasemap_utf8ToUpper(case_map, (char *) result.string_value->bytes, result.string_value->size,
          (char *) arg.string_value->bytes, arg.string_value->size, &status);
    }
    result.string_value->size = result_len;
  }
  ucasemap_close(case_map);
  if (U_FAILURE(status)) {
    env_error(env, -1, "case map error: %s", u_errorName(status));
    return nil_value;
  }
#else
  for (size_t i = 0; i < result.string_value->size; i++) {
    uint8_t byte = arg.string_value->bytes[i];
    if (byte >= 'a' && byte <= 'z') {
      byte -= 32;
    }
    result.string_value->bytes[i] = byte;
  }
#endif
  return result;
}

static Value title(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value arg = args->values[0];
  if (arg.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  if (!arg.string_value->size) {
    return arg;
  }
  Value result = allocate_string(arg.string_value->size, env->arena);
  result.string_value->size = arg.string_value->size;
#ifdef WITH_UNICODE
  UErrorCode status = U_ZERO_ERROR;
  char *locale = NULL; // TODO
  UCaseMap *case_map = ucasemap_open(locale, U_FOLD_CASE_DEFAULT, &status);
  if (U_SUCCESS(status)) {
    int32_t result_len = ucasemap_utf8ToTitle(case_map, (char *) result.string_value->bytes,
        result.string_value->size, (char *) arg.string_value->bytes, arg.string_value->size, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
      result = reallocate_string(result.string_value, result_len, env->arena);
      status = U_ZERO_ERROR;
      result_len = ucasemap_utf8ToTitle(case_map, (char *) result.string_value->bytes, result.string_value->size,
          (char *) arg.string_value->bytes, arg.string_value->size, &status);
    }
    result.string_value->size = result_len;
  }
  ucasemap_close(case_map);
  if (U_FAILURE(status)) {
    env_error(env, -1, "case map error: %s", u_errorName(status));
    return nil_value;
  }
#else
  for (size_t i = 0; i < result.string_value->size; i++) {
    uint8_t byte = arg.string_value->bytes[i];
    if (i == 0 || isspace(arg.string_value->bytes[i - 1])) {
      if (byte >= 'a' && byte <= 'z') {
        byte -= 32;
      }
    } else if (byte >= 'A' && byte <= 'Z') {
      byte += 32;
    }
    result.string_value->bytes[i] = byte;
  }
#endif
  return result;
}

static Value starts_with(const Tuple *args, Env *env) {
  check_args(2, args, env);
  Value obj = args->values[0];
  if (obj.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  Value prefix = args->values[1];
  if (prefix.type != V_STRING) {
    arg_type_error(1, V_STRING, args, env);
    return nil_value;
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
    return nil_value;
  }
  Value suffix = args->values[1];
  if (suffix.type != V_STRING) {
    arg_type_error(1, V_STRING, args, env);
    return nil_value;
  }
  if (!suffix.string_value->size) {
    return true_value;
  }
  if (obj.string_value->size < suffix.string_value->size) {
    return nil_value;
  }
  for (size_t i = 0; i < suffix.string_value->size; i++) {
    if (suffix.string_value->bytes[i] != obj.string_value->bytes[obj.string_value->size - suffix.string_value->size + i]) {
      return nil_value;
    }
  }
  return true_value;
}

static Value replace(const Tuple *args, Env *env) {
  check_args(3, args, env);
  Value haystack = args->values[0];
  if (haystack.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
  Value needle = args->values[1];
  if (needle.type != V_STRING) {
    arg_type_error(1, V_STRING, args, env);
    return nil_value;
  }
  Value replacement = args->values[2];
  if (replacement.type != V_STRING) {
    arg_type_error(2, V_STRING, args, env);
    return nil_value;
  }
  return string_replace(needle.string_value, replacement.string_value, haystack.string_value, env->arena);
}

static Value symbol(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value arg = args->values[0];
  if (arg.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
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
          case '\b':
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
            if (byte < 32 || byte == 127) {
              buffer_printf(buffer, "\\x%02x", byte); // not valid json, use \uXXXX for valid unicode codepoints
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
      ObjectIterator it = iterate_object(value.object_value);
      Value entry_key, entry_value;
      int first = 1;
      while (object_iterator_next(&it, &entry_key, &entry_value)) {
        if (!first) {
          buffer_printf(buffer, ",");
          first = 0;
        }
        if (entry_key.type == V_STRING || entry_key.type == V_SYMBOL) {
          json_encode_value(entry_key, buffer);
        } else {
          Buffer buffer2 = create_buffer(0);
          json_encode_value(entry_key, &buffer2);
          String *string = allocate(sizeof(String) + buffer2.size);
          string->size = buffer2.size;
          memcpy(string->bytes, buffer2.data, buffer2.size);
          json_encode_value((Value) { .type = V_STRING, .string_value = string }, buffer);
          free(string);
          delete_buffer(buffer2);
        }
        buffer_printf(buffer, ":");
        json_encode_value(entry_value, buffer);
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
  env_def_fn("title", title, env);
  env_def_fn("starts_with", starts_with, env);
  env_def_fn("ends_with", ends_with, env);
  env_def_fn("replace", replace, env);
  env_def_fn("symbol", symbol, env);
  env_def_fn("json", json, env);
}

int string_equals(const char *c_string, const String *string) {
  size_t c_string_length = strlen(c_string);
  if (c_string_length != string->size) {
    return 0;
  }
  return memcmp(c_string, string->bytes, c_string_length) == 0;
}

int string_starts_with(const char *prefix, const String *string) {
  size_t prefix_length = strlen(prefix);
  if (prefix_length > string->size) {
    return 0;
  }
  return memcmp(prefix, string->bytes, prefix_length) == 0;
}

int string_ends_with(const char *prefix, const String *string) {
  size_t prefix_length = strlen(prefix);
  if (prefix_length > string->size) {
    return 0;
  }
  return memcmp(prefix, string->bytes + string->size - prefix_length, prefix_length) == 0;
}

Value string_replace(const String *needle, const String *replacement, String *haystack, Arena *arena) {
  if (needle->size > 0 && needle->size <= haystack->size) {
    for (size_t i = 0; i <= haystack->size - needle->size; i++) {
      if (memcmp(needle->bytes, haystack->bytes + i, needle->size) == 0) {
        Value result = allocate_string(haystack->size - needle->size + replacement->size, arena);
        if (i > 0) {
          memcpy(result.string_value->bytes, haystack->bytes, i);
        }
        if (replacement->size > 0) {
          memcpy(result.string_value->bytes + i, replacement->bytes, replacement->size);
        }
        if (i < haystack->size - needle->size) {
          memcpy(result.string_value->bytes + i + replacement->size, haystack->bytes + i + needle->size, haystack->size - i - needle->size);
        }
        return result;
      }
    }
  }
  return (Value) { .type = V_STRING, .string_value = haystack };
}

StringBuffer create_string_buffer(size_t capacity, Arena *arena) {
  if (capacity < INITIAL_BUFFER_SIZE) {
    capacity = INITIAL_BUFFER_SIZE;
  }
  Value string = allocate_string(capacity, arena);
  string.string_value->size = 0;
  return (StringBuffer) { .arena = arena, .string = string.string_value, .capacity = capacity };
}

Value finalize_string_buffer(StringBuffer buffer) {
  return (Value) { .type = V_STRING, .string_value = buffer.string };
}

void string_buffer_put(StringBuffer *buffer, uint8_t byte) {
  if (buffer->string->size >= buffer->capacity) {
    buffer->capacity <<= 1;
    size_t existing_size = buffer->string->size;
    Value string = reallocate_string(buffer->string, buffer->capacity, buffer->arena);
    string.string_value->size = existing_size;
    buffer->string = string.string_value;
  }
  buffer->string->bytes[buffer->string->size++] = byte;
}

void string_buffer_append(StringBuffer *buffer, String *suffix) {
  if (!suffix->size) {
    return;
  }
  size_t size = buffer->string->size + suffix->size;
  if (size > buffer->capacity) {
    while (size > buffer->capacity) {
      buffer->capacity <<= 1;
    }
    size_t existing_size = buffer->string->size;
    Value string = reallocate_string(buffer->string, buffer->capacity, buffer->arena);
    string.string_value->size = existing_size;
    buffer->string = string.string_value;
  }
  memcpy(buffer->string->bytes + buffer->string->size, suffix->bytes, suffix->size);
  buffer->string->size += suffix->size;
}

void string_buffer_append_bytes(StringBuffer *buffer, const uint8_t *bytes, size_t size) {
  if (!size) {
    return;
  }
  size_t new_size = buffer->string->size + size;
  if (new_size > buffer->capacity) {
    while (new_size > buffer->capacity) {
      buffer->capacity <<= 1;
    }
    size_t existing_size = buffer->string->size;
    Value string = reallocate_string(buffer->string, buffer->capacity, buffer->arena);
    string.string_value->size = existing_size;
    buffer->string = string.string_value;
  }
  memcpy(buffer->string->bytes + buffer->string->size, bytes, size);
  buffer->string->size = new_size;
}

void string_buffer_vprintf(StringBuffer *buffer, const char *format, va_list va) {
  int n;
  va_list va2;
  size_t size;
  size = buffer->capacity - buffer->string->size;
  if (size <= 0) {
    buffer->capacity <<= 1;
    size_t existing_size = buffer->string->size;
    Value string = reallocate_string(buffer->string, buffer->capacity, buffer->arena);
    string.string_value->size = existing_size;
    buffer->string = string.string_value;
  }
  while (1) {
    uint8_t *dest = buffer->string->bytes + buffer->string->size;
    va_copy(va2, va);
    n = vsnprintf((char *) dest, size, format, va2);
    va_end(va2);
    if (n < 0) {
      return;
    }
    if (n < size) {
      buffer->string->size += n;
      break;
    }
    size = n + 1;
    while (size + buffer->string->size > buffer->capacity) {
      buffer->capacity <<= 1;
    }
    size_t existing_size = buffer->string->size;
    Value string = reallocate_string(buffer->string, buffer->capacity, buffer->arena);
    string.string_value->size = existing_size;
    buffer->string = string.string_value;
  }
}

void string_buffer_printf(StringBuffer *buffer, const char *format, ...) {
  va_list va;
  va_start(va, format);
  string_buffer_vprintf(buffer, format, va);
  va_end(va);
}

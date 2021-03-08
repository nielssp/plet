/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef STRINGS_H
#define STRINGS_H

#include "value.h"

#include <stdarg.h>

void import_strings(Env *env);

int string_equals(const char *c_string, const String *string);
int string_starts_with(const char *prefix, const String *string);
int string_ends_with(const char *prefix, const String *string);
Value string_replace(const String *needle, const String *replacement, String *haystack, Arena *arena);

typedef struct {
  Arena *arena;
  String *string;
  size_t capacity;
} StringBuffer;

StringBuffer create_string_buffer(size_t capacity, Arena *arena);
Value finalize_string_buffer(StringBuffer buffer);
void string_buffer_put(StringBuffer *buffer, uint8_t byte);
void string_buffer_append(StringBuffer *buffer, String *string);
void string_buffer_append_value(StringBuffer *buffer, Value value);
void string_buffer_append_bytes(StringBuffer *buffer, const uint8_t *bytes, size_t size);
void string_buffer_vprintf(StringBuffer *buffer, const char *format, va_list va);
void string_buffer_printf(StringBuffer *buffer, const char *format, ...);

Value combine_string_paths(String *path1, String *path2, Env *env);
Value string_ltrim(String *string, const uint8_t *bytes, size_t num_bytes, Arena *arena);
Value string_rtrim(String *string, const uint8_t *bytes, size_t num_bytes, Arena *arena);
Value string_trim(String *string, const uint8_t *bytes, size_t num_bytes, Arena *arena);

#endif

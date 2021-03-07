/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "../src/strings.h"

#include "test.h"

#include <string.h>

static void test_string_buffer_put(void) {
  Arena *arena = create_arena();
  StringBuffer buffer = create_string_buffer(0, arena);
  assert(buffer.string->size == 0);
  for (int i = 0; i < 5000; i++) {
    string_buffer_put(&buffer, i % 256);
  }
  Value string = finalize_string_buffer(buffer);
  assert(string.type == V_STRING);
  assert(string.string_value->size == 5000);
  for (int i = 0; i < 5000; i++) {
    assert(string.string_value->bytes[i] == (i % 256));
  }
  delete_arena(arena);
}

void test_strings(void) {
  run_test(test_string_buffer_put);
}


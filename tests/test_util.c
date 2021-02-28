/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "../src/util.h"

#include "test.h"

#include <string.h>

static void test_arena(void) {
  Arena *arena = create_arena();
  for (int i = 0; i < 1000; i++) {
    char *s = arena_allocate(7, arena);
    memcpy(s, "tester", 7);
  }
  char *s = arena_allocate(10000, arena);
  for (int i = 0; i < 10000; i++) {
    *(s++) = 0;
  }
  delete_arena(arena);
}

static void test_arena_reallocate(void) {
  Arena *arena = create_arena();
  char *p1 = arena_allocate(10, arena);
  for (int i = 0; i < 10; i++) {
    p1[i] = i;
  }
  char *p2 = arena_reallocate(p1, 10, 20, arena);
  for (int i = 0; i < 10; i++) {
    assert(p1[i] == i);
    assert(p2[i] == i);
  }
  for (int i = 10; i < 20; i++) {
    p1[i] = i;
  }
  assert(p1 == p2);
  char *p3 = arena_allocate(10, arena);
  assert(p3 == p1 + 20);
  char *p4 = arena_reallocate(p2, 20, 30, arena);
  assert(p4 != p2);
  assert(p4 == p3 + 10);
  for (int i = 0; i < 20; i++) {
    assert(p4[i] == i);
  }
  delete_arena(arena);
}

static void test_buffer_printf(void) {
  Buffer buffer1 = create_buffer(0);
  for (int i = 0; i < 1000; i++) {
    buffer_printf(&buffer1, "test");
  }
  assert(buffer1.size == 4000);
  assert(strncmp((char *) buffer1.data, "test", 4) == 0);
  assert(strncmp((char *) buffer1.data + 400, "test", 4) == 0);
  assert(strncmp((char *) buffer1.data + 3996, "test", 4) == 0);

  Buffer buffer2 = create_buffer(0);
  buffer_printf(&buffer2, "%s", buffer1.data);
  assert(buffer2.size == 4000);
  assert(strncmp((char *) buffer2.data, "test", 4) == 0);
  assert(strncmp((char *) buffer2.data + 400, "test", 4) == 0);
  assert(strncmp((char *) buffer2.data + 3996, "test", 4) == 0);

  delete_buffer(buffer1);
  delete_buffer(buffer2);
}

void test_util(void) {
  run_test(test_arena);
  run_test(test_arena_reallocate);
  run_test(test_buffer_printf);
}


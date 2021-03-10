/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "../src/util.h"

#include "test.h"

#include <stdlib.h>
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

static void test_create_path(void) {
  Path *path;

  path = create_path("", -1);
  assert(strcmp(path->path, "") == 0);
  delete_path(path);

  path = create_path("foo", -1);
  assert(strcmp(path->path, "foo") == 0);
  delete_path(path);

#if defined(_WIN32)
#else
  path = create_path("foo/bar", -1);
  assert(strcmp(path->path, "foo/bar") == 0);
  delete_path(path);

  path = create_path(".", -1);
  assert(strcmp(path->path, "") == 0);
  delete_path(path);

  path = create_path("./foo//bar/", -1);
  assert(strcmp(path->path, "foo/bar") == 0);
  delete_path(path);

  path = create_path("/", -1);
  assert(strcmp(path->path, "/") == 0);
  delete_path(path);

  path = create_path("////", -1);
  assert(strcmp(path->path, "/") == 0);
  delete_path(path);

  path = create_path("/foo/bar/", -1);
  assert(strcmp(path->path, "/foo/bar") == 0);
  delete_path(path);

  path = create_path("..", -1);
  assert(strcmp(path->path, "..") == 0);
  delete_path(path);

  path = create_path("/..", -1);
  assert(strcmp(path->path, "/") == 0);
  delete_path(path);

  path = create_path("./..", -1);
  assert(strcmp(path->path, "..") == 0);
  delete_path(path);

  path = create_path("../../..", -1);
  assert(strcmp(path->path, "../../..") == 0);
  delete_path(path);

  path = create_path("../foo", -1);
  assert(strcmp(path->path, "../foo") == 0);
  delete_path(path);

  path = create_path("foo/..", -1);
  assert(strcmp(path->path, "") == 0);
  delete_path(path);

  path = create_path("foo/bar/baz/../..", -1);
  assert(strcmp(path->path, "foo") == 0);
  delete_path(path);

  path = create_path("../a/b/../c/d/e/../../f", -1);
  assert(strcmp(path->path, "../a/c/f") == 0);
  delete_path(path);

  path = create_path("/../a/b/../c/d/e/../../f", -1);
  assert(strcmp(path->path, "/a/c/f") == 0);
  delete_path(path);
#endif
}

static void test_copy_path(void) {
  Path *path1, *path2;
#if defined(_WIN32)
#else
  path1 = create_path("/../a/b/../c/d/e/../../f", -1);
  path2 = copy_path(path1);
  assert(strcmp(path1->path, "/a/c/f") == 0);
  assert(strcmp(path2->path, "/a/c/f") == 0);
  delete_path(path1);
  delete_path(path2);
#endif
}

static void test_path_is_absolute(void) {
  Path *path;

  path = create_path("", -1);
  assert(!path_is_absolute(path));
  delete_path(path);

  path = create_path("foo", -1);
  assert(!path_is_absolute(path));
  delete_path(path);

#if defined(_WIN32)
#else
  path = create_path("foo/bar", -1);
  assert(!path_is_absolute(path));
  delete_path(path);

  path = create_path("/", -1);
  assert(path_is_absolute(path));
  delete_path(path);

  path = create_path("/foo/bar/", -1);
  assert(path_is_absolute(path));
  delete_path(path);
#endif
}

static void test_path_get_parent(void) {
  Path *path1, *path2;

  path1 = create_path("", -1);
  path2 = path_get_parent(path1);
  assert(strcmp(path2->path, "..") == 0);
  delete_path(path1);
  delete_path(path2);

  path1 = create_path("foo", -1);
  path2 = path_get_parent(path1);
  assert(strcmp(path2->path, "") == 0);
  delete_path(path1);
  delete_path(path2);

#if defined(_WIN32)
#else
  path1 = create_path("foo/bar", -1);
  path2 = path_get_parent(path1);
  assert(strcmp(path2->path, "foo") == 0);
  delete_path(path1);
  delete_path(path2);

  path1 = create_path("/bar", -1);
  path2 = path_get_parent(path1);
  assert(strcmp(path2->path, "/") == 0);
  delete_path(path1);
  delete_path(path2);

  path1 = create_path("/", -1);
  path2 = path_get_parent(path1);
  assert(strcmp(path2->path, "/") == 0);
  delete_path(path1);
  delete_path(path2);

  path1 = create_path("/foo/bar/baz", -1);
  path2 = path_get_parent(path1);
  assert(strcmp(path2->path, "/foo/bar") == 0);
  delete_path(path1);
  delete_path(path2);

  path1 = create_path("..", -1);
  path2 = path_get_parent(path1);
  assert(strcmp(path2->path, "../..") == 0);
  delete_path(path1);
  delete_path(path2);
#endif
}

static void test_path_get_name(void) {
  Path *path;

  path = create_path("", -1);
  assert(strcmp(path_get_name(path), "") == 0);
  delete_path(path);

  path = create_path("foo", -1);
  assert(strcmp(path_get_name(path), "foo") == 0);
  delete_path(path);

#if defined(_WIN32)
#else
  path = create_path("foo/bar", -1);
  assert(strcmp(path_get_name(path), "bar") == 0);
  delete_path(path);

  path = create_path("/bar", -1);
  assert(strcmp(path_get_name(path), "bar") == 0);
  delete_path(path);

  path = create_path("/", -1);
  assert(strcmp(path_get_name(path), "/") == 0);
  delete_path(path);

  path = create_path("/foo/bar/baz", -1);
  assert(strcmp(path_get_name(path), "baz") == 0);
  delete_path(path);

  path = create_path("..", -1);
  assert(strcmp(path_get_name(path), "..") == 0);
  delete_path(path);
#endif
}

static void test_path_join(void) {
  Path *path1, *path2, *path3;

  path1 = create_path("", -1);
  path2 = create_path("", -1);
  path3 = path_join(path1, path2);
  assert(strcmp(path3->path, "") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("foo", -1);
  path2 = create_path("", -1);
  path3 = path_join(path1, path2);
  assert(strcmp(path3->path, "foo") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("", -1);
  path2 = create_path("foo", -1);
  path3 = path_join(path1, path2);
  assert(strcmp(path3->path, "foo") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

#if defined(_WIN32)
#else
  path1 = create_path("foo", -1);
  path2 = create_path("bar", -1);
  path3 = path_join(path1, path2);
  assert(strcmp(path3->path, "foo/bar") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("/foo", -1);
  path2 = create_path("bar", -1);
  path3 = path_join(path1, path2);
  assert(strcmp(path3->path, "/foo/bar") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("/foo", -1);
  path2 = create_path("/bar", -1);
  path3 = path_join(path1, path2);
  assert(strcmp(path3->path, "/bar") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("/foo", -1);
  path2 = create_path("../bar", -1);
  path3 = path_join(path1, path2);
  assert(strcmp(path3->path, "/bar") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("..", -1);
  path2 = create_path("..", -1);
  path3 = path_join(path1, path2);
  assert(strcmp(path3->path, "../..") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("..", -1);
  path2 = create_path("../foo", -1);
  path3 = path_join(path1, path2);
  assert(strcmp(path3->path, "../../foo") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("/a/b", -1);
  path2 = create_path("../../../c/d", -1);
  path3 = path_join(path1, path2);
  assert(strcmp(path3->path, "/c/d") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);
#endif
}

static void test_path_get_relative(void) {
  Path *path1, *path2, *path3;

  path1 = create_path("", -1);
  path2 = create_path("", -1);
  path3 = path_get_relative(path1, path2);
  assert(strcmp(path3->path, "") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("foo", -1);
  path2 = create_path("", -1);
  path3 = path_get_relative(path1, path2);
  assert(strcmp(path3->path, "..") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("", -1);
  path2 = create_path("foo", -1);
  path3 = path_get_relative(path1, path2);
  assert(strcmp(path3->path, "foo") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

#if defined(_WIN32)
#else
  path1 = create_path("foo", -1);
  path2 = create_path("bar", -1);
  path3 = path_get_relative(path1, path2);
  assert(strcmp(path3->path, "../bar") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("/foo", -1);
  path2 = create_path("bar", -1);
  path3 = path_get_relative(path1, path2);
  assert(path3 == NULL);
  delete_path(path1);
  delete_path(path2);

  path1 = create_path("/foo", -1);
  path2 = create_path("/bar", -1);
  path3 = path_get_relative(path1, path2);
  assert(strcmp(path3->path, "../bar") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("../foo", -1);
  path2 = create_path("../bar", -1);
  path3 = path_get_relative(path1, path2);
  assert(strcmp(path3->path, "../bar") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("..", -1);
  path2 = create_path("..", -1);
  path3 = path_get_relative(path1, path2);
  assert(strcmp(path3->path, "") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("..", -1);
  path2 = create_path("../foo", -1);
  path3 = path_get_relative(path1, path2);
  assert(strcmp(path3->path, "foo") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);

  path1 = create_path("/a/b/c/d/e", -1);
  path2 = create_path("/a/b/f/g", -1);
  path3 = path_get_relative(path1, path2);
  assert(strcmp(path3->path, "../../../f/g") == 0);
  delete_path(path1);
  delete_path(path2);
  delete_path(path3);
#endif
}

void test_util(void) {
  run_test(test_arena);
  run_test(test_arena_reallocate);
  run_test(test_buffer_printf);
  run_test(test_create_path);
  run_test(test_copy_path);
  run_test(test_path_is_absolute);
  run_test(test_path_get_parent);
  run_test(test_path_get_name);
  run_test(test_path_join);
  run_test(test_path_get_relative);
}


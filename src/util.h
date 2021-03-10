/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef UTIL_H
#define UTIL_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define SGR_RESET "\001\033[0m\002"
#define SGR_RED "\001\033[31m\002"
#define SGR_BOLD "\001\033[1m\002"
#define SGR_BOLD_RED "\001\033[1;31m\002"
#define SGR_BOLD_GREEN "\001\033[1;32m\002"
#define SGR_BOLD_YELLOW "\001\033[1;33m\002"
#define SGR_BOLD_MAGENTA "\001\033[1;35m\002"
#define SGR_BOLD_CYAN "\001\033[1;36m\002"
#define ERROR_LABEL SGR_BOLD_RED "error: " SGR_RESET SGR_BOLD
#define WARN_LABEL SGR_BOLD_MAGENTA "warning: " SGR_RESET SGR_BOLD
#define INFO_LABEL SGR_BOLD_CYAN "info: " SGR_RESET SGR_BOLD

#define MIN_ARENA_SIZE 4096
#define INITIAL_BUFFER_SIZE 32

#if defined(_WIN32)
#define PATH_SEP '\\'
#define IS_PATH_SEP(BYTE) ((BYTE) == '\\' || (BYTE) == '/')
#else
#define PATH_SEP '/'
#define IS_PATH_SEP(BYTE) ((BYTE) == '/')
#endif

typedef struct Arena Arena;

typedef struct {
  int line;
  int column;
} Pos;

struct Arena {
  Arena *next;
  Arena *last;
  size_t capacity;
  size_t size;
  uint8_t data[];
};

typedef struct {
  uint8_t *data;
  size_t capacity;
  size_t size;
} Buffer;

void *allocate(size_t size);

void *reallocate(void *old, size_t size);

Arena *create_arena(void);

void delete_arena(Arena *arena);

void *arena_allocate(size_t size, Arena *arena);
void *arena_reallocate(void *old, size_t old_size, size_t size, Arena *arena);

char *copy_string(const char *src);

Buffer create_buffer(size_t capacity);
void delete_buffer(Buffer buffer);
void buffer_put(Buffer *buffer, uint8_t byte);
void buffer_vprintf(Buffer *buffer, const char *format, va_list va);
void buffer_printf(Buffer *buffer, const char *format, ...);

size_t get_line_in_file(int line, char **output, FILE *f);
void print_error_line(const char *file_name, Pos start, Pos end);

char *combine_paths(const char *path1, const char *path2);
time_t get_mtime(const char *path);
int is_dir(const char *path);
int copy_file(const char *src_path, const char *dest_path);
int mkdir_rec(const char *path);

typedef struct PathComponent PathComponent;

typedef struct {
  int32_t size;
  char path[];
} Path;

Path *create_path(const char *path_bytes, int32_t length);
Path *copy_path(const Path *path);
void delete_path(Path *path);
int path_is_absolute(const Path *path);
int path_is_descending(const Path *path);
Path *path_get_parent(const Path *path);
const char *path_get_name(const Path *path);
Path *path_join(const Path *path1, const Path *path2);
Path *path_get_relative(const Path *start, const Path *end);

#endif

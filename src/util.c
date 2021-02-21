/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_ARENA_SIZE 4096
#define INITIAL_BUFFER_SIZE 32

void *allocate(size_t size) {
  void *p = malloc(size);
  if (!p) {
    fprintf(stderr, ERROR_LABEL "memory allocation failed!\n" SGR_RESET);
    exit(-1);
  }
  return p;
}

void *reallocate(void *old, size_t size) {
  void *new = realloc(old, size);
  if (!new) {
    fprintf(stderr, ERROR_LABEL "memory allocation failed!\n" SGR_RESET);
    exit(-1);
  }
  return new;
}

Arena *create_arena(void) {
  Arena *arena = allocate(sizeof(Arena) + MIN_ARENA_SIZE);
  arena->next = NULL;
  arena->last = arena;
  arena->capacity = MIN_ARENA_SIZE;
  arena->size = 0;
  return arena;
}

void delete_arena(Arena *arena) {
  if (arena->next) {
    delete_arena(arena->next);
  }
  free(arena);
}

void *arena_allocate(size_t size, Arena *arena) {
  if (!arena) {
    return allocate(size);
  }
  Arena *last = arena->last;
  if (size <= last->capacity - last->size) {
    void *p = last->data + last->size;
    last->size += size;
    return p;
  } else {
    size_t new_size = size > MIN_ARENA_SIZE ? size : MIN_ARENA_SIZE;
    Arena *new = allocate(sizeof(Arena) + new_size);
    new->next = NULL;
    new->last = new;
    new->capacity = new_size;
    new->size = size;
    last->next = new;
    last->last = new;
    arena->last = new;
    return new->data;
  }
}

void *arena_reallocate(void *old, size_t old_size, size_t size, Arena *arena) {
  if (!arena) {
    return reallocate(old, size);
  }
  Arena *last = arena->last;
  if (old != last->data + last->size - old_size || size - old_size > last->capacity - last->size) {
    return arena_allocate(size, arena);
  }
  last->size += size - old_size;
  return old;
}

char *copy_string(const char *src) {
  size_t l = strlen(src) + 1;
  char *dest = allocate(l);
  memcpy(dest, src, l);
  return dest;
}

Buffer create_buffer(size_t capacity) {
  if (capacity < INITIAL_BUFFER_SIZE) {
    capacity = INITIAL_BUFFER_SIZE;
  }
  return (Buffer) { .data = allocate(capacity), .capacity = capacity, .size = 0 };
}

void delete_buffer(Buffer buffer) {
  free(buffer.data);
}

void buffer_put(Buffer *buffer, uint8_t byte) {
  if (buffer->size >= buffer->capacity) {
    buffer->capacity <<= 1;
    buffer->data = reallocate(buffer->data, buffer->capacity);
  }
  buffer->data[buffer->size++] = byte;
}

void buffer_vprintf(Buffer *buffer, const char *format, va_list va) {
  int n;
  va_list va2;
  size_t size;
  size = buffer->capacity - buffer->size;
  if (size <= 0) {
    buffer->capacity <<= 1;
    buffer->data = reallocate(buffer->data, buffer->capacity);
  }
  while (1) {
    uint8_t *dest = buffer->data + buffer->size;
    va_copy(va2, va);
    n = vsnprintf((char *) dest, size, format, va2);
    va_end(va2);
    if (n < 0) {
      return;
    }
    if (n < size) {
      buffer->size += n;
      break;
    }
    size = n + 1;
    while (size + buffer->size > buffer->capacity) {
      buffer->capacity <<= 1;
    }
    buffer->data = reallocate(buffer->data, buffer->capacity);
  }
}

void buffer_printf(Buffer *buffer, const char *format, ...) {
  va_list va;
  va_start(va, format);
  buffer_vprintf(buffer, format, va);
  va_end(va);
}

size_t get_line_in_file(int line, char **output, FILE *f) {
  int lines = 1;
  size_t length = 0;
  long offset = 0;
  int c = getc(f);
  while (c != EOF) {
    if (line == lines) {
      if (c == '\n') {
        break;
      }
      length++;
    } else if (c == '\n') {
      lines++;
      offset = ftell(f);
    }
    c = getc(f);
  }
  if (length == 0) {
    *output = NULL;
    return 0;
  }
  char *line_buf = allocate(length + 1);
  fseek(f, offset, SEEK_SET);
  size_t r = fread(line_buf, 1, length, f);
  line_buf[r] = 0;
  *output = line_buf;
  return r;
}

void print_error_line(const char *file_name, Pos start, Pos end) {
  FILE *f = fopen(file_name, "r");
  if (!f) {
    return;
  }
  char *line;
  size_t line_length = get_line_in_file(start.line, &line, f);
  fclose(f);
  if (!line) {
    return;
  }
  int line_num_length = fprintf(stderr, "% 5d | ", start.line);
  if (start.column <= line_length) {
    fwrite(line, 1, start.column - 1, stderr);
    fprintf(stderr, SGR_BOLD_RED);
    if ((start.line == end.line && end.column - start.column > 1) || start.line < end.line) {
      size_t length = start.line == end.line ? end.column - start.column : line_length - start.column + 1;
      fwrite(line + start.column - 1, 1, length, stderr);
      fprintf(stderr, SGR_RESET);
      if (start.column + length <= line_length) {
        fwrite(line + start.column + length - 1, 1, line_length - start.column - length + 1, stderr);
      }
    } else {
      fwrite(line + start.column - 1, 1, 1, stderr);
      fprintf(stderr, SGR_RESET);
      if (start.column < line_length) {
        fwrite(line + start.column, 1, line_length - start.column, stderr);
      }
    }
    fprintf(stderr, "\n");
  } else {
    fprintf(stderr, "%s\n", line);
    free(line);
    return;
  }
  free(line);
  for (size_t i = 1; i < start.column + line_num_length; i++) {
    fprintf(stderr, " ");
  }
  fprintf(stderr, SGR_BOLD_RED);
  if ((start.line == end.line && end.column - start.column > 1) || start.line < end.line) {
    size_t length = start.line == end.line ? end.column - start.column : line_length - start.column + 1;
    for (size_t i = 0; i < length; i++) {
      fprintf(stderr, "~");
    }
  } else {
    fprintf(stderr, "^");
  }
  fprintf(stderr, SGR_RESET "\n");
}

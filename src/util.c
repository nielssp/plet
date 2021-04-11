/* Plet
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "util.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#if defined(_WIN32)
#include <io.h>
#endif

void *allocate(size_t size) {
  void *p = malloc(size);
  if (!p) {
    fprintf(stderr, ERROR_LABEL "allocation of %zd bytes failed!" SGR_RESET "\n", size);
    exit(-1);
  }
  return p;
}

void *reallocate(void *old, size_t size) {
  void *new = realloc(old, size);
  if (!new) {
    fprintf(stderr, ERROR_LABEL "allocation of %zd bytes failed!" SGR_RESET "\n", size);
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
    void *new =  arena_allocate(size, arena);
    memcpy(new, old, old_size);
    return new;
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

void buffer_append_bytes(Buffer *buffer, const uint8_t *bytes, size_t size) {
  if (!size) {
    return;
  }
  size_t new_size = buffer->size + size;
  if (new_size > buffer->capacity) {
    while (new_size > buffer->capacity) {
      buffer->capacity <<= 1;
    }
    buffer->data = reallocate(buffer->data, buffer->capacity);
  }
  memcpy(buffer->data + buffer->size, bytes, size);
  buffer->size = new_size;
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

char *combine_paths(const char *path1, const char *path2) {
  size_t length1 = strlen(path1);
  size_t length2 = strlen(path2);
  size_t combined_length = length1 + length2 + 2;
  char *combined_path = allocate(combined_length);
  memcpy(combined_path, path1, length1);
  if (path1[length1 - 1] != PATH_SEP) {
    combined_path[length1++] = PATH_SEP;
  }
  memcpy(combined_path + length1, path2, length2);
  combined_path[length1 + length2] = '\0';
  return combined_path;
}

time_t get_mtime(const char *path) {
  struct stat stat_buffer;
  if (stat(path, &stat_buffer) == 0) {
    return stat_buffer.st_mtime;
  }
  return 0;
}

int is_dir(const char *path) {
  struct stat stat_buffer;
  return stat(path, &stat_buffer) == 0 && S_ISDIR(stat_buffer.st_mode);
}

int copy_file(const char *src_path, const char *dest_path) {
  FILE *src = fopen(src_path, "r");
  if (!src) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "copy error: %s" SGR_RESET "\n", src_path, strerror(errno));
    return 0;
  }
  FILE *dest = fopen(dest_path, "w");
  int status = 1;
  if (!dest) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "copy error: %s" SGR_RESET "\n", dest_path, strerror(errno));
    status = 0;
  } else {
    char *buffer = allocate(8192);
    size_t n;
    do {
      n = fread(buffer, 1, 8192, src);
      if (fwrite(buffer, 1, n, dest) != n) {
        fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "copy error: %s" SGR_RESET "\n", dest_path, strerror(errno));
        status = 0;
        break;
      }
    } while (n == 8192);
    if (status && !feof(src)) {
      fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "copy error: %s" SGR_RESET "\n", src_path, strerror(errno));
      status = 0;
    }
    free(buffer);
    fclose(dest);
  }
  fclose(src);
  if (status) {
    struct stat stat_buffer;
    if (stat(src_path, &stat_buffer) == 0) {
      struct utimbuf utime_buffer;
      utime_buffer.actime = stat_buffer.st_atime;
      utime_buffer.modtime = stat_buffer.st_mtime;
      utime(dest_path, &utime_buffer);
    }
  }
  return status;
}

static int check_dir(const char *path) {
  struct stat stat_buffer;
  if (stat(path, &stat_buffer) == 0 && S_ISDIR(stat_buffer.st_mode)) {
    return 1;
  }
#if defined(_WIN32)
  if (_mkdir(path) == 0) {
    return 1;
  }
#else
  if (mkdir(path, 0777) == 0) {
    return 1;
  }
#endif
  fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "directory creation failed: %s" SGR_RESET "\n", path, strerror(errno));
  return 0;
}

int mkdir_rec(const char *path) {
  size_t length = strlen(path);
  char *buffer = allocate(length + 1);
  char *p;
  memcpy(buffer, path, length + 1);
  if (buffer[length - 1] == PATH_SEP) {
    buffer[length - 1] = 0;
  }
  for (p = buffer + 1; *p; p++) {
    if (*p == PATH_SEP) {
      *p = 0;
      if (!check_dir(buffer)) {
        free(buffer);
        return 0;
      }
      *p = PATH_SEP;
    }
  }
  if (!check_dir(buffer)) {
    free(buffer);
    return 0;
  }
  free(buffer);
  return 1;
}

int delete_dir(const Path *path) {
  DIR *dir = opendir(path->path);
  int status = 1;
  if (dir) {
    struct dirent *file;
    while ((file = readdir(dir))) {
      if (file->d_name[0] != '.') {
        Path *subpath = path_append(path, file->d_name);
        if (!is_dir(subpath->path)) {
          if (unlink(subpath->path)) {
            status = 0;
            fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "delete error: %s" SGR_RESET "\n", subpath->path, strerror(errno));
          }
        } else {
          status = status && delete_dir(subpath);
        }
        delete_path(subpath);
      }
    }
    closedir(dir);
    if (status && rmdir(path->path)) {
      status = 0;
      fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "delete error: %s" SGR_RESET "\n", path->path, strerror(errno));
    }
  } else {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "unable to read dir: %s" SGR_RESET "\n", path->path, strerror(errno));
  }
  return status;
}

Path *create_path(const char *path_bytes, int32_t length) {
  if (length < 0) {
    length = strlen(path_bytes);
  }
  Path *path = allocate(sizeof(Path) + length + 1);
  if (!length) {
    path->size = 0;
    path->path[0] = '\0';
    return path;
  }
  int32_t root_size = 0;
  int32_t i = 0;
#if defined(_WIN32)
  if (length >= 3 && path_bytes[1] == ':' && (path_bytes[2] == '\\' || path_bytes[2] == '/')) {
    root_size = 3;
    path->path[0] = path_bytes[0];
    path->path[1] = ':';
    path->path[2] = '\\';
    i += 3;
  } else if (length >= 2 && ((path_bytes[0] == '\\' && path_bytes[1] == '\\')
      || (path_bytes[0] == '/' && path_bytes[1] == '/'))) {
    root_size = 2;
    path->path[0] = '\\';
    path->path[1] = '\\';
    i += 2;
  } else if (length >= 1 && (path_bytes[0] == '\\' || path_bytes[0] == '/')) {
    root_size = 1;
    path->path[0] = '\\';
    i += 1;
  }
#else
  if (length >= 1 && path_bytes[0] == '/') {
    root_size = 1;
    path->path[0] = '/';
    i += 1;
  }
#endif
  path->size = i;
  while (1) {
    while (i < length && IS_PATH_SEP(path_bytes[i])) {
      i++;
    }
    if (i >= length) {
      break;
    }
    int32_t j = i;
    while (j < length && !IS_PATH_SEP(path_bytes[j])) {
      j++;
    }
    int32_t component_length = j - i;
    if (component_length == 2 && path_bytes[i] == '.' && path_bytes[i + 1] == '.' && (root_size
          || (path->size  && !((path->size == 2 || (path->size > 2 && path->path[path->size - 3] == PATH_SEP))
              && memcmp(path->path + path->size - 2, "..", 2) == 0)))) {
      while (path->size > root_size) {
        path->size--;
        if (IS_PATH_SEP(path->path[path->size])) {
          break;
        }
      }
    } else if (component_length != 1 || path_bytes[i] != '.') {
      if (path->size > root_size) {
        path->path[path->size++] = PATH_SEP;
      }
      memcpy(path->path + path->size, path_bytes + i, component_length);
      path->size += component_length;
    }
    i = j;
  }
  path->path[path->size] = '\0';
  return path;
}

Path *copy_path(const Path *path) {
  Path *path_copy = allocate(sizeof(Path) + path->size + 1);
  memcpy(path_copy, path, sizeof(Path) + path->size + 1);
  return path_copy;
}

void delete_path(Path *path) {
  free(path);
}

Path *get_cwd_path(void) {
  char *cwd = getcwd(NULL, 0);
  Path *path = create_path(cwd, -1);
  free(cwd);
  return path;
}

int path_is_absolute(const Path *path) {
#if defined(_WIN32)
  if (path->size >= 3 && path->path[1] == ':' && path->path[2] == '\\') {
    return 3;
  } else if (path->size >= 2 && path->path[0] == '\\' && path->path[1] == '\\') {
    return 2;
  } else if (path->size >= 1 && path->path[0] == '\\') {
    return 1;
  }
#else
  if (path->size >= 1 && path->path[0] == '/') {
    return 1;
  }
#endif
  return 0;
}

int path_is_descending(const Path *path) {
  return !((path->size == 2 || (path->size > 2 && path->path[2] == PATH_SEP))
    && memcmp(path->path, "..", 2) == 0);
}

Path *path_get_parent(const Path *path) {
  int root_size = path_is_absolute(path);
  if (path->size == root_size) {
    if (root_size) {
      return copy_path(path);
    }
    return create_path("..", 2);
  }
  if ((path->size == 2 || (path->size > 2 && path->path[path->size - 3] == PATH_SEP))
      && memcmp(path->path + path->size - 2, "..", 2) == 0) {
    Path *parent = allocate(sizeof(Path) + path->size + 4);
    parent->size = path->size + 3;
    memcpy(parent->path, path->path, path->size);
    parent->path[path->size] = PATH_SEP;
    memcpy(parent->path + path->size + 1, "..", 3);
    return parent;
  }
  int32_t size = path->size;
  while (size > root_size) {
    size--;
    if (path->path[size] == PATH_SEP) {
      break;
    }
  }
  Path *parent = allocate(sizeof(Path) + size + 1);
  parent->size = size;
  memcpy(parent->path, path->path, size);
  parent->path[size] = '\0';
  return parent;
}

const char *path_get_name(const Path *path) {
  int root_size = path_is_absolute(path);
  if (path->size == root_size) {
    return path->path;
  }
  int32_t size = path->size;
  while (size > root_size) {
    size--;
    if (path->path[size] == PATH_SEP) {
      size++;
      break;
    }
  }
  return path->path + size;
}

const char *path_get_extension(const Path *path) {
  const char *name = path_get_name(path);
  char *extension = strrchr(name, '.');
  if (extension) {
    return extension + 1;
  }
  return "";
}

char *path_get_lowercase_extension(const Path *path) {
  const char *extension = path_get_extension(path);
  size_t length = strlen(extension);
  char *lowercase = allocate(length + 1);
  for (size_t i = 0; i < length; i++) {
    lowercase[i] = tolower(extension[i]);
  }
  lowercase[length] = '\0';
  return lowercase;
}

Path *path_join(const Path *path1, const Path *path2, int path1_is_root) {
  int32_t path2_root_size = path_is_absolute(path2);
  if (!path1_is_root && path2_root_size) {
    return copy_path(path2);
  }
  Path *result = allocate(sizeof(Path) + path1->size + path2->size + 2);
  result->size = path1->size;
  memcpy(result->path, path1->path, path1->size);
  int32_t root_size = path_is_absolute(path1);
  int32_t i = path2_root_size;
  while (1) {
    while (i < path2->size && path2->path[i] == PATH_SEP) {
      i++;
    }
    if (i >= path2->size) {
      break;
    }
    int32_t j = i;
    while (j < path2->size && path2->path[j] != PATH_SEP) {
      j++;
    }
    int32_t component_length = j - i;
    if (component_length == 2 && path2->path[i] == '.' && path2->path[i + 1] == '.' && (root_size
          || ((result->size == 2 || (result->size > 2 && result->path[result->size - 3] == PATH_SEP))
            && memcmp(result->path + result->size - 2, "..", 2) != 0))) {
      if (!path1_is_root) {
        while (result->size > root_size) {
          result->size--;
          if (result->path[result->size] == PATH_SEP) {
            break;
          }
        }
      }
    } else {
      if (result->size > root_size) {
        result->path[result->size++] = PATH_SEP;
      }
      memcpy(result->path + result->size, path2->path + i, component_length);
      result->size += component_length;
    }
    i = j;
  }
  result->path[result->size] = '\0';
  return result;
}

Path *path_append(const Path *path, const char *component) {
  size_t component_size = strlen(component);
  if (!component_size) {
    return copy_path(path);
  }
  Path *result = allocate(sizeof(Path) + path->size + component_size + 2);
  result->size = path->size;
  memcpy(result->path, path->path, path->size);
  int32_t root_size = path_is_absolute(path);
  if (root_size == path->size) {
    result->size = path->size + component_size;
    memcpy(result->path + path->size, component, component_size);
  } else {
    result->size = path->size + component_size + 1;
    result->path[path->size] = PATH_SEP;
    memcpy(result->path + path->size + 1, component, component_size);
  }
  result->path[result->size] = '\0';
  return result;
}

static int32_t get_end_of_component(const char *path, int32_t offset, int32_t size) {
  while (offset < size && path[offset] != PATH_SEP) {
    offset++;
  }
  return offset;
}

Path *path_get_relative(const Path *start, const Path *end) {
  int32_t start_root_size = path_is_absolute(start);
  int32_t end_root_size = path_is_absolute(end);
  if (start_root_size != end_root_size || (start_root_size && start->path[0] != end->path[0])) {
    return NULL;
  }
  Buffer buffer = create_buffer(end->size > start->size ? end->size : start->size);
  int32_t i = start_root_size;
  while (i < start->size && i < end->size) {
    int32_t start_j = get_end_of_component(start->path, i, start->size);
    int32_t end_j = get_end_of_component(end->path, i, end->size);
    if (start_j != end_j || memcmp(start->path + i, end->path + i, start_j - i) != 0) {
      break;
    }
    i = start_j + 1;
  }
  int32_t end_offset = i;
  while (i < start->size) {
    i = get_end_of_component(start->path, i, start->size);
    if (buffer.size) {
      buffer_put(&buffer, PATH_SEP);
    }
    buffer_printf(&buffer, "..");
    i++;
  }
  if (end_offset < end->size) {
    if (buffer.size) {
      buffer_put(&buffer, PATH_SEP);
    }
    buffer_printf(&buffer, "%.*s", end->size - end_offset, end->path + end_offset);
  }
  buffer_put(&buffer, '\0');
  Path *path = allocate(sizeof(Path) + buffer.size);
  path->size = buffer.size - 1;
  memcpy(path->path, buffer.data, buffer.size);
  delete_buffer(buffer);
  return path;
}

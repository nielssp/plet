/* Plet
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#define _GNU_SOURCE
#include "server.h"

#include "datetime.h"
#include "module.h"
#include "sitemap.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
  SymbolMap *symbol_map;
  ModuleMap *modules;
  Path *src_root;
  Path *dist_root;
  Env *env;
} ServerInfo;

static void write_buffer(int fd, Buffer buffer) {
  write(fd, buffer.data, buffer.size);
}

static void write_server_headers(int cfd, int status_code, const char *status) {
  Buffer buffer = create_buffer(32);
  buffer_printf(&buffer, "HTTP/1.1 %d %s\r\nConnection: close\r\nAllow: GET\r\n", status_code, status);
  write_buffer(cfd, buffer);
  buffer.size = 0;
  buffer_printf(&buffer, "Date: ");
  if (rfc2822_date(time(NULL), &buffer)) {
    buffer_printf(&buffer, "\r\n");
    write_buffer(cfd, buffer);
  }
  delete_buffer(buffer);
}

static void not_found_response(int cfd) {
  write_server_headers(cfd, 404, "Not Found");
  Buffer response = create_buffer(32);
  buffer_printf(&response, "Content-Type: text/plain\r\n\r\nNot Found");
  write_buffer(cfd, response);
  delete_buffer(response);
}

static void internal_server_error_response(int cfd, const char *error) {
  write_server_headers(cfd, 500, "Internal Server Error");
  Buffer response = create_buffer(32);
  buffer_printf(&response, "Content-Type: text/plain\r\n\r\n%s", error);
  write_buffer(cfd, response);
  delete_buffer(response);
}

static const char *get_mime_type(const char *file_extension) {
  if (strcmp(file_extension, "") == 0) {
    return "text/html";
  } else if (strcmp(file_extension, "htm") == 0) {
    return "text/html";
  } else if (strcmp(file_extension, "html") == 0) {
    return "text/html";
  } else if (strcmp(file_extension, "css") == 0) {
    return "text/css";
  } else if (strcmp(file_extension, "js") == 0) {
    return "text/javascript";
  }
  return "text/plain";
}

static void ok_response(int cfd, const char *file_extension, String *content) {
  write_server_headers(cfd, 200, "OK");
  Buffer response = create_buffer(32);
  buffer_printf(&response, "Content-Length: %zd\r\n", content->size);
  buffer_printf(&response, "Content-Type: %s\r\n\r\n", get_mime_type(file_extension));
  write_buffer(cfd, response);
  delete_buffer(response);
  write(cfd, content->bytes, content->size);
}

static void file_response(int cfd, const Path *path) {
  FILE *file = fopen(path->path, "r");
  if (!file) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "%s" SGR_RESET "\n", path->path, strerror(errno));
    not_found_response(cfd);
    return;
  }
  write_server_headers(cfd, 200, "OK");
  Buffer response = create_buffer(32);
  buffer_printf(&response, "Content-Type: %s\r\n\r\n", get_mime_type(path_get_extension(path)));
  write_buffer(cfd, response);
  delete_buffer(response);
  char *buffer = allocate(4096);
  size_t n;
  do {
    n = fread(buffer, 1, 4096, file);
    write(cfd, buffer, n);
  } while (n == 4096);
  if (!feof(file)) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "read error: %s" SGR_RESET "\n", path->path, strerror(errno));
  }
  free(buffer);
}

static char *get_next_token(char *buffer, ssize_t n, ssize_t *offset) {
  while (*offset < n && isspace(buffer[*offset])) {
      (*offset)++;
  }
  char *token = buffer + *offset;
  while (*offset < n && !isspace(buffer[*offset])) {
      (*offset)++;
  }
  if (*offset >= n) {
    return NULL;
  }
  buffer[*offset] = '\0';
  (*offset)++;
  return token;
}

static Object *find_in_site_map(const Path *dist_path, Env *env) {
  Value site_map;
  if (!env_get_symbol("SITE_MAP", &site_map, env) || site_map.type != V_ARRAY) {
    fprintf(stderr, ERROR_LABEL "SITE_MAP is missign or not an object" SGR_RESET "\n");
    return NULL;
  }
  Path *index_path = path_append(dist_path, "index.html");
  Value dist = path_to_string(dist_path, env->arena);
  Value index = path_to_string(index_path, env->arena);
  delete_path(index_path);
  for (size_t i = 0; i < site_map.array_value->size; i++) {
    Value page = site_map.array_value->cells[i];
    Value dest;
    if (page.type == V_OBJECT && object_get_symbol(page.object_value, "dest", &dest)) {
      if (equals(dest, dist) || equals(dest, index)) {
        return page.object_value;
      }
    }
  }
  return NULL;
}

static void handle_request(int cfd, ServerInfo *info) {
  char *buffer = allocate(4096);
  ssize_t n = recv(cfd, buffer, 4096, 0);
  if (n == -1) {
    fprintf(stderr, ERROR_LABEL "request failed: %s" SGR_RESET "\n", strerror(errno));
  } else if (n == 0) {
    fprintf(stderr, ERROR_LABEL "client closed connection" SGR_RESET "\n");
  } else {
    ssize_t i = 0;
    char *method = get_next_token(buffer, n, &i);
    if (!method) {
      fprintf(stderr, ERROR_LABEL "invalid request" SGR_RESET "\n");
    } else if (strcmp(method, "GET") == 0) {
      char *uri = get_next_token(buffer, n, &i);
      if (!uri) {
        fprintf(stderr, ERROR_LABEL "invalid request" SGR_RESET "\n");
      } else {
        Path *path = create_path(uri, -1);
        Path *dist_path = get_dist_path(path, info->env);
        delete_path(path);
        if (dist_path) {
          Object *page = find_in_site_map(dist_path, info->env);
          if (page) {
            fprintf(stderr, "Compiling %s\n", dist_path->path);
            Env *template_env = NULL;
            Value output = compile_page_object(page, info->env, &template_env);
            if (output.type == V_STRING) {
              ok_response(cfd, path_get_extension(dist_path), output.string_value);
            } else {
              internal_server_error_response(cfd, "Invalid template output");
            }
            if (template_env) {
              delete_template_env(template_env);
            }
          } else {
            file_response(cfd, dist_path);
          }
          delete_path(dist_path);
        } else {
          not_found_response(cfd);
        }
      }
    } else {
      write_server_headers(cfd, 405, "Method Not Allowed");
      Buffer response = create_buffer(32);
      buffer_printf(&response, "Content-Type: text/plain\r\n\r\nMethod Not Allowed");
      write_buffer(cfd, response);
      delete_buffer(response);
    }
  }
  free(buffer);
  shutdown(cfd, SHUT_RDWR);
  close(cfd);
}

int serve(GlobalArgs args) {
  ServerInfo info;
  info.src_root = find_project_root();
  if (!info.src_root) {
    fprintf(stderr, ERROR_LABEL "index.plet not found" SGR_RESET "\n");
    return 1;
  }
  info.dist_root = path_append(info.src_root, "dist");
  int status = 0;
  info.symbol_map = create_symbol_map();
  info.modules = create_module_map();
  add_system_modules(info.modules);
  info.env = eval_index(info.src_root, info.modules, info.symbol_map);
  if (info.env) {
    struct addrinfo hints, *res, *p;
    memset (&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(NULL, args.port, &hints, &res) != 0) {
      fprintf(stderr, ERROR_LABEL "getaddrinfo error: %s" SGR_RESET "\n", strerror(errno));
      status = 1;
    } else {
      int sfd;
      for (p = res; p != NULL; p = p->ai_next) {
        int option = 1;
        sfd = socket(p->ai_family, p->ai_socktype, 0);
        setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
        if (sfd == -1) {
          continue;
        }
        if (bind(sfd, p->ai_addr, p->ai_addrlen) == 0) {
          break;
        }
      }
      if (p == NULL) {
        fprintf(stderr, ERROR_LABEL "could not bind to port %s: %s" SGR_RESET "\n", args.port, strerror(errno));
        status = 1;
      } else {
        freeaddrinfo(res);
        if (listen (sfd, 1000000) != 0) {
          fprintf(stderr, ERROR_LABEL "could not listen to port %s: %s" SGR_RESET "\n", args.port, strerror(errno));
          status = 1;
        } else {
          signal(SIGPIPE, SIG_IGN);
          fprintf(stderr, INFO_LABEL "server listening on http://localhost:%s/" SGR_RESET "\n", args.port);
          while (1) {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            int cfd = accept(sfd, (struct sockaddr *) &client_addr, &client_addr_len);
            handle_request(cfd, &info);
          }
        }
      }
    }
    delete_arena(info.env->arena);
  }
  delete_symbol_map(info.symbol_map);
  delete_module_map(info.modules);
  delete_path(info.src_root);
  delete_path(info.dist_root);
  return status;
}

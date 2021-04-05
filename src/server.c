/* Plet
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#define _GNU_SOURCE
#include "server.h"

#include "datetime.h"
#include "module.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
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
        fprintf(stderr, "%s %s\n", method, uri);
        write_server_headers(cfd, 404, "Not Found");
        Buffer response = create_buffer(32);
        buffer_printf(&response, "Content-Type: text/plain\r\n\r\nNot Found");
        write_buffer(cfd, response);
        delete_buffer(response);
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
  delete_symbol_map(info.symbol_map);
  delete_module_map(info.modules);
  delete_path(info.src_root);
  delete_path(info.dist_root);
  return status;
}

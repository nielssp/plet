/* Plet
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "lipsum.h"

#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

const char *words[] = {
  "a", "ac", "accumsan", "ad", "adipiscing", "aenean", "aenean", "aliquam", "aliquam", "aliquet", "amet", "ante",
  "aptent", "arcu", "at", "auctor", "augue", "bibendum", "blandit", "class", "commodo", "condimentum", "congue",
  "consectetur", "consequat", "conubia", "convallis", "cras", "cubilia", "curabitur", "curabitur", "curae", "cursus",
  "dapibus", "diam", "dictum", "dictumst", "dolor", "donec", "donec", "dui", "duis", "egestas", "eget", "eleifend",
  "elementum", "elit", "enim", "erat", "eros", "est", "et", "etiam", "etiam", "eu", "euismod", "facilisis", "fames",
  "faucibus", "felis", "fermentum", "feugiat", "fringilla", "fusce", "gravida", "habitant", "habitasse", "hac",
  "hendrerit", "himenaeos", "iaculis", "id", "imperdiet", "in", "inceptos", "integer", "interdum", "ipsum", "justo",
  "lacinia", "lacus", "laoreet", "lectus", "leo", "libero", "ligula", "litora", "lobortis", "lorem", "luctus",
  "maecenas", "magna", "malesuada", "massa", "mattis", "mauris", "metus", "mi", "molestie", "mollis", "morbi", "nam",
  "nec", "neque", "netus", "nibh", "nisi", "nisl", "non", "nostra", "nulla", "nullam", "nunc", "odio", "orci", "ornare",
  "pellentesque", "per", "pharetra", "phasellus", "placerat", "platea", "porta", "porttitor", "posuere", "potenti",
  "praesent", "pretium", "primis", "proin", "pulvinar", "purus", "quam", "quis", "quisque", "quisque", "rhoncus",
  "risus", "rutrum", "sagittis", "sapien", "scelerisque", "sed", "sem", "semper", "senectus", "sit", "sociosqu",
  "sodales", "sollicitudin", "suscipit", "suspendisse", "taciti", "tellus", "tempor", "tempus", "tincidunt", "torquent",
  "tortor", "tristique", "turpis", "ullamcorper", "ultrices", "ultricies", "urna", "ut", "ut", "varius", "vehicula",
  "vel", "velit", "venenatis", "vestibulum", "vitae", "vivamus", "viverra", "volutpat", "vulputate"
};

const size_t num_words = sizeof(words) / sizeof(char *);

static char *lipsum_words(int length) {
  Buffer buffer = create_buffer(0);
  for (int i = 0; i < length; i++) {
    if (i) {
      if (rand() % 100 < 10) {
        buffer_put(&buffer, ',');
      }
      buffer_put(&buffer, ' ');
    }
    buffer_printf(&buffer, "%s", words[rand() % num_words]);
  }
  buffer_put(&buffer, '\0');
  return (char *) buffer.data;
}

static char *lipsum_paragraph(int sentences) {
  Buffer buffer = create_buffer(0);
  for (int i = 0; i < sentences; i++) {
    if (i) {
      buffer_put(&buffer, ' ');
    }
    char *sentence = lipsum_words(rand() % 30 + 5);
    sentence[0] = toupper(sentence[0]);
    buffer_printf(&buffer, "%s.", sentence);
    free(sentence);
  }
  buffer_put(&buffer, '\0');
  return (char *) buffer.data;
}

static char *create_file_name(const char *title) {
  Buffer buffer = create_buffer(strlen(title) + 4);
  while (*title) {
    char byte = *title;
    if (byte >= 'a' && byte <= 'z') {
      buffer_put(&buffer, byte);
    } else if (byte == ' ') {
      buffer_put(&buffer, '-');
    }
    title++;
  }
  buffer_printf(&buffer, ".md");
  buffer_put(&buffer, '\0');
  return (char *) buffer.data;
}

int lipsum(GlobalArgs args) {
  srand(time(NULL) ^ getpid());
  char *title = lipsum_words(rand() % 6 + 1);
  FILE *out = stdout;
  if (args.argc >= 1) {
    Path *dir = create_path(args.argv[0], -1);
    char *name = create_file_name(title);
    Path *file = path_append(dir, name);
    free(name);
    delete_path(dir);
    out = fopen(file->path, "w");
    if (!out) {
      fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "%s" SGR_RESET "\n", file->path, strerror(errno));
      delete_path(file);
      free(title);
      return 1;
    }
    delete_path(file);
  }
  fprintf(out, "{\n");
  time_t published = time(NULL) - rand() % (5 * 365 * 24 * 60 * 60);
  struct tm *t = localtime(&published);
  if (t) {
    char date[26];
    if (strftime(date, sizeof(date), "%Y-%m-%d %H:%M", t)) {
      fprintf(out, "  published: '%s',\n", date);
    }
  }
  fprintf(out, "  tags: [");
  int tags = rand() % 5;
  for (int i = 0; i < tags; i++) {
    if (i) {
      fprintf(out, ", ");
    }
    char *tag = lipsum_words(1);
    fprintf(out, "'%s'", tag);
    free(tag);
  }
  fprintf(out, "],\n");
  fprintf(out, "}\n");
  fprintf(out, "\n");
  title[0] = toupper(title[0]);
  fprintf(out, "# %s\n", title);
  free(title);
  int paragraphs = rand() % 3 + 1;
  for (int i = 0; i < paragraphs; i++) {
    char *paragraph = lipsum_paragraph(rand() % 6 + 1);
    fprintf(out, "\n%s\n", paragraph);
    free(paragraph);
  }
  if (out != stdout) {
    fclose(out);
  }
  return 0;
}

/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "lipsum.h"

#include "util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

void lipsum(void) {
  srand(time(NULL));
  printf("{\n");
  time_t published = time(NULL) - rand() % (5 * 365 * 24 * 60 * 60);
  struct tm *t = localtime(&published);
  if (t) {
    char date[26];
    if (strftime(date, sizeof(date), "%Y-%m-%d %H:%M", t)) {
      printf("  published: '%s',\n", date);
    }
  }
  printf("  tags: [");
  int tags = rand() % 5;
  for (int i = 0; i < tags; i++) {
    if (i) {
      printf(", ");
    }
    char *tag = lipsum_words(1);
    printf("'%s'", tag);
    free(tag);
  }
  printf("],\n");
  printf("}\n");
  printf("\n");
  char *title = lipsum_words(rand() % 6 + 1);
  title[0] = toupper(title[0]);
  printf("# %s\n", title);
  free(title);
  int paragraphs = rand() % 3 + 1;
  for (int i = 0; i < paragraphs; i++) {
    char *paragraph = lipsum_paragraph(rand() % 6 + 1);
    printf("\n%s\n", paragraph);
    free(paragraph);
  }
}

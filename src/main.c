/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "collections.h"
#include "core.h"
#include "datetime.h"
#include "interpreter.h"
#include "parser.h"
#include "reader.h"
#include "strings.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

const char *short_options = "hvo:";

const struct option long_options[] = {
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'v'},
  {0, 0, 0, 0}
};

static void describe_option(const char *short_option, const char *long_option, const char *description) {
  printf("  -%-14s --%-18s %s\n", short_option, long_option, description);
}

int main(int argc, char *argv[]) {
  int opt;
  int option_index;
  while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
    switch (opt) {
      case 'h':
        printf("usage: %s [options] infile\n", argv[0]);
        puts("options:");
        describe_option("h", "help", "Show help.");
        describe_option("v", "version", "Show version information.");
        return 0;
      case 'v':
        puts("tsc 0.1.0");
        return 0;
    }
  }
  if (optind >= argc) {
    printf("usage: %s [options] infile\n", argv[0]);
    return 1;
  }
  char *infile = argv[optind];
  FILE *in = fopen(infile, "r");
  if (!in) {
    fprintf(stderr, "error: %s: %s\n", infile, strerror(errno));
    return errno;
  }
  SymbolMap *symbol_map = create_symbol_map();
  Reader *reader = open_reader(in, infile, symbol_map);
  TokenStream tokens = read_all(reader, 0);
  Module *module = parse(tokens, infile);
  close_reader(reader);
  fclose(in);

  ModuleMap *modules = create_module_map();
  add_module(module, modules);

  Arena *arena = create_arena();
  Env *env = create_env(arena, modules, symbol_map);
  import_core(env);
  import_strings(env);
  import_collections(env);
  import_datetime(env);
  Value output = interpret(*module->root, env);
  if (output.type == V_STRING) {
    for (size_t i = 0 ; i < output.string_value->size; i++) {
      putchar((char) output.string_value->bytes[i]);
    }
  }
  delete_arena(arena);

  delete_module_map(modules);
  delete_symbol_map(symbol_map);
  return 0;
}

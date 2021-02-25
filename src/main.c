/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "build.h"
#include "collections.h"
#include "contentmap.h"
#include "core.h"
#include "datetime.h"
#include "interpreter.h"
#include "parser.h"
#include "reader.h"
#include "sitemap.h"
#include "strings.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char *short_options = "hvo:";

const struct option long_options[] = {
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'v'},
  {0, 0, 0, 0}
};

static void describe_option(const char *short_option, const char *long_option, const char *description) {
  printf("  -%-14s --%-18s %s\n", short_option, long_option, description);
}

static void print_help(const char *program_name) {
  printf("usage: %s [options] <command> [<args>]\n", program_name);
  puts("options:");
  describe_option("h", "help", "Show help.");
  describe_option("v", "version", "Show version information.");
  puts("commands:");
  puts("  build             Build site from index.tss");
  puts("  eval <file>       Evaluate a single source file");
  puts("  init              Create a new site in the current directory");
}

static int eval(GlobalArgs args) {
  if (args.argc < 1) {
    printf("usage: %s eval <file>\n", args.program_name);
    return 1;
  }
  char *infile = args.argv[0];
  FILE *in = fopen(infile, "r");
  if (!in) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "%s" SGR_RESET "\n", infile, strerror(errno));
    return errno;
  }
  SymbolMap *symbol_map = create_symbol_map();
  Reader *reader = open_reader(in, infile, symbol_map);
  TokenStream tokens = read_all(reader, 0);
  if (!reader_errors(reader)) {
    Module *module = parse(tokens, infile);
    close_reader(reader);
    fclose(in);

    if (!module->parse_error) {
      ModuleMap *modules = create_module_map();
      add_module(module, modules);

      Arena *arena = create_arena();
      Env *env = create_env(arena, modules, symbol_map);
      import_core(env);
      import_strings(env);
      import_collections(env);
      import_datetime(env);
      import_sitemap(env);
      import_contentmap(env);
      Value output = interpret(*module->root, env);
      if (output.type == V_STRING) {
        for (size_t i = 0 ; i < output.string_value->size; i++) {
          putchar((char) output.string_value->bytes[i]);
        }
      }
      delete_arena(arena);
      delete_module_map(modules);
    } else {
      delete_module(module);
    }
  } else {
    close_reader(reader);
  }
  delete_symbol_map(symbol_map);
  return 0;
}

static int init(GlobalArgs args) {
  FILE *index = fopen("index.tss", "r");
  if (index) {
    fprintf(stderr, SGR_BOLD "index.tss: " ERROR_LABEL "file exists" SGR_RESET "\n");
    fclose(index);
    return 1;
  }
  index = fopen("index.tss", "w");
  if (!index) {
    fprintf(stderr, SGR_BOLD "index.tss: " ERROR_LABEL "%s" SGR_RESET "\n", strerror(errno));
    return 1;
  }
  // TODO: do more stuff
  fclose(index);
  return 0;
}

int main(int argc, char *argv[]) {
  GlobalArgs args;
  int opt;
  int option_index;
  while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
    switch (opt) {
      case 'h':
        print_help(argv[0]);
        return 0;
      case 'v':
        puts("tsc 0.1.0");
        return 0;
    }
  }
  if (optind >= argc) {
    print_help(argv[0]);
    return 1;
  }
  args.program_name = argv[0];
  char *command = argv[optind];
  args.command_name = command;
  args.argc = argc - optind - 1;
  args.argv = argv + optind + 1;
  if (strcmp(command, "build") == 0) {
    return build(args);
  } else if (strcmp(command, "eval") == 0) {
    return eval(args);
  } else if (strcmp(command, "init") == 0) {
    return init(args);
  } else {
    fprintf(stderr, ERROR_LABEL "unrecognized command: %s" SGR_RESET "\n", command);
    return 1;
  }
}

/* Plet
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "build.h"
#include "collections.h"
#include "contentmap.h"
#include "core.h"
#include "datetime.h"
#include "html.h"
#include "interpreter.h"
#include "lipsum.h"
#include "markdown.h"
#include "parser.h"
#include "reader.h"
#include "server.h"
#include "sitemap.h"
#include "strings.h"

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char *short_options = "hvtp:";

const struct option long_options[] = {
  {"help", no_argument, NULL, 'h'},
  {"version", no_argument, NULL, 'v'},
  {"template", no_argument, NULL, 't'},
  {"port", required_argument, NULL, 'p'},
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
  describe_option("t", "template", "Parse file as a template.");
  describe_option("p", "port", "Port for built-in web server.");
  puts("commands:");
  puts("  build             Build site from index.plet");
  puts("  watch             Build site from index.plet and watch for changes");
  puts("  serve             Serve site (for development/testing purposes)");
  puts("  eval <file>       Evaluate a single source file");
  puts("  init              Create a new site in the current directory");
  puts("  clean             Remove generated files");
  puts("  lipsum [<dir>]    Generate random markdown content");
}

static int eval(GlobalArgs args) {
  if (args.argc < 1) {
    printf("usage: %s eval <file>\n", args.program_name);
    return 1;
  }
  Path *infile = create_path(args.argv[0], -1);
  FILE *in = fopen(infile->path, "r");
  if (!in) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "%s" SGR_RESET "\n", infile->path, strerror(errno));
    delete_path(infile);
    return errno;
  }
  SymbolMap *symbol_map = create_symbol_map();
  Reader *reader = open_reader(in, infile, symbol_map);
  TokenStream tokens = read_all(reader, args.parse_as_template);
  if (!reader_errors(reader)) {
    Module *module = parse(tokens, infile);
    close_reader(reader);
    fclose(in);

    if (!module->user_value.parse_error) {
      ModuleMap *modules = create_module_map();
      add_system_modules(modules);
      add_module(module, modules);

      Env *env = create_user_env(module, modules, symbol_map);
      import_sitemap(env);
      import_contentmap(env);
      import_html(env);
      import_markdown(env);
      Value output = interpret(*module->user_value.root, env).value;
      if (output.type == V_STRING) {
        for (size_t i = 0 ; i < output.string_value->size; i++) {
          putchar((char) output.string_value->bytes[i]);
        }
      }
      delete_arena(env->arena);
      delete_module_map(modules);
    } else {
      delete_module(module);
    }
  } else {
    close_reader(reader);
  }
  delete_symbol_map(symbol_map);
  delete_path(infile);
  return 0;
}

static int init(GlobalArgs args) {
  FILE *index = fopen("index.plet", "r");
  if (index) {
    fprintf(stderr, SGR_BOLD "index.plet: " ERROR_LABEL "file exists" SGR_RESET "\n");
    fclose(index);
    return 1;
  }
  index = fopen("index.plet", "w");
  if (!index) {
    fprintf(stderr, SGR_BOLD "index.plet: " ERROR_LABEL "%s" SGR_RESET "\n", strerror(errno));
    return 1;
  }
  // TODO: do more stuff
  fclose(index);
  return 0;
}

static int clean(GlobalArgs args) {
  Path *root = find_project_root();
  if (!root) {
    fprintf(stderr, ERROR_LABEL "project root not found" SGR_RESET "\n");
    return 1;
  }
  int status = 0;
  Path *dist = path_append(root, "dist");
  if (is_dir(dist->path)) {
    if (!delete_dir(dist)) {
      status = 1;
    }
  }
  delete_path(dist);
  delete_path(root);
  return status;
}

int main(int argc, char *argv[]) {
  GlobalArgs args;
  args.parse_as_template = 0;
  args.port = "6500";
  int opt;
  int option_index;
  while ((opt = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
    switch (opt) {
      case 'h':
        print_help(argv[0]);
        return 0;
      case 'v':
        puts("Plet 0.1.0");
        return 0;
      case 't':
        args.parse_as_template = 1;
        break;
      case 'p':
        args.port = optarg;
        break;
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
  } else if (strcmp(command, "watch") == 0) {
    return watch(args);
  } else if (strcmp(command, "serve") == 0) {
    return serve(args);
  } else if (strcmp(command, "eval") == 0) {
    return eval(args);
  } else if (strcmp(command, "init") == 0) {
    return init(args);
  } else if (strcmp(command, "clean") == 0) {
    return clean(args);
  } else if (strcmp(command, "lipsum") == 0) {
    return lipsum(args);
  } else {
    fprintf(stderr, ERROR_LABEL "unrecognized command: %s" SGR_RESET "\n", command);
    return 1;
  }
}

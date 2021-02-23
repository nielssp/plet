/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "build.h"

#include "collections.h"
#include "core.h"
#include "datetime.h"
#include "interpreter.h"
#include "parser.h"
#include "reader.h"
#include "sitemap.h"
#include "strings.h"

#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  char *src_root;
  char *dist_root;
  SymbolMap *symbol_map;
  ModuleMap *modules;
} BuildInfo;

static void import_build_info(BuildInfo *build_info, Env *env) {
  env_def("SRC_ROOT", copy_c_string(build_info->src_root, env->arena), env);
  env_def("DIST_ROOT", copy_c_string(build_info->dist_root, env->arena), env);
}

static int eval_script(FILE *file, const char *file_name, BuildInfo *build_info) {
  Reader *reader = open_reader(file, file_name, build_info->symbol_map);
  TokenStream tokens = read_all(reader, 0);
  if (reader_errors(reader)) {
    close_reader(reader);
    return 0;
  }
  Module *module = parse(tokens, file_name);
  close_reader(reader);
  if (module->parse_error) {
    delete_module(module);
    return 0;
  }
  add_module(module, build_info->modules);

  Arena *arena = create_arena();
  Env *env = create_env(arena, build_info->modules, build_info->symbol_map);
  import_core(env);
  import_strings(env);
  import_collections(env);
  import_datetime(env);
  import_sitemap(env);
  import_build_info(build_info, env);
  env_def("FILE", copy_c_string(file_name, env->arena), env);
  char *file_name_copy = copy_string(file_name);
  char *dir = dirname(file_name_copy);
  env_def("DIR", copy_c_string(dir, env->arena), env);
  free(file_name_copy);
  interpret(*module->root, env);
  delete_arena(arena);
  return 1;
}

int build(GlobalArgs args) {
  char *index_name = "index.tss";
  size_t name_length = strlen(index_name);
  char *cwd = getcwd(NULL, 0);
  size_t length = strlen(cwd);
  char *index_path = allocate(length + 1 + name_length + 1);
  memcpy(index_path, cwd, length);
  index_path[length] = PATH_SEP;
  memcpy(index_path + length + 1, index_name, name_length + 1);
  FILE *index = fopen(index_path, "r");
  if (!index) {
    do {
      length--;
      if (cwd[length] == PATH_SEP) {
        memcpy(index_path + length + 1, index_name, name_length + 1);
        index = fopen(index_path, "r");
        if (index) {
          cwd[length] = '\0';
          break;
        }
      }
    } while (length > 0);
  }
  if (index) {
    fprintf(stderr, INFO_LABEL "building %s" SGR_RESET "\n", index_path);
    BuildInfo build_info;
    build_info.src_root = cwd;
    build_info.dist_root = combine_paths(cwd, "dist");
    if (mkdir_rec(build_info.dist_root)) {
      build_info.symbol_map = create_symbol_map();
      build_info.modules = create_module_map();
      eval_script(index, index_path, &build_info);
      delete_symbol_map(build_info.symbol_map);
      delete_module_map(build_info.modules);
    }
    free(build_info.dist_root);
    fclose(index);
  } else {
    fprintf(stderr, ERROR_LABEL "index.tss not found" SGR_RESET "\n");
  }
  free(index_path);
  free(cwd);
  return 0;
}

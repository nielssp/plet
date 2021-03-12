/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef MODULE_H
#define MODULE_H

#include "ast.h"

typedef struct Module Module;
typedef struct ModuleMap ModuleMap;

typedef enum {
  M_SYSTEM,
  M_USER,
  M_ASSET
} ModuleType;

struct Module {
  ModuleType type;
  char *file_name;
  time_t mtime;
  union {
    /*struct {
      void import_func(Env *);
    } system_value;*/
    struct {
      Node *root;
      int parse_error;
    } user_value;
    struct {
      int width;
      int height;
    } asset_value;
  };
};

Module *create_module(const char *file_name);
void delete_module(Module *module);

ModuleMap *create_module_map(void);
void delete_module_map(ModuleMap *module_map);
Module *get_module(const char *file_name, ModuleMap *module_map);
void add_module(Module *module, ModuleMap *module_map);

#endif

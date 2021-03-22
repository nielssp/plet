/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef MODULE_H
#define MODULE_H

#include "value.h"

Module *create_module(const Path *file_name, ModuleType type);
void delete_module(Module *module);

ModuleMap *create_module_map(void);
void delete_module_map(ModuleMap *module_map);
Module *get_module(const Path *file_name, ModuleMap *module_map);
void add_module(Module *module, ModuleMap *module_map);
void add_system_module(const char *name, void (*import_func)(Env *), ModuleMap *module_map);
void add_system_modules(ModuleMap *module_map);

Path *get_src_path(const Path *path, Env *env);
Module *load_asset_module(const Path *name, Env *env);
Module *load_data_module(const Path *name, Env *env);
Module *load_user_module(const Path *name, Env *env);
Module *load_module(const Path *name, Env *env);

Env *create_user_env(Module *module, ModuleMap *modules, SymbolMap *symbol_map);
Value import_module(Module *module, Env *env);

#endif

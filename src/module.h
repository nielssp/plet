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

Module *load_module(const Path *name, Env *env);

#endif

/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef SITEMAP_H
#define SITEMAP_H

#include "value.h"

void import_sitemap(Env *env);

void notify_output_observers(const Path *path, Env *env);
int compile_pages(Env *env);

#endif


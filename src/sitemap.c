/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "sitemap.h"

static Value add_static(const Tuple *args, Env *env) {
  check_args(1, args, env);
  return nil_value;
}

static Value add_page(const Tuple *args, Env *env) {
  check_args_between(2, 3, args, env);
  return nil_value;
}

static Value paginate(const Tuple *args, Env *env) {
  check_args_between(4, 5, args, env);
  return nil_value;
}

void import_sitemap(Env *env) {
  env_def_fn("add_static", add_static, env);
  env_def_fn("add_page", add_page, env);
  env_def_fn("paginate", paginate, env);
}

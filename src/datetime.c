/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "datetime.h"

#include <time.h>

static Value now(const Tuple *args, Env *env) {
  check_args(0, args, env);
  return create_time(time(NULL));
}

void import_datetime(Env *env) {
  env_def_fn("now", now, env);
}

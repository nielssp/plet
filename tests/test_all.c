/* Plet
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "test.h"

int main(void) {
  run_test_suite(test_hashmap);
  run_test_suite(test_strings);
  run_test_suite(test_util);
  run_test_suite(test_value);
  return 0;
}

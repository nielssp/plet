/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef TEST_H
#define TEST_H

#include "../src/util.h"

#include <assert.h>
#include <stdio.h>

#define run_test(test) \
  printf("  %s...", #test);\
  test();\
  printf(SGR_BOLD_GREEN " [OK]\n" SGR_RESET)

#define run_test_suite(test) \
  printf("Running %s:\n", #test);\
  test();\
  printf("%s: All tests passed\n", #test)

void test_hashmap(void);
void test_util(void);

#endif

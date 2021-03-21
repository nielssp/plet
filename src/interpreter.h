/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "value.h"

typedef enum {
  IR_VALUE,
  IR_RETURN,
  IR_BREAK,
  IR_CONTINUE
} InterpreterResultType;

typedef struct InterpreterResult {
  InterpreterResultType type;
  Value value;
  int64_t level;
} InterpreterResult;

int apply(Value func, const Tuple *args, Value *return_value, Env *env);
InterpreterResult interpret(Node node, Env *env);

#endif

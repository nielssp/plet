/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#define _GNU_SOURCE
#include "datetime.h"

#include <alloca.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <time.h>

typedef struct {
  uint8_t *input;
  size_t length;
} DateParseInput;

static Value now(const Tuple *args, Env *env) {
  check_args(0, args, env);
  return create_time(time(NULL));
}

static int skip_sep(DateParseInput *input, char *chars) {
  if (input->length && strchr(chars, *input->input)) {
    input->input++;
    input->length--;
    return 1;
  }
  return 0;
}

static int parse_int(DateParseInput *input, int max_length) {
  int result = 0;
  while (input->length && isdigit(*input->input) && max_length > 0) {
    result *= 10;
    result += *input->input - '0';
    input->input++;
    input->length--;
    max_length--;
  }
  return result;
}

static time_t parse_iso8601(DateParseInput *input) {
  time_t offset = (time_t) 0;
  int local = 0;
  struct tm t;
  t.tm_year = parse_int(input, 4) - 1900;
  skip_sep(input, "-");
  t.tm_mon = parse_int(input, 2) - 1;
  skip_sep(input, "-");
  t.tm_mday = parse_int(input, 2);
  if (skip_sep(input, "T ")) {
    t.tm_hour = parse_int(input, 2);
    skip_sep(input, ":");
    t.tm_min = parse_int(input, 2);
    skip_sep(input, ":");
    t.tm_sec = parse_int(input, 2);
    if (skip_sep(input, ".")) {
      parse_int(input, 3);
    }
    if (skip_sep(input, "Z")) {
      t.tm_isdst = 0;
    } else if (*input->input == '-' || *input->input == '+') {
      int sign = *input->input == '+' ? 1 : -1;
      t.tm_isdst = 0;
      input->input++;
      input->length--;
      offset = sign * parse_int(input, 2) * 3600;
      skip_sep(input, ":");
      offset += sign * parse_int(input, 2) * 60;
    } else {
      t.tm_isdst = -1;
      local = 1;
    }
  } else {
    t.tm_hour = 0;
    t.tm_min = 0;
    t.tm_sec = 0;
    t.tm_isdst = 0;
  }
  time_t result;
  if (local) {
    result = mktime(&t);
  } else {
    result = timegm(&t);
    result -= offset;
  }
  return result;
}

static int parse_time_value(Value arg, time_t *result) {
  if (arg.type == V_TIME) {
    *result = arg.time_value;
  } else if (arg.type == V_INT) {
    *result = (time_t) arg.int_value;
  } else if (arg.type == V_STRING) {
    DateParseInput input = {arg.string_value->bytes, arg.string_value->size};
    *result = parse_iso8601(&input);
  } else {
    return 0;
  }
  return 1;
}

static Value time_(const Tuple *args, Env *env) {
  check_args(1, args, env);
  time_t t;
  if (parse_time_value(args->values[0], &t)) {
    return create_time(t);
  } else {
    arg_error(0, "time|int|string", args, env);
    return nil_value;
  }
}

static Value date(const Tuple *args, Env *env) {
  check_args(2, args, env);
  time_t arg;
  if (!parse_time_value(args->values[0], &arg)) {
    arg_error(0, "time|int|string", args, env);
    return nil_value;
  }
  Value format = args->values[1];
  if (format.type != V_STRING) {
    arg_type_error(1, V_STRING, args, env);
    return nil_value;
  }
  char *format_string = alloca(format.string_value->size + 1);
  memcpy(format_string, format.string_value->bytes, format.string_value->size);
  format_string[format.string_value->size] = '\0';
  struct tm *t = localtime(&arg);
  // TODO: improve
  char output[100];
  if (t) {
    size_t size = strftime(output, sizeof(output), format_string, t);
    if (size) {
      return create_string((uint8_t *) output, size, env->arena);
    } else {
      env_error(env, -1, "date formatting error: empty result");
      return nil_value;
    }
  } else {
    env_error(env, -1, "date formatting error: %s", strerror(errno));
    return nil_value;
  }
}

void import_datetime(Env *env) {
  env_def_fn("now", now, env);
  env_def_fn("time", time_, env);
  env_def_fn("date", date, env);
}

/* Plet
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#define _GNU_SOURCE
#include "datetime.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *rfc2822_day_names[] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char *rfc2822_month_names[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

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
    } else if (input->length && (*input->input == '-' || *input->input == '+')) {
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
  struct tm *t = localtime(&arg);
  // TODO: improve
  char output[100];
  if (t) {
    char *format_string = allocate(format.string_value->size + 1);
    memcpy(format_string, format.string_value->bytes, format.string_value->size);
    format_string[format.string_value->size] = '\0';
    size_t size = strftime(output, sizeof(output), format_string, t);
    if (size) {
      free(format_string);
      return create_string((uint8_t *) output, size, env->arena);
    } else {
      env_error(env, -1, "date formatting error: empty result");
      free(format_string);
      return nil_value;
    }
  } else {
    env_error(env, -1, "date formatting error: %s", strerror(errno));
    return nil_value;
  }
}

static Value iso8601(const Tuple *args, Env *env) {
  check_args(1, args, env);
  time_t arg;
  if (!parse_time_value(args->values[0], &arg)) {
    arg_error(0, "time|int|string", args, env);
    return nil_value;
  }
  struct tm *t = localtime(&arg);
  char output[100];
  if (t) {
    size_t size = strftime(output, sizeof(output), "%Y-%m-%dT%H:%M:%S%z", t);
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

static Value rfc2822(const Tuple *args, Env *env) {
  check_args(1, args, env);
  time_t arg;
  if (!parse_time_value(args->values[0], &arg)) {
    arg_error(0, "time|int|string", args, env);
    return nil_value;
  }
  Buffer buffer = create_buffer(32);
  if (rfc2822_date(arg, &buffer)) {
    Value output = create_string(buffer.data, buffer.size, env->arena);
    delete_buffer(buffer);
    return output;
  }
  delete_buffer(buffer);
  env_error(env, -1, "date formatting error: %s", strerror(errno));
  return nil_value;
}

void import_datetime(Env *env) {
  env_def_fn("now", now, env);
  env_def_fn("time", time_, env);
  env_def_fn("date", date, env);
  env_def_fn("iso8601", iso8601, env);
  env_def_fn("rfc2822", rfc2822, env);
}

int rfc2822_date(time_t timestamp, Buffer *buffer) {
  struct tm *t = localtime(&timestamp);
  if (t) {
    char timezone[10];
    if (!strftime(timezone, sizeof(timezone), " %z", t)) {
      timezone[0] = '\0';
    }
    buffer_printf(buffer, "%s, %d %s %d %02d:%02d:%02d%s", rfc2822_day_names[t->tm_wday], t->tm_mday,
        rfc2822_month_names[t->tm_mon], t->tm_year + 1900, t->tm_hour, t->tm_min, t->tm_sec, timezone);
    return 1;
  } else {
    return 0;
  }
}

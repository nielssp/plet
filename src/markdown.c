/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "markdown.h"

#include "strings.h"

#ifdef WITH_MARKDOWN
#include <md4c-html.h>

static void process_output(const MD_CHAR *output, MD_SIZE size, void *context) {
  StringBuffer *buffer = (StringBuffer *) context;
  string_buffer_append_bytes(buffer, (const uint8_t *) output, size);
}

static Value convert_markdown(String *input, Env *env) {
  StringBuffer buffer = create_string_buffer(0, env->arena);
  if (md_html((const char *) input->bytes, input->size, process_output, &buffer, MD_DIALECT_GITHUB, 0) != 0) {
    env_error(env, -1, "unknown markdown parser error");
  }
  return finalize_string_buffer(buffer);
}
#endif

static Value markdown(const Tuple *args, Env *env) {
  check_args(1, args, env);
  Value input = args->values[0];
  if (input.type != V_STRING) {
    arg_type_error(0, V_STRING, args, env);
    return nil_value;
  }
#ifdef WITH_MARKDOWN
  return convert_markdown(input.string_value, env);
#else
  env_error(env, -1, "tsc was not compiled with markdown support");
  return input;
#endif
}

void import_markdown(Env *env) {
  env_def_fn("markdown", markdown, env);
#ifdef WITH_MARKDOWN
  Value content_handlers;
  if (!env_get(get_symbol("CONTENT_HANDLERS", env->symbol_map), &content_handlers, env)) {
    content_handlers = create_object(0, env->arena);
    env_def("CONTENT_HANDLERS", content_handlers, env);
  }
  if (content_handlers.type == V_OBJECT) {
    object_put(content_handlers.object_value, copy_c_string("md", env->arena),
        (Value) { .type = V_FUNCTION, .function_value = markdown }, env->arena);
  }
#endif
}

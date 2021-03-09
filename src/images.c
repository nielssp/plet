/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "images.h"

#include "build.h"
#include "html.h"

#include <stdlib.h>

#ifdef WITH_IMAGEMAGICK
#include <MagickWand/MagickWand.h>
#endif

typedef struct {
  int64_t max_width;
  int64_t max_height;
  int64_t quality;
  int link_full;
  Env *env;
} ImageArgs;

static HtmlTransformation image_acceptor(Value node, void *context) {
  ImageArgs *args = context;
  if (html_is_tag(node, "img")) {
    Value src = html_get_attribute(node, "src");
    if (src.type == V_STRING) {
    }
  }
  return HTML_NO_ACTION;
}

static Value images(const Tuple *args, Env *env) {
  check_args_between(1, 5, args, env);
  Value src = args->values[0];
  Value max_width = create_int(640);
  if (args->size > 1) {
    max_width = args->values[1];
    if (max_width.type != V_INT) {
      arg_type_error(1, V_INT, args, env);
      return nil_value;
    }
  }
  Value max_height = create_int(480);
  if (args->size > 2) {
    max_width = args->values[2];
    if (max_width.type != V_INT) {
      arg_type_error(2, V_INT, args, env);
      return nil_value;
    }
  }
  Value quality = create_int(100);
  if (args->size > 3) {
    max_width = args->values[3];
    if (max_width.type != V_INT) {
      arg_type_error(3, V_INT, args, env);
      return nil_value;
    }
  }
  int link_full = 1;
  if (args->size > 4) {
    link_full = is_truthy(args->values[4]);
  }
  ImageArgs context = {max_width.int_value, max_height.int_value, quality.int_value, link_full, env};
  return html_transform(src, image_acceptor, &context);
}

void import_images(Env *env) {
  env_def_fn("images", images, env);
}

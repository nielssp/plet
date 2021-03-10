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
  Path *src_root;
  Path *dist_root;
  Path *asset_root;
  Env *env;
} ImageArgs;

static HtmlTransformation image_acceptor(Value node, void *context) {
  ImageArgs *args = context;
  if (html_is_tag(node, "img")) {
    Value src = html_get_attribute(node, "src");
    if (src.type == V_STRING && string_starts_with("tscasset:", src.string_value)) {
      Path *asset_path = create_path((char *) src.string_value->bytes + sizeof("tscasset:") - 1,
          src.string_value->size - (sizeof("tscasset:") - 1));
      Path *src_path = path_join(args->src_root, asset_path);
      Path *asset_web_path = path_join(args->asset_root, asset_path);
      Path *dist_path = path_join(args->dist_root, asset_web_path);
      copy_asset(src_path, dist_path);
      StringBuffer new_link = create_string_buffer(sizeof("tsclink:") + asset_web_path->size, args->env->arena);
      string_buffer_printf(&new_link, "tsclink:%s", asset_web_path->path);
      html_set_attribute(node, "src", finalize_string_buffer(new_link).string_value, args->env);
      delete_path(dist_path);
      delete_path(asset_web_path);
      delete_path(src_path);
      delete_path(asset_path);
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
  Path *src_root = get_src_root(env);
  if (src_root) {
    Path *dist_root = get_dist_root(env);
    if (dist_root) {
      Path *asset_root = create_path("assets", -1);
      ImageArgs context = {max_width.int_value, max_height.int_value, quality.int_value, link_full, src_root,
        dist_root, asset_root, env};
      src = html_transform(src, image_acceptor, &context);
      delete_path(asset_root);
      delete_path(dist_root);
    } else {
      env_error(env, -1, "DIST_ROOT missing or not a string");
    }
    delete_path(src_root);
  } else {
    env_error(env, -1, "SRC_ROOT missing or not a string");
  }
  return src;
}

void import_images(Env *env) {
  env_def_fn("images", images, env);
}

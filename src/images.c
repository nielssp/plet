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

#ifdef WITH_IMAGEMAGICK

static Path *handle_image(const Path *asset_path, const Path *src_path, int *attr_width, int *attr_height,
    ImageArgs *args) {
  Path *asset_web_path = path_join(args->asset_root, asset_path);
  Path *dist_path = path_join(args->dist_root, asset_web_path);
  Path *dest_dir = path_get_parent(dist_path);
  if (mkdir_rec(dest_dir->path)) {
    MagickWandGenesis();
    MagickWand *wand = NewMagickWand();
    MagickBooleanType status = MagickReadImage(wand, src_path->path);
    if (status == MagickFalse) {
      ExceptionType severity;
      char *description = MagickGetException(wand, &severity);
      env_error(args->env, -1, "%s", description);
      MagickRelinquishMemory(description);
    } else {
      int width = MagickGetImageWidth(wand);
      int height = MagickGetImageHeight(wand);
      if (width > args->max_width || height > args->max_height || *attr_width || *attr_height) {
        int requested_width, requested_height;
        if (*attr_width && *attr_height) {
          requested_width = *attr_width;
          requested_height = *attr_height;
        } else if (*attr_width) {
          requested_width = *attr_width;
          requested_height = requested_width * height / width;
        } else if (*attr_height) {
          requested_height = *attr_height;
          requested_width = requested_height * width / height;
        } else {
          requested_width = width;
          requested_height = height;
        }
        int target_width, target_height;
        double ratio = requested_width / (double) requested_height;
        if (ratio < args->max_width / (double) args->max_height) {
          target_height = requested_height < args->max_height ? requested_height : args->max_height;
          target_width = (int) (target_height * ratio);
        } else {
          target_width = requested_width < args->max_width ? requested_width : args->max_width;
          target_height = (int) (target_width / ratio);
        }
        MagickResizeImage(wand, target_width, target_height, LanczosFilter);
        MagickSetImageCompressionQuality(wand, args->quality);

        const char *name = path_get_name(dist_path);
        const char *ext = path_get_extension(dist_path);
        Buffer new_name = create_buffer(0);
        if (ext[0]) {
          buffer_printf(&new_name, "%.*s.%dx%dq%d.%s", ext - name, name, target_width, target_height,
              args->quality, ext);
        } else {
          buffer_printf(&new_name, "%s.%dx%dq%d.jpg", name, target_width, target_height, args->quality);
        }
        Path *new_name_path = create_path((char *) new_name.data, new_name.size);
        delete_buffer(new_name);
        Path *asset_web_path_parent = path_get_parent(asset_web_path);
        delete_path(asset_web_path);
        asset_web_path = path_join(asset_web_path_parent, new_name_path);
        delete_path(asset_web_path_parent);
        delete_path(new_name_path);
        delete_path(dist_path);
        dist_path = path_join(args->dist_root, asset_web_path);

        MagickWriteImage(wand, dist_path->path);
      } else {
        copy_file(src_path->path, dist_path->path);
      }
    }
    DestroyMagickWand(wand);
    MagickWandTerminus();
  }
  delete_path(dest_dir);
  delete_path(dist_path);
  return asset_web_path;
}

#else

static Path *handle_image(const Path *asset_path, const Path *src_path, int *attr_width, int *attr_height,
    ImageArgs *args) {
  Path *asset_web_path = path_join(args->asset_root, asset_path);
  Path *dist_path = path_join(args->dist_root, asset_web_path);
  copy_asset(src_path, dist_path);
  delete_path(dist_path);
  return asset_web_path;
}

#endif

static void get_size_attributes(Value node, int *width, int *height) {
  Value value = html_get_attribute(node, "width");
  if (value.type == V_STRING) {
    char *value_string = string_to_c_string(value.string_value);
    *width = atoi(value_string);
    free(value_string);
  } else {
    *width = 0;
  }
  value = html_get_attribute(node, "height");
  if (value.type == V_STRING) {
    char *value_string = string_to_c_string(value.string_value);
    *height = atoi(value_string);
    free(value_string);
  } else {
    *height = 0;
  }
}

static HtmlTransformation transform_images(Value node, void *context) {
  ImageArgs *args = context;
  if (html_is_tag(node, "img")) {
    Value src = html_get_attribute(node, "src");
    if (src.type == V_STRING && string_starts_with("tscasset:", src.string_value)) {
      Path *asset_path = create_path((char *) src.string_value->bytes + sizeof("tscasset:") - 1,
          src.string_value->size - (sizeof("tscasset:") - 1));
      Path *src_path = path_join(args->src_root, asset_path);
      int attr_width, attr_height;
      get_size_attributes(node, &attr_width, &attr_height);
      Path *asset_web_path = handle_image(asset_path, src_path, &attr_width, &attr_height, args);
      StringBuffer new_link = create_string_buffer(sizeof("tsclink:") + asset_web_path->size, args->env->arena);
      string_buffer_printf(&new_link, "tsclink:%s", asset_web_path->path);
      html_set_attribute(node, "src", finalize_string_buffer(new_link).string_value, args->env);
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
    max_height = args->values[2];
    if (max_height.type != V_INT) {
      arg_type_error(2, V_INT, args, env);
      return nil_value;
    }
  }
  Value quality = create_int(100);
  if (args->size > 3) {
    quality = args->values[3];
    if (quality.type != V_INT) {
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
      src = html_transform(src, transform_images, &context);
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

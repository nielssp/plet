/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#include "images.h"

#include "build.h"
#include "html.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>

#ifdef WITH_IMAGEMAGICK
#include <MagickWand/MagickWand.h>
#endif

const char *supported_image_types[] = {"png", "jpg", "jpeg"};

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

static int is_supported(const char *extension) {
  size_t length = sizeof(supported_image_types) / sizeof(char *);
  for (size_t i = 0; i < length; i++) {
    if (strcmp(extension, supported_image_types[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

static void resize_image(const Path *src_path, const Path *dist_path, int width, int height, ImageArgs *args) {
#ifdef WITH_IMAGEMAGICK
  MagickWandGenesis();
  MagickWand *wand = NewMagickWand();
  MagickBooleanType status = MagickReadImage(wand, src_path->path);
  if (status == MagickFalse) {
    ExceptionType severity;
    char *description = MagickGetException(wand, &severity);
    env_error(args->env, ENV_ARG_ALL, "ImageMagick error: %s", description);
    MagickRelinquishMemory(description);
  } else {
    MagickResizeImage(wand, width, height, LanczosFilter);
    MagickSetImageCompressionQuality(wand, args->quality);
    status = MagickWriteImage(wand, dist_path->path);
    if (status == MagickTrue) {
      struct stat stat_buffer;
      if (stat(src_path->path, &stat_buffer) == 0) {
        struct utimbuf utime_buffer;
        utime_buffer.actime = stat_buffer.st_atime;
        utime_buffer.modtime = stat_buffer.st_mtime;
        utime(dist_path->path, &utime_buffer);
      }
    }
  }
  DestroyMagickWand(wand);
  MagickWandTerminus();
#else
  copy_file(src_path->path, dist_path->path);
#endif
}

static Path *handle_image(const Path *asset_path, const Path *src_path, int *attr_width, int *attr_height,
    Path **original_asset_web_path, ImageArgs *args) {
  Path *asset_web_path = path_join(args->asset_root, asset_path, 1);
  Path *dist_path = path_join(args->dist_root, asset_web_path, 1);
  Path *dest_dir = path_get_parent(dist_path);
  if (mkdir_rec(dest_dir->path)) {
    char *extension = path_get_lowercase_extension(src_path);
    if (is_supported(extension)) {
      TscImageInfo info = get_image_info(src_path);
      if (info.type == IMG_UNKNOWN) {
        env_error(args->env, ENV_ARG_ALL, "unknown image type: %s", src_path->path);
      } else if (info.type == IMG_NOT_FOUND) {
        env_error(args->env, ENV_ARG_ALL, "error reading image");
      } else {
        int width = info.width;
        int height = info.height;
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
          *attr_width = target_width;
          *attr_height = target_height;
          const char *name = path_get_name(dist_path);
          const char *ext = path_get_extension(dist_path);
          Buffer new_name = create_buffer(0);
          if (extension[0]) {
            buffer_printf(&new_name, "%.*s.%dx%dq%d.%s", ext - name - 1, name, target_width, target_height,
                args->quality, ext);
          } else {
            buffer_printf(&new_name, "%s.%dx%dq%d.jpg", name, target_width, target_height, args->quality);
          }
          Path *new_name_path = create_path((char *) new_name.data, new_name.size);
          delete_buffer(new_name);
          Path *asset_web_path_parent = path_get_parent(asset_web_path);
          if (original_asset_web_path) {
            if (asset_has_changed(src_path, dist_path)) {
              copy_file(src_path->path, dist_path->path);
            }
            *original_asset_web_path = asset_web_path;
          } else {
            delete_path(asset_web_path);
          }
          asset_web_path = path_join(asset_web_path_parent, new_name_path, 1);
          delete_path(asset_web_path_parent);
          delete_path(new_name_path);
          delete_path(dist_path);
          dist_path = path_join(args->dist_root, asset_web_path, 1);

          if (asset_has_changed(src_path, dist_path)) {
            resize_image(src_path, dist_path, target_width, target_height, args);
          }
        } else {
          *attr_width = width;
          *attr_height = height;
          if (asset_has_changed(src_path, dist_path)) {
            copy_file(src_path->path, dist_path->path);
          }
        }
      }
    } else if (asset_has_changed(src_path, dist_path)) {
      copy_file(src_path->path, dist_path->path);
    }
    free(extension);
  }
  delete_path(dest_dir);
  delete_path(dist_path);
  return asset_web_path;
}

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
      Path *src_path = path_join(args->src_root, asset_path, 1);
      int attr_width, attr_height;
      get_size_attributes(node, &attr_width, &attr_height);

      Path *original_asset_web_path = NULL;

      Path *asset_web_path = handle_image(asset_path, src_path, &attr_width, &attr_height,
          args->link_full ? &original_asset_web_path : NULL, args);

      StringBuffer new_link = create_string_buffer(sizeof("tsclink:") + asset_web_path->size, args->env->arena);
      string_buffer_printf(&new_link, "tsclink:%s", asset_web_path->path);
      html_set_attribute(node, "src", finalize_string_buffer(new_link).string_value, args->env);
      delete_path(asset_web_path);
      delete_path(src_path);
      delete_path(asset_path);

      if (attr_width) {
        StringBuffer buffer = create_string_buffer(0, args->env->arena);
        string_buffer_printf(&buffer, "%d", attr_width);
        html_set_attribute(node, "width", buffer.string, args->env);
      }
      if (attr_height) {
        StringBuffer buffer = create_string_buffer(0, args->env->arena);
        string_buffer_printf(&buffer, "%d", attr_height);
        html_set_attribute(node, "height", buffer.string, args->env);
      }

      if (original_asset_web_path) {
        Value link_node = html_create_element("a", 0, args->env);
        StringBuffer original_link = create_string_buffer(sizeof("tsclink:") + original_asset_web_path->size,
            args->env->arena);
        string_buffer_printf(&original_link, "tsclink:%s", original_asset_web_path->path);
        html_set_attribute(link_node, "href", finalize_string_buffer(original_link).string_value, args->env);
        html_append_child(link_node, node, args->env->arena);
        delete_path(original_asset_web_path);
        return HTML_REPLACE(link_node);
      }
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

static TscImageInfo get_jpeg_size(FILE *f) {
  TscImageInfo info = { IMG_UNKNOWN };
  while (1) {
    int c = fgetc(f);
    if (c == EOF) {
      break;
    } else if (c == 0xFF) {
      int marker = fgetc(f);
      if (marker == EOF || marker == 0xDA || marker == 0xD9) { // Start of scan, end of image
        break;
      }
      if ((marker >> 4) == 0xC && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) { // Start of frame
        if (fseek(f, 3, SEEK_CUR)) {
          break;
        }
        uint8_t dimensions[4];
        if (fread(dimensions, 1, 4, f) != 4) {
          break;
        }
        info.height = (dimensions[0] << 8) | dimensions[1];
        info.width = (dimensions[2] << 8) | dimensions[3];
        info.type = IMG_JPEG;
        break;
      }
      uint8_t length_bytes[2];
      if (fread(length_bytes, 1, 2, f) != 2) {
        break;
      }
      uint16_t length = (length_bytes[0] << 8) | length_bytes[1];
      if (length > 2) {
        if (fseek(f, length - 2, SEEK_CUR)) {
          break;
        }
      }
    }
  }
  return info;
}

static TscImageInfo get_png_size(FILE *f) {
  TscImageInfo info = { IMG_UNKNOWN };
  if (fseek(f, 8, SEEK_CUR)) {
    return info;
  }
  uint8_t dimensions[8];
  if (fread(dimensions, 1, 8, f) != 8) {
    return info;
  }
  info.width = (dimensions[0] << 24) | (dimensions[1] << 16) | (dimensions[2] << 8) | dimensions[3];
  info.height = (dimensions[4] << 24) | (dimensions[5] << 16) | (dimensions[6] << 8) | dimensions[7];
  info.type = IMG_PNG;
  return info;
}

#define JPEG_SIGNATURE "\xff\xd8\xff"
#define PNG_SIGNATURE "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a"

TscImageInfo get_image_info(const Path *path) {
  TscImageInfo info = { IMG_UNKNOWN };
  FILE *f = fopen(path->path, "r");
  if (!f) {
    fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "%s" SGR_RESET "\n", path->path, strerror(errno));
    info.type = IMG_NOT_FOUND;
    return info;
  }
  uint8_t signature[8];
  if (fread(signature, 1, 3, f) == 3) {
    if (memcmp(signature, JPEG_SIGNATURE, 3) == 0) {
      info = get_jpeg_size(f);
    } else if (memcmp(signature, PNG_SIGNATURE, 3) == 0) {
      if (fread(signature + 3, 1, 5, f) == 5 && memcmp(signature, PNG_SIGNATURE, 8) == 0) {
        info = get_png_size(f);
      } else {
        fprintf(stderr, SGR_BOLD "%s: " ERROR_LABEL "invalid or corrupt PNG signature" SGR_RESET "\n", path->path);
        // corrupt png? error?
      }
    }
  }
  fclose(f);
  return info;
}

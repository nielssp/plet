/* tsc
 * Copyright (c) 2021 Niels Sonnich Poulsen (http://nielssp.dk)
 * Licensed under the MIT license.
 * See the LICENSE file or http://opensource.org/licenses/MIT for more information.
 */

#ifndef IMAGES_H
#define IMAGES_H

#include "value.h"

void import_images(Env *env);

typedef enum {
  IMG_NOT_FOUND,
  IMG_UNKNOWN,
  IMG_PNG,
  IMG_JPEG
} TscImageType;

typedef struct {
  TscImageType type;
  int width;
  int height;
} TscImageInfo;

TscImageInfo get_image_info(const Path *path);

#endif

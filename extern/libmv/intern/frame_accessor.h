/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef LIBMV_C_API_FRAME_ACCESSOR_H_
#define LIBMV_C_API_FRAME_ACCESSOR_H_

#include <stdint.h>

#include "intern/region.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libmv_FloatImage libmv_FloatImage;
typedef struct libmv_FrameAccessor libmv_FrameAccessor;
typedef struct libmv_FrameTransform libmv_FrameTransform;
typedef struct libmv_FrameAccessorUserData libmv_FrameAccessorUserData;
typedef void *libmv_CacheKey;

typedef enum {
  LIBMV_IMAGE_MODE_MONO,
  LIBMV_IMAGE_MODE_RGBA,
} libmv_InputMode;

typedef libmv_CacheKey (*libmv_GetImageCallback) (
    libmv_FrameAccessorUserData* user_data,
    int clip,
    int frame,
    libmv_InputMode input_mode,
    int downscale,
    const libmv_Region* region,
    const libmv_FrameTransform* transform,
    float** destination,
    int* width,
    int* height,
    int* channels);

typedef void (*libmv_ReleaseImageCallback) (libmv_CacheKey cache_key);

libmv_FrameAccessor* libmv_FrameAccessorNew(
    libmv_FrameAccessorUserData* user_data,
    libmv_GetImageCallback get_image_callback,
    libmv_ReleaseImageCallback release_image_callback);
void libmv_FrameAccessorDestroy(libmv_FrameAccessor* frame_accessor);

int64_t libmv_frameAccessorgetTransformKey(const libmv_FrameTransform *transform);

void libmv_frameAccessorgetTransformRun(const libmv_FrameTransform *transform,
                                        const libmv_FloatImage *input_image,
                                        libmv_FloatImage *output_image);
#ifdef __cplusplus
}
#endif

#endif  // LIBMV_C_API_FRAME_ACCESSOR_H_

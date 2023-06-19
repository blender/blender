/* SPDX-FileCopyrightText: 2014 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef LIBMV_C_API_FRAME_ACCESSOR_H_
#define LIBMV_C_API_FRAME_ACCESSOR_H_

#include <stdint.h>

#include "intern/image.h"
#include "intern/region.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct libmv_FrameAccessor libmv_FrameAccessor;
typedef struct libmv_FrameTransform libmv_FrameTransform;
typedef struct libmv_FrameAccessorUserData libmv_FrameAccessorUserData;
typedef void* libmv_CacheKey;

typedef enum {
  LIBMV_IMAGE_MODE_MONO,
  LIBMV_IMAGE_MODE_RGBA,
} libmv_InputMode;

typedef libmv_CacheKey (*libmv_GetImageCallback)(
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

typedef void (*libmv_ReleaseImageCallback)(libmv_CacheKey cache_key);

typedef libmv_CacheKey (*libmv_GetMaskForTrackCallback)(
    libmv_FrameAccessorUserData* user_data,
    int clip,
    int frame,
    int track,
    const libmv_Region* region,
    float** destination,
    int* width,
    int* height);
typedef void (*libmv_ReleaseMaskCallback)(libmv_CacheKey cache_key);

libmv_FrameAccessor* libmv_FrameAccessorNew(
    libmv_FrameAccessorUserData* user_data,
    libmv_GetImageCallback get_image_callback,
    libmv_ReleaseImageCallback release_image_callback,
    libmv_GetMaskForTrackCallback get_mask_for_track_callback,
    libmv_ReleaseMaskCallback release_mask_callback);
void libmv_FrameAccessorDestroy(libmv_FrameAccessor* frame_accessor);

int64_t libmv_frameAccessorgetTransformKey(
    const libmv_FrameTransform* transform);

void libmv_frameAccessorgetTransformRun(const libmv_FrameTransform* transform,
                                        const libmv_FloatImage* input_image,
                                        libmv_FloatImage* output_image);
#ifdef __cplusplus
}
#endif

#endif  // LIBMV_C_API_FRAME_ACCESSOR_H_

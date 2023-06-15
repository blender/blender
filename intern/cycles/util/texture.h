/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __UTIL_TEXTURE_H__
#define __UTIL_TEXTURE_H__

#include "util/transform.h"

CCL_NAMESPACE_BEGIN

/* Color to use when textures are not found. */
#define TEX_IMAGE_MISSING_R 1
#define TEX_IMAGE_MISSING_G 0
#define TEX_IMAGE_MISSING_B 1
#define TEX_IMAGE_MISSING_A 1

/* Interpolation types for textures
 * CUDA also use texture space to store other objects. */
typedef enum InterpolationType {
  INTERPOLATION_NONE = -1,
  INTERPOLATION_LINEAR = 0,
  INTERPOLATION_CLOSEST = 1,
  INTERPOLATION_CUBIC = 2,
  INTERPOLATION_SMART = 3,

  INTERPOLATION_NUM_TYPES,
} InterpolationType;

typedef enum ImageDataType {
  IMAGE_DATA_TYPE_FLOAT4 = 0,
  IMAGE_DATA_TYPE_BYTE4 = 1,
  IMAGE_DATA_TYPE_HALF4 = 2,
  IMAGE_DATA_TYPE_FLOAT = 3,
  IMAGE_DATA_TYPE_BYTE = 4,
  IMAGE_DATA_TYPE_HALF = 5,
  IMAGE_DATA_TYPE_USHORT4 = 6,
  IMAGE_DATA_TYPE_USHORT = 7,
  IMAGE_DATA_TYPE_NANOVDB_FLOAT = 8,
  IMAGE_DATA_TYPE_NANOVDB_FLOAT3 = 9,
  IMAGE_DATA_TYPE_NANOVDB_FPN = 10,
  IMAGE_DATA_TYPE_NANOVDB_FP16 = 11,

  IMAGE_DATA_NUM_TYPES
} ImageDataType;

/* Alpha types
 * How to treat alpha in images. */
typedef enum ImageAlphaType {
  IMAGE_ALPHA_UNASSOCIATED = 0,
  IMAGE_ALPHA_ASSOCIATED = 1,
  IMAGE_ALPHA_CHANNEL_PACKED = 2,
  IMAGE_ALPHA_IGNORE = 3,
  IMAGE_ALPHA_AUTO = 4,

  IMAGE_ALPHA_NUM_TYPES,
} ImageAlphaType;

/* Extension types for textures.
 *
 * Defines how the image is extrapolated past its original bounds. */
typedef enum ExtensionType {
  /* Cause the image to repeat horizontally and vertically. */
  EXTENSION_REPEAT = 0,
  /* Extend by repeating edge pixels of the image. */
  EXTENSION_EXTEND = 1,
  /* Clip to image size and set exterior pixels as transparent. */
  EXTENSION_CLIP = 2,
  /* Repeatedly flip the image horizontally and vertically. */
  EXTENSION_MIRROR = 3,

  EXTENSION_NUM_TYPES,
} ExtensionType;

typedef struct TextureInfo {
  /* Pointer, offset or texture depending on device. */
  uint64_t data;
  /* Data Type */
  uint data_type;
  /* Interpolation and extension type. */
  uint interpolation, extension;
  /* Dimensions. */
  uint width, height, depth;
  /* Transform for 3D textures. */
  uint use_transform_3d;
  Transform transform_3d;
} TextureInfo;

CCL_NAMESPACE_END

#endif /* __UTIL_TEXTURE_H__ */

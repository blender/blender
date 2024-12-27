/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/transform.h"

CCL_NAMESPACE_BEGIN

/* Color to use when textures are not found. */
enum {
  TEX_IMAGE_MISSING_R = 1,
  TEX_IMAGE_MISSING_G = 0,
  TEX_IMAGE_MISSING_B = 1,
  TEX_IMAGE_MISSING_A = 1
};

/* Interpolation types for textures
 * CUDA also use texture space to store other objects. */
enum InterpolationType {
  INTERPOLATION_NONE = ~0,
  INTERPOLATION_LINEAR = 0,
  INTERPOLATION_CLOSEST = 1,
  INTERPOLATION_CUBIC = 2,
  INTERPOLATION_SMART = 3,

  INTERPOLATION_NUM_TYPES,
};

enum ImageDataType {
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
};

/* Alpha types
 * How to treat alpha in images. */
enum ImageAlphaType {
  IMAGE_ALPHA_UNASSOCIATED = 0,
  IMAGE_ALPHA_ASSOCIATED = 1,
  IMAGE_ALPHA_CHANNEL_PACKED = 2,
  IMAGE_ALPHA_IGNORE = 3,
  IMAGE_ALPHA_AUTO = 4,

  IMAGE_ALPHA_NUM_TYPES,
};

/* Extension types for textures.
 *
 * Defines how the image is extrapolated past its original bounds. */
enum ExtensionType {
  /* Cause the image to repeat horizontally and vertically. */
  EXTENSION_REPEAT = 0,
  /* Extend by repeating edge pixels of the image. */
  EXTENSION_EXTEND = 1,
  /* Clip to image size and set exterior pixels as transparent. */
  EXTENSION_CLIP = 2,
  /* Repeatedly flip the image horizontally and vertically. */
  EXTENSION_MIRROR = 3,

  EXTENSION_NUM_TYPES,
};

struct TextureInfo {
  /* Pointer, offset or texture depending on device. */
  uint64_t data = 0;
  /* Data Type */
  uint data_type = IMAGE_DATA_NUM_TYPES;
  /* Interpolation and extension type. */
  uint interpolation = INTERPOLATION_NONE;
  uint extension = EXTENSION_REPEAT;
  /* Dimensions. */
  uint width = 0;
  uint height = 0;
  uint depth = 0;
  /* Transform for 3D textures. */
  uint use_transform_3d = false;
  Transform transform_3d = transform_zero();
};

CCL_NAMESPACE_END

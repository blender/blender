/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/defines.h"
#include "util/transform.h"

#ifndef __KERNEL_GPU__
#  include <climits>
#endif

CCL_NAMESPACE_BEGIN

/* Color to use when images are not found. */
#define IMAGE_MISSING_RGBA make_float4(1, 0, 1, 1)

#define KERNEL_IMAGE_NONE INT_MAX

/* Interpolation types for images. */
enum InterpolationType {
  INTERPOLATION_NONE = ~0,
  INTERPOLATION_LINEAR = 0,
  INTERPOLATION_CLOSEST = 1,
  INTERPOLATION_CUBIC = 2,
  INTERPOLATION_SMART = 3,

  INTERPOLATION_NUM_TYPES,
};

/* Image data types supported by the kernel. */
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
  IMAGE_DATA_TYPE_NANOVDB_FLOAT4 = 10,
  IMAGE_DATA_TYPE_NANOVDB_FPN = 11,
  IMAGE_DATA_TYPE_NANOVDB_FP16 = 12,
  IMAGE_DATA_TYPE_NANOVDB_EMPTY = 13,

  IMAGE_DATA_NUM_TYPES
};

ccl_device_inline bool is_nanovdb_type(int type)
{
  return (type >= IMAGE_DATA_TYPE_NANOVDB_FLOAT && type <= IMAGE_DATA_TYPE_NANOVDB_EMPTY);
}

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

/* Image format types */
enum ImageFormatType {
  IMAGE_FORMAT_PLAIN,
  IMAGE_FORMAT_EQUIANGULAR,
};

/* Extension types for image.
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

/* Kernel data structure describing device image objects. */
struct KernelImageInfo {
  /* Pointer, offset or image/texture object depending on device. */
  uint64_t data = 0;
  /* Data Type */
  uint data_type = IMAGE_DATA_NUM_TYPES;
  /* Interpolation and extension type. */
  uint interpolation = INTERPOLATION_NONE;
  uint extension = EXTENSION_REPEAT;
  /* Dimensions. */
  uint width = 0;
  uint height = 0;
};

/* KernelImageTexture index for UDIM tile. */
struct KernelImageUDIM {
  int tile;
  int image_texture_id;
};

/* Kernel data structure for image textures.
 *
 * This describes a logical image texture for the shading system, that may be stored
 * in one or more device image objects described by KernelImageInfo. This is to
 * support on demand loading of tiles. */
struct KernelImageTexture {
  /* Index into image object map. */
  uint image_info_id = KERNEL_IMAGE_NONE;
  /* Image dimensions */
  uint width = 0;
  uint height = 0;
  /* Interpolation and extension type. */
  uint interpolation = INTERPOLATION_NONE;
  uint extension = EXTENSION_REPEAT;
  /* Transform for 3D textures. */
  uint use_transform_3d = false;
  Transform transform_3d = transform_zero();
};

CCL_NAMESPACE_END

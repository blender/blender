/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/defines.h"
#include "util/transform.h"

#ifndef __KERNEL_GPU__
#  include <functional>
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

/* Kernel data structure to describe image. */
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

struct KernelImageUDIM {
  /* KernelImageTexture index for UDIM tile. */
  int tile;
  int tex_id;
};

#define KERNEL_TILE_LOAD_NONE 0xFFFFFFFFU
#define KERNEL_TILE_LOAD_REQUEST KERNEL_TILE_LOAD_NONE - 1
#define KERNEL_TILE_LOAD_FAILED KERNEL_TILE_LOAD_NONE - 2

struct KernelImageTexture {
  /* Index into image object map. */
  uint64_t slot = 0;
  /* TODO */
  uint tile_descriptor_offset = KERNEL_TILE_LOAD_NONE;
  int tile_size_shift = 0;
  int tile_levels = 0;
  /* Image dimensions */
  int width = 0;
  int height = 0;
  /* Interpolation and extension type. */
  uint interpolation = INTERPOLATION_NONE;
  uint extension = EXTENSION_REPEAT;
  /* Transform for 3D textures. */
  uint use_transform_3d = false;
  Transform transform_3d = transform_zero();
  /* Fallback or fixed color. */
  float4 average_color = zero_float4();
};

#define KERNEL_IMAGE_TEX_PADDING 2

using KernelTileDescriptor = uint;

ccl_device_inline KernelTileDescriptor kernel_tile_descriptor_encode(const uint slot,
                                                                     const uint offset)
{
  return slot | (offset << 24);
}

ccl_device_inline uint kernel_tile_descriptor_slot(const KernelTileDescriptor tile)
{
  return tile & 0xffffff;
}

ccl_device_inline uint kernel_tile_descriptor_offset(const KernelTileDescriptor tile)
{
  return tile >> 24;
}

ccl_device_inline bool kernel_tile_descriptor_loaded(const KernelTileDescriptor tile)
{
  return tile < KERNEL_TILE_LOAD_FAILED;
}

#ifndef __KERNEL_GPU__
using KernelImageCacheLoadTileFunc =
    std::function<void(size_t, int, int, int, KernelTileDescriptor *tile_descriptor)>;
using KernelImageCacheUpdateFunc = std::function<void()>;
#endif

CCL_NAMESPACE_END

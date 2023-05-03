/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "vk_common.hh"

namespace blender::gpu {
VkImageAspectFlagBits to_vk_image_aspect_flag_bits(const eGPUTextureFormat format)
{
  switch (format) {
    /* Formats texture & render-buffer */
    case GPU_RGBA32UI:
    case GPU_RG32UI:
    case GPU_R32UI:
    case GPU_RGBA16UI:
    case GPU_RG16UI:
    case GPU_R16UI:
    case GPU_RGBA8UI:
    case GPU_RG8UI:
    case GPU_R8UI:
    case GPU_RGBA32I:
    case GPU_RG32I:
    case GPU_R32I:
    case GPU_RGBA16I:
    case GPU_RG16I:
    case GPU_R16I:
    case GPU_RGBA8I:
    case GPU_RG8I:
    case GPU_R8I:
    case GPU_RGBA32F:
    case GPU_RG32F:
    case GPU_R32F:
    case GPU_RGBA16F:
    case GPU_RG16F:
    case GPU_R16F:
    case GPU_RGBA16:
    case GPU_RG16:
    case GPU_R16:
    case GPU_RGBA8:
    case GPU_RG8:
    case GPU_R8:
      return VK_IMAGE_ASPECT_COLOR_BIT;

    /* Special formats texture & render-buffer */
    case GPU_RGB10_A2:
    case GPU_RGB10_A2UI:
    case GPU_R11F_G11F_B10F:
    case GPU_SRGB8_A8:
      return VK_IMAGE_ASPECT_COLOR_BIT;

    /* Depth Formats. */
    case GPU_DEPTH_COMPONENT32F:
    case GPU_DEPTH_COMPONENT24:
    case GPU_DEPTH_COMPONENT16:
    case GPU_DEPTH32F_STENCIL8:
    case GPU_DEPTH24_STENCIL8:
      return VK_IMAGE_ASPECT_DEPTH_BIT;

    /* Texture only formats. */
    case GPU_RGB32UI:
    case GPU_RGB16UI:
    case GPU_RGB8UI:
    case GPU_RGB32I:
    case GPU_RGB16I:
    case GPU_RGB8I:
    case GPU_RGB16:
    case GPU_RGB8:
    case GPU_RGBA16_SNORM:
    case GPU_RGB16_SNORM:
    case GPU_RG16_SNORM:
    case GPU_R16_SNORM:
    case GPU_RGBA8_SNORM:
    case GPU_RGB8_SNORM:
    case GPU_RG8_SNORM:
    case GPU_R8_SNORM:
    case GPU_RGB32F:
    case GPU_RGB16F:
      return VK_IMAGE_ASPECT_COLOR_BIT;

    /* Special formats, texture only. */
    case GPU_SRGB8_A8_DXT1:
    case GPU_SRGB8_A8_DXT3:
    case GPU_SRGB8_A8_DXT5:
    case GPU_RGBA8_DXT1:
    case GPU_RGBA8_DXT3:
    case GPU_RGBA8_DXT5:
    case GPU_SRGB8:
    case GPU_RGB9_E5:
      return VK_IMAGE_ASPECT_COLOR_BIT;
  }
  BLI_assert_unreachable();
  return static_cast<VkImageAspectFlagBits>(0);
}

VkFormat to_vk_format(const eGPUTextureFormat format)
{
  switch (format) {
    /* Formats texture & render-buffer */
    case GPU_RGBA32UI:
      return VK_FORMAT_R32G32B32A32_UINT;
    case GPU_RG32UI:
      return VK_FORMAT_R32G32_UINT;
    case GPU_R32UI:
      return VK_FORMAT_R32_UINT;
    case GPU_RGBA16UI:
      return VK_FORMAT_R16G16B16A16_UINT;
    case GPU_RG16UI:
      return VK_FORMAT_R16G16_UINT;
    case GPU_R16UI:
      return VK_FORMAT_R16_UINT;
    case GPU_RGBA8UI:
      return VK_FORMAT_R8G8B8A8_UINT;
    case GPU_RG8UI:
      return VK_FORMAT_R8G8_UINT;
    case GPU_R8UI:
      return VK_FORMAT_R8_UINT;
    case GPU_RGBA32I:
      return VK_FORMAT_R32G32B32A32_SINT;
    case GPU_RG32I:
      return VK_FORMAT_R32G32_SINT;
    case GPU_R32I:
      return VK_FORMAT_R32_SINT;
    case GPU_RGBA16I:
      return VK_FORMAT_R16G16B16A16_SINT;
    case GPU_RG16I:
      return VK_FORMAT_R16G16_SINT;
    case GPU_R16I:
      return VK_FORMAT_R16_SINT;
    case GPU_RGBA8I:
      return VK_FORMAT_R8G8B8A8_SINT;
    case GPU_RG8I:
      return VK_FORMAT_R8G8_SINT;
    case GPU_R8I:
      return VK_FORMAT_R8_SINT;
    case GPU_RGBA32F:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case GPU_RG32F:
      return VK_FORMAT_R32G32_SFLOAT;
    case GPU_R32F:
      return VK_FORMAT_R32_SFLOAT;
    case GPU_RGBA16F:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case GPU_RG16F:
      return VK_FORMAT_R16G16_SFLOAT;
    case GPU_R16F:
      return VK_FORMAT_R16_SFLOAT;
    case GPU_RGBA16:
      return VK_FORMAT_R16G16B16A16_UNORM;
    case GPU_RG16:
      return VK_FORMAT_R16G16_UNORM;
    case GPU_R16:
      return VK_FORMAT_R16_UNORM;
    case GPU_RGBA8:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case GPU_RG8:
      return VK_FORMAT_R8G8_UNORM;
    case GPU_R8:
      return VK_FORMAT_R8_UNORM;

    /* Special formats texture & render-buffer */
    case GPU_RGB10_A2:
      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case GPU_RGB10_A2UI:
      return VK_FORMAT_A2B10G10R10_UINT_PACK32;
    case GPU_R11F_G11F_B10F:
      return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case GPU_SRGB8_A8:
      return VK_FORMAT_R8G8B8A8_SRGB;
    case GPU_DEPTH32F_STENCIL8:
      return VK_FORMAT_D32_SFLOAT_S8_UINT;
    case GPU_DEPTH24_STENCIL8:
      return VK_FORMAT_D24_UNORM_S8_UINT;

    /* Depth Formats. */
    case GPU_DEPTH_COMPONENT32F:
      return VK_FORMAT_D32_SFLOAT;
    case GPU_DEPTH_COMPONENT24:
      return VK_FORMAT_X8_D24_UNORM_PACK32;
    case GPU_DEPTH_COMPONENT16:
      return VK_FORMAT_D16_UNORM;

    /* Texture only formats. */
    case GPU_RGB32UI:
      return VK_FORMAT_R32G32B32_UINT;
    case GPU_RGB16UI:
      return VK_FORMAT_R16G16B16_UINT;
    case GPU_RGB8UI:
      return VK_FORMAT_R8G8B8_UINT;
    case GPU_RGB32I:
      return VK_FORMAT_R32G32B32_SINT;
    case GPU_RGB16I:
      return VK_FORMAT_R16G16B16_SINT;
    case GPU_RGB8I:
      return VK_FORMAT_R8G8B8_SINT;
    case GPU_RGB16:
      return VK_FORMAT_R16G16B16_UNORM;
    case GPU_RGB8:
      return VK_FORMAT_R8G8B8_UNORM;
    case GPU_RGBA16_SNORM:
      return VK_FORMAT_R16G16B16A16_SNORM;
    case GPU_RGB16_SNORM:
      return VK_FORMAT_R16G16B16_SNORM;
    case GPU_RG16_SNORM:
      return VK_FORMAT_R16G16_SNORM;
    case GPU_R16_SNORM:
      return VK_FORMAT_R16_SNORM;
    case GPU_RGBA8_SNORM:
      return VK_FORMAT_R8G8B8A8_SNORM;
    case GPU_RGB8_SNORM:
      return VK_FORMAT_R8G8B8_SNORM;
    case GPU_RG8_SNORM:
      return VK_FORMAT_R8G8_SNORM;
    case GPU_R8_SNORM:
      return VK_FORMAT_R8_SNORM;
    case GPU_RGB32F:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case GPU_RGB16F:
      return VK_FORMAT_R16G16B16_SFLOAT;

    /* Special formats, texture only. */
    case GPU_SRGB8_A8_DXT1:
      return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case GPU_SRGB8_A8_DXT3:
      return VK_FORMAT_BC2_SRGB_BLOCK;
    case GPU_SRGB8_A8_DXT5:
      return VK_FORMAT_BC3_SRGB_BLOCK;
    case GPU_RGBA8_DXT1:
      return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case GPU_RGBA8_DXT3:
      return VK_FORMAT_BC2_UNORM_BLOCK;
    case GPU_RGBA8_DXT5:
      return VK_FORMAT_BC3_UNORM_BLOCK;
    case GPU_SRGB8:
      return VK_FORMAT_R8G8B8_SRGB;
    case GPU_RGB9_E5:
      return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
  }
  return VK_FORMAT_UNDEFINED;
}

VkFormat to_vk_format(const GPUVertCompType type, const uint32_t size)
{
  switch (type) {
    case GPU_COMP_I8:
      switch (size) {
        case 1:
          return VK_FORMAT_R8_SNORM;
        case 2:
          return VK_FORMAT_R8G8_SNORM;
        case 3:
          return VK_FORMAT_R8G8B8_SNORM;
        case 4:
          return VK_FORMAT_R8G8B8A8_SNORM;
        default:
          break;
      }
      break;

    case GPU_COMP_U8:
      switch (size) {
        case 1:
          return VK_FORMAT_R8_UNORM;
        case 2:
          return VK_FORMAT_R8G8_UNORM;
        case 3:
          return VK_FORMAT_R8G8B8_UNORM;
        case 4:
          return VK_FORMAT_R8G8B8A8_UNORM;
        default:
          break;
      }
      break;

    case GPU_COMP_I16:
      switch (size) {
        case 2:
          return VK_FORMAT_R16_SNORM;
        case 4:
          return VK_FORMAT_R16G16_SNORM;
        case 6:
          return VK_FORMAT_R16G16B16_SNORM;
        case 8:
          return VK_FORMAT_R16G16B16A16_SNORM;
        default:
          break;
      }
      break;

    case GPU_COMP_U16:
      switch (size) {
        case 2:
          return VK_FORMAT_R16_UNORM;
        case 4:
          return VK_FORMAT_R16G16_UNORM;
        case 6:
          return VK_FORMAT_R16G16B16_UNORM;
        case 8:
          return VK_FORMAT_R16G16B16A16_UNORM;
        default:
          break;
      }
      break;

    case GPU_COMP_I32:
      switch (size) {
        case 4:
          return VK_FORMAT_R32_SINT;
        case 8:
          return VK_FORMAT_R32G32_SINT;
        case 12:
          return VK_FORMAT_R32G32B32_SINT;
        case 16:
          return VK_FORMAT_R32G32B32A32_SINT;
        default:
          break;
      }
      break;

    case GPU_COMP_U32:
      switch (size) {
        case 4:
          return VK_FORMAT_R32_UINT;
        case 8:
          return VK_FORMAT_R32G32_UINT;
        case 12:
          return VK_FORMAT_R32G32B32_UINT;
        case 16:
          return VK_FORMAT_R32G32B32A32_UINT;
        default:
          break;
      }
      break;

    case GPU_COMP_F32:
      switch (size) {
        case 4:
          return VK_FORMAT_R32_SFLOAT;
        case 8:
          return VK_FORMAT_R32G32_SFLOAT;
        case 12:
          return VK_FORMAT_R32G32B32_SFLOAT;
        case 16:
          return VK_FORMAT_R32G32B32A32_SFLOAT;
        case 64:
          return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
          break;
      }
      break;

    case GPU_COMP_I10:
      BLI_assert(size == 4);
      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;

    default:
      break;
  }
  BLI_assert_unreachable();
  return VK_FORMAT_R32_SFLOAT;
}

VkImageType to_vk_image_type(const eGPUTextureType type)
{
  switch (type) {
    case GPU_TEXTURE_1D:
    case GPU_TEXTURE_BUFFER:
    case GPU_TEXTURE_1D_ARRAY:
      return VK_IMAGE_TYPE_1D;
    case GPU_TEXTURE_2D:
    case GPU_TEXTURE_2D_ARRAY:
      return VK_IMAGE_TYPE_2D;
    case GPU_TEXTURE_3D:
    case GPU_TEXTURE_CUBE:
    case GPU_TEXTURE_CUBE_ARRAY:
      return VK_IMAGE_TYPE_3D;

    case GPU_TEXTURE_ARRAY:
      /* GPU_TEXTURE_ARRAY should always be used together with 1D, 2D, or CUBE*/
      break;
  }

  BLI_assert_unreachable();
  return VK_IMAGE_TYPE_1D;
}

VkImageViewType to_vk_image_view_type(const eGPUTextureType type)
{
  switch (type) {
    case GPU_TEXTURE_1D:
    case GPU_TEXTURE_BUFFER:
      return VK_IMAGE_VIEW_TYPE_1D;
    case GPU_TEXTURE_2D:
      return VK_IMAGE_VIEW_TYPE_2D;
    case GPU_TEXTURE_3D:
      return VK_IMAGE_VIEW_TYPE_3D;
    case GPU_TEXTURE_CUBE:
      return VK_IMAGE_VIEW_TYPE_CUBE;
    case GPU_TEXTURE_1D_ARRAY:
      return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    case GPU_TEXTURE_2D_ARRAY:
      return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case GPU_TEXTURE_CUBE_ARRAY:
      return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;

    case GPU_TEXTURE_ARRAY:
      /* GPU_TEXTURE_ARRAY should always be used together with 1D, 2D, or CUBE*/
      break;
  }

  BLI_assert_unreachable();
  return VK_IMAGE_VIEW_TYPE_1D;
}

VkComponentMapping to_vk_component_mapping(const eGPUTextureFormat /*format*/)
{
  /* TODO: this should map to OpenGL defaults based on the eGPUTextureFormat. The implementation of
   * this function will be implemented when implementing other parts of VKTexture. */
  VkComponentMapping component_mapping;
  component_mapping.r = VK_COMPONENT_SWIZZLE_R;
  component_mapping.g = VK_COMPONENT_SWIZZLE_G;
  component_mapping.b = VK_COMPONENT_SWIZZLE_B;
  component_mapping.a = VK_COMPONENT_SWIZZLE_A;
  return component_mapping;
}

template<typename T> void copy_color(T dst[4], const T *src)
{
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
}

VkClearColorValue to_vk_clear_color_value(const eGPUDataFormat format, const void *data)
{
  VkClearColorValue result = {0.0f};
  switch (format) {
    case GPU_DATA_FLOAT: {
      const float *float_data = static_cast<const float *>(data);
      copy_color<float>(result.float32, float_data);
      break;
    }

    case GPU_DATA_INT: {
      const int32_t *int_data = static_cast<const int32_t *>(data);
      copy_color<int32_t>(result.int32, int_data);
      break;
    }

    case GPU_DATA_UINT: {
      const uint32_t *uint_data = static_cast<const uint32_t *>(data);
      copy_color<uint32_t>(result.uint32, uint_data);
      break;
    }

    case GPU_DATA_HALF_FLOAT:
    case GPU_DATA_UBYTE:
    case GPU_DATA_UINT_24_8:
    case GPU_DATA_10_11_11_REV:
    case GPU_DATA_2_10_10_10_REV: {
      BLI_assert_unreachable();
      break;
    }
  }
  return result;
}

VkIndexType to_vk_index_type(const GPUIndexBufType index_type)
{
  switch (index_type) {
    case GPU_INDEX_U16:
      return VK_INDEX_TYPE_UINT16;
    case GPU_INDEX_U32:
      return VK_INDEX_TYPE_UINT32;
    default:
      break;
  }
  BLI_assert_unreachable();
  return VK_INDEX_TYPE_UINT16;
}

VkPrimitiveTopology to_vk_primitive_topology(const GPUPrimType prim_type)
{
  switch (prim_type) {
    case GPU_PRIM_POINTS:
      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case GPU_PRIM_LINES:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case GPU_PRIM_TRIS:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case GPU_PRIM_LINE_STRIP:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case GPU_PRIM_LINE_LOOP:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case GPU_PRIM_TRI_STRIP:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case GPU_PRIM_TRI_FAN:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    case GPU_PRIM_LINES_ADJ:
      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
    case GPU_PRIM_TRIS_ADJ:
      return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
    case GPU_PRIM_LINE_STRIP_ADJ:
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;

    case GPU_PRIM_NONE:
      break;
  }

  BLI_assert_unreachable();
  return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
}

VkCullModeFlags to_vk_cull_mode_flags(const eGPUFaceCullTest cull_test)
{
  switch (cull_test) {
    case GPU_CULL_FRONT:
      return VK_CULL_MODE_FRONT_BIT;
    case GPU_CULL_BACK:
      return VK_CULL_MODE_BACK_BIT;
    case GPU_CULL_NONE:
      return VK_CULL_MODE_NONE;
  }
  BLI_assert_unreachable();
  return VK_CULL_MODE_NONE;
}

}  // namespace blender::gpu

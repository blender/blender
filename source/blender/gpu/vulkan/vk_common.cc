/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_utildefines.h"

#include "vk_common.hh"

namespace blender::gpu {
VkImageAspectFlags to_vk_image_aspect_flag_bits(const TextureFormat format)
{
  switch (format) {
    /* Formats texture & render-buffer */
    case TextureFormat::UINT_32_32_32_32:
    case TextureFormat::UINT_32_32:
    case TextureFormat::UINT_32:
    case TextureFormat::UINT_16_16_16_16:
    case TextureFormat::UINT_16_16:
    case TextureFormat::UINT_16:
    case TextureFormat::UINT_8_8_8_8:
    case TextureFormat::UINT_8_8:
    case TextureFormat::UINT_8:
    case TextureFormat::SINT_32_32_32_32:
    case TextureFormat::SINT_32_32:
    case TextureFormat::SINT_32:
    case TextureFormat::SINT_16_16_16_16:
    case TextureFormat::SINT_16_16:
    case TextureFormat::SINT_16:
    case TextureFormat::SINT_8_8_8_8:
    case TextureFormat::SINT_8_8:
    case TextureFormat::SINT_8:
    case TextureFormat::SFLOAT_32_32_32_32:
    case TextureFormat::SFLOAT_32_32:
    case TextureFormat::SFLOAT_32:
    case TextureFormat::SFLOAT_16_16_16_16:
    case TextureFormat::SFLOAT_16_16:
    case TextureFormat::SFLOAT_16:
    case TextureFormat::UNORM_16_16_16_16:
    case TextureFormat::UNORM_16_16:
    case TextureFormat::UNORM_16:
    case TextureFormat::UNORM_8_8_8_8:
    case TextureFormat::UNORM_8_8:
    case TextureFormat::UNORM_8:
      return VK_IMAGE_ASPECT_COLOR_BIT;

    /* Special formats texture & render-buffer */
    case TextureFormat::UNORM_10_10_10_2:
    case TextureFormat::UINT_10_10_10_2:
    case TextureFormat::UFLOAT_11_11_10:
    case TextureFormat::SRGBA_8_8_8_8:
      return VK_IMAGE_ASPECT_COLOR_BIT;

    /* Depth Formats. */
    case TextureFormat::SFLOAT_32_DEPTH:
    case TextureFormat::UNORM_16_DEPTH:
      return VK_IMAGE_ASPECT_DEPTH_BIT;

    case TextureFormat::SFLOAT_32_DEPTH_UINT_8:
      return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    /* Texture only formats. */
    case TextureFormat::UINT_32_32_32:
    case TextureFormat::UINT_16_16_16:
    case TextureFormat::UINT_8_8_8:
    case TextureFormat::SINT_32_32_32:
    case TextureFormat::SINT_16_16_16:
    case TextureFormat::SINT_8_8_8:
    case TextureFormat::UNORM_16_16_16:
    case TextureFormat::UNORM_8_8_8:
    case TextureFormat::SNORM_16_16_16_16:
    case TextureFormat::SNORM_16_16_16:
    case TextureFormat::SNORM_16_16:
    case TextureFormat::SNORM_16:
    case TextureFormat::SNORM_8_8_8_8:
    case TextureFormat::SNORM_8_8_8:
    case TextureFormat::SNORM_8_8:
    case TextureFormat::SNORM_8:
    case TextureFormat::SFLOAT_32_32_32:
    case TextureFormat::SFLOAT_16_16_16:
      return VK_IMAGE_ASPECT_COLOR_BIT;

    /* Special formats, texture only. */
    case TextureFormat::SRGB_DXT1:
    case TextureFormat::SRGB_DXT3:
    case TextureFormat::SRGB_DXT5:
    case TextureFormat::SNORM_DXT1:
    case TextureFormat::SNORM_DXT3:
    case TextureFormat::SNORM_DXT5:
    case TextureFormat::SRGBA_8_8_8:
    case TextureFormat::UFLOAT_9_9_9_EXP_5:
      return VK_IMAGE_ASPECT_COLOR_BIT;

    case TextureFormat::Invalid:
      BLI_assert_unreachable();
      break;
  }
  BLI_assert_unreachable();
  return 0;
}

VkImageAspectFlags to_vk_image_aspect_flag_bits(const GPUFrameBufferBits buffers)
{
  VkImageAspectFlags result = 0;
  if (buffers & GPU_COLOR_BIT) {
    result |= VK_IMAGE_ASPECT_COLOR_BIT;
  }
  if (buffers & GPU_DEPTH_BIT) {
    result |= VK_IMAGE_ASPECT_DEPTH_BIT;
  }
  if (buffers & GPU_STENCIL_BIT) {
    result |= VK_IMAGE_ASPECT_STENCIL_BIT;
  }
  return result;
}

TextureFormat to_gpu_format(const VkFormat format)
{
  switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
      return TextureFormat::UNORM_8_8_8_8;

    case VK_FORMAT_R16G16B16A16_SFLOAT:
      return TextureFormat::SFLOAT_16_16_16_16;

    default:
      BLI_assert_unreachable();
  }
  return TextureFormat::SFLOAT_32_32_32_32;
}

VkFormat to_vk_format(const TextureFormat format)
{
#define CASE(a, b, c, blender_enum, vk_enum, e, f, g, h) \
  case TextureFormat::blender_enum: \
    return VK_FORMAT_##vk_enum;

  switch (format) {
    GPU_TEXTURE_FORMAT_EXPAND(CASE)
    case TextureFormat::Invalid:
      break;
  }
#undef CASE
  return VK_FORMAT_UNDEFINED;
}

static VkFormat to_vk_format_norm(const GPUVertCompType type, const uint32_t size)
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
        case 16:
          return VK_FORMAT_R8G8B8A8_SNORM;
        default:
          BLI_assert_unreachable();
          return VK_FORMAT_R8_SNORM;
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
        case 16:
          return VK_FORMAT_R8G8B8A8_UNORM;
        default:
          BLI_assert_unreachable();
          return VK_FORMAT_R8_UNORM;
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
          BLI_assert_unreachable();
          return VK_FORMAT_R16_SNORM;
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
          BLI_assert_unreachable();
          return VK_FORMAT_R16_UNORM;
      }
      break;

    case GPU_COMP_I10:
      BLI_assert(size == 4);
      return VK_FORMAT_A2B10G10R10_SNORM_PACK32;

    case GPU_COMP_I32:
    case GPU_COMP_U32:
    case GPU_COMP_F32:
    default:
      break;
  }
  BLI_assert_unreachable();
  return VK_FORMAT_R32_SFLOAT;
}

static VkFormat to_vk_format_float(const GPUVertCompType type, const uint32_t size)
{
  switch (type) {
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
          BLI_assert_unreachable();
          return VK_FORMAT_R32_SFLOAT;
      }

    default:
      break;
  }
  BLI_assert_unreachable();
  return VK_FORMAT_R32_SFLOAT;
}

static VkFormat to_vk_format_int(const GPUVertCompType type, const uint32_t size)
{
  switch (type) {
    case GPU_COMP_I8:
      switch (size) {
        case 1:
          return VK_FORMAT_R8_SINT;
        case 2:
          return VK_FORMAT_R8G8_SINT;
        case 3:
          return VK_FORMAT_R8G8B8_SINT;
        case 4:
          return VK_FORMAT_R8G8B8A8_SINT;
        default:
          BLI_assert_unreachable();
          return VK_FORMAT_R8_SINT;
      }
      break;

    case GPU_COMP_U8:
      switch (size) {
        case 1:
          return VK_FORMAT_R8_UINT;
        case 2:
          return VK_FORMAT_R8G8_UINT;
        case 3:
          return VK_FORMAT_R8G8B8_UINT;
        case 4:
          return VK_FORMAT_R8G8B8A8_UINT;
        default:
          BLI_assert_unreachable();
          return VK_FORMAT_R8_UINT;
      }
      break;

    case GPU_COMP_I16:
      switch (size) {
        case 2:
          return VK_FORMAT_R16_SINT;
        case 4:
          return VK_FORMAT_R16G16_SINT;
        case 6:
          return VK_FORMAT_R16G16B16_SINT;
        case 8:
          return VK_FORMAT_R16G16B16A16_SINT;
        default:
          BLI_assert_unreachable();
          return VK_FORMAT_R16_SINT;
      }
      break;

    case GPU_COMP_U16:
      switch (size) {
        case 2:
          return VK_FORMAT_R16_UINT;
        case 4:
          return VK_FORMAT_R16G16_UINT;
        case 6:
          return VK_FORMAT_R16G16B16_UINT;
        case 8:
          return VK_FORMAT_R16G16B16A16_UINT;
        default:
          BLI_assert_unreachable();
          return VK_FORMAT_R16_UINT;
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
          BLI_assert_unreachable();
          return VK_FORMAT_R32_SINT;
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
          BLI_assert_unreachable();
          return VK_FORMAT_R32_UINT;
      }
      break;

    case GPU_COMP_F32:
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
          BLI_assert_unreachable();
          return VK_FORMAT_R32_SINT;
      }
      break;

    case GPU_COMP_I10:
      BLI_assert(size == 4);
      return VK_FORMAT_A2B10G10R10_SINT_PACK32;

    default:
      break;
  }

  BLI_assert_unreachable();
  return VK_FORMAT_R32_SFLOAT;
}

VkFormat to_vk_format(const GPUVertCompType type, const uint32_t size, GPUVertFetchMode fetch_mode)
{
  switch (fetch_mode) {
    case GPU_FETCH_FLOAT:
      return to_vk_format_float(type, size);
    case GPU_FETCH_INT:
      return to_vk_format_int(type, size);
      break;
    case GPU_FETCH_INT_TO_FLOAT_UNIT:
      return to_vk_format_norm(type, size);
      break;
    default:
      break;
  }

  BLI_assert_unreachable();
  return VK_FORMAT_R32_SFLOAT;
}

VkFormat to_vk_format(const shader::Type type)
{
  switch (type) {
    case shader::Type::float_t:
      return VK_FORMAT_R32_SFLOAT;
    case shader::Type::float2_t:
      return VK_FORMAT_R32G32_SFLOAT;
    case shader::Type::float3_t:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case shader::Type::float4_t:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case shader::Type::uint_t:
      return VK_FORMAT_R32_UINT;
    case shader::Type::uint2_t:
      return VK_FORMAT_R32G32_UINT;
    case shader::Type::uint3_t:
      return VK_FORMAT_R32G32B32_UINT;
    case shader::Type::uint4_t:
      return VK_FORMAT_R32G32B32A32_UINT;
    case shader::Type::int_t:
      return VK_FORMAT_R32_SINT;
    case shader::Type::int2_t:
      return VK_FORMAT_R32G32_SINT;
    case shader::Type::int3_t:
      return VK_FORMAT_R32G32B32_SINT;
    case shader::Type::int4_t:
      return VK_FORMAT_R32G32B32A32_SINT;
    case shader::Type::float4x4_t:
      return VK_FORMAT_R32G32B32A32_SFLOAT;

    case shader::Type::float3x3_t:
    case shader::Type::bool_t:
    case shader::Type::float3_10_10_10_2_t:
    case shader::Type::uchar_t:
    case shader::Type::uchar2_t:
    case shader::Type::uchar3_t:
    case shader::Type::uchar4_t:
    case shader::Type::char_t:
    case shader::Type::char2_t:
    case shader::Type::char3_t:
    case shader::Type::char4_t:
    case shader::Type::short_t:
    case shader::Type::short2_t:
    case shader::Type::short3_t:
    case shader::Type::short4_t:
    case shader::Type::ushort_t:
    case shader::Type::ushort2_t:
    case shader::Type::ushort3_t:
    case shader::Type::ushort4_t:
      break;
  }

  BLI_assert_unreachable();
  return VK_FORMAT_R32G32B32A32_SFLOAT;
}

VkQueryType to_vk_query_type(const GPUQueryType query_type)
{
  switch (query_type) {
    case GPU_QUERY_OCCLUSION:
      return VK_QUERY_TYPE_OCCLUSION;
  }
  BLI_assert_unreachable();
  return VK_QUERY_TYPE_OCCLUSION;
}

VkImageType to_vk_image_type(const GPUTextureType type)
{
  /* See
   * https://vulkan.lunarg.com/doc/view/1.3.243.0/linux/1.3-extensions/vkspec.html#resources-image-views-compatibility
   * for reference */
  switch (type) {
    case GPU_TEXTURE_1D:
    case GPU_TEXTURE_BUFFER:
    case GPU_TEXTURE_1D_ARRAY:
      return VK_IMAGE_TYPE_1D;

    case GPU_TEXTURE_2D:
    case GPU_TEXTURE_2D_ARRAY:
    case GPU_TEXTURE_CUBE:
    case GPU_TEXTURE_CUBE_ARRAY:
      return VK_IMAGE_TYPE_2D;

    case GPU_TEXTURE_3D:
      return VK_IMAGE_TYPE_3D;

    case GPU_TEXTURE_ARRAY:
      /* GPU_TEXTURE_ARRAY should always be used together with 1D, 2D, or CUBE. */
      break;
  }

  BLI_assert_unreachable();
  return VK_IMAGE_TYPE_1D;
}

VkImageViewType to_vk_image_view_type(const GPUTextureType type,
                                      const eImageViewUsage view_type,
                                      VKImageViewArrayed arrayed)
{
  VkImageViewType result = VK_IMAGE_VIEW_TYPE_1D;

  switch (type) {
    case GPU_TEXTURE_1D:
    case GPU_TEXTURE_BUFFER:
      result = VK_IMAGE_VIEW_TYPE_1D;
      break;
    case GPU_TEXTURE_2D:
      result = VK_IMAGE_VIEW_TYPE_2D;
      break;
    case GPU_TEXTURE_3D:
      result = VK_IMAGE_VIEW_TYPE_3D;
      break;
    case GPU_TEXTURE_CUBE:
      result = view_type == eImageViewUsage::Attachment ? VK_IMAGE_VIEW_TYPE_2D_ARRAY :
                                                          VK_IMAGE_VIEW_TYPE_CUBE;
      break;
    case GPU_TEXTURE_1D_ARRAY:
      result = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
      break;
    case GPU_TEXTURE_2D_ARRAY:
      result = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
      break;
    case GPU_TEXTURE_CUBE_ARRAY:
      result = view_type == eImageViewUsage::Attachment ? VK_IMAGE_VIEW_TYPE_2D_ARRAY :
                                                          VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
      break;

    case GPU_TEXTURE_ARRAY:
      /* GPU_TEXTURE_ARRAY should always be used together with 1D, 2D, or CUBE. */
      break;
  }

  if (arrayed == VKImageViewArrayed::NOT_ARRAYED) {
    if (result == VK_IMAGE_VIEW_TYPE_1D_ARRAY) {
      result = VK_IMAGE_VIEW_TYPE_1D;
    }
    else if (result == VK_IMAGE_VIEW_TYPE_2D_ARRAY) {
      result = VK_IMAGE_VIEW_TYPE_2D;
    }
    else if (result == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY) {
      result = VK_IMAGE_VIEW_TYPE_CUBE;
    }
  }
  else if (arrayed == VKImageViewArrayed::ARRAYED) {
    if (result == VK_IMAGE_VIEW_TYPE_1D) {
      result = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    }
    else if (result == VK_IMAGE_VIEW_TYPE_2D) {
      result = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    }
    else if (result == VK_IMAGE_VIEW_TYPE_CUBE) {
      result = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    }
  }

  return result;
}

VkComponentSwizzle to_vk_component_swizzle(const char swizzle)
{
  switch (swizzle) {
    case '0':
      return VK_COMPONENT_SWIZZLE_ZERO;
    case '1':
      return VK_COMPONENT_SWIZZLE_ONE;
    case 'r':
      return VK_COMPONENT_SWIZZLE_R;
    case 'g':
      return VK_COMPONENT_SWIZZLE_G;
    case 'b':
      return VK_COMPONENT_SWIZZLE_B;
    case 'a':
      return VK_COMPONENT_SWIZZLE_A;

    default:
      break;
  }
  BLI_assert_unreachable();
  return VK_COMPONENT_SWIZZLE_IDENTITY;
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
  VkClearColorValue result = {{0.0f}};
  switch (format) {
    /* All float-like formats (i.e. everything except literal int/uint go
     * into VkClearColorValue float color fields. */
    case GPU_DATA_FLOAT:
    case GPU_DATA_HALF_FLOAT:
    case GPU_DATA_UBYTE:
    case GPU_DATA_10_11_11_REV:
    case GPU_DATA_2_10_10_10_REV: {
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
    case GPU_DATA_UINT_24_8_DEPRECATED: {
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
      return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
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

VkCullModeFlags to_vk_cull_mode_flags(const GPUFaceCullTest cull_test)
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

VkSamplerAddressMode to_vk_sampler_address_mode(const GPUSamplerExtendMode extend_mode)
{
  switch (extend_mode) {
    case GPU_SAMPLER_EXTEND_MODE_EXTEND:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case GPU_SAMPLER_EXTEND_MODE_REPEAT:
      return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case GPU_SAMPLER_EXTEND_MODE_MIRRORED_REPEAT:
      return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER:
      return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  }

  BLI_assert_unreachable();
  return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

static VkDescriptorType to_vk_descriptor_type_image(const shader::ImageType &image_type)
{
  switch (image_type) {
    case shader::ImageType::Float1D:
    case shader::ImageType::Float1DArray:
    case shader::ImageType::Float2D:
    case shader::ImageType::Float2DArray:
    case shader::ImageType::Float3D:
    case shader::ImageType::FloatCube:
    case shader::ImageType::FloatCubeArray:
    case shader::ImageType::Int1D:
    case shader::ImageType::Int1DArray:
    case shader::ImageType::Int2D:
    case shader::ImageType::Int2DArray:
    case shader::ImageType::Int3D:
    case shader::ImageType::IntCube:
    case shader::ImageType::IntCubeArray:
    case shader::ImageType::AtomicInt2D:
    case shader::ImageType::AtomicInt2DArray:
    case shader::ImageType::AtomicInt3D:
    case shader::ImageType::Uint1D:
    case shader::ImageType::Uint1DArray:
    case shader::ImageType::Uint2D:
    case shader::ImageType::Uint2DArray:
    case shader::ImageType::Uint3D:
    case shader::ImageType::UintCube:
    case shader::ImageType::UintCubeArray:
    case shader::ImageType::AtomicUint2D:
    case shader::ImageType::AtomicUint2DArray:
    case shader::ImageType::AtomicUint3D:
      return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    case shader::ImageType::FloatBuffer:
    case shader::ImageType::IntBuffer:
    case shader::ImageType::UintBuffer:
      return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;

    default:
      BLI_assert_msg(false, "ImageType not supported.");
  }

  return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
}

static VkDescriptorType to_vk_descriptor_type_sampler(const shader::ImageType &image_type)
{
  switch (image_type) {
    case shader::ImageType::undefined:
    case shader::ImageType::Float1D:
    case shader::ImageType::Float1DArray:
    case shader::ImageType::Float2D:
    case shader::ImageType::Float2DArray:
    case shader::ImageType::Float3D:
    case shader::ImageType::FloatCube:
    case shader::ImageType::FloatCubeArray:
    case shader::ImageType::Int1D:
    case shader::ImageType::Int1DArray:
    case shader::ImageType::Int2D:
    case shader::ImageType::Int2DArray:
    case shader::ImageType::Int3D:
    case shader::ImageType::IntCube:
    case shader::ImageType::IntCubeArray:
    case shader::ImageType::AtomicInt2D:
    case shader::ImageType::AtomicInt2DArray:
    case shader::ImageType::AtomicInt3D:
    case shader::ImageType::Uint1D:
    case shader::ImageType::Uint1DArray:
    case shader::ImageType::Uint2D:
    case shader::ImageType::Uint2DArray:
    case shader::ImageType::Uint3D:
    case shader::ImageType::UintCube:
    case shader::ImageType::UintCubeArray:
    case shader::ImageType::AtomicUint2D:
    case shader::ImageType::AtomicUint2DArray:
    case shader::ImageType::AtomicUint3D:
    case shader::ImageType::Shadow2D:
    case shader::ImageType::Shadow2DArray:
    case shader::ImageType::ShadowCube:
    case shader::ImageType::ShadowCubeArray:
    case shader::ImageType::Depth2D:
    case shader::ImageType::Depth2DArray:
    case shader::ImageType::DepthCube:
    case shader::ImageType::DepthCubeArray:
      return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    case shader::ImageType::FloatBuffer:
    case shader::ImageType::IntBuffer:
    case shader::ImageType::UintBuffer:
      return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
  }

  return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
}

VkDescriptorType to_vk_descriptor_type(const shader::ShaderCreateInfo::Resource &resource)
{
  switch (resource.bind_type) {
    case shader::ShaderCreateInfo::Resource::BindType::IMAGE:
      return to_vk_descriptor_type_image(resource.image.type);
    case shader::ShaderCreateInfo::Resource::BindType::SAMPLER:
      return to_vk_descriptor_type_sampler(resource.sampler.type);
    case shader::ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER:
      return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER:
      return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  }
  BLI_assert_unreachable();
  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

}  // namespace blender::gpu

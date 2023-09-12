/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_utildefines.h"

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
      return VK_IMAGE_ASPECT_DEPTH_BIT;

    case GPU_DEPTH32F_STENCIL8:
    case GPU_DEPTH24_STENCIL8:
      return static_cast<VkImageAspectFlagBits>(VK_IMAGE_ASPECT_DEPTH_BIT |
                                                VK_IMAGE_ASPECT_STENCIL_BIT);

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

VkImageAspectFlagBits to_vk_image_aspect_flag_bits(const eGPUFrameBufferBits buffers)
{
  VkImageAspectFlagBits result = static_cast<VkImageAspectFlagBits>(0);
  if (buffers & GPU_COLOR_BIT) {
    result = static_cast<VkImageAspectFlagBits>(result | VK_IMAGE_ASPECT_COLOR_BIT);
  }
  if (buffers & GPU_DEPTH_BIT) {
    result = static_cast<VkImageAspectFlagBits>(result | VK_IMAGE_ASPECT_DEPTH_BIT);
  }
  if (buffers & GPU_STENCIL_BIT) {
    result = static_cast<VkImageAspectFlagBits>(result | VK_IMAGE_ASPECT_STENCIL_BIT);
  }
  return result;
}

eGPUTextureFormat to_gpu_format(const VkFormat format)
{
  switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
      return GPU_RGBA8;

    default:
      BLI_assert_unreachable();
  }
  return GPU_RGBA32F;
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
    case GPU_COMP_I8:
      switch (size) {
        case 1:
          return VK_FORMAT_R8_SSCALED;
        case 2:
          return VK_FORMAT_R8G8_SSCALED;
        case 3:
          return VK_FORMAT_R8G8B8_SSCALED;
        case 4:
          return VK_FORMAT_R8G8B8A8_SSCALED;
        default:
          BLI_assert_unreachable();
          return VK_FORMAT_R8_SSCALED;
      }
    case GPU_COMP_U8:
      switch (size) {
        case 1:
          return VK_FORMAT_R8_USCALED;
        case 2:
          return VK_FORMAT_R8G8_USCALED;
        case 3:
          return VK_FORMAT_R8G8B8_USCALED;
        case 4:
          return VK_FORMAT_R8G8B8A8_USCALED;
        default:
          BLI_assert_unreachable();
          return VK_FORMAT_R8_USCALED;
      }
    case GPU_COMP_I16:
      switch (size) {
        case 2:
          return VK_FORMAT_R16_SSCALED;
        case 4:
          return VK_FORMAT_R16G16_SSCALED;
        case 6:
          return VK_FORMAT_R16G16B16_SSCALED;
        case 8:
          return VK_FORMAT_R16G16B16A16_SSCALED;
        default:
          BLI_assert_unreachable();
          return VK_FORMAT_R16_SSCALED;
      }
    case GPU_COMP_U16:
      switch (size) {
        case 2:
          return VK_FORMAT_R16_USCALED;
        case 4:
          return VK_FORMAT_R16G16_USCALED;
        case 6:
          return VK_FORMAT_R16G16B16_USCALED;
        case 8:
          return VK_FORMAT_R16G16B16A16_USCALED;
        default:
          BLI_assert_unreachable();
          return VK_FORMAT_R16_USCALED;
      }

    case GPU_COMP_I32:
    case GPU_COMP_U32:
      /* NOTE: GPU_COMP_I32/U32 using GPU_FETCH_INT_TO_FLOAT isn't natively supported. These
       * are converted on host-side to signed floats. */
      switch (size) {
        case 4:
          return VK_FORMAT_R32_SFLOAT;
        case 8:
          return VK_FORMAT_R32G32_SFLOAT;
        case 12:
          return VK_FORMAT_R32G32B32_SFLOAT;
        case 16:
          return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
          BLI_assert_unreachable();
          return VK_FORMAT_R32_SFLOAT;
      }

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

    case GPU_COMP_I10:
      BLI_assert(size == 4);
      return VK_FORMAT_A2B10G10R10_SSCALED_PACK32;

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
    case GPU_FETCH_INT_TO_FLOAT:
      return to_vk_format_float(type, size);
      break;
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
    case shader::Type::FLOAT:
      return VK_FORMAT_R32_SFLOAT;
    case shader::Type::VEC2:
      return VK_FORMAT_R32G32_SFLOAT;
    case shader::Type::VEC3:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case shader::Type::VEC4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case shader::Type::UINT:
      return VK_FORMAT_R32_UINT;
    case shader::Type::UVEC2:
      return VK_FORMAT_R32G32_UINT;
    case shader::Type::UVEC3:
      return VK_FORMAT_R32G32B32_UINT;
    case shader::Type::UVEC4:
      return VK_FORMAT_R32G32B32A32_UINT;
    case shader::Type::INT:
      return VK_FORMAT_R32_SINT;
    case shader::Type::IVEC2:
      return VK_FORMAT_R32G32_SINT;
    case shader::Type::IVEC3:
      return VK_FORMAT_R32G32B32_SINT;
    case shader::Type::IVEC4:
      return VK_FORMAT_R32G32B32A32_SINT;
    case shader::Type::MAT4:
      return VK_FORMAT_R32G32B32A32_SFLOAT;

    case shader::Type::MAT3:
    case shader::Type::BOOL:
    case shader::Type::VEC3_101010I2:
    case shader::Type::UCHAR:
    case shader::Type::UCHAR2:
    case shader::Type::UCHAR3:
    case shader::Type::UCHAR4:
    case shader::Type::CHAR:
    case shader::Type::CHAR2:
    case shader::Type::CHAR3:
    case shader::Type::CHAR4:
    case shader::Type::SHORT:
    case shader::Type::SHORT2:
    case shader::Type::SHORT3:
    case shader::Type::SHORT4:
    case shader::Type::USHORT:
    case shader::Type::USHORT2:
    case shader::Type::USHORT3:
    case shader::Type::USHORT4:
      break;
  }

  BLI_assert_unreachable();
  return VK_FORMAT_R32G32B32A32_SFLOAT;
}

VkImageType to_vk_image_type(const eGPUTextureType type)
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
      /* GPU_TEXTURE_ARRAY should always be used together with 1D, 2D, or CUBE*/
      break;
  }

  BLI_assert_unreachable();
  return VK_IMAGE_TYPE_1D;
}

VkImageViewType to_vk_image_view_type(const eGPUTextureType type, const eImageViewUsage view_type)
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
      return view_type == eImageViewUsage::Attachment ? VK_IMAGE_VIEW_TYPE_2D_ARRAY :
                                                        VK_IMAGE_VIEW_TYPE_CUBE;
    case GPU_TEXTURE_1D_ARRAY:
      return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    case GPU_TEXTURE_2D_ARRAY:
      return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case GPU_TEXTURE_CUBE_ARRAY:
      return view_type == eImageViewUsage::Attachment ? VK_IMAGE_VIEW_TYPE_2D_ARRAY :
                                                        VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;

    case GPU_TEXTURE_ARRAY:
      /* GPU_TEXTURE_ARRAY should always be used together with 1D, 2D, or CUBE*/
      break;
  }

  BLI_assert_unreachable();
  return VK_IMAGE_VIEW_TYPE_1D;
}

VkComponentMapping to_vk_component_mapping(const eGPUTextureFormat /*format*/)
{
  /* TODO: this should map to OpenGL defaults based on the eGPUTextureFormat. The
   * implementation of this function will be implemented when implementing other parts of
   * VKTexture. */
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
  VkClearColorValue result = {{0.0f}};
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

const char *to_string(VkObjectType type)
{

  switch (type) {
    case VK_OBJECT_TYPE_UNKNOWN:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_UNKNOWN);
    case VK_OBJECT_TYPE_INSTANCE:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_INSTANCE);
    case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_PHYSICAL_DEVICE);
    case VK_OBJECT_TYPE_DEVICE:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_DEVICE);
    case VK_OBJECT_TYPE_QUEUE:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_QUEUE);
    case VK_OBJECT_TYPE_SEMAPHORE:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_SEMAPHORE);
    case VK_OBJECT_TYPE_COMMAND_BUFFER:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_COMMAND_BUFFER);
    case VK_OBJECT_TYPE_FENCE:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_FENCE);
    case VK_OBJECT_TYPE_DEVICE_MEMORY:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_DEVICE_MEMORY);
    case VK_OBJECT_TYPE_BUFFER:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_BUFFER);
    case VK_OBJECT_TYPE_IMAGE:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_IMAGE);
    case VK_OBJECT_TYPE_EVENT:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_EVENT);
    case VK_OBJECT_TYPE_QUERY_POOL:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_QUERY_POOL);
    case VK_OBJECT_TYPE_BUFFER_VIEW:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_BUFFER_VIEW);
    case VK_OBJECT_TYPE_IMAGE_VIEW:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_IMAGE_VIEW);
    case VK_OBJECT_TYPE_SHADER_MODULE:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_SHADER_MODULE);
    case VK_OBJECT_TYPE_PIPELINE_CACHE:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_PIPELINE_CACHE);
    case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_PIPELINE_LAYOUT);
    case VK_OBJECT_TYPE_RENDER_PASS:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_RENDER_PASS);
    case VK_OBJECT_TYPE_PIPELINE:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_PIPELINE);
    case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
    case VK_OBJECT_TYPE_SAMPLER:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_SAMPLER);
    case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_DESCRIPTOR_POOL);
    case VK_OBJECT_TYPE_DESCRIPTOR_SET:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_DESCRIPTOR_SET);
    case VK_OBJECT_TYPE_FRAMEBUFFER:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_FRAMEBUFFER);
    case VK_OBJECT_TYPE_COMMAND_POOL:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_COMMAND_POOL);
    case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION);
    case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE);
    case VK_OBJECT_TYPE_SURFACE_KHR:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_SURFACE_KHR);
    case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_SWAPCHAIN_KHR);
    case VK_OBJECT_TYPE_DISPLAY_KHR:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_DISPLAY_KHR);
    case VK_OBJECT_TYPE_DISPLAY_MODE_KHR:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_DISPLAY_MODE_KHR);
    case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT);
#ifdef VK_ENABLE_BETA_EXTENSIONS
    case VK_OBJECT_TYPE_VIDEO_SESSION_KHR:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_VIDEO_SESSION_KHR);
#endif
#ifdef VK_ENABLE_BETA_EXTENSIONS
    case VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_VIDEO_SESSION_PARAMETERS_KHR);
#endif
    case VK_OBJECT_TYPE_CU_MODULE_NVX:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_CU_MODULE_NVX);
    case VK_OBJECT_TYPE_CU_FUNCTION_NVX:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_CU_FUNCTION_NVX);
    case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT);
    case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR);
    case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_VALIDATION_CACHE_EXT);
    case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV);
    case VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL);
    case VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR);
    case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV);
    case VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT);
    case VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA:
      return STRINGIFY_ARG(VK_OBJECT_TYPE_BUFFER_COLLECTION_FUCHSIA);
    default:
      BLI_assert_unreachable();
  }
  return "NotFound";
};
}  // namespace blender::gpu

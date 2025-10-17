/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_texture_private.hh"

#include "vk_context.hh"
#include "vk_image_view.hh"
#include "vk_memory.hh"

#include "BLI_enum_flags.hh"

namespace blender::gpu {

class VKSampler;
class VKDescriptorSetTracker;
class VKVertexBuffer;
class VKPixelBuffer;

/** Additional modifiers when requesting image views. */
enum class VKImageViewFlags {
  DEFAULT = 0,
  NO_SWIZZLING = 1 << 0,
};
ENUM_OPERATORS(VKImageViewFlags)

class VKTexture : public Texture {
  friend class VKDescriptorSetUpdator;
  friend class VKContext;

  /**
   * Texture format how the texture is stored on the device.
   *
   * This can be a different format then #Texture.format_ in case the texture format isn't natively
   * supported by the device.
   */
  TextureFormat device_format_ = TextureFormat::Invalid;

  /** When set the instance is considered to be a texture view from `source_texture_` */
  VKTexture *source_texture_ = nullptr;

  /**
   * Store of source vertex buffer. Related to `GPU_texture_create_from_vertbuf`.
   *
   * In vulkan a texel buffer is a buffer and not a texture. Calls will be forwarded to the vertex
   * buffer in this case. GPU_texture_create_from_vertbuf should be phased out (currently only used
   * by particle hair).
   */
  VKVertexBuffer *source_buffer_ = nullptr;
  VkImage vk_image_ = VK_NULL_HANDLE;
  VmaAllocation allocation_ = VK_NULL_HANDLE;
  VmaAllocationInfo allocation_info_ = {};

  /**
   * Image views are owned by VKTexture. When a specific image view is needed it will be created
   * and stored here. Image view can be requested by calling `image_view_get` method.
   */
  Vector<VKImageView> image_views_;

  int layer_offset_ = 0;
  bool use_stencil_ = false;

  char swizzle_[4] = {'r', 'g', 'b', 'a'};
  VKImageViewInfo image_view_info_ = {eImageViewUsage::ShaderBinding,
                                      IndexRange(0, VK_REMAINING_ARRAY_LAYERS),
                                      IndexRange(0, VK_REMAINING_MIP_LEVELS),
                                      {{'r', 'g', 'b', 'a'}},
                                      false,
                                      false,
                                      VKImageViewArrayed::DONT_CARE};

 public:
  VKTexture(const char *name) : Texture(name) {}

  virtual ~VKTexture() override;

  void generate_mipmap() override;
  void copy_to(Texture *tex) override;
  void copy_to(VKTexture &dst_texture, VkImageAspectFlags vk_image_aspect);
  void clear(eGPUDataFormat format, const void *data) override;
  void clear_depth_stencil(const GPUFrameBufferBits buffer,
                           float clear_depth,
                           uint clear_stencil,
                           std::optional<int> layer);
  void swizzle_set(const char swizzle_mask[4]) override;
  void mip_range_set(int min, int max) override;
  void *read(int mip, eGPUDataFormat format) override;
  void read_sub(
      int mip, eGPUDataFormat format, const int region[6], IndexRange layers, void *r_data);
  void update_sub(int mip,
                  int offset[3],
                  int extent[3],
                  eGPUDataFormat format,
                  const void *data,
                  VKPixelBuffer *pixel_buffer);

  void update_sub(
      int mip, int offset[3], int extent[3], eGPUDataFormat format, const void *data) override;
  void update_sub(int offset[3],
                  int extent[3],
                  eGPUDataFormat format,
                  GPUPixelBuffer *pixbuf) override;

  /**
   * Export the memory associated with this texture to be imported by a different
   * API/Process/Instance.
   *
   * Returns the handle + offset of the image inside the handle.
   */
  VKMemoryExport export_memory(VkExternalMemoryHandleTypeFlagBits handle_type);

  VkImage vk_image_handle() const
  {
    if (is_texture_view()) {
      return source_texture_->vk_image_handle();
    }
    BLI_assert(vk_image_ != VK_NULL_HANDLE);
    return vk_image_;
  }

  /**
   * Get the texture format how the texture is stored on the device.
   */
  TextureFormat device_format_get() const
  {
    return device_format_;
  }

  /**
   * Get a specific image view for this texture. The specification of the image view are passed
   * inside the `info` parameter.
   */
  const VKImageView &image_view_get(const VKImageViewInfo &info);

  /**
   * Get the current image view for this texture.
   */
  const VKImageView &image_view_get(VKImageViewArrayed arrayed, VKImageViewFlags flags);

 protected:
  bool init_internal() override;
  bool init_internal(VertBuf *vbo) override;
  bool init_internal(gpu::Texture *src,
                     int mip_offset,
                     int layer_offset,
                     bool use_stencil) override;
  /* Initialize VKTexture with a swapchain image. */
  void init_swapchain(VkImage vk_image, TextureFormat gpu_format);

 private:
  /** Is this texture a view of another texture. */
  bool is_texture_view() const;

  /**
   * Allocate the texture of the device. Result is `true` when texture is successfully allocated
   * on the device.
   */
  bool allocate();

  /**
   * Determine the layerCount for vulkan based on the texture type. Will pass the
   * #non_layered_value for non layered textures.
   */
  int vk_layer_count(int non_layered_value) const;

  /**
   * Determine the VkExtent3D for the given mip_level.
   */
  VkExtent3D vk_extent_3d(int mip_level) const;

  /* -------------------------------------------------------------------- */
  /** \name Image Views
   * \{ */

 private:
  IndexRange mip_map_range() const;
  IndexRange layer_range() const;

  /** \} */
};

BLI_INLINE VKTexture *unwrap(Texture *tex)
{
  return static_cast<VKTexture *>(tex);
}

BLI_INLINE Texture *wrap(VKTexture *texture)
{
  return static_cast<Texture *>(texture);
}

}  // namespace blender::gpu

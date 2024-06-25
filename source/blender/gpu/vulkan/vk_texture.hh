/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_texture_private.hh"

#include "vk_bindable_resource.hh"
#include "vk_context.hh"
#include "vk_image_view.hh"

namespace blender::gpu {

class VKSampler;

class VKTexture : public Texture, public VKBindableResource {
  /**
   * Texture format how the texture is stored on the device.
   *
   * This can be a different format then #Texture.format_ in case the texture format isn't natively
   * supported by the device.
   */
  eGPUTextureFormat device_format_ = (eGPUTextureFormat)-1;

  /** When set the instance is considered to be a texture view from `source_texture_` */
  VKTexture *source_texture_ = nullptr;
  VkImage vk_image_ = VK_NULL_HANDLE;
  VmaAllocation allocation_ = VK_NULL_HANDLE;

  /**
   * Image views are owned by VKTexture. When a specific image view is needed it will be created
   * and stored here. Image view can be requested by calling `image_view_get` method.
   */
  Vector<VKImageView> image_views_;

  /* Last image layout of the texture. Frame-buffer and barriers can alter/require the actual
   * layout to be changed. During this it requires to set the current layout in order to know which
   * conversion should happen. #current_layout_ keep track of the layout so the correct conversion
   * can be done. */
  VkImageLayout current_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;

  int layer_offset_ = 0;
  bool use_stencil_ = false;

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

  void init(VkImage vk_image, VkImageLayout layout, eGPUTextureFormat texture_format);

  void generate_mipmap() override;
  void copy_to(Texture *tex) override;
  void copy_to(VKTexture &dst_texture, VkImageAspectFlags vk_image_aspect);
  void clear(eGPUDataFormat format, const void *data) override;
  void clear_depth_stencil(const eGPUFrameBufferBits buffer,
                           float clear_depth,
                           uint clear_stencil);
  void swizzle_set(const char swizzle_mask[4]) override;
  void mip_range_set(int min, int max) override;
  void *read(int mip, eGPUDataFormat format) override;
  void read_sub(
      int mip, eGPUDataFormat format, const int area[6], IndexRange layers, void *r_data);
  void update_sub(
      int mip, int offset[3], int extent[3], eGPUDataFormat format, const void *data) override;
  void update_sub(int offset[3],
                  int extent[3],
                  eGPUDataFormat format,
                  GPUPixelBuffer *pixbuf) override;

  /* TODO(fclem): Legacy. Should be removed at some point. */
  uint gl_bindcode_get() const override;

  void add_to_descriptor_set(AddToDescriptorSetContext &data,
                             int location,
                             shader::ShaderCreateInfo::Resource::BindType bind_type,
                             const GPUSamplerState sampler_state) override;

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
  eGPUTextureFormat device_format_get() const
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
  const VKImageView &image_view_get(VKImageViewArrayed arrayed);

 protected:
  bool init_internal() override;
  bool init_internal(VertBuf *vbo) override;
  bool init_internal(GPUTexture *src, int mip_offset, int layer_offset, bool use_stencil) override;

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

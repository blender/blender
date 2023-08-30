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
  VkImage vk_image_ = VK_NULL_HANDLE;
  VmaAllocation allocation_ = VK_NULL_HANDLE;

  /* Image view when used in a shader. */
  std::optional<VKImageView> image_view_;

  /* Last image layout of the texture. Frame-buffer and barriers can alter/require the actual
   * layout to be changed. During this it requires to set the current layout in order to know which
   * conversion should happen. #current_layout_ keep track of the layout so the correct conversion
   * can be done. */
  VkImageLayout current_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;

  enum eDirtyFlags {
    IMAGE_VIEW_DIRTY = (1 << 0),
  };

  int flags_ = IMAGE_VIEW_DIRTY;

 public:
  VKTexture(const char *name) : Texture(name) {}

  virtual ~VKTexture() override;

  void init(VkImage vk_image, VkImageLayout layout, eGPUTextureFormat texture_format);

  void generate_mipmap() override;
  void copy_to(Texture *tex) override;
  void clear(eGPUDataFormat format, const void *data) override;
  void clear_depth_stencil(const eGPUFrameBufferBits buffer,
                           float clear_depth,
                           uint clear_stencil);
  void swizzle_set(const char swizzle_mask[4]) override;
  void mip_range_set(int min, int max) override;
  void *read(int mip, eGPUDataFormat format) override;
  void read_sub(int mip, eGPUDataFormat format, const int area[4], void *r_data);
  void update_sub(
      int mip, int offset[3], int extent[3], eGPUDataFormat format, const void *data) override;
  void update_sub(int offset[3],
                  int extent[3],
                  eGPUDataFormat format,
                  GPUPixelBuffer *pixbuf) override;

  /* TODO(fclem): Legacy. Should be removed at some point. */
  uint gl_bindcode_get() const override;

  void bind(int location, shader::ShaderCreateInfo::Resource::BindType bind_type) override;

  VkImage vk_image_handle() const
  {
    BLI_assert(vk_image_ != VK_NULL_HANDLE);
    return vk_image_;
  }

  void ensure_allocated();

 protected:
  bool init_internal() override;
  bool init_internal(GPUVertBuf *vbo) override;
  bool init_internal(GPUTexture *src, int mip_offset, int layer_offset, bool use_stencil) override;

 private:
  /** Is this texture already allocated on device. */
  bool is_allocated() const;

  /**
   * Allocate the texture of the device. Result is `true` when texture is successfully allocated
   * on the device.
   */
  bool allocate();

  int layer_count();

  VkImageViewType vk_image_view_type() const;

  /* -------------------------------------------------------------------- */
  /** \name Image Layout
   * \{ */
 public:
  /**
   * Update the current layout attribute, without actually changing the layout.
   *
   * Vulkan can change the layout of an image, when a command is being executed.
   * The start of a render pass or the end of a render pass can also alter the
   * actual layout of the image. This method allows to change the last known layout
   * that the image is using.
   *
   * NOTE: When we add command encoding, this should partly being done inside
   * the command encoder, as there is more accurate determination of the transition
   * of the layout. Only the final transition should then be stored inside the texture
   * to be used by as initial layout for the next set of commands.
   */
  void current_layout_set(VkImageLayout new_layout);
  VkImageLayout current_layout_get() const;

  /**
   * Ensure the layout of the texture. This also performs the conversion by adding a memory
   * barrier to the active command buffer to perform the conversion.
   *
   * When texture is already in the requested layout, nothing will be done.
   */
  void layout_ensure(VKContext &context, VkImageLayout requested_layout);

 private:
  /**
   * Internal function to ensure the layout of a single mipmap level. Note that the caller is
   * responsible to update the current_layout of the image at the end of the operation and make
   * sure that all mipmap levels are in that given layout.
   */
  void layout_ensure(VKContext &context,
                     IndexRange mipmap_range,
                     VkImageLayout current_layout,
                     VkImageLayout requested_layout);

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Image Views
   * \{ */
 public:
  VKImageView &image_view_get()
  {
    image_view_ensure();
    return *image_view_;
  }

 private:
  IndexRange mip_map_range() const;
  void image_view_ensure();
  void image_view_update();

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

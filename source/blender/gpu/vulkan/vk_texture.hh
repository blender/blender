/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_texture_private.hh"
#include "vk_context.hh"

#include "vk_mem_alloc.h"

namespace blender::gpu {

class VKTexture : public Texture {
  VkImage vk_image_ = VK_NULL_HANDLE;
  VkImageView vk_image_view_ = VK_NULL_HANDLE;
  VmaAllocation allocation_ = VK_NULL_HANDLE;

 public:
  VKTexture(const char *name) : Texture(name)
  {
  }
  virtual ~VKTexture() override;

  void generate_mipmap() override;
  void copy_to(Texture *tex) override;
  void clear(eGPUDataFormat format, const void *data) override;
  void swizzle_set(const char swizzle_mask[4]) override;
  void stencil_texture_mode_set(bool use_stencil) override;
  void mip_range_set(int min, int max) override;
  void *read(int mip, eGPUDataFormat format) override;
  void update_sub(
      int mip, int offset[3], int extent[3], eGPUDataFormat format, const void *data) override;
  void update_sub(int offset[3],
                  int extent[3],
                  eGPUDataFormat format,
                  GPUPixelBuffer *pixbuf) override;

  /* TODO(fclem): Legacy. Should be removed at some point. */
  uint gl_bindcode_get() const override;

  void image_bind(int location);
  VkImage vk_image_handle() const
  {
    return vk_image_;
  }
  VkImageView vk_image_view_handle() const
  {
    return vk_image_view_;
  }

 protected:
  bool init_internal() override;
  bool init_internal(GPUVertBuf *vbo) override;
  bool init_internal(const GPUTexture *src, int mip_offset, int layer_offset) override;

 private:
  /** Is this texture already allocated on device.*/
  bool is_allocated();
  /**
   * Allocate the texture of the device. Result is `true` when texture is successfully allocated
   * on the device.
   */
  bool allocate();

  VkImageViewType vk_image_view_type() const;
};

static inline VKTexture *unwrap(Texture *tex)
{
  return static_cast<VKTexture *>(tex);
}

}  // namespace blender::gpu

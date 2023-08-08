/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_math_vector.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "gpu_framebuffer_private.hh"

#include "vk_common.hh"
#include "vk_image_view.hh"

namespace blender::gpu {

class VKFrameBuffer : public FrameBuffer {
 private:
  /* Vulkan object handle. */
  VkFramebuffer vk_framebuffer_ = VK_NULL_HANDLE;
  /* Vulkan device who created the handle. */
  VkDevice vk_device_ = VK_NULL_HANDLE;
  /* Base render pass used for framebuffer creation. */
  VkRenderPass vk_render_pass_ = VK_NULL_HANDLE;
  VkImage vk_image_ = VK_NULL_HANDLE;
  /* Number of layers if the attachments are layered textures. */
  int depth_ = 1;
  /** Internal frame-buffers are immutable. */
  bool immutable_;

  /**
   * Should we flip the viewport to match Blenders coordinate system. We flip the viewport for
   * off-screen frame-buffers.
   *
   * When two frame-buffers are blitted we also check if the coordinate system should be flipped
   * during blitting.
   */
  bool flip_viewport_ = false;

  Vector<VKImageView, GPU_FB_MAX_ATTACHMENT> image_views_;

 public:
  /**
   * Create a conventional framebuffer to attach texture to.
   **/
  VKFrameBuffer(const char *name);

  /**
   * Special frame-buffer encapsulating internal window frame-buffer.
   * This just act as a wrapper, the actual allocations are done by GHOST_ContextVK.
   **/
  VKFrameBuffer(const char *name,
                VkImage vk_image,
                VkFramebuffer vk_framebuffer,
                VkRenderPass vk_render_pass,
                VkExtent2D vk_extent);

  ~VKFrameBuffer();

  void bind(bool enabled_srgb) override;
  bool check(char err_out[256]) override;
  void clear(eGPUFrameBufferBits buffers,
             const float clear_color[4],
             float clear_depth,
             uint clear_stencil) override;
  void clear_multi(const float (*clear_color)[4]) override;
  void clear_attachment(GPUAttachmentType type,
                        eGPUDataFormat data_format,
                        const void *clear_value) override;

  void attachment_set_loadstore_op(GPUAttachmentType type,
                                   eGPULoadOp load_action,
                                   eGPUStoreOp store_action) override;

  void read(eGPUFrameBufferBits planes,
            eGPUDataFormat format,
            const int area[4],
            int channel_len,
            int slot,
            void *r_data) override;

  void blit_to(eGPUFrameBufferBits planes,
               int src_slot,
               FrameBuffer *dst,
               int dst_slot,
               int dst_offset_x,
               int dst_offset_y) override;

  bool is_valid() const
  {
    return vk_framebuffer_ != VK_NULL_HANDLE;
  }

  VkFramebuffer vk_framebuffer_get() const
  {
    BLI_assert(vk_framebuffer_ != VK_NULL_HANDLE);
    return vk_framebuffer_;
  }

  VkRenderPass vk_render_pass_get() const
  {
    BLI_assert(vk_render_pass_ != VK_NULL_HANDLE);
    return vk_render_pass_;
  }
  Array<VkViewport, 16> vk_viewports_get() const;
  Array<VkRect2D, 16> vk_render_areas_get() const;
  VkImage vk_image_get() const
  {
    BLI_assert(vk_image_ != VK_NULL_HANDLE);
    return vk_image_;
  }

  /**
   * Is this frame-buffer immutable?
   *
   * Frame-buffers that are owned by GHOST are immutable and
   * don't have any attachments assigned. It should be assumed that there is a single color texture
   * in slot 0.
   */
  bool is_immutable() const
  {
    return immutable_;
  }

 private:
  void update_attachments();
  void render_pass_free();
  void render_pass_create();

  /* Clearing attachments */
  void build_clear_attachments_depth_stencil(eGPUFrameBufferBits buffers,
                                             float clear_depth,
                                             uint32_t clear_stencil,
                                             Vector<VkClearAttachment> &r_attachments) const;
  void build_clear_attachments_color(const float (*clear_colors)[4],
                                     const bool multi_clear_colors,
                                     Vector<VkClearAttachment> &r_attachments) const;
  void clear(const Vector<VkClearAttachment> &attachments) const;
};

static inline VKFrameBuffer *unwrap(FrameBuffer *framebuffer)
{
  return static_cast<VKFrameBuffer *>(framebuffer);
}

}  // namespace blender::gpu

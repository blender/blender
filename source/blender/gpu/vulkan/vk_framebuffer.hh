/* SPDX-FileCopyrightText: 2022 Blender Authors
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

#include "render_graph/vk_render_graph.hh"
#include "vk_common.hh"
#include "vk_image_view.hh"

namespace blender::gpu {
class VKContext;

class VKFrameBuffer : public FrameBuffer {
 private:
  /* Vulkan object handle. */
  VkFramebuffer vk_framebuffer_ = VK_NULL_HANDLE;
  /* Vulkan device who created the handle. */
  VkDevice vk_device_ = VK_NULL_HANDLE;
  /* Base render pass used for frame-buffer creation. */
  VkRenderPass vk_render_pass_ = VK_NULL_HANDLE;
  /* Number of layers if the attachments are layered textures. */
  int depth_ = 1;

  Vector<VKImageView, GPU_FB_MAX_ATTACHMENT> image_views_;

  /** Is the first attachment an SRGB texture. */
  bool srgb_;
  bool enabled_srgb_;
  bool is_rendering_ = false;

 public:
  /**
   * Create a conventional frame-buffer to attach texture to.
   */
  VKFrameBuffer(const char *name);

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

  void attachment_set_loadstore_op(GPUAttachmentType type, GPULoadStore /*ls*/) override;

  void begin_rendering(VKContext &context);
  void end_rendering(VKContext &context);

 protected:
  void subpass_transition_impl(const GPUAttachmentState depth_attachment_state,
                               Span<GPUAttachmentState> color_attachment_states) override;

 public:
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

  void vk_render_pass_ensure();
  VkRenderPass vk_render_pass_get() const
  {
    BLI_assert(vk_render_pass_ != VK_NULL_HANDLE);
    BLI_assert(!dirty_attachments_);
    return vk_render_pass_;
  }

  Array<VkViewport, 16> vk_viewports_get() const;
  Array<VkRect2D, 16> vk_render_areas_get() const;

  void depth_attachment_layout_ensure(VKContext &context, VkImageLayout requested_layout);
  void color_attachment_layout_ensure(VKContext &context,
                                      int color_attachment,
                                      VkImageLayout requested_layout);
  /**
   * Ensure that the size of the frame-buffer matches the first attachment resolution.
   *
   * Frame buffers attachments are updated when actually used as the image layout has to be
   * correct. After binding frame-buffers the layout of images can still be modified.
   *
   * But for correct behavior of blit/clear operation the size of the frame-buffer should be
   * set, when activating the frame buffer.
   */
  void update_size();

  void update_srgb();

  /**
   * Mark this framebuffer to be not being rendered on.
   *
   * Between binding a framebuffer and actually using it the state and clear operations can change.
   * The rendering state is used to find out if the framebuffer begin rendering command should be
   * recorded
   */
  void rendering_reset();

  /**
   * Ensure that the framebuffer is ready to be rendered on and that its state is up to date with
   * the latest changes that can happen between drawing commands inside `VKStateManager`.
   */
  void rendering_ensure(VKContext &context);

  /**
   * End the rendering on this framebuffer.
   * Is being triggered when framebuffer is deactivated or when
   */
  void rendering_end(VKContext &context);

  /**
   * Return the number of color attachments of this frame buffer, including unused color
   * attachments.
   *
   * Frame-buffers can have unused attachments. When higher attachment slots are being used, unused
   * lower attachment slots will be counted as they are required resources in render-passes.
   */
  int color_attachments_resource_size() const;

 private:
  void update_attachments();
  void render_pass_free();
  void render_pass_create();

  /* Clearing attachments */
  void build_clear_attachments_depth_stencil(
      eGPUFrameBufferBits buffers,
      float clear_depth,
      uint32_t clear_stencil,
      render_graph::VKClearAttachmentsNode::CreateInfo &clear_attachments) const;
  void build_clear_attachments_color(
      const float (*clear_colors)[4],
      const bool multi_clear_colors,
      render_graph::VKClearAttachmentsNode::CreateInfo &clear_attachments) const;
  void clear(render_graph::VKClearAttachmentsNode::CreateInfo &clear_attachments);
};

static inline VKFrameBuffer *unwrap(FrameBuffer *framebuffer)
{
  return static_cast<VKFrameBuffer *>(framebuffer);
}

}  // namespace blender::gpu

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
  /** Is the first attachment an SRGB texture. */
  bool srgb_;
  bool enabled_srgb_;
  bool is_rendering_ = false;

  VkFormat depth_attachment_format_ = VK_FORMAT_UNDEFINED;
  VkFormat stencil_attachment_format_ = VK_FORMAT_UNDEFINED;
  Vector<VkFormat> color_attachment_formats_;

  Array<GPULoadStore, GPU_FB_MAX_ATTACHMENT> load_stores;
  Array<GPUAttachmentState, GPU_FB_MAX_ATTACHMENT> attachment_states_;

 public:
  uint32_t color_attachment_size = 0u;

  /**
   * Create a conventional frame-buffer to attach texture to.
   */
  VKFrameBuffer(const char *name);
  virtual ~VKFrameBuffer();

  void bind(bool enabled_srgb) override;
  bool check(char err_out[256]) override;
  void clear(GPUFrameBufferBits buffers,
             const float clear_color[4],
             float clear_depth,
             uint clear_stencil) override;
  void clear_multi(const float (*clear_color)[4]) override;
  void clear_attachment(GPUAttachmentType type,
                        eGPUDataFormat data_format,
                        const void *clear_value) override;

  void attachment_set_loadstore_op(GPUAttachmentType type, GPULoadStore /*ls*/) override;

 protected:
  void subpass_transition_impl(const GPUAttachmentState depth_attachment_state,
                               Span<GPUAttachmentState> color_attachment_states) override;

 public:
  void read(GPUFrameBufferBits planes,
            eGPUDataFormat format,
            const int area[4],
            int channel_len,
            int slot,
            void *r_data) override;

  void blit_to(GPUFrameBufferBits planes,
               int src_slot,
               FrameBuffer *dst,
               int dst_slot,
               int dst_offset_x,
               int dst_offset_y) override;
  uint32_t viewport_size() const;
  void vk_viewports_append(Vector<VkViewport> &r_viewports) const;
  void vk_render_areas_append(Vector<VkRect2D> &r_render_areas) const;
  void render_area_update(VkRect2D &render_area) const;
  VkFormat depth_attachment_format_get() const;
  VkFormat stencil_attachment_format_get() const;
  Span<VkFormat> color_attachment_formats_get() const;

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
  void rendering_ensure_dynamic_rendering(VKContext &context, const VKExtensions &extensions);

  /**
   * End the rendering on this framebuffer.
   * Is being triggered when framebuffer is deactivated or when
   */
  void rendering_end(VKContext &context);

  bool is_rendering() const
  {
    return is_rendering_;
  }

  /**
   * Return the number of color attachments of this frame buffer, including unused color
   * attachments.
   *
   * Frame-buffers can have unused attachments. When higher attachment slots are being used, unused
   * lower attachment slots will be counted as they are required resources in render-passes.
   */
  int color_attachments_resource_size() const;

 private:
  /* Clearing attachments */
  void build_clear_attachments_depth_stencil(
      GPUFrameBufferBits buffers,
      float clear_depth,
      uint32_t clear_stencil,
      render_graph::VKClearAttachmentsNode::CreateInfo &clear_attachments) const;
  void build_clear_attachments_color(
      const float (*clear_colors)[4],
      const bool multi_clear_colors,
      render_graph::VKClearAttachmentsNode::CreateInfo &clear_attachments) const;
  void clear(render_graph::VKClearAttachmentsNode::CreateInfo &clear_attachments);
};

static inline VKFrameBuffer *unwrap(gpu::FrameBuffer *framebuffer)
{
  return static_cast<VKFrameBuffer *>(framebuffer);
}

}  // namespace blender::gpu

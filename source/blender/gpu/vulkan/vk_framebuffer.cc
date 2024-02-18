/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_framebuffer.hh"
#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_memory.hh"
#include "vk_state_manager.hh"
#include "vk_texture.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

VKFrameBuffer::VKFrameBuffer(const char *name) : FrameBuffer(name)
{
  size_set(1, 1);
  srgb_ = false;
  enabled_srgb_ = false;
}

VKFrameBuffer::~VKFrameBuffer()
{
  render_pass_free();
}

/** \} */

void VKFrameBuffer::bind(bool enabled_srgb)
{
  VKContext &context = *VKContext::get();
  /* Updating attachments can issue pipeline barriers, this should be done outside the render pass.
   * When done inside a render pass there should be a self-dependency between sub-passes on the
   * active render pass. As the active render pass isn't aware of the new render pass (and should
   * not) it is better to deactivate it before updating the attachments. For more information check
   * `VkSubpassDependency`. */
  if (context.has_active_framebuffer()) {
    context.deactivate_framebuffer();
  }

  context.activate_framebuffer(*this);
  enabled_srgb_ = enabled_srgb;
  Shader::set_framebuffer_srgb_target(enabled_srgb && srgb_);
}

Array<VkViewport, 16> VKFrameBuffer::vk_viewports_get() const
{
  Array<VkViewport, 16> viewports(this->multi_viewport_ ? GPU_MAX_VIEWPORTS : 1);

  int index = 0;
  for (VkViewport &viewport : viewports) {
    viewport.x = viewport_[index][0];
    viewport.y = viewport_[index][1];
    viewport.width = viewport_[index][2];
    viewport.height = viewport_[index][3];
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    index++;
  }
  return viewports;
}

Array<VkRect2D, 16> VKFrameBuffer::vk_render_areas_get() const
{
  Array<VkRect2D, 16> render_areas(this->multi_viewport_ ? GPU_MAX_VIEWPORTS : 1);

  for (VkRect2D &render_area : render_areas) {
    if (scissor_test_get()) {
      int scissor_rect[4];
      scissor_get(scissor_rect);
      render_area.offset.x = clamp_i(scissor_rect[0], 0, width_);
      render_area.offset.y = clamp_i(scissor_rect[1], 0, height_);
      render_area.extent.width = clamp_i(scissor_rect[2], 1, width_ - scissor_rect[0]);
      render_area.extent.height = clamp_i(scissor_rect[3], 1, height_ - scissor_rect[1]);
    }
    else {
      render_area.offset.x = 0;
      render_area.offset.y = 0;
      render_area.extent.width = width_;
      render_area.extent.height = height_;
    }
  }
  return render_areas;
}

bool VKFrameBuffer::check(char /*err_out*/[256])
{
  return true;
}

void VKFrameBuffer::build_clear_attachments_depth_stencil(
    const eGPUFrameBufferBits buffers,
    float clear_depth,
    uint32_t clear_stencil,
    Vector<VkClearAttachment> &r_attachments) const
{
  VkClearAttachment clear_attachment = {};
  clear_attachment.aspectMask = (buffers & GPU_DEPTH_BIT ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) |
                                (buffers & GPU_STENCIL_BIT ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
  clear_attachment.clearValue.depthStencil.depth = clear_depth;
  clear_attachment.clearValue.depthStencil.stencil = clear_stencil;
  r_attachments.append(clear_attachment);
}

void VKFrameBuffer::build_clear_attachments_color(const float (*clear_colors)[4],
                                                  const bool multi_clear_colors,
                                                  Vector<VkClearAttachment> &r_attachments) const
{
  int color_index = 0;
  for (int color_slot = 0; color_slot < GPU_FB_MAX_COLOR_ATTACHMENT; color_slot++) {
    const GPUAttachment &attachment = attachments_[GPU_FB_COLOR_ATTACHMENT0 + color_slot];
    if (attachment.tex == nullptr) {
      continue;
    }
    VkClearAttachment clear_attachment = {};
    clear_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_attachment.colorAttachment = color_slot;
    eGPUDataFormat data_format = to_data_format(GPU_texture_format(attachment.tex));
    clear_attachment.clearValue.color = to_vk_clear_color_value(data_format,
                                                                &clear_colors[color_index]);
    r_attachments.append(clear_attachment);

    color_index += multi_clear_colors ? 1 : 0;
  }
}

/* -------------------------------------------------------------------- */
/** \name Clear
 * \{ */

void VKFrameBuffer::clear(const Span<VkClearAttachment> attachments) const
{
  if (attachments.is_empty()) {
    return;
  }
  VkClearRect clear_rect = {};
  clear_rect.rect = vk_render_areas_get()[0];
  clear_rect.baseArrayLayer = 0;
  clear_rect.layerCount = 1;

  VKContext &context = *VKContext::get();
  VKCommandBuffers &command_buffers = context.command_buffers_get();
  command_buffers.clear(attachments, Span<VkClearRect>(&clear_rect, 1));
}

void VKFrameBuffer::clear(const eGPUFrameBufferBits buffers,
                          const float clear_color[4],
                          float clear_depth,
                          uint clear_stencil)
{
  Vector<VkClearAttachment> attachments;
  if (buffers & (GPU_DEPTH_BIT | GPU_STENCIL_BIT)) {
    VKContext &context = *VKContext::get();
    eGPUWriteMask needed_mask = GPU_WRITE_NONE;
    if (buffers & GPU_DEPTH_BIT) {
      needed_mask |= GPU_WRITE_DEPTH;
    }
    if (buffers & GPU_STENCIL_BIT) {
      needed_mask |= GPU_WRITE_STENCIL;
    }

    /* Clearing depth via vkCmdClearAttachments requires a render pass with write depth or stencil
     * enabled. When not enabled, clearing should be done via texture directly. */
    if ((context.state_manager_get().state.write_mask & needed_mask) == needed_mask) {
      build_clear_attachments_depth_stencil(buffers, clear_depth, clear_stencil, attachments);
    }
    else {
      VKTexture *depth_texture = unwrap(unwrap(depth_tex()));
      if (depth_texture != nullptr) {
        if (G.debug & G_DEBUG_GPU) {
          std::cout
              << "PERFORMANCE: impact clearing depth texture in render pass that doesn't allow "
                 "depth writes.\n";
        }
        depth_attachment_layout_ensure(context, VK_IMAGE_LAYOUT_GENERAL);
        depth_texture->clear_depth_stencil(buffers, clear_depth, clear_stencil);
      }
    }
  }
  if (buffers & GPU_COLOR_BIT) {
    float clear_color_single[4];
    copy_v4_v4(clear_color_single, clear_color);
    build_clear_attachments_color(&clear_color_single, false, attachments);
  }

  if (!attachments.is_empty()) {
    clear(attachments);
  }
}

void VKFrameBuffer::clear_multi(const float (*clear_color)[4])
{
  Vector<VkClearAttachment> attachments;
  build_clear_attachments_color(clear_color, true, attachments);
  clear(attachments);
}

void VKFrameBuffer::clear_attachment(GPUAttachmentType /*type*/,
                                     eGPUDataFormat /*data_format*/,
                                     const void * /*clear_value*/)
{
  /* Clearing of a single attachment was added to implement `clear_multi` in OpenGL. As
   * `clear_multi` is supported in Vulkan it isn't needed to implement this method.
   */
  BLI_assert_unreachable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Load/Store operations
 * \{ */

void VKFrameBuffer::attachment_set_loadstore_op(GPUAttachmentType /*type*/, GPULoadStore /*ls*/)
{
  NOT_YET_IMPLEMENTED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sub-pass transition
 * \{ */

void VKFrameBuffer::subpass_transition_impl(const GPUAttachmentState /*depth_attachment_state*/,
                                            Span<GPUAttachmentState> /*color_attachment_states*/)
{
  NOT_YET_IMPLEMENTED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read back
 * \{ */

void VKFrameBuffer::read(eGPUFrameBufferBits plane,
                         eGPUDataFormat format,
                         const int area[4],
                         int /*channel_len*/,
                         int slot,
                         void *r_data)
{
  VKContext &context = *VKContext::get();
  GPUAttachment *attachment = nullptr;
  switch (plane) {
    case GPU_COLOR_BIT:
      attachment = &attachments_[GPU_FB_COLOR_ATTACHMENT0 + slot];
      color_attachment_layout_ensure(context, slot, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
      break;

    case GPU_DEPTH_BIT:
      attachment = attachments_[GPU_FB_DEPTH_ATTACHMENT].tex ?
                       &attachments_[GPU_FB_DEPTH_ATTACHMENT] :
                       &attachments_[GPU_FB_DEPTH_STENCIL_ATTACHMENT];
      depth_attachment_layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
      break;

    default:
      BLI_assert_unreachable();
      return;
  }

  VKTexture *texture = unwrap(unwrap(attachment->tex));
  BLI_assert_msg(texture,
                 "Trying to read back texture from framebuffer, but no texture is available in "
                 "requested slot.");
  if (texture == nullptr) {
    return;
  }
  const int area6[6] = {area[0], area[1], 0, area[2], area[3], 1};
  IndexRange layers(max_ii(attachment->layer, 0), 1);
  texture->read_sub(0, format, area6, layers, r_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blit operations
 * \{ */

static void blit_aspect(VKCommandBuffers &command_buffer,
                        VKTexture &dst_texture,
                        VKTexture &src_texture,
                        int dst_offset_x,
                        int dst_offset_y,
                        VkImageAspectFlags image_aspect)
{
  /* Prefer texture copy, as some platforms don't support using D32_SFLOAT_S8_UINT to be used as
   * a blit destination. */
  if (dst_offset_x == 0 && dst_offset_y == 0 &&
      dst_texture.device_format_get() == src_texture.device_format_get() &&
      src_texture.width_get() == dst_texture.width_get() &&
      src_texture.height_get() == dst_texture.height_get())
  {
    src_texture.copy_to(dst_texture, image_aspect);
    return;
  }

  VkImageBlit image_blit = {};
  image_blit.srcSubresource.aspectMask = image_aspect;
  image_blit.srcSubresource.mipLevel = 0;
  image_blit.srcSubresource.baseArrayLayer = 0;
  image_blit.srcSubresource.layerCount = 1;
  image_blit.srcOffsets[0].x = 0;
  image_blit.srcOffsets[0].y = 0;
  image_blit.srcOffsets[0].z = 0;
  image_blit.srcOffsets[1].x = src_texture.width_get();
  image_blit.srcOffsets[1].y = src_texture.height_get();
  image_blit.srcOffsets[1].z = 1;

  image_blit.dstSubresource.aspectMask = image_aspect;
  image_blit.dstSubresource.mipLevel = 0;
  image_blit.dstSubresource.baseArrayLayer = 0;
  image_blit.dstSubresource.layerCount = 1;
  image_blit.dstOffsets[0].x = min_ii(dst_offset_x, dst_texture.width_get());
  image_blit.dstOffsets[0].y = min_ii(dst_offset_y, dst_texture.height_get());
  image_blit.dstOffsets[0].z = 0;
  image_blit.dstOffsets[1].x = min_ii(dst_offset_x + src_texture.width_get(),
                                      dst_texture.width_get());
  image_blit.dstOffsets[1].y = min_ii(dst_offset_y + src_texture.height_get(),
                                      dst_texture.height_get());
  image_blit.dstOffsets[1].z = 1;

  command_buffer.blit(dst_texture, src_texture, Span<VkImageBlit>(&image_blit, 1));
}

void VKFrameBuffer::blit_to(eGPUFrameBufferBits planes,
                            int src_slot,
                            FrameBuffer *dst,
                            int dst_slot,
                            int dst_offset_x,
                            int dst_offset_y)
{
  BLI_assert(dst);
  BLI_assert_msg(ELEM(planes, GPU_COLOR_BIT, GPU_DEPTH_BIT),
                 "VKFrameBuffer::blit_to only supports a single color or depth aspect.");
  UNUSED_VARS_NDEBUG(planes);

  VKContext &context = *VKContext::get();
  VKCommandBuffers &command_buffers = context.command_buffers_get();
  if (!context.has_active_framebuffer()) {
    BLI_assert_unreachable();
    return;
  }

  VKFrameBuffer &dst_framebuffer = *unwrap(dst);
  if (planes & GPU_COLOR_BIT) {
    const GPUAttachment &src_attachment = attachments_[GPU_FB_COLOR_ATTACHMENT0 + src_slot];
    const GPUAttachment &dst_attachment =
        dst_framebuffer.attachments_[GPU_FB_COLOR_ATTACHMENT0 + dst_slot];
    if (src_attachment.tex && dst_attachment.tex) {
      VKTexture &src_texture = *unwrap(unwrap(src_attachment.tex));
      VKTexture &dst_texture = *unwrap(unwrap(dst_attachment.tex));
      color_attachment_layout_ensure(context, src_slot, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
      dst_framebuffer.color_attachment_layout_ensure(
          context, dst_slot, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      blit_aspect(command_buffers,
                  dst_texture,
                  src_texture,
                  dst_offset_x,
                  dst_offset_y,
                  VK_IMAGE_ASPECT_COLOR_BIT);
    }
  }

  if (planes & GPU_DEPTH_BIT) {
    /* Retrieve source texture. */
    const GPUAttachment &src_attachment = attachments_[GPU_FB_DEPTH_STENCIL_ATTACHMENT].tex ?
                                              attachments_[GPU_FB_DEPTH_STENCIL_ATTACHMENT] :
                                              attachments_[GPU_FB_DEPTH_ATTACHMENT];
    const GPUAttachment &dst_attachment =
        dst_framebuffer.attachments_[GPU_FB_DEPTH_STENCIL_ATTACHMENT].tex ?
            dst_framebuffer.attachments_[GPU_FB_DEPTH_STENCIL_ATTACHMENT] :
            dst_framebuffer.attachments_[GPU_FB_DEPTH_ATTACHMENT];
    if (src_attachment.tex && dst_attachment.tex) {
      VKTexture &src_texture = *unwrap(unwrap(src_attachment.tex));
      VKTexture &dst_texture = *unwrap(unwrap(dst_attachment.tex));
      depth_attachment_layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
      dst_framebuffer.depth_attachment_layout_ensure(context,
                                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      blit_aspect(command_buffers,
                  dst_texture,
                  src_texture,
                  dst_offset_x,
                  dst_offset_y,
                  VK_IMAGE_ASPECT_DEPTH_BIT);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update attachments
 * \{ */

void VKFrameBuffer::vk_render_pass_ensure()
{
  if (!dirty_attachments_) {
    return;
  }
  render_pass_free();
  render_pass_create();

  dirty_attachments_ = false;
}

void VKFrameBuffer::render_pass_create()
{
  BLI_assert(vk_render_pass_ == VK_NULL_HANDLE);
  BLI_assert(vk_framebuffer_ == VK_NULL_HANDLE);

  VK_ALLOCATION_CALLBACKS

  /* Track first attachment for size. */
  GPUAttachmentType first_attachment = GPU_FB_MAX_ATTACHMENT;

  std::array<VkAttachmentDescription, GPU_FB_MAX_ATTACHMENT> attachment_descriptions;
  std::array<VkImageView, GPU_FB_MAX_ATTACHMENT> image_views;
  std::array<VkAttachmentReference, GPU_FB_MAX_ATTACHMENT> attachment_references;
  image_views_.clear();

  bool has_depth_attachment = false;
  bool found_attachment = false;
  const VKImageView &dummy_attachment =
      VKBackend::get().device_get().dummy_color_attachment_get().image_view_get();
  int depth_location = -1;

  for (int type = GPU_FB_MAX_ATTACHMENT - 1; type >= 0; type--) {
    GPUAttachment &attachment = attachments_[type];
    if (attachment.tex == nullptr && !found_attachment) {
      /* Move the depth texture to the next binding point after all color textures. The binding
       * location of the color textures should be kept in sync between ShaderCreateInfos and the
       * framebuffer attachments. The depth buffer should be the last slot. */
      depth_location = max_ii(type - GPU_FB_COLOR_ATTACHMENT0, 0);
      continue;
    }
    found_attachment |= attachment.tex != nullptr;

    /* Keep the first attachment to the first color attachment, or to the depth buffer when there
     * is no color attachment. */
    if (attachment.tex != nullptr &&
        (first_attachment == GPU_FB_MAX_ATTACHMENT || type >= GPU_FB_COLOR_ATTACHMENT0))
    {
      first_attachment = static_cast<GPUAttachmentType>(type);
    }

    int attachment_location = type >= GPU_FB_COLOR_ATTACHMENT0 ? type - GPU_FB_COLOR_ATTACHMENT0 :
                                                                 depth_location;
    const bool is_depth_attachment = ELEM(
        type, GPU_FB_DEPTH_ATTACHMENT, GPU_FB_DEPTH_STENCIL_ATTACHMENT);

    if (attachment.tex) {
      BLI_assert_msg(!is_depth_attachment || !has_depth_attachment,
                     "There can only be one depth/stencil attachment.");
      has_depth_attachment |= is_depth_attachment;

      /* Ensure texture is allocated to ensure the image view. */
      VKTexture &texture = *static_cast<VKTexture *>(unwrap(attachment.tex));
      const bool use_stencil = false;
      const bool use_srgb = srgb_ && enabled_srgb_;
      image_views_.append(VKImageView(texture,
                                      eImageViewUsage::Attachment,
                                      IndexRange(max_ii(attachment.layer, 0), 1),
                                      IndexRange(attachment.mip, 1),
                                      use_stencil,
                                      use_srgb,
                                      name_));
      const VKImageView &image_view = image_views_.last();
      image_views[attachment_location] = image_view.vk_handle();

      VkAttachmentDescription &attachment_description =
          attachment_descriptions[attachment_location];
      attachment_description.flags = 0;
      attachment_description.format = image_view.vk_format();
      attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
      attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachment_description.initialLayout = texture.current_layout_get();
      attachment_description.finalLayout = texture.current_layout_get();

      /* Create the attachment reference. */
      VkAttachmentReference &attachment_reference = attachment_references[attachment_location];
      attachment_reference.attachment = attachment_location;
      attachment_reference.layout = is_depth_attachment ?
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                                        VK_IMAGE_LAYOUT_GENERAL;
    }
    else if (!is_depth_attachment) {
      image_views[attachment_location] = dummy_attachment.vk_handle();

      VkAttachmentDescription &attachment_description =
          attachment_descriptions[attachment_location];
      attachment_description.flags = 0;
      attachment_description.format = VK_FORMAT_R32_SFLOAT;
      attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
      attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachment_description.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
      attachment_description.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

      VkAttachmentReference &attachment_reference = attachment_references[attachment_location];
      attachment_reference.attachment = VK_ATTACHMENT_UNUSED;
      attachment_reference.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
  }

  /* Update the size, viewport & scissor based on the first attachment. */
  if (first_attachment != GPU_FB_MAX_ATTACHMENT) {
    GPUAttachment &attachment = attachments_[first_attachment];
    BLI_assert(attachment.tex);

    int size[3];
    GPU_texture_get_mipmap_size(attachment.tex, attachment.mip, size);
    size_set(size[0], size[1]);
  }
  else {
    /* A frame-buffer should at least be 1 by 1. */
    this->size_set(1, 1);
  }
  viewport_reset();
  scissor_reset();

  /* Create render pass. */
  const int attachment_len = has_depth_attachment ? depth_location + 1 : depth_location;
  const int color_attachment_len = depth_location;
  VkSubpassDescription subpass = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = color_attachment_len;
  subpass.pColorAttachments = attachment_references.data();
  if (has_depth_attachment) {
    subpass.pDepthStencilAttachment = &attachment_references[depth_location];
  }

  VkRenderPassCreateInfo render_pass_info = {};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = attachment_len;
  render_pass_info.pAttachments = attachment_descriptions.data();
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;

  const VKDevice &device = VKBackend::get().device_get();
  vkCreateRenderPass(
      device.device_get(), &render_pass_info, vk_allocation_callbacks, &vk_render_pass_);

  /* We might want to split frame-buffer and render pass. */
  VkFramebufferCreateInfo framebuffer_create_info = {};
  framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_create_info.renderPass = vk_render_pass_;
  framebuffer_create_info.attachmentCount = attachment_len;
  framebuffer_create_info.pAttachments = image_views.data();
  framebuffer_create_info.width = width_;
  framebuffer_create_info.height = height_;
  framebuffer_create_info.layers = 1;

  vkCreateFramebuffer(
      device.device_get(), &framebuffer_create_info, vk_allocation_callbacks, &vk_framebuffer_);
}

void VKFrameBuffer::render_pass_free()
{
  if (vk_render_pass_ == VK_NULL_HANDLE) {
    return;
  }

  VKDevice &device = VKBackend::get().device_get();
  if (device.is_initialized()) {
    device.discard_render_pass(vk_render_pass_);
    device.discard_frame_buffer(vk_framebuffer_);
  }
  image_views_.clear();
  vk_render_pass_ = VK_NULL_HANDLE;
  vk_framebuffer_ = VK_NULL_HANDLE;
}

void VKFrameBuffer::color_attachment_layout_ensure(VKContext &context,
                                                   int color_attachment,
                                                   VkImageLayout requested_layout)
{
  VKTexture *color_texture = unwrap(unwrap(color_tex(color_attachment)));
  if (color_texture == nullptr) {
    return;
  }

  if (color_texture->current_layout_get() == requested_layout) {
    return;
  }

  color_texture->layout_ensure(context, requested_layout);
  dirty_attachments_ = true;
}

void VKFrameBuffer::depth_attachment_layout_ensure(VKContext &context,
                                                   VkImageLayout requested_layout)
{
  VKTexture *depth_texture = unwrap(unwrap(depth_tex()));
  if (depth_texture == nullptr) {
    return;
  }

  if (depth_texture->current_layout_get() == requested_layout) {
    return;
  }
  depth_texture->layout_ensure(context, requested_layout);
  dirty_attachments_ = true;
}

void VKFrameBuffer::update_size()
{
  if (!dirty_attachments_) {
    return;
  }

  for (int i = 0; i < GPU_FB_MAX_ATTACHMENT; i++) {
    GPUAttachment &attachment = attachments_[i];
    if (attachment.tex) {
      int size[3];
      GPU_texture_get_mipmap_size(attachment.tex, attachment.mip, size);
      size_set(size[0], size[1]);
      return;
    }
  }
  size_set(1, 1);
}

void VKFrameBuffer::update_srgb()
{
  for (int i : IndexRange(GPU_FB_MAX_COLOR_ATTACHMENT)) {
    VKTexture *texture = unwrap(unwrap(color_tex(i)));
    if (texture) {
      srgb_ = (texture->format_flag_get() & GPU_FORMAT_SRGB) != 0;
      return;
    }
  }
}

int VKFrameBuffer::color_attachments_resource_size() const
{
  int size = 0;
  for (int color_slot : IndexRange(GPU_FB_MAX_COLOR_ATTACHMENT)) {
    if (color_tex(color_slot) != nullptr) {
      size = max_ii(color_slot + 1, size);
    }
  }
  return size;
}

/** \} */

}  // namespace blender::gpu

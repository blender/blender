/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "vk_framebuffer.hh"
#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_memory.hh"
#include "vk_texture.hh"

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Creation & Deletion
 * \{ */

VKFrameBuffer::VKFrameBuffer(const char *name) : FrameBuffer(name)
{
  immutable_ = false;
  flip_viewport_ = false;
  size_set(1, 1);
}

VKFrameBuffer::VKFrameBuffer(const char *name,
                             VkImage vk_image,
                             VkFramebuffer vk_framebuffer,
                             VkRenderPass vk_render_pass,
                             VkExtent2D vk_extent)
    : FrameBuffer(name)
{
  immutable_ = true;
  flip_viewport_ = true;
  /* Never update an internal frame-buffer. */
  dirty_attachments_ = false;
  vk_image_ = vk_image;
  vk_framebuffer_ = vk_framebuffer;
  vk_render_pass_ = vk_render_pass;

  size_set(vk_extent.width, vk_extent.height);
  viewport_reset();
  scissor_reset();
}

VKFrameBuffer::~VKFrameBuffer()
{
  if (!immutable_) {
    render_pass_free();
  }
}

/** \} */

void VKFrameBuffer::bind(bool /*enabled_srgb*/)
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

  update_attachments();

  context.activate_framebuffer(*this);
}

VkViewport VKFrameBuffer::vk_viewport_get() const
{
  VkViewport viewport;
  int viewport_rect[4];
  viewport_get(viewport_rect);

  viewport.x = viewport_rect[0];
  viewport.y = viewport_rect[1];
  viewport.width = viewport_rect[2];
  viewport.height = viewport_rect[3];
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  /*
   * Vulkan has origin to the top left, Blender bottom left. We counteract this by using a negative
   * viewport when flip_viewport_ is set. This flips the viewport making any draw/blit use the
   * correct orientation.
   */
  if (flip_viewport_) {
    viewport.y = height_ - viewport_rect[1];
    viewport.height = -viewport_rect[3];
  }

  return viewport;
}

VkRect2D VKFrameBuffer::vk_render_area_get() const
{
  VkRect2D render_area = {};

  if (scissor_test_get()) {
    int scissor_rect[4];
    scissor_get(scissor_rect);
    render_area.offset.x = scissor_rect[0];
    render_area.offset.y = scissor_rect[1];
    render_area.extent.width = scissor_rect[2];
    render_area.extent.height = scissor_rect[3];
  }
  else {
    render_area.offset.x = 0;
    render_area.offset.y = 0;
    render_area.extent.width = width_;
    render_area.extent.height = height_;
  }

  return render_area;
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

void VKFrameBuffer::clear(const Vector<VkClearAttachment> &attachments) const
{
  if (attachments.is_empty()) {
    return;
  }
  VkClearRect clear_rect = {};
  clear_rect.rect = vk_render_area_get();
  clear_rect.baseArrayLayer = 0;
  clear_rect.layerCount = 1;

  VKContext &context = *VKContext::get();
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.clear(attachments, Span<VkClearRect>(&clear_rect, 1));
}

void VKFrameBuffer::clear(const eGPUFrameBufferBits buffers,
                          const float clear_color[4],
                          float clear_depth,
                          uint clear_stencil)
{
  Vector<VkClearAttachment> attachments;
  if (buffers & (GPU_DEPTH_BIT | GPU_STENCIL_BIT)) {
    build_clear_attachments_depth_stencil(buffers, clear_depth, clear_stencil, attachments);
  }
  if (buffers & GPU_COLOR_BIT) {
    float clear_color_single[4];
    copy_v4_v4(clear_color_single, clear_color);
    build_clear_attachments_color(&clear_color_single, false, attachments);
  }
  clear(attachments);
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

void VKFrameBuffer::attachment_set_loadstore_op(GPUAttachmentType /*type*/,
                                                eGPULoadOp /*load_action*/,
                                                eGPUStoreOp /*store_action*/)
{
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read back
 * \{ */

void VKFrameBuffer::read(eGPUFrameBufferBits plane,
                         eGPUDataFormat format,
                         const int /*area*/[4],
                         int channel_len,
                         int slot,
                         void *r_data)
{
  VKTexture *texture = nullptr;
  switch (plane) {
    case GPU_COLOR_BIT:
      texture = unwrap(unwrap(attachments_[GPU_FB_COLOR_ATTACHMENT0 + slot].tex));
      break;

    default:
      BLI_assert_unreachable();
      return;
  }

  BLI_assert_msg(texture,
                 "Trying to read back color texture from framebuffer, but no color texture is "
                 "available in requested slot.");
  void *data = texture->read(0, format);

  /*
   * TODO:
   * - Add support for area.
   * - Add support for channel_len.
   * Best option would be to add this to VKTexture so we don't over-allocate and reduce number of
   * times copies are made.
   */
  BLI_assert(format == GPU_DATA_FLOAT);
  BLI_assert(channel_len == 4);
  int mip_size[3] = {1, 1, 1};
  texture->mip_size_get(0, mip_size);
  const size_t mem_size = mip_size[0] * mip_size[1] * mip_size[2] * sizeof(float) * channel_len;
  memcpy(r_data, data, mem_size);
  MEM_freeN(data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blit operations
 * \{ */

void VKFrameBuffer::blit_to(eGPUFrameBufferBits planes,
                            int src_slot,
                            FrameBuffer *dst,
                            int dst_slot,
                            int dst_offset_x,
                            int dst_offset_y)
{
  BLI_assert(dst);
  BLI_assert(planes == GPU_COLOR_BIT);
  UNUSED_VARS_NDEBUG(planes);

  VKContext &context = *VKContext::get();
  if (!context.has_active_framebuffer()) {
    BLI_assert_unreachable();
    return;
  }

  /* Retrieve source texture. */
  const GPUAttachment &src_attachment = attachments_[GPU_FB_COLOR_ATTACHMENT0 + src_slot];
  if (src_attachment.tex == nullptr) {
    return;
  }
  VKTexture &src_texture = *unwrap(unwrap(src_attachment.tex));
  src_texture.layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  /* Retrieve destination texture. */
  const VKFrameBuffer &dst_framebuffer = *unwrap(dst);
  const GPUAttachment &dst_attachment =
      dst_framebuffer.attachments_[GPU_FB_COLOR_ATTACHMENT0 + dst_slot];
  VKTexture *dst_texture = nullptr;
  VKTexture tmp_texture("FramebufferTexture");
  if (dst_attachment.tex) {
    dst_texture = unwrap(unwrap(dst_attachment.tex));
    dst_texture->layout_ensure(context, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  }
  else {
    tmp_texture.init(dst_framebuffer.vk_image_get(), VK_IMAGE_LAYOUT_GENERAL);
    dst_texture = &tmp_texture;
  }

  VkImageBlit image_blit = {};
  image_blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_blit.srcSubresource.mipLevel = 0;
  image_blit.srcSubresource.baseArrayLayer = 0;
  image_blit.srcSubresource.layerCount = 1;
  image_blit.srcOffsets[0].x = 0;
  image_blit.srcOffsets[0].y = 0;
  image_blit.srcOffsets[0].z = 0;
  image_blit.srcOffsets[1].x = src_texture.width_get();
  image_blit.srcOffsets[1].y = src_texture.height_get();
  image_blit.srcOffsets[1].z = 1;

  image_blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_blit.dstSubresource.mipLevel = 0;
  image_blit.dstSubresource.baseArrayLayer = 0;
  image_blit.dstSubresource.layerCount = 1;
  image_blit.dstOffsets[0].x = dst_offset_x;
  image_blit.dstOffsets[0].y = dst_offset_y;
  image_blit.dstOffsets[0].z = 0;
  image_blit.dstOffsets[1].x = dst_offset_x + src_texture.width_get();
  image_blit.dstOffsets[1].y = dst_offset_x + src_texture.height_get();
  image_blit.dstOffsets[1].z = 1;

  const bool should_flip = flip_viewport_ != dst_framebuffer.flip_viewport_;
  if (should_flip) {
    image_blit.dstOffsets[0].y = dst_framebuffer.height_ - dst_offset_y;
    image_blit.dstOffsets[1].y = dst_framebuffer.height_ - dst_offset_y - src_texture.height_get();
  }

  context.command_buffer_get().blit(*dst_texture, src_texture, Span<VkImageBlit>(&image_blit, 1));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update attachments
 * \{ */

void VKFrameBuffer::update_attachments()
{
  if (immutable_) {
    return;
  }
  if (!dirty_attachments_) {
    return;
  }

  render_pass_free();
  render_pass_create();

  dirty_attachments_ = false;
}

void VKFrameBuffer::render_pass_create()
{
  BLI_assert(!immutable_);
  BLI_assert(vk_render_pass_ == VK_NULL_HANDLE);
  BLI_assert(vk_framebuffer_ == VK_NULL_HANDLE);

  VK_ALLOCATION_CALLBACKS

  /* Track first attachment for size. */
  GPUAttachmentType first_attachment = GPU_FB_MAX_ATTACHMENT;

  std::array<VkAttachmentDescription, GPU_FB_MAX_ATTACHMENT> attachment_descriptions;
  std::array<VkImageView, GPU_FB_MAX_ATTACHMENT> image_views;
  std::array<VkAttachmentReference, GPU_FB_MAX_ATTACHMENT> attachment_references;
  bool has_depth_attachment = false;
  bool found_attachment = false;
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

    if (attachment.tex) {
      /* Ensure texture is allocated to ensure the image view. */
      VKTexture &texture = *static_cast<VKTexture *>(unwrap(attachment.tex));
      texture.ensure_allocated();
      image_views[attachment_location] = texture.vk_image_view_handle();

      VkAttachmentDescription &attachment_description =
          attachment_descriptions[attachment_location];
      attachment_description.flags = 0;
      attachment_description.format = to_vk_format(texture.format_get());
      attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
      attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachment_description.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
      attachment_description.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

      /* Create the attachment reference. */
      const bool is_depth_attachment = ELEM(
          type, GPU_FB_DEPTH_ATTACHMENT, GPU_FB_DEPTH_STENCIL_ATTACHMENT);

      BLI_assert_msg(!is_depth_attachment || !has_depth_attachment,
                     "There can only be one depth/stencil attachment.");
      has_depth_attachment |= is_depth_attachment;
      VkAttachmentReference &attachment_reference = attachment_references[attachment_location];
      attachment_reference.attachment = attachment_location;
      attachment_reference.layout = is_depth_attachment ?
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
                                        VK_IMAGE_LAYOUT_GENERAL;
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
    /* A framebuffer should at least be 1 by 1.*/
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
  BLI_assert(!immutable_);
  if (vk_render_pass_ == VK_NULL_HANDLE) {
    return;
  }
  VK_ALLOCATION_CALLBACKS

  const VKDevice &device = VKBackend::get().device_get();
  if (device.is_initialized()) {
    vkDestroyRenderPass(device.device_get(), vk_render_pass_, vk_allocation_callbacks);
    vkDestroyFramebuffer(device.device_get(), vk_framebuffer_, vk_allocation_callbacks);
  }
  vk_render_pass_ = VK_NULL_HANDLE;
  vk_framebuffer_ = VK_NULL_HANDLE;
}

/** \} */

}  // namespace blender::gpu

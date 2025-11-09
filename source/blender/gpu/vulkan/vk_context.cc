/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "DNA_userdef_types.h"

#include "GPU_debug.hh"

#include "gpu_capabilities_private.hh"

#include "vk_backend.hh"
#include "vk_context.hh"
#include "vk_debug.hh"
#include "vk_framebuffer.hh"
#include "vk_immediate.hh"
#include "vk_shader.hh"
#include "vk_shader_interface.hh"
#include "vk_state_manager.hh"
#include "vk_texture.hh"

#include "GHOST_C-api.h"

namespace blender::gpu {

VKContext::VKContext(void *ghost_window, void *ghost_context)
{
  ghost_window_ = ghost_window;
  ghost_context_ = ghost_context;

  state_manager = new VKStateManager();
  imm = new VKImmediate();

  back_left = new VKFrameBuffer("back_left");
  front_left = new VKFrameBuffer("front_left");
  active_fb = back_left;
}

VKContext::~VKContext()
{
  if (surface_texture_) {
    back_left->attachment_remove(GPU_FB_COLOR_ATTACHMENT0);
    front_left->attachment_remove(GPU_FB_COLOR_ATTACHMENT0);
    GPU_texture_free(surface_texture_);
    surface_texture_ = nullptr;
  }
  free_resources();
  delete imm;
  imm = nullptr;
  VKDevice &device = VKBackend::get().device;
  device.context_unregister(*this);

  this->process_frame_timings();
}

void VKContext::sync_backbuffer()
{
  if (ghost_window_) {
    GHOST_VulkanSwapChainData swap_chain_data = {};
    GHOST_GetVulkanSwapChainFormat((GHOST_WindowHandle)ghost_window_, &swap_chain_data);

    const bool reset_framebuffer = swap_chain_format_.format !=
                                       swap_chain_data.surface_format.format ||
                                   swap_chain_format_.colorSpace !=
                                       swap_chain_data.surface_format.colorSpace ||
                                   vk_extent_.width != swap_chain_data.extent.width ||
                                   vk_extent_.height != swap_chain_data.extent.height;
    if (reset_framebuffer) {
      if (has_active_framebuffer()) {
        deactivate_framebuffer();
      }
      if (surface_texture_) {
        GPU_texture_free(surface_texture_);
        surface_texture_ = nullptr;
      }
      vk_extent_ = swap_chain_data.extent;
      vk_extent_.width = max_uu(vk_extent_.width, 1u);
      vk_extent_.height = max_uu(vk_extent_.height, 1u);
      surface_texture_ = GPU_texture_create_2d(
          "back-left",
          vk_extent_.width,
          vk_extent_.height,
          1,
          to_gpu_format(swap_chain_data.surface_format.format),
          GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ,
          nullptr);

      back_left->attachment_set(GPU_FB_COLOR_ATTACHMENT0,
                                GPU_ATTACHMENT_TEXTURE(surface_texture_));
      front_left->attachment_set(GPU_FB_COLOR_ATTACHMENT0,
                                 GPU_ATTACHMENT_TEXTURE(surface_texture_));

      back_left->bind(false);

      swap_chain_format_ = swap_chain_data.surface_format;
      GCaps.hdr_viewport_support = (swap_chain_format_.format == VK_FORMAT_R16G16B16A16_SFLOAT) &&
                                   ELEM(swap_chain_format_.colorSpace,
                                        VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT,
                                        VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    }
  }
}

void VKContext::activate()
{
  /* Make sure no other context is already bound to this thread. */
  BLI_assert(is_active_ == false);

  VKDevice &device = VKBackend::get().device;
  VKThreadData &thread_data = device.current_thread_data();
  thread_data_ = std::reference_wrapper<VKThreadData>(thread_data);

  if (!render_graph_.has_value()) {
    render_graph_ = std::reference_wrapper<render_graph::VKRenderGraph>(
        *device.render_graph_new());
    /* Recreate the debug group stack for the new graph.
     * Note: there is no associated `debug_group_end` as the graph groups
     * are implicitly closed on submission. */
    for (const StringRef &group : debug_stack) {
      std::string str_group = group;
      render_graph_.value().get().debug_group_begin(str_group.c_str(),
                                                    debug::get_debug_group_color(str_group));
    }
  }

  is_active_ = true;

  sync_backbuffer();

  immActivate();
}

void VKContext::deactivate()
{
  flush_render_graph(RenderGraphFlushFlags(0));
  immDeactivate();
  thread_data_.reset();

  is_active_ = false;
}

void VKContext::begin_frame() {}

void VKContext::end_frame()
{
  this->process_frame_timings();
}

void VKContext::flush()
{
  flush_render_graph(RenderGraphFlushFlags::RENEW_RENDER_GRAPH);
}

TimelineValue VKContext::flush_render_graph(RenderGraphFlushFlags flags,
                                            VkPipelineStageFlags wait_dst_stage_mask,
                                            VkSemaphore wait_semaphore,
                                            VkSemaphore signal_semaphore,
                                            VkFence signal_fence)
{
  if (has_active_framebuffer()) {
    VKFrameBuffer &framebuffer = *active_framebuffer_get();
    if (framebuffer.is_rendering()) {
      framebuffer.rendering_end(*this);
    }
  }
  VKDevice &device = VKBackend::get().device;
  descriptor_set_get().upload_descriptor_sets();
  TimelineValue timeline = device.render_graph_submit(
      &render_graph_.value().get(),
      discard_pool,
      bool(flags & RenderGraphFlushFlags::SUBMIT),
      bool(flags & RenderGraphFlushFlags::WAIT_FOR_COMPLETION),
      wait_dst_stage_mask,
      wait_semaphore,
      signal_semaphore,
      signal_fence);
  render_graph_.reset();
  streaming_buffers_.clear();
  if (bool(flags & RenderGraphFlushFlags::RENEW_RENDER_GRAPH)) {
    render_graph_ = std::reference_wrapper<render_graph::VKRenderGraph>(
        *device.render_graph_new());
    /* Recreate the debug group stack for the new graph.
     * Note: there is no associated `debug_group_end` as the graph groups
     * are implicitly closed on submission. */
    for (const StringRef &group : debug_stack) {
      std::string str_group = group;
      render_graph_.value().get().debug_group_begin(str_group.c_str(),
                                                    debug::get_debug_group_color(str_group));
    }
  }
  return timeline;
}

void VKContext::finish() {}

void VKContext::memory_statistics_get(int *r_total_mem_kb, int *r_free_mem_kb)
{
  const VKDevice &device = VKBackend::get().device;
  device.memory_statistics_get(r_total_mem_kb, r_free_mem_kb);
}

/* -------------------------------------------------------------------- */
/** \name State manager
 * \{ */

VKDescriptorPools &VKContext::descriptor_pools_get()
{
  return thread_data_.value().get().descriptor_pools;
}

VKDescriptorSetTracker &VKContext::descriptor_set_get()
{
  return thread_data_.value().get().descriptor_set;
}

VKStateManager &VKContext::state_manager_get() const
{
  return *static_cast<VKStateManager *>(state_manager);
}

void VKContext::debug_unbind_all_ubo()
{
  state_manager_get().uniform_buffer_unbind_all();
};

void VKContext::debug_unbind_all_ssbo()
{
  state_manager_get().storage_buffer_unbind_all();
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame-buffer
 * \{ */

void VKContext::activate_framebuffer(VKFrameBuffer &framebuffer)
{
  if (has_active_framebuffer()) {
    deactivate_framebuffer();
  }

  BLI_assert(active_fb == nullptr);
  active_fb = &framebuffer;
  framebuffer.update_size();
  framebuffer.update_srgb();
  framebuffer.rendering_reset();
}

VKFrameBuffer *VKContext::active_framebuffer_get() const
{
  return unwrap(active_fb);
}

bool VKContext::has_active_framebuffer() const
{
  return active_framebuffer_get() != nullptr;
}

void VKContext::deactivate_framebuffer()
{
  VKFrameBuffer *framebuffer = active_framebuffer_get();
  BLI_assert(framebuffer != nullptr);
  if (framebuffer->is_rendering()) {
    framebuffer->rendering_end(*this);
  }
  active_fb = nullptr;
}

void VKContext::rendering_end()
{
  VKFrameBuffer *framebuffer = active_framebuffer_get();
  if (framebuffer) {
    framebuffer->rendering_end(*this);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pipeline
 * \{ */

void VKContext::update_pipeline_data(GPUPrimType primitive,
                                     VKVertexAttributeObject &vao,
                                     render_graph::VKPipelineDataGraphics &r_pipeline_data)
{
  VKShader &vk_shader = unwrap(*shader);
  VKFrameBuffer &framebuffer = *active_framebuffer_get();

  /* Override size of point shader when GPU_point size < 0 */
  const float point_size = state_manager_get().mutable_state.point_size;
  if (primitive == GPU_PRIM_POINTS && point_size < 0.0) {
    GPU_shader_uniform_1f(shader, "size", -point_size);
  }

  /* Dynamic state line width */
  const bool is_line_primitive = ELEM(primitive,
                                      GPU_PRIM_LINES,
                                      GPU_PRIM_LINE_LOOP,
                                      GPU_PRIM_LINE_STRIP,
                                      GPU_PRIM_LINES_ADJ,
                                      GPU_PRIM_LINE_STRIP_ADJ);

  if (is_line_primitive) {
    const bool supports_wide_lines = VKBackend::get().device.extensions_get().wide_lines;
    r_pipeline_data.line_width = supports_wide_lines ?
                                     state_manager_get().mutable_state.line_width :
                                     1.0f;
  }
  else {
    r_pipeline_data.line_width.reset();
  }

  update_pipeline_data(vk_shader,
                       vk_shader.ensure_and_get_graphics_pipeline(
                           primitive, vao, state_manager_get(), framebuffer, constants_state_),
                       r_pipeline_data.pipeline_data);
}

void VKContext::update_pipeline_data(render_graph::VKPipelineData &r_pipeline_data)
{
  VKShader &vk_shader = unwrap(*shader);
  update_pipeline_data(
      vk_shader, vk_shader.ensure_and_get_compute_pipeline(constants_state_), r_pipeline_data);
}

void VKContext::update_pipeline_data(VKShader &vk_shader,
                                     VkPipeline vk_pipeline,
                                     render_graph::VKPipelineData &r_pipeline_data)
{
  r_pipeline_data.vk_pipeline_layout = vk_shader.vk_pipeline_layout;
  r_pipeline_data.vk_pipeline = vk_pipeline;

  /* Update push constants. */
  r_pipeline_data.push_constants_data = nullptr;
  r_pipeline_data.push_constants_size = 0;
  const VKPushConstants::Layout &push_constants_layout =
      vk_shader.interface_get().push_constants_layout_get();
  if (push_constants_layout.storage_type_get() == VKPushConstants::StorageType::PUSH_CONSTANTS) {
    r_pipeline_data.push_constants_size = push_constants_layout.size_in_bytes();
    r_pipeline_data.push_constants_data = vk_shader.push_constants.data();
  }

  /* Update descriptor set. */
  r_pipeline_data.vk_descriptor_set = VK_NULL_HANDLE;
  if (vk_shader.has_descriptor_set()) {
    VKDescriptorSetTracker &descriptor_set = descriptor_set_get();
    descriptor_set.update_descriptor_set(*this, access_info_, r_pipeline_data);
  }
}

render_graph::VKResourceAccessInfo &VKContext::reset_and_get_access_info()
{
  access_info_.reset();
  return access_info_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Graphics pipeline
 * \{ */

void VKContext::swap_buffer_acquired_callback()
{
  VKContext *context = VKContext::get();
  BLI_assert(context);
  context->swap_buffer_acquired_handler();
}

void VKContext::swap_buffer_draw_callback(const GHOST_VulkanSwapChainData *swap_chain_data)
{
  VKContext *context = VKContext::get();
  BLI_assert(context);
  context->swap_buffer_draw_handler(*swap_chain_data);
}

void VKContext::swap_buffer_acquired_handler()
{
  sync_backbuffer();
}

void VKContext::swap_buffer_draw_handler(const GHOST_VulkanSwapChainData &swap_chain_data)
{
  const bool do_blit_to_swapchain = swap_chain_data.image != VK_NULL_HANDLE;
  const bool use_shader = swap_chain_data.surface_format.colorSpace ==
                          VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;

  /* When swapchain is invalid/minimized we only flush the render graph to free GPU resources. */
  if (!do_blit_to_swapchain) {
    flush_render_graph(RenderGraphFlushFlags::SUBMIT | RenderGraphFlushFlags::RENEW_RENDER_GRAPH);
    return;
  }

  VKDevice &device = VKBackend::get().device;
  render_graph::VKRenderGraph &render_graph = this->render_graph();
  VKFrameBuffer &framebuffer = *unwrap(active_fb);
  framebuffer.rendering_end(*this);
  VKTexture *color_attachment = unwrap(unwrap(framebuffer.color_tex(0)));
  device.resources.add_swapchain_image(swap_chain_data.image, "SwapchainImage");

  GPU_debug_group_begin("BackBuffer.Blit");
  if (use_shader) {
    VKTexture swap_chain_texture("swap_chain_texture");
    swap_chain_texture.init_swapchain(swap_chain_data.image,
                                      to_gpu_format(swap_chain_data.surface_format.format));
    Shader *shader = device.vk_backbuffer_blit_sh_get();
    GPU_shader_bind(shader);
    GPU_shader_uniform_1f(shader, "sdr_scale", swap_chain_data.sdr_scale);
    VKStateManager &state_manager = state_manager_get();
    state_manager.image_bind(color_attachment, 0);
    state_manager.image_bind(&swap_chain_texture, 1);
    int2 dispatch_size = math::divide_ceil(
        int2(swap_chain_data.extent.width, swap_chain_data.extent.height), int2(16));
    VKBackend::get().compute_dispatch(UNPACK2(dispatch_size), 1);
  }
  else {
    render_graph::VKBlitImageNode::CreateInfo blit_image = {};
    blit_image.src_image = color_attachment->vk_image_handle();
    blit_image.dst_image = swap_chain_data.image;
    blit_image.filter = VK_FILTER_LINEAR;

    VkImageBlit &region = blit_image.region;
    region.srcOffsets[0] = {0, 0, 0};
    region.srcOffsets[1] = {color_attachment->width_get(), color_attachment->height_get(), 1};
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.mipLevel = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount = 1;

    region.dstOffsets[0] = {0, int32_t(swap_chain_data.extent.height), 0};
    region.dstOffsets[1] = {int32_t(swap_chain_data.extent.width), 0, 1};
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount = 1;

    render_graph.add_node(blit_image);
  }

  render_graph::VKSynchronizationNode::CreateInfo synchronization = {};
  synchronization.vk_image = swap_chain_data.image;
  synchronization.vk_image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  synchronization.vk_image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  render_graph.add_node(synchronization);
  GPU_debug_group_end();

  flush_render_graph(RenderGraphFlushFlags::SUBMIT | RenderGraphFlushFlags::RENEW_RENDER_GRAPH,
                     VK_PIPELINE_STAGE_TRANSFER_BIT,
                     swap_chain_data.acquire_semaphore,
                     swap_chain_data.present_semaphore,
                     swap_chain_data.submission_fence);

  device.resources.remove_image(swap_chain_data.image);
#if 0
  device.debug_print();
#endif
}

void VKContext::specialization_constants_set(
    const shader::SpecializationConstants *constants_state)
{
  constants_state_ = (constants_state != nullptr) ? *constants_state :
                                                    shader::SpecializationConstants{};
}

std::unique_ptr<VKStreamingBuffer> &VKContext::get_or_create_streaming_buffer(
    VKBuffer &buffer, VkDeviceSize min_offset_alignment)
{
  for (std::unique_ptr<VKStreamingBuffer> &streaming_buffer : streaming_buffers_) {
    if (streaming_buffer->vk_buffer_dst() == buffer.vk_handle()) {
      return streaming_buffer;
    }
  }

  streaming_buffers_.append(std::make_unique<VKStreamingBuffer>(buffer, min_offset_alignment));
  return streaming_buffers_.last();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name OpenXR
 * \{ */

void VKContext::openxr_acquire_framebuffer_image_callback(GHOST_VulkanOpenXRData *openxr_data)
{
  VKContext *context = VKContext::get();
  BLI_assert(context);
  context->openxr_acquire_framebuffer_image_handler(*openxr_data);
}

void VKContext::openxr_release_framebuffer_image_callback(GHOST_VulkanOpenXRData *openxr_data)
{
  VKContext *context = VKContext::get();
  BLI_assert(context);
  context->openxr_release_framebuffer_image_handler(*openxr_data);
}

void VKContext::openxr_acquire_framebuffer_image_handler(GHOST_VulkanOpenXRData &openxr_data)
{
  VKFrameBuffer &framebuffer = *unwrap(active_fb);
  VKTexture *color_attachment = unwrap(unwrap(framebuffer.color_tex(0)));
  openxr_data.extent.width = color_attachment->width_get();
  openxr_data.extent.height = color_attachment->height_get();

  /* Determine the data format for data transfer. */
  const TextureFormat device_format = color_attachment->device_format_get();
  eGPUDataFormat data_format = GPU_DATA_HALF_FLOAT;
  if (ELEM(device_format, TextureFormat::UNORM_8_8_8_8)) {
    data_format = GPU_DATA_UBYTE;
  }

  switch (openxr_data.data_transfer_mode) {
    case GHOST_kVulkanXRModeCPU:
      openxr_data.cpu.image_data = color_attachment->read(0, data_format);
      break;

    case GHOST_kVulkanXRModeFD: {
      flush_render_graph(RenderGraphFlushFlags::SUBMIT |
                         RenderGraphFlushFlags::WAIT_FOR_COMPLETION |
                         RenderGraphFlushFlags::RENEW_RENDER_GRAPH);
      if (openxr_data.gpu.vk_image_blender != color_attachment->vk_image_handle()) {
        VKMemoryExport exported_memory = color_attachment->export_memory(
            VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT);
        openxr_data.gpu.image_handle = exported_memory.handle;
        openxr_data.gpu.new_handle = true;
        openxr_data.gpu.image_format = to_vk_format(color_attachment->device_format_get());
        openxr_data.gpu.memory_size = exported_memory.memory_size;
        openxr_data.gpu.memory_offset = exported_memory.memory_offset;
        openxr_data.gpu.vk_image_blender = color_attachment->vk_image_handle();
      }
      break;
    }

    case GHOST_kVulkanXRModeWin32: {
      flush_render_graph(RenderGraphFlushFlags::SUBMIT |
                         RenderGraphFlushFlags::WAIT_FOR_COMPLETION |
                         RenderGraphFlushFlags::RENEW_RENDER_GRAPH);
      if (openxr_data.gpu.vk_image_blender != color_attachment->vk_image_handle()) {
        VKMemoryExport exported_memory = color_attachment->export_memory(
            VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT);
        openxr_data.gpu.image_handle = exported_memory.handle;
        openxr_data.gpu.new_handle = true;
        openxr_data.gpu.image_format = to_vk_format(color_attachment->device_format_get());
        openxr_data.gpu.memory_size = exported_memory.memory_size;
        openxr_data.gpu.memory_offset = exported_memory.memory_offset;
        openxr_data.gpu.vk_image_blender = color_attachment->vk_image_handle();
      }
      break;
    }
  }
}

void VKContext::openxr_release_framebuffer_image_handler(GHOST_VulkanOpenXRData &openxr_data)
{
  switch (openxr_data.data_transfer_mode) {
    case GHOST_kVulkanXRModeCPU:
      MEM_freeN(openxr_data.cpu.image_data);
      openxr_data.cpu.image_data = nullptr;
      break;

    case GHOST_kVulkanXRModeFD:
      /* Nothing to do as import of the handle by the XrInstance removes the ownership of the
       * handle. Ref
       * https://registry.khronos.org/vulkan/specs/latest/man/html/VK_KHR_external_memory_fd.html#_issues
       */
      break;

    case GHOST_kVulkanXRModeWin32:
#ifdef _WIN32
      if (openxr_data.gpu.new_handle) {
        /* Exported handle isn't consumed during import and should be freed after use. */
        CloseHandle(HANDLE(openxr_data.gpu.image_handle));
        openxr_data.gpu.image_handle = 0;
      }
#endif
      break;
  }
}

/** \} */

}  // namespace blender::gpu

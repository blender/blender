/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "GHOST_C-api.h"

#include "gpu_capabilities_private.hh"
#include "gpu_platform_private.hh"

#include "vk_batch.hh"
#include "vk_context.hh"
#include "vk_drawlist.hh"
#include "vk_fence.hh"
#include "vk_framebuffer.hh"
#include "vk_index_buffer.hh"
#include "vk_pixel_buffer.hh"
#include "vk_query.hh"
#include "vk_shader.hh"
#include "vk_state_manager.hh"
#include "vk_storage_buffer.hh"
#include "vk_texture.hh"
#include "vk_uniform_buffer.hh"
#include "vk_vertex_buffer.hh"

#include "vk_backend.hh"

namespace blender::gpu {

static eGPUOSType determine_os_type()
{
#ifdef _WIN32
  return GPU_OS_WIN;
#elif defined(__APPLE__)
  return GPU_OS_MAC;
#else
  return GPU_OS_UNIX;
#endif
}

void VKBackend::platform_init()
{
  GPG.init(GPU_DEVICE_ANY,
           determine_os_type(),
           GPU_DRIVER_ANY,
           GPU_SUPPORT_LEVEL_SUPPORTED,
           GPU_BACKEND_VULKAN,
           "",
           "",
           "",
           GPU_ARCHITECTURE_IMR);
}

void VKBackend::platform_init(const VKDevice &device)
{
  const VkPhysicalDeviceProperties &properties = device.physical_device_properties_get();

  eGPUDeviceType device_type = device.device_type();
  eGPUOSType os = determine_os_type();
  eGPUDriverType driver = GPU_DRIVER_ANY;
  eGPUSupportLevel support_level = GPU_SUPPORT_LEVEL_SUPPORTED;

  std::string vendor_name = device.vendor_name();
  std::string driver_version = device.driver_version();

  GPG.init(device_type,
           os,
           driver,
           support_level,
           GPU_BACKEND_VULKAN,
           vendor_name.c_str(),
           properties.deviceName,
           driver_version.c_str(),
           GPU_ARCHITECTURE_IMR);
}

void VKBackend::detect_workarounds(VKDevice &device)
{
  VKWorkarounds workarounds;

  if (G.debug & G_DEBUG_GPU_FORCE_WORKAROUNDS) {
    printf("\n");
    printf("VK: Forcing workaround usage and disabling features and extensions.\n");
    printf("    Vendor: %s\n", device.vendor_name().c_str());
    printf("    Device: %s\n", device.physical_device_properties_get().deviceName);
    printf("    Driver: %s\n", device.driver_version().c_str());
    /* Force workarounds. */
    workarounds.not_aligned_pixel_formats = true;
    workarounds.shader_output_layer = true;
    workarounds.shader_output_viewport_index = true;
    workarounds.vertex_formats.r8g8b8 = true;

    device.workarounds_ = workarounds;
    return;
  }

  workarounds.shader_output_layer =
      !device.physical_device_vulkan_12_features_get().shaderOutputLayer;
  workarounds.shader_output_viewport_index =
      !device.physical_device_vulkan_12_features_get().shaderOutputViewportIndex;

  /* AMD GPUs don't support texture formats that use are aligned to 24 or 48 bits. */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY) ||
      GPU_type_matches(GPU_DEVICE_APPLE, GPU_OS_MAC, GPU_DRIVER_ANY))
  {
    workarounds.not_aligned_pixel_formats = true;
  }

  VkFormatProperties format_properties = {};
  vkGetPhysicalDeviceFormatProperties(
      device.physical_device_get(), VK_FORMAT_R8G8B8_UNORM, &format_properties);
  workarounds.vertex_formats.r8g8b8 = (format_properties.bufferFeatures &
                                       VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) == 0;

  device.workarounds_ = workarounds;
}

void VKBackend::platform_exit()
{
  GPG.clear();
  VKDevice &device = VKBackend::get().device_;
  if (device.is_initialized()) {
    device.deinit();
  }
}

void VKBackend::delete_resources() {}

void VKBackend::samplers_update()
{
  VKDevice &device = VKBackend::get().device_;
  if (device.is_initialized()) {
    device.reinit();
  }
}

void VKBackend::compute_dispatch(int groups_x_len, int groups_y_len, int groups_z_len)
{
  VKContext &context = *VKContext::get();
  if (use_render_graph) {
    render_graph::VKDispatchCreateInfo &dispatch_info = context.update_and_get_dispatch_info();
    dispatch_info.dispatch_node.group_count_x = groups_x_len;
    dispatch_info.dispatch_node.group_count_y = groups_y_len;
    dispatch_info.dispatch_node.group_count_z = groups_z_len;
    context.render_graph.add_node(dispatch_info);
  }
  else {
    render_graph::VKResourceAccessInfo resource_access_info = {};
    context.state_manager_get().apply_bindings(context, resource_access_info);
    context.bind_compute_pipeline();
    VKCommandBuffers &command_buffers = context.command_buffers_get();
    command_buffers.dispatch(groups_x_len, groups_y_len, groups_z_len);
  }
}

void VKBackend::compute_dispatch_indirect(StorageBuf *indirect_buf)
{
  BLI_assert(indirect_buf);
  VKContext &context = *VKContext::get();
  render_graph::VKResourceAccessInfo resource_access_info = {};
  context.state_manager_get().apply_bindings(context, resource_access_info);
  context.bind_compute_pipeline();
  VKStorageBuffer &indirect_buffer = *unwrap(indirect_buf);
  VKCommandBuffers &command_buffers = context.command_buffers_get();
  command_buffers.dispatch(indirect_buffer);
}

Context *VKBackend::context_alloc(void *ghost_window, void *ghost_context)
{
  if (ghost_window) {
    BLI_assert(ghost_context == nullptr);
    ghost_context = GHOST_GetDrawingContext((GHOST_WindowHandle)ghost_window);
  }

  BLI_assert(ghost_context != nullptr);
  if (!device_.is_initialized()) {
    device_.init(ghost_context);
  }

  VKContext *context = new VKContext(ghost_window, ghost_context, device_.resources);
  device_.context_register(*context);
  GHOST_SetVulkanSwapBuffersCallbacks((GHOST_ContextHandle)ghost_context,
                                      VKContext::swap_buffers_pre_callback,
                                      VKContext::swap_buffers_post_callback);
  return context;
}

Batch *VKBackend::batch_alloc()
{
  return new VKBatch();
}

DrawList *VKBackend::drawlist_alloc(int list_length)
{
  return new VKDrawList(list_length);
}

Fence *VKBackend::fence_alloc()
{
  return new VKFence();
}

FrameBuffer *VKBackend::framebuffer_alloc(const char *name)
{
  return new VKFrameBuffer(name);
}

IndexBuf *VKBackend::indexbuf_alloc()
{
  return new VKIndexBuffer();
}

PixelBuffer *VKBackend::pixelbuf_alloc(uint size)
{
  return new VKPixelBuffer(size);
}

QueryPool *VKBackend::querypool_alloc()
{
  return new VKQueryPool();
}

Shader *VKBackend::shader_alloc(const char *name)
{
  return new VKShader(name);
}

Texture *VKBackend::texture_alloc(const char *name)
{
  return new VKTexture(name);
}

UniformBuf *VKBackend::uniformbuf_alloc(int size, const char *name)
{
  return new VKUniformBuffer(size, name);
}

StorageBuf *VKBackend::storagebuf_alloc(int size, GPUUsageType usage, const char *name)
{
  return new VKStorageBuffer(size, usage, name);
}

VertBuf *VKBackend::vertbuf_alloc()
{
  return new VKVertexBuffer();
}

void VKBackend::render_begin() {}

void VKBackend::render_end() {}

void VKBackend::render_step() {}

shaderc::Compiler &VKBackend::get_shaderc_compiler()
{
  return shaderc_compiler_;
}

void VKBackend::capabilities_init(VKDevice &device)
{
  const VkPhysicalDeviceProperties &properties = device.physical_device_properties_get();
  const VkPhysicalDeviceLimits &limits = properties.limits;

  /* Reset all capabilities from previous context. */
  GCaps = {};
  GCaps.geometry_shader_support = true;
  GCaps.shader_draw_parameters_support =
      device.physical_device_vulkan_11_features_get().shaderDrawParameters;

  GCaps.max_texture_size = max_ii(limits.maxImageDimension1D, limits.maxImageDimension2D);
  GCaps.max_texture_3d_size = limits.maxImageDimension3D;
  GCaps.max_texture_layers = limits.maxImageArrayLayers;
  GCaps.max_textures = limits.maxDescriptorSetSampledImages;
  GCaps.max_textures_vert = limits.maxPerStageDescriptorSampledImages;
  GCaps.max_textures_geom = limits.maxPerStageDescriptorSampledImages;
  GCaps.max_textures_frag = limits.maxPerStageDescriptorSampledImages;
  GCaps.max_samplers = limits.maxSamplerAllocationCount;
  GCaps.max_images = limits.maxPerStageDescriptorStorageImages;
  for (int i = 0; i < 3; i++) {
    GCaps.max_work_group_count[i] = limits.maxComputeWorkGroupCount[i];
    GCaps.max_work_group_size[i] = limits.maxComputeWorkGroupSize[i];
  }
  GCaps.max_uniforms_vert = limits.maxPerStageDescriptorUniformBuffers;
  GCaps.max_uniforms_frag = limits.maxPerStageDescriptorUniformBuffers;
  GCaps.max_batch_indices = limits.maxDrawIndirectCount;
  GCaps.max_batch_vertices = limits.maxDrawIndexedIndexValue;
  GCaps.max_vertex_attribs = limits.maxVertexInputAttributes;
  GCaps.max_varying_floats = limits.maxVertexOutputComponents;
  GCaps.max_shader_storage_buffer_bindings = limits.maxPerStageDescriptorStorageBuffers;
  GCaps.max_compute_shader_storage_blocks = limits.maxPerStageDescriptorStorageBuffers;
  GCaps.max_storage_buffer_size = size_t(limits.maxStorageBufferRange);

  GCaps.mem_stats_support = true;

  detect_workarounds(device);
}

}  // namespace blender::gpu

/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

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
           "");
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
           driver_version.c_str());
}

void VKBackend::detect_workarounds(VKDevice &device)
{
  VKWorkarounds workarounds;

  /* AMD GPUs don't support texture formats that use are aligned to 24 or 48 bits. */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    workarounds.not_aligned_pixel_formats = true;
  }

  device.workarounds_ = workarounds;
}

void VKBackend::platform_exit()
{
  GPG.clear();
}

void VKBackend::delete_resources()
{
  if (device_.is_initialized()) {
    device_.deinit();
  }
}

void VKBackend::samplers_update() {}

void VKBackend::compute_dispatch(int groups_x_len, int groups_y_len, int groups_z_len)
{
  VKContext &context = *VKContext::get();
  context.state_manager_get().apply_bindings();
  context.bind_compute_pipeline();
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  command_buffer.dispatch(groups_x_len, groups_y_len, groups_z_len);
}

void VKBackend::compute_dispatch_indirect(StorageBuf * /*indirect_buf*/) {}

Context *VKBackend::context_alloc(void *ghost_window, void *ghost_context)
{
  return new VKContext(ghost_window, ghost_context);
}

Batch *VKBackend::batch_alloc()
{
  return new VKBatch();
}

DrawList *VKBackend::drawlist_alloc(int /*list_length*/)
{
  return new VKDrawList();
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
  GCaps.compute_shader_support = true;
  GCaps.shader_storage_buffer_objects_support = true;
  GCaps.shader_image_load_store_support = true;

  GCaps.max_texture_size = max_ii(limits.maxImageDimension1D, limits.maxImageDimension2D);
  GCaps.max_texture_3d_size = limits.maxImageDimension3D;
  GCaps.max_texture_layers = limits.maxImageArrayLayers;
  GCaps.max_textures = limits.maxDescriptorSetSampledImages;
  GCaps.max_textures_vert = limits.maxPerStageDescriptorSampledImages;
  GCaps.max_textures_geom = limits.maxPerStageDescriptorSampledImages;
  GCaps.max_textures_frag = limits.maxPerStageDescriptorSampledImages;
  GCaps.max_samplers = limits.maxSamplerAllocationCount;
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

  detect_workarounds(device);
}

}  // namespace blender::gpu

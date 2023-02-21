/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

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
#include "vk_storage_buffer.hh"
#include "vk_texture.hh"
#include "vk_uniform_buffer.hh"
#include "vk_vertex_buffer.hh"

#include "vk_backend.hh"

namespace blender::gpu {

void VKBackend::init_platform()
{
  BLI_assert(!GPG.initialized);

  eGPUDeviceType device = GPU_DEVICE_ANY;
  eGPUOSType os = GPU_OS_ANY;
  eGPUDriverType driver = GPU_DRIVER_ANY;
  eGPUSupportLevel support_level = GPU_SUPPORT_LEVEL_SUPPORTED;

#ifdef _WIN32
  os = GPU_OS_WIN;
#elif defined(__APPLE__)
  os = GPU_OS_MAC;
#else
  os = GPU_OS_UNIX;
#endif

  GPG.init(device, os, driver, support_level, GPU_BACKEND_VULKAN, "", "", "");
}

void VKBackend::platform_exit()
{
  BLI_assert(GPG.initialized);
  GPG.clear();
}

void VKBackend::delete_resources()
{
}

void VKBackend::samplers_update()
{
}

void VKBackend::compute_dispatch(int groups_x_len, int groups_y_len, int groups_z_len)
{
  VKContext &context = *VKContext::get();
  VKShader *shader = static_cast<VKShader *>(context.shader);
  VKCommandBuffer &command_buffer = context.command_buffer_get();
  VKPipeline &pipeline = shader->pipeline_get();
  VKDescriptorSet &descriptor_set = pipeline.descriptor_set_get();
  descriptor_set.update(context.device_get());
  command_buffer.bind(
      descriptor_set, shader->vk_pipeline_layout_get(), VK_PIPELINE_BIND_POINT_COMPUTE);
  command_buffer.dispatch(groups_x_len, groups_y_len, groups_z_len);
}

void VKBackend::compute_dispatch_indirect(StorageBuf * /*indirect_buf*/)
{
}

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

void VKBackend::render_begin()
{
}

void VKBackend::render_end()
{
}

void VKBackend::render_step()
{
}

shaderc::Compiler &VKBackend::get_shaderc_compiler()
{
  return shaderc_compiler_;
}

void VKBackend::capabilities_init(VKContext & /*context*/)
{
  /* Reset all capabilities from previous context. */
  GCaps = {};
  GCaps.compute_shader_support = true;
  GCaps.shader_storage_buffer_objects_support = true;
  GCaps.shader_image_load_store_support = true;
}

}  // namespace blender::gpu

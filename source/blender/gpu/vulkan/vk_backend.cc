/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#include "vk_backend.hh"

#include "vk_batch.hh"
#include "vk_context.hh"
#include "vk_drawlist.hh"
#include "vk_framebuffer.hh"
#include "vk_index_buffer.hh"
#include "vk_query.hh"
#include "vk_shader.hh"
#include "vk_storage_buffer.hh"
#include "vk_texture.hh"
#include "vk_uniform_buffer.hh"
#include "vk_vertex_buffer.hh"

namespace blender::gpu {

void VKBackend::delete_resources()
{
}

void VKBackend::samplers_update()
{
}

void VKBackend::compute_dispatch(int /*groups_x_len*/, int /*groups_y_len*/, int /*groups_z_len*/)
{
}

void VKBackend::compute_dispatch_indirect(StorageBuf * /*indirect_buf*/)
{
}

Context *VKBackend::context_alloc(void * /*ghost_window*/, void * /*ghost_context*/)
{
  return new VKContext();
}

Batch *VKBackend::batch_alloc()
{
  return new VKBatch();
}

DrawList *VKBackend::drawlist_alloc(int /*list_length*/)
{
  return new VKDrawList();
}

FrameBuffer *VKBackend::framebuffer_alloc(const char *name)
{
  return new VKFrameBuffer(name);
}

IndexBuf *VKBackend::indexbuf_alloc()
{
  return new VKIndexBuffer();
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

StorageBuf *VKBackend::storagebuf_alloc(int size, GPUUsageType /*usage*/, const char *name)
{
  return new VKStorageBuffer(size, name);
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

}  // namespace blender::gpu
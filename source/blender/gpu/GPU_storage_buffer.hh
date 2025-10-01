/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Storage buffers API. Used to handle many way bigger buffers than Uniform buffers update at once.
 * Make sure that the data structure is compatible with what the implementation expect.
 * (see "7.8 Shader Buffer Variables and Shader Storage Blocks" from the OpenGL spec for more info
 * about std430 layout)
 * Rule of thumb: Padding to 16bytes, don't use vec3.
 */

#pragma once

#include "GPU_texture.hh"
#include "GPU_vertex_buffer.hh"

namespace blender::gpu {
class StorageBuf;
}  // namespace blender::gpu

blender::gpu::StorageBuf *GPU_storagebuf_create_ex(size_t size,
                                                   const void *data,
                                                   GPUUsageType usage,
                                                   const char *name);

#define GPU_storagebuf_create(size) \
  GPU_storagebuf_create_ex(size, nullptr, GPU_USAGE_DYNAMIC, __func__);

void GPU_storagebuf_free(blender::gpu::StorageBuf *ssbo);

/**
 * Limit the size of the storage buffer.
 *
 * Backends can optimize data transfers using the size that is actually used.
 */
void GPU_storagebuf_usage_size_set(blender::gpu::StorageBuf *ssbo, size_t size);
void GPU_storagebuf_update(blender::gpu::StorageBuf *ssbo, const void *data);

void GPU_storagebuf_bind(blender::gpu::StorageBuf *ssbo, int slot);
void GPU_storagebuf_unbind(blender::gpu::StorageBuf *ssbo);
/**
 * Resets the internal slot usage tracking. But there is no guarantee that
 * this actually undo the bindings for the next draw call. Only has effect when G_DEBUG_GPU is set.
 */
void GPU_storagebuf_debug_unbind_all();

void GPU_storagebuf_clear_to_zero(blender::gpu::StorageBuf *ssbo);

/**
 * Clear the content of the buffer using the given #clear_value. #clear_value will be used as a
 * repeatable pattern of 32bits.
 */
void GPU_storagebuf_clear(blender::gpu::StorageBuf *ssbo, uint32_t clear_value);

/**
 * Explicitly sync updated storage buffer contents back to host within the GPU command stream. This
 * ensures any changes made by the GPU are visible to the host.
 * NOTE: This command is only valid for host-visible storage buffers.
 */
void GPU_storagebuf_sync_to_host(blender::gpu::StorageBuf *ssbo);

/**
 * Read back content of the buffer to CPU for inspection.
 * Slow! Only use for inspection / debugging.
 *
 * NOTE: If GPU_storagebuf_sync_to_host is called, this command is synchronized against that call.
 * If pending GPU updates to the storage buffer are not yet visible to the host, the command will
 * stall until dependent GPU work has completed.
 *
 * Otherwise, this command is synchronized against this call and will stall the CPU until the
 * buffer content can be read by the host.
 */
void GPU_storagebuf_read(blender::gpu::StorageBuf *ssbo, void *data);

/**
 * \brief Copy a part of a vertex buffer to a storage buffer.
 *
 * \param ssbo: destination storage buffer
 * \param src: source vertex buffer
 * \param dst_offset: where to start copying to (in bytes).
 * \param src_offset: where to start copying from (in bytes).
 * \param copy_size: byte size of the segment to copy.
 */
void GPU_storagebuf_copy_sub_from_vertbuf(blender::gpu::StorageBuf *ssbo,
                                          blender::gpu::VertBuf *src,
                                          uint dst_offset,
                                          uint src_offset,
                                          uint copy_size);

/**
 * Ensure the ssbo is ready to be used as an indirect buffer in `GPU_batch_draw_indirect`.
 * NOTE: Internally, this is only required for the OpenGL backend.
 */
void GPU_storagebuf_sync_as_indirect_buffer(blender::gpu::StorageBuf *ssbo);

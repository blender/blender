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

/** Opaque type hiding blender::gpu::StorageBuf. */
struct GPUStorageBuf;

GPUStorageBuf *GPU_storagebuf_create_ex(size_t size,
                                        const void *data,
                                        GPUUsageType usage,
                                        const char *name);

#define GPU_storagebuf_create(size) \
  GPU_storagebuf_create_ex(size, nullptr, GPU_USAGE_DYNAMIC, __func__);

void GPU_storagebuf_free(GPUStorageBuf *ssbo);

void GPU_storagebuf_update(GPUStorageBuf *ssbo, const void *data);

void GPU_storagebuf_bind(GPUStorageBuf *ssbo, int slot);
void GPU_storagebuf_unbind(GPUStorageBuf *ssbo);
void GPU_storagebuf_unbind_all();

void GPU_storagebuf_clear_to_zero(GPUStorageBuf *ssbo);

/**
 * Clear the content of the buffer using the given #clear_value. #clear_value will be used as a
 * repeatable pattern of 32bits.
 */
void GPU_storagebuf_clear(GPUStorageBuf *ssbo, uint32_t clear_value);

/**
 * Explicitly sync updated storage buffer contents back to host within the GPU command stream. This
 * ensures any changes made by the GPU are visible to the host.
 * NOTE: This command is only valid for host-visible storage buffers.
 */
void GPU_storagebuf_sync_to_host(GPUStorageBuf *ssbo);

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
void GPU_storagebuf_read(GPUStorageBuf *ssbo, void *data);

/**
 * \brief Copy a part of a vertex buffer to a storage buffer.
 *
 * \param ssbo: destination storage buffer
 * \param src: source vertex buffer
 * \param dst_offset: where to start copying to (in bytes).
 * \param src_offset: where to start copying from (in bytes).
 * \param copy_size: byte size of the segment to copy.
 */
void GPU_storagebuf_copy_sub_from_vertbuf(
    GPUStorageBuf *ssbo, GPUVertBuf *src, uint dst_offset, uint src_offset, uint copy_size);

/**
 * Ensure the ssbo is ready to be used as an indirect buffer in `GPU_batch_draw_indirect`.
 * NOTE: Internally, this is only required for the OpenGL backend.
 */
void GPU_storagebuf_sync_as_indirect_buffer(GPUStorageBuf *ssbo);

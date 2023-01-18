/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

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

#include "GPU_texture.h"
#include "GPU_vertex_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque type hiding blender::gpu::StorageBuf. */
typedef struct GPUStorageBuf GPUStorageBuf;

GPUStorageBuf *GPU_storagebuf_create_ex(size_t size,
                                        const void *data,
                                        GPUUsageType usage,
                                        const char *name);

#define GPU_storagebuf_create(size) \
  GPU_storagebuf_create_ex(size, NULL, GPU_USAGE_DYNAMIC, __func__);

void GPU_storagebuf_free(GPUStorageBuf *ssbo);

void GPU_storagebuf_update(GPUStorageBuf *ssbo, const void *data);

void GPU_storagebuf_bind(GPUStorageBuf *ssbo, int slot);
void GPU_storagebuf_unbind(GPUStorageBuf *ssbo);
void GPU_storagebuf_unbind_all(void);

void GPU_storagebuf_clear(GPUStorageBuf *ssbo,
                          eGPUTextureFormat internal_format,
                          eGPUDataFormat data_format,
                          void *data);
void GPU_storagebuf_clear_to_zero(GPUStorageBuf *ssbo);

/**
 * Read back content of the buffer to CPU for inspection.
 * Slow! Only use for inspection / debugging.
 * NOTE: Not synchronized. Use appropriate barrier before reading.
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

#ifdef __cplusplus
}
#endif

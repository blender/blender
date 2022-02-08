/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

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

#include "GPU_vertex_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;

/** Opaque type hiding blender::gpu::StorageBuf. */
typedef struct GPUStorageBuf GPUStorageBuf;

GPUStorageBuf *GPU_storagebuf_create_ex(size_t size,
                                        const void *data,
                                        GPUUsageType usage,
                                        const char *name);

#define GPU_storagebuf_create(size) \
  GPU_storagebuf_create_ex(size, NULL, GPU_USAGE_DYNAMIC, __func__);

void GPU_storagebuf_free(GPUStorageBuf *ubo);

void GPU_storagebuf_update(GPUStorageBuf *ubo, const void *data);

void GPU_storagebuf_bind(GPUStorageBuf *ubo, int slot);
void GPU_storagebuf_unbind(GPUStorageBuf *ubo);
void GPU_storagebuf_unbind_all(void);

#ifdef __cplusplus
}
#endif

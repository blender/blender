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
 * Uniform buffers API. Used to handle many uniforms update at once.
 * Make sure that the data structure is compatible with what the implementation expect.
 * (see "7.6.2.2 Standard Uniform Block Layout" from the OpenGL spec for more info about std140
 * layout)
 * Rule of thumb: Padding to 16bytes, don't use vec3, don't use arrays of anything that is not vec4
 * aligned .
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;

/** Opaque type hiding blender::gpu::UniformBuf. */
typedef struct GPUUniformBuf GPUUniformBuf;

GPUUniformBuf *GPU_uniformbuf_create_ex(size_t size, const void *data, const char *name);
GPUUniformBuf *GPU_uniformbuf_create_from_list(struct ListBase *inputs, const char *name);

#define GPU_uniformbuf_create(size) GPU_uniformbuf_create_ex(size, NULL, __func__);

void GPU_uniformbuf_free(GPUUniformBuf *ubo);

void GPU_uniformbuf_update(GPUUniformBuf *ubo, const void *data);

void GPU_uniformbuf_bind(GPUUniformBuf *ubo, int slot);
void GPU_uniformbuf_unbind(GPUUniformBuf *ubo);
void GPU_uniformbuf_unbind_all(void);

#define GPU_UBO_BLOCK_NAME "nodeTree"
#define GPU_ATTRIBUTE_UBO_BLOCK_NAME "uniformAttrs"

#ifdef __cplusplus
}
#endif

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

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
/**
 * Create UBO from inputs list.
 * Return NULL if failed to create or if \param inputs: is empty.
 *
 * \param inputs: ListBase of #BLI_genericNodeN(#GPUInput).
 */
GPUUniformBuf *GPU_uniformbuf_create_from_list(struct ListBase *inputs, const char *name);

#define GPU_uniformbuf_create(size) GPU_uniformbuf_create_ex(size, NULL, __func__);

void GPU_uniformbuf_free(GPUUniformBuf *ubo);

void GPU_uniformbuf_update(GPUUniformBuf *ubo, const void *data);

void GPU_uniformbuf_bind(GPUUniformBuf *ubo, int slot);
void GPU_uniformbuf_bind_as_ssbo(GPUUniformBuf *ubo, int slot);
void GPU_uniformbuf_unbind(GPUUniformBuf *ubo);
void GPU_uniformbuf_unbind_all(void);

void GPU_uniformbuf_clear_to_zero(GPUUniformBuf *ubo);

#define GPU_UBO_BLOCK_NAME "node_tree"
#define GPU_ATTRIBUTE_UBO_BLOCK_NAME "unf_attrs"
#define GPU_LAYER_ATTRIBUTE_UBO_BLOCK_NAME "drw_layer_attrs"

#ifdef __cplusplus
}
#endif

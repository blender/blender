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
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * GPU shader interface (C --> GLSL)
 */

#ifndef __GPU_SHADER_INTERFACE_H__
#define __GPU_SHADER_INTERFACE_H__

#include "GPU_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  GPU_UNIFORM_MODEL = 0,      /* mat4 ModelMatrix */
  GPU_UNIFORM_VIEW,           /* mat4 ViewMatrix */
  GPU_UNIFORM_MODELVIEW,      /* mat4 ModelViewMatrix */
  GPU_UNIFORM_PROJECTION,     /* mat4 ProjectionMatrix */
  GPU_UNIFORM_VIEWPROJECTION, /* mat4 ViewProjectionMatrix */
  GPU_UNIFORM_MVP,            /* mat4 ModelViewProjectionMatrix */

  GPU_UNIFORM_MODEL_INV,          /* mat4 ModelMatrixInverse */
  GPU_UNIFORM_VIEW_INV,           /* mat4 ViewMatrixInverse */
  GPU_UNIFORM_MODELVIEW_INV,      /* mat4 ModelViewMatrixInverse */
  GPU_UNIFORM_PROJECTION_INV,     /* mat4 ProjectionMatrixInverse */
  GPU_UNIFORM_VIEWPROJECTION_INV, /* mat4 ViewProjectionMatrixInverse */

  GPU_UNIFORM_NORMAL,     /* mat3 NormalMatrix */
  GPU_UNIFORM_ORCO,       /* vec4 OrcoTexCoFactors[] */
  GPU_UNIFORM_CLIPPLANES, /* vec4 WorldClipPlanes[] */

  GPU_UNIFORM_COLOR,          /* vec4 color */
  GPU_UNIFORM_BASE_INSTANCE,  /* int baseInstance */
  GPU_UNIFORM_RESOURCE_CHUNK, /* int resourceChunk */
  GPU_UNIFORM_RESOURCE_ID,    /* int resourceId */
  GPU_UNIFORM_SRGB_TRANSFORM, /* bool srgbTarget */

  GPU_NUM_UNIFORMS, /* Special value, denotes number of builtin uniforms. */
} GPUUniformBuiltin;

typedef enum {
  GPU_UNIFORM_BLOCK_VIEW = 0, /* viewBlock */
  GPU_UNIFORM_BLOCK_MODEL,    /* modelBlock */
  GPU_UNIFORM_BLOCK_INFO,     /* infoBlock */

  GPU_NUM_UNIFORM_BLOCKS, /* Special value, denotes number of builtin uniforms block. */
} GPUUniformBlockBuiltin;

typedef struct GPUShaderInput {
  uint32_t name_offset;
  uint32_t name_hash;
  int32_t location;
  /** Defined at interface creation or in shader. Only for Samplers, UBOs and Vertex Attribs. */
  int32_t binding;
} GPUShaderInput;

#define GPU_SHADERINTERFACE_REF_ALLOC_COUNT 16

typedef struct GPUShaderInterface {
  /** Buffer containing all inputs names separated by '\0'. */
  char *name_buffer;
  /** Reference to GPUBatches using this interface */
  struct GPUBatch **batches;
  uint batches_len;
  /** Input counts. */
  uint attribute_len;
  uint ubo_len;
  uint uniform_len;
  /** Enabled bindpoints that needs to be fed with data. */
  uint16_t enabled_attr_mask;
  uint16_t enabled_ubo_mask;
  uint64_t enabled_tex_mask;
  /** Opengl Location of builtin uniforms. Fast access, no lookup needed. */
  int32_t builtins[GPU_NUM_UNIFORMS];
  int32_t builtin_blocks[GPU_NUM_UNIFORM_BLOCKS];
  /** Flat array. In this order: Attributes, Ubos, Uniforms. */
  GPUShaderInput inputs[0];
} GPUShaderInterface;

GPUShaderInterface *GPU_shaderinterface_create(int32_t program_id);
void GPU_shaderinterface_discard(GPUShaderInterface *);

const GPUShaderInput *GPU_shaderinterface_uniform(const GPUShaderInterface *, const char *name);
int32_t GPU_shaderinterface_uniform_builtin(const GPUShaderInterface *shaderface,
                                            GPUUniformBuiltin builtin);
int32_t GPU_shaderinterface_block_builtin(const GPUShaderInterface *shaderface,
                                          GPUUniformBlockBuiltin builtin);
const GPUShaderInput *GPU_shaderinterface_ubo(const GPUShaderInterface *, const char *name);
const GPUShaderInput *GPU_shaderinterface_attr(const GPUShaderInterface *, const char *name);

/* keep track of batches using this interface */
void GPU_shaderinterface_add_batch_ref(GPUShaderInterface *, struct GPUBatch *);
void GPU_shaderinterface_remove_batch_ref(GPUShaderInterface *, struct GPUBatch *);

#ifdef __cplusplus
}
#endif

#endif /* __GPU_SHADER_INTERFACE_H__ */

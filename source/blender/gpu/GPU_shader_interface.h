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

typedef enum {
  GPU_UNIFORM_NONE = 0, /* uninitialized/unknown */

  GPU_UNIFORM_MODEL,          /* mat4 ModelMatrix */
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
  GPU_UNIFORM_ORCO,       /* vec3 OrcoTexCoFactors[] */
  GPU_UNIFORM_CLIPPLANES, /* vec4 WorldClipPlanes[] */

  GPU_UNIFORM_COLOR,       /* vec4 color */
  GPU_UNIFORM_CALLID,      /* int callId */
  GPU_UNIFORM_OBJECT_INFO, /* vec3 objectInfo */

  GPU_UNIFORM_CUSTOM, /* custom uniform, not one of the above built-ins */

  GPU_NUM_UNIFORMS, /* Special value, denotes number of builtin uniforms. */
} GPUUniformBuiltin;

typedef struct GPUShaderInput {
  struct GPUShaderInput *next;
  uint32_t name_offset;
  uint name_hash;
  /** Only for uniform inputs. */
  GPUUniformBuiltin builtin_type;
  /** Only for attribute inputs. */
  uint32_t gl_type;
  /** Only for attribute inputs. */
  int32_t size;
  int32_t location;
} GPUShaderInput;

#define GPU_NUM_SHADERINTERFACE_BUCKETS 257
#define GPU_SHADERINTERFACE_REF_ALLOC_COUNT 16

typedef struct GPUShaderInterface {
  int32_t program;
  uint32_t name_buffer_offset;
  GPUShaderInput *attr_buckets[GPU_NUM_SHADERINTERFACE_BUCKETS];
  GPUShaderInput *uniform_buckets[GPU_NUM_SHADERINTERFACE_BUCKETS];
  GPUShaderInput *ubo_buckets[GPU_NUM_SHADERINTERFACE_BUCKETS];
  GPUShaderInput *builtin_uniforms[GPU_NUM_UNIFORMS];
  char *name_buffer;
  struct GPUBatch **batches; /* references to batches using this interface */
  uint batches_len;
} GPUShaderInterface;

GPUShaderInterface *GPU_shaderinterface_create(int32_t program_id);
void GPU_shaderinterface_discard(GPUShaderInterface *);

const GPUShaderInput *GPU_shaderinterface_uniform(const GPUShaderInterface *, const char *name);
const GPUShaderInput *GPU_shaderinterface_uniform_ensure(const GPUShaderInterface *,
                                                         const char *name);
const GPUShaderInput *GPU_shaderinterface_uniform_builtin(const GPUShaderInterface *,
                                                          GPUUniformBuiltin);
const GPUShaderInput *GPU_shaderinterface_ubo(const GPUShaderInterface *, const char *name);
const GPUShaderInput *GPU_shaderinterface_attr(const GPUShaderInterface *, const char *name);

/* keep track of batches using this interface */
void GPU_shaderinterface_add_batch_ref(GPUShaderInterface *, struct GPUBatch *);
void GPU_shaderinterface_remove_batch_ref(GPUShaderInterface *, struct GPUBatch *);

#endif /* __GPU_SHADER_INTERFACE_H__ */

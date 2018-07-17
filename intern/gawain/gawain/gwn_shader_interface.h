/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/gwn_shader_interface.h
 *  \ingroup gpu
 *
 * Gawain shader interface (C --> GLSL)
 */

#ifndef __GWN_SHADER_INTERFACE_H__
#define __GWN_SHADER_INTERFACE_H__

#include "gwn_common.h"

typedef enum {
	GWN_UNIFORM_NONE = 0, /* uninitialized/unknown */

	GWN_UNIFORM_MODEL,      /* mat4 ModelMatrix */
	GWN_UNIFORM_VIEW,       /* mat4 ViewMatrix */
	GWN_UNIFORM_MODELVIEW,  /* mat4 ModelViewMatrix */
	GWN_UNIFORM_PROJECTION, /* mat4 ProjectionMatrix */
	GWN_UNIFORM_VIEWPROJECTION, /* mat4 ViewProjectionMatrix */
	GWN_UNIFORM_MVP,        /* mat4 ModelViewProjectionMatrix */

	GWN_UNIFORM_MODEL_INV,           /* mat4 ModelMatrixInverse */
	GWN_UNIFORM_VIEW_INV,            /* mat4 ViewMatrixInverse */
	GWN_UNIFORM_MODELVIEW_INV,       /* mat4 ModelViewMatrixInverse */
	GWN_UNIFORM_PROJECTION_INV,      /* mat4 ProjectionMatrixInverse */
	GWN_UNIFORM_VIEWPROJECTION_INV,  /* mat4 ViewProjectionMatrixInverse */

	GWN_UNIFORM_NORMAL,      /* mat3 NormalMatrix */
	GWN_UNIFORM_WORLDNORMAL, /* mat3 WorldNormalMatrix */
	GWN_UNIFORM_CAMERATEXCO, /* vec4 CameraTexCoFactors */
	GWN_UNIFORM_ORCO,        /* vec3 OrcoTexCoFactors[] */

	GWN_UNIFORM_COLOR, /* vec4 color */
	GWN_UNIFORM_EYE, /* vec3 eye */
	GWN_UNIFORM_CALLID, /* int callId */

	GWN_UNIFORM_CUSTOM, /* custom uniform, not one of the above built-ins */

	GWN_NUM_UNIFORMS, /* Special value, denotes number of builtin uniforms. */
} Gwn_UniformBuiltin;

typedef struct Gwn_ShaderInput {
	struct Gwn_ShaderInput* next;
	uint32_t name_offset;
	uint name_hash;
	Gwn_UniformBuiltin builtin_type; /* only for uniform inputs */
	uint32_t gl_type; /* only for attrib inputs */
	int32_t size; /* only for attrib inputs */
	int32_t location;
} Gwn_ShaderInput;

#define GWN_NUM_SHADERINTERFACE_BUCKETS 257
#define GWN_SHADERINTERFACE_REF_ALLOC_COUNT 16

typedef struct Gwn_ShaderInterface {
	int32_t program;
	uint32_t name_buffer_offset;
	Gwn_ShaderInput* attrib_buckets[GWN_NUM_SHADERINTERFACE_BUCKETS];
	Gwn_ShaderInput* uniform_buckets[GWN_NUM_SHADERINTERFACE_BUCKETS];
	Gwn_ShaderInput* ubo_buckets[GWN_NUM_SHADERINTERFACE_BUCKETS];
	Gwn_ShaderInput* builtin_uniforms[GWN_NUM_UNIFORMS];
	char* name_buffer;
	struct Gwn_Batch** batches; /* references to batches using this interface */
	uint batches_len;
} Gwn_ShaderInterface;

Gwn_ShaderInterface* GWN_shaderinterface_create(int32_t program_id);
void GWN_shaderinterface_discard(Gwn_ShaderInterface*);

const Gwn_ShaderInput* GWN_shaderinterface_uniform(const Gwn_ShaderInterface*, const char* name);
const Gwn_ShaderInput* GWN_shaderinterface_uniform_builtin(const Gwn_ShaderInterface*, Gwn_UniformBuiltin);
const Gwn_ShaderInput* GWN_shaderinterface_ubo(const Gwn_ShaderInterface*, const char* name);
const Gwn_ShaderInput* GWN_shaderinterface_attr(const Gwn_ShaderInterface*, const char* name);

/* keep track of batches using this interface */
void GWN_shaderinterface_add_batch_ref(Gwn_ShaderInterface*, struct Gwn_Batch*);
void GWN_shaderinterface_remove_batch_ref(Gwn_ShaderInterface*, struct Gwn_Batch*);

#endif /* __GWN_SHADER_INTERFACE_H__ */

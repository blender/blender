
// Gawain shader interface (C --> GLSL)
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2017 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "gwn_common.h"

typedef enum {
	GWN_UNIFORM_NONE = 0, // uninitialized/unknown

	GWN_UNIFORM_MODEL,      // mat4 ModelMatrix
	GWN_UNIFORM_VIEW,       // mat4 ViewMatrix
	GWN_UNIFORM_MODELVIEW,  // mat4 ModelViewMatrix
	GWN_UNIFORM_PROJECTION, // mat4 ProjectionMatrix
	GWN_UNIFORM_VIEWPROJECTION, // mat4 ViewProjectionMatrix
	GWN_UNIFORM_MVP,        // mat4 ModelViewProjectionMatrix

	GWN_UNIFORM_MODEL_INV,           // mat4 ModelMatrixInverse
	GWN_UNIFORM_VIEW_INV,            // mat4 ViewMatrixInverse
	GWN_UNIFORM_MODELVIEW_INV,       // mat4 ModelViewMatrixInverse
	GWN_UNIFORM_PROJECTION_INV,      // mat4 ProjectionMatrixInverse
	GWN_UNIFORM_VIEWPROJECTION_INV,  // mat4 ViewProjectionMatrixInverse

	GWN_UNIFORM_NORMAL,      // mat3 NormalMatrix
	GWN_UNIFORM_WORLDNORMAL, // mat3 WorldNormalMatrix
	GWN_UNIFORM_CAMERATEXCO, // vec4 CameraTexCoFactors
	GWN_UNIFORM_ORCO,        // vec3 OrcoTexCoFactors[]

	GWN_UNIFORM_COLOR, // vec4 color
	GWN_UNIFORM_EYE, // vec3 eye

	GWN_UNIFORM_CUSTOM, // custom uniform, not one of the above built-ins

	GWN_NUM_UNIFORMS, // Special value, denotes number of builtin uniforms.
} Gwn_UniformBuiltin;

typedef struct Gwn_ShaderInput {
	struct Gwn_ShaderInput* next;
	uint32_t name_offset;
	unsigned name_hash;
	Gwn_UniformBuiltin builtin_type; // only for uniform inputs
	GLenum gl_type; // only for attrib inputs
	GLint size; // only for attrib inputs
	GLint location;
} Gwn_ShaderInput;

#define GWN_NUM_SHADERINTERFACE_BUCKETS 257
#define GWN_SHADERINTERFACE_REF_ALLOC_COUNT 16

typedef struct Gwn_ShaderInterface {
	GLint program;
	uint32_t name_buffer_offset;
	Gwn_ShaderInput* attrib_buckets[GWN_NUM_SHADERINTERFACE_BUCKETS];
	Gwn_ShaderInput* uniform_buckets[GWN_NUM_SHADERINTERFACE_BUCKETS];
	Gwn_ShaderInput* ubo_buckets[GWN_NUM_SHADERINTERFACE_BUCKETS];
	Gwn_ShaderInput* builtin_uniforms[GWN_NUM_UNIFORMS];
	char* name_buffer;
	struct Gwn_Batch** batches; // references to batches using this interface
	unsigned batches_ct;
} Gwn_ShaderInterface;

Gwn_ShaderInterface* GWN_shaderinterface_create(GLint program_id);
void GWN_shaderinterface_discard(Gwn_ShaderInterface*);

const Gwn_ShaderInput* GWN_shaderinterface_uniform(const Gwn_ShaderInterface*, const char* name);
const Gwn_ShaderInput* GWN_shaderinterface_uniform_builtin(const Gwn_ShaderInterface*, Gwn_UniformBuiltin);
const Gwn_ShaderInput* GWN_shaderinterface_ubo(const Gwn_ShaderInterface*, const char* name);
const Gwn_ShaderInput* GWN_shaderinterface_attr(const Gwn_ShaderInterface*, const char* name);

// keep track of batches using this interface
void GWN_shaderinterface_add_batch_ref(Gwn_ShaderInterface*, struct Gwn_Batch*);
void GWN_shaderinterface_remove_batch_ref(Gwn_ShaderInterface*, struct Gwn_Batch*);

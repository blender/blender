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

/** \file blender/gpu/intern/gwn_shader_interface.c
 *  \ingroup gpu
 *
 * Gawain shader interface (C --> GLSL)
 */

#include "gpu_batch_private.h"
#include "GPU_shader_interface.h"
#include "GPU_vertex_array_id.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define DEBUG_SHADER_INTERFACE 0

#if DEBUG_SHADER_INTERFACE
 #include <stdio.h>
#endif

static const char* BuiltinUniform_name(Gwn_UniformBuiltin u)
{
	static const char* names[] = {
		[GWN_UNIFORM_NONE] = NULL,

		[GWN_UNIFORM_MODEL] = "ModelMatrix",
		[GWN_UNIFORM_VIEW] = "ViewMatrix",
		[GWN_UNIFORM_MODELVIEW] = "ModelViewMatrix",
		[GWN_UNIFORM_PROJECTION] = "ProjectionMatrix",
		[GWN_UNIFORM_VIEWPROJECTION] = "ViewProjectionMatrix",
		[GWN_UNIFORM_MVP] = "ModelViewProjectionMatrix",

		[GWN_UNIFORM_MODEL_INV] = "ModelMatrixInverse",
		[GWN_UNIFORM_VIEW_INV] = "ViewMatrixInverse",
		[GWN_UNIFORM_MODELVIEW_INV] = "ModelViewMatrixInverse",
		[GWN_UNIFORM_PROJECTION_INV] = "ProjectionMatrixInverse",
		[GWN_UNIFORM_VIEWPROJECTION_INV] = "ViewProjectionMatrixInverse",

		[GWN_UNIFORM_NORMAL] = "NormalMatrix",
		[GWN_UNIFORM_WORLDNORMAL] = "WorldNormalMatrix",
		[GWN_UNIFORM_CAMERATEXCO] = "CameraTexCoFactors",
		[GWN_UNIFORM_ORCO] = "OrcoTexCoFactors",

		[GWN_UNIFORM_COLOR] = "color",
		[GWN_UNIFORM_EYE] = "eye",
		[GWN_UNIFORM_CALLID] = "callId",

		[GWN_UNIFORM_CUSTOM] = NULL,
		[GWN_NUM_UNIFORMS] = NULL,
	};

	return names[u];
}

GWN_INLINE bool match(const char* a, const char* b)
{
	return strcmp(a, b) == 0;
}

GWN_INLINE uint hash_string(const char *str)
{
	uint i = 0, c;
	while ((c = *str++)) {
		i = i * 37 + c;
	}
	return i;
}

GWN_INLINE void set_input_name(Gwn_ShaderInterface* shaderface, Gwn_ShaderInput* input,
                               const char* name, uint32_t name_len)
{
	input->name_offset = shaderface->name_buffer_offset;
	input->name_hash = hash_string(name);
	shaderface->name_buffer_offset += name_len + 1; /* include NULL terminator */
}

GWN_INLINE void shader_input_to_bucket(Gwn_ShaderInput* input,
                                       Gwn_ShaderInput* buckets[GWN_NUM_SHADERINTERFACE_BUCKETS])
{
	const uint bucket_index = input->name_hash % GWN_NUM_SHADERINTERFACE_BUCKETS;
	input->next = buckets[bucket_index];
	buckets[bucket_index] = input;
}

GWN_INLINE const Gwn_ShaderInput* buckets_lookup(Gwn_ShaderInput* const buckets[GWN_NUM_SHADERINTERFACE_BUCKETS],
                                           const char *name_buffer, const char *name)
{
	const uint name_hash = hash_string(name);
	const uint bucket_index = name_hash % GWN_NUM_SHADERINTERFACE_BUCKETS;
	const Gwn_ShaderInput* input = buckets[bucket_index];
	if (input == NULL) {
		/* Requested uniform is not found at all. */
		return NULL;
	}
	/* Optimization bit: if there is no hash collision detected when constructing shader interface
	 * it means we can only request the single possible uniform. Surely, it's possible we request
	 * uniform which causes hash collision, but that will be detected in debug builds. */
	if (input->next == NULL) {
		if (name_hash == input->name_hash) {
#if TRUST_NO_ONE
			assert(match(name_buffer + input->name_offset, name));
#endif
			return input;
		}
		return NULL;
	}
	/* Work through possible collisions. */
	const Gwn_ShaderInput* next = input;
	while (next != NULL) {
		input = next;
		next = input->next;
		if (input->name_hash != name_hash) {
			continue;
		}
		if (match(name_buffer + input->name_offset, name)) {
			return input;
		}
	}
	return NULL; /* not found */
}

GWN_INLINE void buckets_free(Gwn_ShaderInput* buckets[GWN_NUM_SHADERINTERFACE_BUCKETS])
{
	for (uint bucket_index = 0; bucket_index < GWN_NUM_SHADERINTERFACE_BUCKETS; ++bucket_index) {
		Gwn_ShaderInput *input = buckets[bucket_index];
		while (input != NULL) {
			Gwn_ShaderInput *input_next = input->next;
			free(input);
			input = input_next;
		}
	}
}

static bool setup_builtin_uniform(Gwn_ShaderInput* input, const char* name)
{
	/* TODO: reject DOUBLE, IMAGE, ATOMIC_COUNTER gl_types */

	/* detect built-in uniforms (name must match) */
	for (Gwn_UniformBuiltin u = GWN_UNIFORM_NONE + 1; u < GWN_UNIFORM_CUSTOM; ++u) {
		const char* builtin_name = BuiltinUniform_name(u);
		if (match(name, builtin_name)) {
			input->builtin_type = u;
			return true;
		}
	}
	input->builtin_type = GWN_UNIFORM_CUSTOM;
	return false;
}

static const Gwn_ShaderInput* add_uniform(Gwn_ShaderInterface* shaderface, const char* name)
{
	Gwn_ShaderInput* input = malloc(sizeof(Gwn_ShaderInput));

	input->location = glGetUniformLocation(shaderface->program, name);

	uint name_len = strlen(name);
	shaderface->name_buffer = realloc(shaderface->name_buffer, shaderface->name_buffer_offset + name_len + 1); /* include NULL terminator */
	char* name_buffer = shaderface->name_buffer + shaderface->name_buffer_offset;
	strcpy(name_buffer, name);

	set_input_name(shaderface, input, name, name_len);
	setup_builtin_uniform(input, name);

	shader_input_to_bucket(input, shaderface->uniform_buckets);
	if (input->builtin_type != GWN_UNIFORM_NONE &&
	    input->builtin_type != GWN_UNIFORM_CUSTOM)
	{
		shaderface->builtin_uniforms[input->builtin_type] = input;
	}
#if DEBUG_SHADER_INTERFACE
	printf("Gwn_ShaderInterface %p, program %d, uniform[] '%s' at location %d\n", shaderface,
	                                                                              shaderface->program,
	                                                                              name,
	                                                                              input->location);
#endif
	return input;
}

Gwn_ShaderInterface* GWN_shaderinterface_create(int32_t program)
{
	Gwn_ShaderInterface* shaderface = calloc(1, sizeof(Gwn_ShaderInterface));
	shaderface->program = program;

#if DEBUG_SHADER_INTERFACE
	printf("%s {\n", __func__); /* enter function */
	printf("Gwn_ShaderInterface %p, program %d\n", shaderface, program);
#endif

	GLint max_attrib_name_len, attr_len;
	glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_attrib_name_len);
	glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attr_len);

	GLint max_ubo_name_len, ubo_len;
	glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &max_ubo_name_len);
	glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &ubo_len);

	const uint32_t name_buffer_len = attr_len * max_attrib_name_len + ubo_len * max_ubo_name_len;
	shaderface->name_buffer = malloc(name_buffer_len);

	/* Attributes */
	for (uint32_t i = 0; i < attr_len; ++i) {
		Gwn_ShaderInput* input = malloc(sizeof(Gwn_ShaderInput));
		GLsizei remaining_buffer = name_buffer_len - shaderface->name_buffer_offset;
		char* name = shaderface->name_buffer + shaderface->name_buffer_offset;
		GLsizei name_len = 0;

		glGetActiveAttrib(program, i, remaining_buffer, &name_len, &input->size, &input->gl_type, name);

		/* remove "[0]" from array name */
		if (name[name_len-1] == ']') {
			name[name_len-3] = '\0';
			name_len -= 3;
		}

		/* TODO: reject DOUBLE gl_types */

		input->location = glGetAttribLocation(program, name);

		set_input_name(shaderface, input, name, name_len);

		shader_input_to_bucket(input, shaderface->attrib_buckets);

#if DEBUG_SHADER_INTERFACE
		printf("attrib[%u] '%s' at location %d\n", i, name, input->location);
#endif
	}
	/* Uniform Blocks */
	for (uint32_t i = 0; i < ubo_len; ++i) {
		Gwn_ShaderInput* input = malloc(sizeof(Gwn_ShaderInput));
		GLsizei remaining_buffer = name_buffer_len - shaderface->name_buffer_offset;
		char* name = shaderface->name_buffer + shaderface->name_buffer_offset;
		GLsizei name_len = 0;

		glGetActiveUniformBlockName(program, i, remaining_buffer, &name_len, name);

		input->location = i;

		set_input_name(shaderface, input, name, name_len);

		shader_input_to_bucket(input, shaderface->ubo_buckets);

#if DEBUG_SHADER_INTERFACE
		printf("ubo '%s' at location %d\n", name, input->location);
#endif
	}
	/* Builtin Uniforms */
	for (Gwn_UniformBuiltin u = GWN_UNIFORM_NONE + 1; u < GWN_UNIFORM_CUSTOM; ++u) {
		const char* builtin_name = BuiltinUniform_name(u);
		if (glGetUniformLocation(program, builtin_name) != -1) {
			add_uniform((Gwn_ShaderInterface*)shaderface, builtin_name);
		}
	}
	/* Batches ref buffer */
	shaderface->batches_len = GWN_SHADERINTERFACE_REF_ALLOC_COUNT;
	shaderface->batches = calloc(shaderface->batches_len, sizeof(Gwn_Batch*));

	return shaderface;
}

void GWN_shaderinterface_discard(Gwn_ShaderInterface* shaderface)
{
	/* Free memory used by buckets and has entries. */
	buckets_free(shaderface->uniform_buckets);
	buckets_free(shaderface->attrib_buckets);
	buckets_free(shaderface->ubo_buckets);
	/* Free memory used by name_buffer. */
	free(shaderface->name_buffer);
	/* Remove this interface from all linked Batches vao cache. */
	for (int i = 0; i < shaderface->batches_len; ++i) {
		if (shaderface->batches[i] != NULL) {
			gwn_batch_remove_interface_ref(shaderface->batches[i], shaderface);
		}
	}
	free(shaderface->batches);
	/* Free memory used by shader interface by its self. */
	free(shaderface);
}

const Gwn_ShaderInput* GWN_shaderinterface_uniform(const Gwn_ShaderInterface* shaderface, const char* name)
{
	/* TODO: Warn if we find a matching builtin, since these can be looked up much quicker. */
	const Gwn_ShaderInput* input = buckets_lookup(shaderface->uniform_buckets, shaderface->name_buffer, name);
	/* If input is not found add it so it's found next time. */
	if (input == NULL) {
		input = add_uniform((Gwn_ShaderInterface*)shaderface, name);
	}
	return (input->location != -1) ? input : NULL;
}

const Gwn_ShaderInput* GWN_shaderinterface_uniform_builtin(
        const Gwn_ShaderInterface* shaderface, Gwn_UniformBuiltin builtin)
{
#if TRUST_NO_ONE
	assert(builtin != GWN_UNIFORM_NONE);
	assert(builtin != GWN_UNIFORM_CUSTOM);
	assert(builtin != GWN_NUM_UNIFORMS);
#endif
	return shaderface->builtin_uniforms[builtin];
}

const Gwn_ShaderInput* GWN_shaderinterface_ubo(const Gwn_ShaderInterface* shaderface, const char* name)
{
	return buckets_lookup(shaderface->ubo_buckets, shaderface->name_buffer, name);
}

const Gwn_ShaderInput* GWN_shaderinterface_attr(const Gwn_ShaderInterface* shaderface, const char* name)
{
	return buckets_lookup(shaderface->attrib_buckets, shaderface->name_buffer, name);
}

void GWN_shaderinterface_add_batch_ref(Gwn_ShaderInterface* shaderface, Gwn_Batch* batch)
{
	int i; /* find first unused slot */
	for (i = 0; i < shaderface->batches_len; ++i) {
		if (shaderface->batches[i] == NULL) {
			break;
		}
	}
	if (i == shaderface->batches_len) {
		/* Not enough place, realloc the array. */
		i = shaderface->batches_len;
		shaderface->batches_len += GWN_SHADERINTERFACE_REF_ALLOC_COUNT;
		shaderface->batches = realloc(shaderface->batches, sizeof(Gwn_Batch*) * shaderface->batches_len);
		memset(shaderface->batches + i, 0, sizeof(Gwn_Batch*) * GWN_SHADERINTERFACE_REF_ALLOC_COUNT);
	}
	shaderface->batches[i] = batch;
}

void GWN_shaderinterface_remove_batch_ref(Gwn_ShaderInterface* shaderface, Gwn_Batch* batch)
{
	for (int i = 0; i < shaderface->batches_len; ++i) {
		if (shaderface->batches[i] == batch) {
			shaderface->batches[i] = NULL;
			break; /* cannot have duplicates */
		}
	}
}


// Gawain shader interface (C --> GLSL)
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2017 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "gwn_shader_interface.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define SUPPORT_LEGACY_GLSL 1
#define DEBUG_SHADER_INTERFACE 0

#if DEBUG_SHADER_INTERFACE
 #include <stdio.h>
#endif

static const char* BuiltinUniform_name(Gwn_UniformBuiltin u)
	{
	static const char* names[] =
		{
		[GWN_UNIFORM_NONE] = NULL,

		[GWN_UNIFORM_MODELVIEW] = "ModelViewMatrix",
		[GWN_UNIFORM_PROJECTION] = "ProjectionMatrix",
		[GWN_UNIFORM_MVP] = "ModelViewProjectionMatrix",

		[GWN_UNIFORM_MODELVIEW_INV] = "ModelViewInverseMatrix",
		[GWN_UNIFORM_PROJECTION_INV] = "ProjectionInverseMatrix",

		[GWN_UNIFORM_NORMAL] = "NormalMatrix",

		[GWN_UNIFORM_COLOR] = "color",

		[GWN_UNIFORM_CUSTOM] = NULL,
		[GWN_NUM_UNIFORMS] = NULL,
		};

	return names[u];
	}

GWN_INLINE bool match(const char* a, const char* b)
	{
	return strcmp(a, b) == 0;
	}

GWN_INLINE unsigned hash_string(const char *str)
	{
	unsigned i = 0, c;

	while ((c = *str++))
		{
		i = i * 37 + c;
		}

	return i;
	}

GWN_INLINE void set_input_name(Gwn_ShaderInput* input, const char* name)
	{
	input->name = name;
	input->name_hash = hash_string(name);
	}

GWN_INLINE void shader_input_to_bucket(Gwn_ShaderInput* input,
                                       Gwn_ShaderInput* buckets[GWN_NUM_SHADERINTERFACE_BUCKETS])
	{
	const unsigned bucket_index = input->name_hash % GWN_NUM_SHADERINTERFACE_BUCKETS;
	input->next = buckets[bucket_index];
	buckets[bucket_index] = input;
	}

GWN_INLINE Gwn_ShaderInput* buckets_lookup(Gwn_ShaderInput* const buckets[GWN_NUM_SHADERINTERFACE_BUCKETS],
                                           const char *name)
	{
	const unsigned name_hash = hash_string(name);
	const unsigned bucket_index = name_hash % GWN_NUM_SHADERINTERFACE_BUCKETS;
	Gwn_ShaderInput* input = buckets[bucket_index];
	if (input == NULL)
		{
			// Requested uniform is not found at all.
			return NULL;
		}
	// Optimization bit: if there is no hash collision detected when constructing shader interface
	// it means we can only request the single possible uniform. Surely, it's possible we request
	// uniform which causes hash collision, but that will be detected in debug builds.
	if (input->next == NULL)
		{
			if (name_hash == input->name_hash)
				{
#if TRUST_NO_ONE
				assert(match(input->name, name));
#endif
				return input;
				}
			return NULL;
		}
	// Work through possible collisions.
	while (input != NULL)
		{
		Gwn_ShaderInput* uniform = input;
		input = input->next;
#if SUPPORT_LEGACY_GLSL
		if (uniform->name == NULL) continue;
#endif
		if (uniform->name_hash != name_hash)
			{
				continue;
			}
		if (match(uniform->name, name))
			{
			return uniform;
			}
		}
	return NULL; // not found
	}

// keep these in sync with Gwn_UniformBuiltin order
#define FIRST_MAT4_UNIFORM GWN_UNIFORM_MODELVIEW
#define LAST_MAT4_UNIFORM GWN_UNIFORM_PROJECTION_INV

static bool setup_builtin_uniform(Gwn_ShaderInput* input, const char* name)
	{
	// TODO: reject DOUBLE, IMAGE, ATOMIC_COUNTER gl_types

	// detect built-in uniforms (gl_type and name must match)
	// if a match is found, use BuiltinUniform_name so name buffer space can be reclaimed
	switch (input->gl_type)
		{
		case GL_FLOAT_MAT4:
			for (Gwn_UniformBuiltin u = FIRST_MAT4_UNIFORM; u <= LAST_MAT4_UNIFORM; ++u)
				{
				const char* builtin_name = BuiltinUniform_name(u);
				if (match(name, builtin_name))
					{
					set_input_name(input, builtin_name);
					input->builtin_type = u;
					return true;
					}
				}
			break;
		case GL_FLOAT_MAT3:
			{
			const char* builtin_name = BuiltinUniform_name(GWN_UNIFORM_NORMAL);
			if (match(name, builtin_name))
				{
				set_input_name(input, builtin_name);
				input->builtin_type = GWN_UNIFORM_NORMAL;
				return true;
				}
			}
			break;
		case GL_FLOAT_VEC4:
			{
			const char* builtin_name = BuiltinUniform_name(GWN_UNIFORM_COLOR);
			if (match(name, builtin_name))
				{
				set_input_name(input, builtin_name);
				input->builtin_type = GWN_UNIFORM_COLOR;
				return true;
				}
			}
			break;
		default:
			;
		} 

	input->builtin_type = GWN_UNIFORM_CUSTOM;
	return false;
	}

Gwn_ShaderInterface* GWN_shaderinterface_create(GLint program)
	{
#if DEBUG_SHADER_INTERFACE
	printf("%s {\n", __func__); // enter function
#endif

	GLint uniform_ct, attrib_ct;
	glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniform_ct);
	glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attrib_ct);
	const GLint input_ct = uniform_ct + attrib_ct;

	GLint max_uniform_name_len, max_attrib_name_len;
	glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_uniform_name_len);
	glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &max_attrib_name_len);
	const uint32_t name_buffer_len = uniform_ct * max_uniform_name_len + attrib_ct * max_attrib_name_len;

	// allocate enough space for input counts, details for each input, and a buffer for name strings
	Gwn_ShaderInterface* shaderface = calloc(1, offsetof(Gwn_ShaderInterface, inputs) + input_ct * sizeof(Gwn_ShaderInput) + name_buffer_len);
	shaderface->uniform_ct = uniform_ct;
	shaderface->attrib_ct = attrib_ct;

	char* name_buffer = (char*)shaderface + offsetof(Gwn_ShaderInterface, inputs) + input_ct * sizeof(Gwn_ShaderInput);
	uint32_t name_buffer_offset = 0;

	for (uint32_t i = 0; i < uniform_ct; ++i)
		{
		Gwn_ShaderInput* input = shaderface->inputs + i;
		GLsizei remaining_buffer = name_buffer_len - name_buffer_offset;
		char* name = name_buffer + name_buffer_offset;
		GLsizei name_len = 0;

		glGetActiveUniform(program, i, remaining_buffer, &name_len, &input->size, &input->gl_type, name);

		input->location = glGetUniformLocation(program, name);

#if SUPPORT_LEGACY_GLSL
		if (input->location != -1)
			{
#elif TRUST_NO_ONE
			assert(input->location != -1);
#endif

			if (setup_builtin_uniform(input, name))
				; // reclaim space from name buffer (don't advance offset)
			else
				{
				set_input_name(input, name);
				name_buffer_offset += name_len + 1; // include NULL terminator
				}
#if SUPPORT_LEGACY_GLSL
			}
#endif

#if DEBUG_SHADER_INTERFACE
		printf("uniform[%u] '%s' at location %d\n", i, name, input->location);
#endif
		}

	for (uint32_t i = 0; i < attrib_ct; ++i)
		{
		Gwn_ShaderInput* input = shaderface->inputs + uniform_ct + i;
		GLsizei remaining_buffer = name_buffer_len - name_buffer_offset;
		char* name = name_buffer + name_buffer_offset;
		GLsizei name_len = 0;

		glGetActiveAttrib(program, i, remaining_buffer, &name_len, &input->size, &input->gl_type, name);

		// TODO: reject DOUBLE gl_types

		input->location = glGetAttribLocation(program, name);

#if SUPPORT_LEGACY_GLSL
		if (input->location != -1)
			{
#elif TRUST_NO_ONE
			assert(input->location != -1);
#endif

			set_input_name(input, name);
			name_buffer_offset += name_len + 1; // include NULL terminator
#if SUPPORT_LEGACY_GLSL
			}
#endif

#if DEBUG_SHADER_INTERFACE
		printf("attrib[%u] '%s' at location %d\n", i, name, input->location);
#endif
		}

	const uint32_t name_buffer_used = name_buffer_offset;

#if DEBUG_SHADER_INTERFACE
	printf("using %u of %u bytes from name buffer\n", name_buffer_used, name_buffer_len);
	printf("}\n"); // exit function
#endif

	if (name_buffer_used < name_buffer_len)
		{
		// realloc shaderface to shrink name buffer
		const size_t shaderface_alloc =
		        offsetof(Gwn_ShaderInterface, inputs) + (input_ct * sizeof(Gwn_ShaderInput)) + name_buffer_used;
		const char* shaderface_orig_start = (const char*)shaderface;
		const char* shaderface_orig_end = &shaderface_orig_start[shaderface_alloc];
		shaderface = realloc(shaderface, shaderface_alloc);
		const ptrdiff_t delta = (char*)shaderface - shaderface_orig_start;

		if (delta)
			{
			// each input->name will need adjustment (except static built-in names)
			for (uint32_t i = 0; i < input_ct; ++i)
				{
				Gwn_ShaderInput* input = shaderface->inputs + i;

				if (input->name >= shaderface_orig_start && input->name < shaderface_orig_end)
					input->name += delta;
				}
			}
		}

	memset(shaderface->builtin_uniforms, 0, sizeof(shaderface->builtin_uniforms));
	for (uint32_t i = 0; i < shaderface->uniform_ct; ++i)
		{
			Gwn_ShaderInput* input = &shaderface->inputs[i];
			shader_input_to_bucket(input, shaderface->uniform_buckets);
			if (input->builtin_type != GWN_UNIFORM_NONE &&
			    input->builtin_type != GWN_UNIFORM_CUSTOM)
				{
				shaderface->builtin_uniforms[input->builtin_type] = input;
				}
		}
	for (uint32_t i = 0; i < shaderface->attrib_ct; ++i)
		{
			Gwn_ShaderInput* input = &shaderface->inputs[i + shaderface->uniform_ct];
			shader_input_to_bucket(input, shaderface->attrib_buckets);
		}

	return shaderface;
	}

void GWN_shaderinterface_discard(Gwn_ShaderInterface* shaderface)
	{
	// Free memory used by shader interface by its self.
	free(shaderface);
	}

const Gwn_ShaderInput* GWN_shaderinterface_uniform(const Gwn_ShaderInterface* shaderface, const char* name)
	{
	// TODO: Warn if we find a matching builtin, since these can be looked up much quicker.
	return buckets_lookup(shaderface->uniform_buckets, name);
	}

const Gwn_ShaderInput* GWN_shaderinterface_uniform_builtin(const Gwn_ShaderInterface* shaderface, Gwn_UniformBuiltin builtin)
	{
#if TRUST_NO_ONE
	assert(builtin != GWN_UNIFORM_NONE);
	assert(builtin != GWN_UNIFORM_CUSTOM);
	assert(builtin != GWN_NUM_UNIFORMS);
#endif

	return shaderface->builtin_uniforms[builtin];
	}

const Gwn_ShaderInput* GWN_shaderinterface_attr(const Gwn_ShaderInterface* shaderface, const char* name)
	{
	return buckets_lookup(shaderface->attrib_buckets, name);
	}

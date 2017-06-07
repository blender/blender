
// Gawain shader interface (C --> GLSL)
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2017 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "shader_interface.h"
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define SUPPORT_LEGACY_GLSL 1
#define DEBUG_SHADER_INTERFACE 0

#if DEBUG_SHADER_INTERFACE
 #include <stdio.h>
#endif

static const char* BuiltinUniform_name(BuiltinUniform u)
	{
	static const char* names[] =
		{
		[UNIFORM_NONE] = NULL,

		[UNIFORM_MODELVIEW] = "ModelViewMatrix",
		[UNIFORM_PROJECTION] = "ProjectionMatrix",
		[UNIFORM_MVP] = "ModelViewProjectionMatrix",

		[UNIFORM_MODELVIEW_INV] = "ModelViewInverseMatrix",
		[UNIFORM_PROJECTION_INV] = "ProjectionInverseMatrix",

		[UNIFORM_NORMAL] = "NormalMatrix",

		[UNIFORM_COLOR] = "color",

		[UNIFORM_CUSTOM] = NULL
		};

	return names[u];
	}

static bool match(const char* a, const char* b)
	{
	return strcmp(a, b) == 0;
	}

static unsigned hash_string(const char *str)
	{
	unsigned i = 0, c;

	while ((c = *str++))
		{
		i = i * 37 + c;
		}

	return i;
	}

static void set_input_name(ShaderInput* input, const char* name)
	{
	input->name = name;
	input->name_hash = hash_string(name);
	}

// keep these in sync with BuiltinUniform order
#define FIRST_MAT4_UNIFORM UNIFORM_MODELVIEW
#define LAST_MAT4_UNIFORM UNIFORM_PROJECTION_INV

static bool setup_builtin_uniform(ShaderInput* input, const char* name)
	{
	// TODO: reject DOUBLE, IMAGE, ATOMIC_COUNTER gl_types

	// detect built-in uniforms (gl_type and name must match)
	// if a match is found, use BuiltinUniform_name so name buffer space can be reclaimed
	switch (input->gl_type)
		{
		case GL_FLOAT_MAT4:
			for (BuiltinUniform u = FIRST_MAT4_UNIFORM; u <= LAST_MAT4_UNIFORM; ++u)
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
			const char* builtin_name = BuiltinUniform_name(UNIFORM_NORMAL);
			if (match(name, builtin_name))
				{
				set_input_name(input, builtin_name);
				input->builtin_type = UNIFORM_NORMAL;
				return true;
				}
			}
			break;
		case GL_FLOAT_VEC4:
			{
			const char* builtin_name = BuiltinUniform_name(UNIFORM_COLOR);
			if (match(name, builtin_name))
				{
				set_input_name(input, builtin_name);
				input->builtin_type = UNIFORM_COLOR;
				return true;
				}
			}
			break;
		default:
			;
		} 

	input->builtin_type = UNIFORM_CUSTOM;
	return false;
	}

ShaderInterface* ShaderInterface_create(GLint program)
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
	ShaderInterface* shaderface = calloc(1, offsetof(ShaderInterface, inputs) + input_ct * sizeof(ShaderInput) + name_buffer_len);
	shaderface->uniform_ct = uniform_ct;
	shaderface->attrib_ct = attrib_ct;

	char* name_buffer = (char*)shaderface + offsetof(ShaderInterface, inputs) + input_ct * sizeof(ShaderInput);
	uint32_t name_buffer_offset = 0;

	for (uint32_t i = 0; i < uniform_ct; ++i)
		{
		ShaderInput* input = shaderface->inputs + i;
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
		ShaderInput* input = shaderface->inputs + uniform_ct + i;
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
		        offsetof(ShaderInterface, inputs) + (input_ct * sizeof(ShaderInput)) + name_buffer_used;
		const char* shaderface_orig_start = (const char*)shaderface;
		const char* shaderface_orig_end = &shaderface_orig_start[shaderface_alloc];
		shaderface = realloc(shaderface, shaderface_alloc);
		const ptrdiff_t delta = (char*)shaderface - shaderface_orig_start;

		if (delta)
			{
			// each input->name will need adjustment (except static built-in names)
			for (uint32_t i = 0; i < input_ct; ++i)
				{
				ShaderInput* input = shaderface->inputs + i;

				if (input->name >= shaderface_orig_start && input->name < shaderface_orig_end)
					input->name += delta;
				}
			}
		}

	return shaderface;
	}

void ShaderInterface_discard(ShaderInterface* shaderface)
	{
	// allocated as one chunk, so discard is simple
	free(shaderface);
	}

const ShaderInput* ShaderInterface_uniform(const ShaderInterface* shaderface, const char* name)
	{
	const unsigned name_hash = hash_string(name);
	for (uint32_t i = 0; i < shaderface->uniform_ct; ++i)
		{
		const ShaderInput* uniform = shaderface->inputs + i;

#if SUPPORT_LEGACY_GLSL
		if (uniform->name == NULL) continue;
#endif

		if (uniform->name_hash != name_hash) continue;

		if (match(uniform->name, name))
			return uniform;

		// TODO: warn if we find a matching builtin, since these can be looked up much quicker --v
		}

	return NULL; // not found
	}

const ShaderInput* ShaderInterface_builtin_uniform(const ShaderInterface* shaderface, BuiltinUniform builtin)
	{
#if TRUST_NO_ONE
	assert(builtin != UNIFORM_NONE);
	assert(builtin != UNIFORM_CUSTOM);
#endif

	// look up by enum, not name
	for (uint32_t i = 0; i < shaderface->uniform_ct; ++i)
		{
		const ShaderInput* uniform = shaderface->inputs + i;

		if (uniform->builtin_type == builtin)
			return uniform;
		}
	return NULL; // not found
	}

const ShaderInput* ShaderInterface_attrib(const ShaderInterface* shaderface, const char* name)
	{
	// attribs are stored after uniforms
	const uint32_t input_ct = shaderface->uniform_ct + shaderface->attrib_ct;
	const unsigned name_hash = hash_string(name);
	for (uint32_t i = shaderface->uniform_ct; i < input_ct; ++i)
		{
		const ShaderInput* attrib = shaderface->inputs + i;

#if SUPPORT_LEGACY_GLSL
		if (attrib->name == NULL) continue;
#endif

		if (attrib->name_hash != name_hash) continue;

		if (match(attrib->name, name))
			return attrib;
		}
	return NULL; // not found
	}

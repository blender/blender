
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

#define DEBUG_SHADER_INTERFACE 0

#if DEBUG_SHADER_INTERFACE
 #include <stdio.h>
#endif

static const char* BuiltinUniform_name(BuiltinUniform u)
	{
	static const char* names[] =
		{
		[UNIFORM_NONE] = NULL,

		[UNIFORM_MODELVIEW_3D] = "ModelViewMatrix",
		[UNIFORM_PROJECTION_3D] = "ProjectionMatrix",
		[UNIFORM_MVP_3D] = "ModelViewProjectionMatrix",
		[UNIFORM_NORMAL_3D] = "NormalMatrix",

		[UNIFORM_MODELVIEW_INV_3D] = "ModelViewInverseMatrix",
		[UNIFORM_PROJECTION_INV_3D] = "ProjectionInverseMatrix",

		[UNIFORM_MODELVIEW_2D] = "ModelViewMatrix",
		[UNIFORM_PROJECTION_2D] = "ProjectionMatrix",
		[UNIFORM_MVP_2D] = "ModelViewProjectionMatrix",

		[UNIFORM_COLOR] = "color",

		[UNIFORM_CUSTOM] = NULL
		};

	return names[u];
	}

static bool setup_builtin_uniform(ShaderInput* input, const char* name)
	{
	// TODO: reject DOUBLE, IMAGE, ATOMIC_COUNTER gl_types

	// TODO: detect built-in uniforms (gl_type and name must match)
	//       if a match is found, use BuiltinUniform_name so name buffer space can be reclaimed
	input->name = name;
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

#if TRUST_NO_ONE
		assert(input->location != -1);
#endif

		if (setup_builtin_uniform(input, name))
			; // reclaim space from name buffer (don't advance offset)
		else
			{
			input->name = name;
			name_buffer_offset += name_len + 1; // include NULL terminator
			}

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

#if TRUST_NO_ONE
		assert(input->location != -1);
#endif

		input->name = name;
		name_buffer_offset += name_len + 1; // include NULL terminator

#if DEBUG_SHADER_INTERFACE
		printf("attrib[%u] '%s' at location %d\n", i, name, input->location);
#endif
		}

	// TODO: realloc shaderface to shrink name buffer
	//       each input->name will need adjustment (except static built-in names)

#if DEBUG_SHADER_INTERFACE
	printf("using %u of %u bytes from name buffer\n", name_buffer_offset, name_buffer_len);
	printf("}\n"); // exit function
#endif

	return shaderface;
	}

void ShaderInterface_discard(ShaderInterface* shaderface)
	{
	// allocated as one chunk, so discard is simple
	free(shaderface);
	}

const ShaderInput* ShaderInterface_uniform(const ShaderInterface* shaderface, const char* name)
	{
	for (uint32_t i = 0; i < shaderface->uniform_ct; ++i)
		{
		const ShaderInput* uniform = shaderface->inputs + i;

		if (strcmp(uniform->name, name) == 0)
			return uniform;
		}
	return NULL; // not found
	}

const ShaderInput* ShaderInterface_builtin_uniform(const ShaderInterface* shaderface, BuiltinUniform builtin)
	{
	// TODO: look up by enum, not name (fix setup_builtin_uniform first)
	return ShaderInterface_uniform(shaderface, BuiltinUniform_name(builtin));
	}

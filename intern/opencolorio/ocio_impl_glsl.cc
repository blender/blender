/*
 * Adapted from OpenColorIO with this license:
 *
 * Copyright (c) 2003-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Modifications Copyright 2013, Blender Foundation.
 *
 * Contributor(s): Sergey Sharybin
 *
 */

#include <sstream>
#include <string.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/glew.h>
#endif

#include <OpenColorIO/OpenColorIO.h>

using namespace OCIO_NAMESPACE;

#include "MEM_guardedalloc.h"

#include "ocio_impl.h"

static const int LUT3D_EDGE_SIZE = 32;


/* **** OpenGL drawing routines using GLSL for color space transform ***** */

typedef struct OCIO_GLSLDrawState {
	bool lut3d_texture_allocated;  /* boolean flag indicating whether
	                                * lut texture is allocated
	                                */
	bool lut3d_texture_valid;

	GLuint lut3d_texture;  /* OGL texture ID for 3D LUT */

	float *lut3d;  /* 3D LUT table */

	/* Cache */
	std::string lut3dcacheid;
	std::string shadercacheid;

	/* GLSL stuff */
	GLuint fragShader;
	GLuint program;

	/* Previous OpenGL state. */
	GLint last_texture, last_texture_unit;
} OCIO_GLSLDrawState;

/* Hardcoded to do alpha predivide before color space conversion */
/* NOTE: This is true we only do de-premul here and NO premul
 *       and the reason is simple -- opengl is always configured
 *       for straight alpha at this moment
 */
static const char *g_fragShaderText = ""
"\n"
"uniform sampler2D tex1;\n"
"uniform sampler3D tex2;\n"
"uniform bool predivide;\n"
"\n"
"void main()\n"
"{\n"
"    vec4 col = texture2D(tex1, gl_TexCoord[0].st);\n"
"    if (predivide && col[3] > 0.0 && col[3] < 1.0) {\n"
"        float inv_alpha = 1.0 / col[3];\n"
"        col[0] *= inv_alpha;\n"
"        col[1] *= inv_alpha;\n"
"        col[2] *= inv_alpha;\n"
"}\n"
"    gl_FragColor = OCIODisplay(col, tex2);\n"
"\n"
"}\n";

static GLuint compileShaderText(GLenum shaderType, const char *text)
{
	GLuint shader;
	GLint stat;

	shader = glCreateShader(shaderType);
	glShaderSource(shader, 1, (const GLchar **) &text, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &stat);

	if (!stat) {
		GLchar log[1000];
		GLsizei len;
		glGetShaderInfoLog(shader, 1000, &len, log);
		fprintf(stderr, "Shader compile error:\n%s\n", log);
		return 0;
	}

	return shader;
}

static GLuint linkShaders(GLuint fragShader)
{
	if (!fragShader)
		return 0;

	GLuint program = glCreateProgram();

	if (fragShader)
		glAttachShader(program, fragShader);

	glLinkProgram(program);

	/* check link */
	{
		GLint stat;
		glGetProgramiv(program, GL_LINK_STATUS, &stat);
		if (!stat) {
			GLchar log[1000];
			GLsizei len;
			glGetProgramInfoLog(program, 1000, &len, log);
			fprintf(stderr, "Shader link error:\n%s\n", log);
			return 0;
		}
	}

	return program;
}

static OCIO_GLSLDrawState *allocateOpenGLState(void)
{
	OCIO_GLSLDrawState *state;

	/* Allocate memory for state. */
	state = (OCIO_GLSLDrawState *) MEM_callocN(sizeof(OCIO_GLSLDrawState),
	                                           "OCIO OpenGL State struct");

	/* Call constructors on new memory. */
	new (&state->lut3dcacheid) std::string("");
	new (&state->shadercacheid) std::string("");

	return state;
}

/* Ensure LUT texture and array are allocated */
static bool ensureLUT3DAllocated(OCIO_GLSLDrawState *state)
{
	int num_3d_entries = 3 * LUT3D_EDGE_SIZE * LUT3D_EDGE_SIZE * LUT3D_EDGE_SIZE;

	if (state->lut3d_texture_allocated)
		return state->lut3d_texture_valid;

	glGenTextures(1, &state->lut3d_texture);

	state->lut3d = (float *) MEM_callocN(sizeof(float) * num_3d_entries, "OCIO GPU 3D LUT");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D, state->lut3d_texture);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	/* clean glError buffer */
	while (glGetError() != GL_NO_ERROR) {}

	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB16F_ARB,
	             LUT3D_EDGE_SIZE, LUT3D_EDGE_SIZE, LUT3D_EDGE_SIZE,
	             0, GL_RGB, GL_FLOAT, state->lut3d);

	state->lut3d_texture_allocated = true;

	/* GL_RGB16F_ARB could be not supported at some drivers
	 * in this case we could not use GLSL display
	 */
	state->lut3d_texture_valid = glGetError() == GL_NO_ERROR;

	return state->lut3d_texture_valid;
}

/**
 * Setup OpenGL contexts for a transform defined by processor using GLSL
 * All LUT allocating baking and shader compilation happens here.
 *
 * Once this function is called, callee could start drawing images
 * using regular 2D texture.
 *
 * When all drawing is finished, finishGLSLDraw shall be called to
 * restore OpenGL context to it's pre-GLSL draw state.
 */
bool OCIOImpl::setupGLSLDraw(OCIO_GLSLDrawState **state_r, OCIO_ConstProcessorRcPtr *processor, bool predivide)
{
	ConstProcessorRcPtr ocio_processor = *(ConstProcessorRcPtr *) processor;

	/* Create state if needed. */
	OCIO_GLSLDrawState *state;
	if (!*state_r)
		*state_r = allocateOpenGLState();
	state = *state_r;

	glGetIntegerv(GL_TEXTURE_2D, &state->last_texture);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &state->last_texture_unit);

	if (!ensureLUT3DAllocated(state)) {
		glActiveTexture(state->last_texture_unit);
		glBindTexture(GL_TEXTURE_2D, state->last_texture);

		return false;
	}

	/* Step 1: Create a GPU Shader Description */
	GpuShaderDesc shaderDesc;
	shaderDesc.setLanguage(GPU_LANGUAGE_GLSL_1_0);
	shaderDesc.setFunctionName("OCIODisplay");
	shaderDesc.setLut3DEdgeLen(LUT3D_EDGE_SIZE);

	/* Step 2: Compute the 3D LUT */
	std::string lut3dCacheID = ocio_processor->getGpuLut3DCacheID(shaderDesc);
	if (lut3dCacheID != state->lut3dcacheid) {
		state->lut3dcacheid = lut3dCacheID;
		ocio_processor->getGpuLut3D(state->lut3d, shaderDesc);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D, state->lut3d_texture);
		glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0,
		                LUT3D_EDGE_SIZE, LUT3D_EDGE_SIZE, LUT3D_EDGE_SIZE,
		                GL_RGB, GL_FLOAT, state->lut3d);
	}

	/* Step 3: Compute the Shader */
	std::string shaderCacheID = ocio_processor->getGpuShaderTextCacheID(shaderDesc);
	if (state->program == 0 || shaderCacheID != state->shadercacheid) {
		state->shadercacheid = shaderCacheID;

		std::ostringstream os;
		os << ocio_processor->getGpuShaderText(shaderDesc) << "\n";
		os << g_fragShaderText;

		if (state->fragShader)
			glDeleteShader(state->fragShader);

		state->fragShader = compileShaderText(GL_FRAGMENT_SHADER, os.str().c_str());

		if (state->fragShader) {
			if (state->program)
				glDeleteProgram(state->program);

			state->program = linkShaders(state->fragShader);
		}
	}

	if (state->program) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D, state->lut3d_texture);

		glActiveTexture(GL_TEXTURE0);

		glUseProgram(state->program);
		glUniform1i(glGetUniformLocation(state->program, "tex1"), 0);
		glUniform1i(glGetUniformLocation(state->program, "tex2"), 1);
		glUniform1i(glGetUniformLocation(state->program, "predivide"), predivide);

		return true;
	}
	else {
		glActiveTexture(state->last_texture_unit);
		glBindTexture(GL_TEXTURE_2D, state->last_texture);

		return false;
	}
}

void OCIOImpl::finishGLSLDraw(OCIO_GLSLDrawState *state)
{
	glActiveTexture(state->last_texture_unit);
	glBindTexture(GL_TEXTURE_2D, state->last_texture);
	glUseProgram(0);
}

void OCIOImpl::freeGLState(struct OCIO_GLSLDrawState *state)
{
	using std::string;

	if (state->lut3d_texture_allocated)
		glDeleteTextures(1, &state->lut3d_texture);

	if (state->lut3d)
		MEM_freeN(state->lut3d);

	if (state->program)
		glDeleteProgram(state->program);

	if (state->fragShader)
		glDeleteShader(state->fragShader);

	state->lut3dcacheid.~string();
	state->shadercacheid.~string();

	MEM_freeN(state);
}

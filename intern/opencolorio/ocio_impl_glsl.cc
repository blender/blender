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

#include <limits>
#include <sstream>
#include <string.h>

#include "glew-mx.h"

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4251 4275)
#endif
#include <OpenColorIO/OpenColorIO.h>
#ifdef _MSC_VER
#  pragma warning(pop)
#endif

extern "C" {
#include "gawain/immediate.h"
}

using namespace OCIO_NAMESPACE;

#include "MEM_guardedalloc.h"

#include "ocio_impl.h"

static const int LUT3D_EDGE_SIZE = 64;

extern "C" char datatoc_gpu_shader_display_transform_glsl[];
extern "C" char datatoc_gpu_shader_display_transform_vertex_glsl[];

/* **** OpenGL drawing routines using GLSL for color space transform ***** */

typedef struct OCIO_GLSLDrawState {
	bool lut3d_texture_allocated;  /* boolean flag indicating whether
	                                * lut texture is allocated
	                                */
	bool lut3d_texture_valid;

	GLuint lut3d_texture;  /* OGL texture ID for 3D LUT */

	float *lut3d;  /* 3D LUT table */

	bool dither_used;

	bool curve_mapping_used;
	bool curve_mapping_texture_allocated;
	bool curve_mapping_texture_valid;
	GLuint curve_mapping_texture;
	size_t curve_mapping_cache_id;

	bool predivide_used;

	bool texture_size_used;

	/* Cache */
	std::string lut3dcacheid;
	std::string shadercacheid;

	/* GLSL stuff */
	GLuint ocio_shader;
	GLuint vert_shader;
	GLuint program;
	ShaderInterface *shader_interface;

	/* Previous OpenGL state. */
	GLint last_texture, last_texture_unit;
} OCIO_GLSLDrawState;

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

static GLuint linkShaders(GLuint ocio_shader, GLuint vert_shader)
{
	if (!ocio_shader || !vert_shader)
		return 0;

	GLuint program = glCreateProgram();

	glAttachShader(program, ocio_shader);
	glAttachShader(program, vert_shader);

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

static bool ensureCurveMappingAllocated(OCIO_GLSLDrawState *state, OCIO_CurveMappingSettings *curve_mapping_settings)
{
	if (state->curve_mapping_texture_allocated)
		return state->curve_mapping_texture_valid;

	glGenTextures(1, &state->curve_mapping_texture);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_1D, state->curve_mapping_texture);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	/* clean glError buffer */
	while (glGetError() != GL_NO_ERROR) {}

	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16F_ARB, curve_mapping_settings->lut_size,
	             0, GL_RGBA, GL_FLOAT, curve_mapping_settings->lut);

	state->curve_mapping_texture_allocated = true;

	/* GL_RGB16F_ARB could be not supported at some drivers
	 * in this case we could not use GLSL display
	 */
	state->curve_mapping_texture_valid = glGetError() == GL_NO_ERROR;

	return state->curve_mapping_texture_valid;
}

/* Detect if we can support GLSL drawing */
bool OCIOImpl::supportGLSLDraw()
{
	/* uses GL_RGB16F_ARB */
	return GLEW_VERSION_3_0 || GLEW_ARB_texture_float;
}

static bool supportGLSL13()
{
	const char *version = (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
	int major = 1, minor = 0;

	if (version && sscanf(version, "%d.%d", &major, &minor) == 2)
		return (major > 1 || (major == 1 && minor >= 30));

	return false;
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
bool OCIOImpl::setupGLSLDraw(OCIO_GLSLDrawState **state_r, OCIO_ConstProcessorRcPtr *processor,
                             OCIO_CurveMappingSettings *curve_mapping_settings,
                             float dither, bool use_predivide)
{
	ConstProcessorRcPtr ocio_processor = *(ConstProcessorRcPtr *) processor;
	bool use_curve_mapping = curve_mapping_settings != NULL;
	bool use_dither = dither > std::numeric_limits<float>::epsilon();

	/* Create state if needed. */
	OCIO_GLSLDrawState *state;
	if (!*state_r)
		*state_r = allocateOpenGLState();
	state = *state_r;

	glGetIntegerv(GL_TEXTURE_BINDING_2D, &state->last_texture);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &state->last_texture_unit);

	if (!ensureLUT3DAllocated(state)) {
		glActiveTexture(state->last_texture_unit);
		glBindTexture(GL_TEXTURE_2D, state->last_texture);

		return false;
	}

	if (use_curve_mapping) {
		if (!ensureCurveMappingAllocated(state, curve_mapping_settings)) {
			glActiveTexture(state->last_texture_unit);
			glBindTexture(GL_TEXTURE_2D, state->last_texture);

			return false;
		}
	}
	else {
		if (state->curve_mapping_texture_allocated) {
			glDeleteTextures(1, &state->curve_mapping_texture);
			state->curve_mapping_texture_allocated = false;
		}
	}

	/* Step 1: Create a GPU Shader Description */
	GpuShaderDesc shaderDesc;
	shaderDesc.setLanguage(GPU_LANGUAGE_GLSL_1_3);
	shaderDesc.setFunctionName("OCIODisplay");
	shaderDesc.setLut3DEdgeLen(LUT3D_EDGE_SIZE);

	if (use_curve_mapping) {
		if (state->curve_mapping_cache_id != curve_mapping_settings->cache_id) {
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_1D, state->curve_mapping_texture);
			glTexSubImage1D(GL_TEXTURE_1D, 0, 0, curve_mapping_settings->lut_size,
			                GL_RGBA, GL_FLOAT, curve_mapping_settings->lut);
		}
	}

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
	if (state->program == 0 ||
	    shaderCacheID != state->shadercacheid ||
	    use_predivide != state->predivide_used ||
	    use_curve_mapping != state->curve_mapping_used ||
	    use_dither != state->dither_used)
	{
		state->shadercacheid = shaderCacheID;

		if (state->program) {
			glDeleteProgram(state->program);
		}

		if (state->ocio_shader) {
			glDeleteShader(state->ocio_shader);
		}

		if (state->vert_shader) {
			glDeleteShader(state->vert_shader);
		}

		/* Vertex shader */
		std::ostringstream osv;

		if (supportGLSL13()) {
			osv << "#version 130\n";
		}
		else {
			osv << "#version 120\n";
		}

		osv << datatoc_gpu_shader_display_transform_vertex_glsl;

		state->vert_shader = compileShaderText(GL_VERTEX_SHADER, osv.str().c_str());

		/* Fragment shader */
		std::ostringstream os;

		if (supportGLSL13()) {
			os << "#version 130\n";
		}
		else {
			os << "#define USE_TEXTURE_SIZE\n";
			state->texture_size_used = use_dither;
		}

		if (use_predivide) {
			os << "#define USE_PREDIVIDE\n";
		}

		if (use_dither) {
			os << "#define USE_DITHER\n";
		}

		if (use_curve_mapping) {
			os << "#define USE_CURVE_MAPPING\n";
		}

		os << ocio_processor->getGpuShaderText(shaderDesc) << "\n";
		os << datatoc_gpu_shader_display_transform_glsl;

		state->ocio_shader = compileShaderText(GL_FRAGMENT_SHADER, os.str().c_str());

		if (state->ocio_shader && state->vert_shader) {
			state->program = linkShaders(state->ocio_shader, state->vert_shader);
		}

		state->curve_mapping_used = use_curve_mapping;
		state->dither_used = use_dither;
		state->predivide_used = use_predivide;
	}

	if (state->program) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D, state->lut3d_texture);

		if (use_curve_mapping) {
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_1D, state->curve_mapping_texture);
		}

		glActiveTexture(GL_TEXTURE0);

		state->shader_interface = ShaderInterface_create(state->program);

		immBindProgram(state->program, state->shader_interface);

		immUniform1i("image_texture", 0);
		immUniform1i("lut3d_texture", 1);

		if (state->texture_size_used) {
			/* we use textureSize() if possible for best performance, if not
			 * supported we query the size and pass it as uniform variables */
			GLint width, height;

			glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
			glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

			immUniform1f("image_texture_width", (float)width);
			immUniform1f("image_texture_height", (float)height);
		}

		if (use_dither) {
			immUniform1f("dither", dither);
		}

		if (use_curve_mapping) {
			immUniform1i("curve_mapping_texture", 2);
			immUniform1i("curve_mapping_lut_size", curve_mapping_settings->lut_size);
			immUniform4iv("use_curve_mapping_extend_extrapolate", curve_mapping_settings->use_extend_extrapolate);
			immUniform4fv("curve_mapping_mintable", curve_mapping_settings->mintable);
			immUniform4fv("curve_mapping_range", curve_mapping_settings->range);
			immUniform4fv("curve_mapping_ext_in_x", curve_mapping_settings->ext_in_x);
			immUniform4fv("curve_mapping_ext_in_y", curve_mapping_settings->ext_in_y);
			immUniform4fv("curve_mapping_ext_out_x", curve_mapping_settings->ext_out_x);
			immUniform4fv("curve_mapping_ext_out_y", curve_mapping_settings->ext_out_y);
			immUniform4fv("curve_mapping_first_x", curve_mapping_settings->first_x);
			immUniform4fv("curve_mapping_first_y", curve_mapping_settings->first_y);
			immUniform4fv("curve_mapping_last_x", curve_mapping_settings->last_x);
			immUniform4fv("curve_mapping_last_y", curve_mapping_settings->last_y);
			immUniform3fv("curve_mapping_black", curve_mapping_settings->black);
			immUniform3fv("curve_mapping_bwmul", curve_mapping_settings->bwmul);
		}

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
	immUnbindProgram();
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

	if (state->shader_interface)
		ShaderInterface_discard(state->shader_interface);		

	if (state->ocio_shader)
		glDeleteShader(state->ocio_shader);

	state->lut3dcacheid.~string();
	state->shadercacheid.~string();

	MEM_freeN(state);
}

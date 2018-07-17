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
#include "GPU_immediate.h"
}

using namespace OCIO_NAMESPACE;

#include "MEM_guardedalloc.h"

#include "ocio_impl.h"

static const int LUT3D_EDGE_SIZE = 64;
static const int SHADER_CACHE_SIZE = 4;

extern "C" char datatoc_gpu_shader_display_transform_glsl[];
extern "C" char datatoc_gpu_shader_display_transform_vertex_glsl[];

/* **** OpenGL drawing routines using GLSL for color space transform ***** */

typedef struct OCIO_GLSLShader {
	/* Cache ID */
	std::string lut3dCacheID;
	std::string shaderCacheID;

	/* LUT */
	bool lut3d_texture_allocated;  /* boolean flag indicating whether
	                                * lut texture is allocated
	                                */
	bool lut3d_texture_valid;

	GLuint lut3d_texture;  /* OGL texture ID for 3D LUT */

	float *lut3d;  /* 3D LUT table */

	/* Dither */
	bool use_dither;

	/* Curve Mapping */
	bool use_curve_mapping;
	bool curve_mapping_texture_allocated;
	bool curve_mapping_texture_valid;
	GLuint curve_mapping_texture;
	size_t curve_mapping_cache_id;

	/* Alpha Predivide */
	bool use_predivide;

	/* GLSL stuff */
	GLuint ocio_shader;
	GLuint vert_shader;
	GLuint program;
	Gwn_ShaderInterface *shader_interface;
} GLSLDrawState;

typedef struct OCIO_GLSLDrawState {
	/* Shader Cache */
	OCIO_GLSLShader *shader_cache[SHADER_CACHE_SIZE];

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
	return (OCIO_GLSLDrawState *) MEM_callocN(sizeof(OCIO_GLSLDrawState),
	                                          "OCIO OpenGL State struct");
}

/* Ensure LUT texture and array are allocated */
static bool ensureLUT3DAllocated(OCIO_GLSLShader *shader)
{
	int num_3d_entries = 3 * LUT3D_EDGE_SIZE * LUT3D_EDGE_SIZE * LUT3D_EDGE_SIZE;

	if (shader->lut3d_texture_allocated)
		return shader->lut3d_texture_valid;

	glGenTextures(1, &shader->lut3d_texture);

	shader->lut3d = (float *) MEM_callocN(sizeof(float) * num_3d_entries, "OCIO GPU 3D LUT");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D, shader->lut3d_texture);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	/* clean glError buffer */
	while (glGetError() != GL_NO_ERROR) {}

	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB16F_ARB,
	             LUT3D_EDGE_SIZE, LUT3D_EDGE_SIZE, LUT3D_EDGE_SIZE,
	             0, GL_RGB, GL_FLOAT, shader->lut3d);

	shader->lut3d_texture_allocated = true;

	/* GL_RGB16F_ARB could be not supported at some drivers
	 * in this case we could not use GLSL display
	 */
	shader->lut3d_texture_valid = glGetError() == GL_NO_ERROR;

	return shader->lut3d_texture_valid;
}

static bool ensureCurveMappingAllocated(OCIO_GLSLShader *shader, OCIO_CurveMappingSettings *curve_mapping_settings)
{
	if (shader->curve_mapping_texture_allocated)
		return shader->curve_mapping_texture_valid;

	glGenTextures(1, &shader->curve_mapping_texture);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_1D, shader->curve_mapping_texture);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	/* clean glError buffer */
	while (glGetError() != GL_NO_ERROR) {}

	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16F, curve_mapping_settings->lut_size,
	             0, GL_RGBA, GL_FLOAT, curve_mapping_settings->lut);

	shader->curve_mapping_texture_allocated = true;

	/* GL_RGB16F_ARB could be not supported at some drivers
	 * in this case we could not use GLSL display
	 */
	shader->curve_mapping_texture_valid = glGetError() == GL_NO_ERROR;

	return shader->curve_mapping_texture_valid;
}

static void freeGLSLShader(OCIO_GLSLShader *shader)
{
	if (shader->curve_mapping_texture_allocated) {
		glDeleteTextures(1, &shader->curve_mapping_texture);
	}

	if (shader->lut3d_texture_allocated) {
		glDeleteTextures(1, &shader->lut3d_texture);
	}

	if (shader->lut3d) {
		MEM_freeN(shader->lut3d);
	}

	if (shader->program) {
		glDeleteProgram(shader->program);
	}

	if (shader->shader_interface) {
		GWN_shaderinterface_discard(shader->shader_interface);
	}

	if (shader->ocio_shader) {
		glDeleteShader(shader->ocio_shader);
	}

	using std::string;
	shader->lut3dCacheID.~string();
	shader->shaderCacheID.~string();

	MEM_freeN(shader);
}



/* Detect if we can support GLSL drawing */
bool OCIOImpl::supportGLSLDraw()
{
	/* uses GL_RGB16F_ARB */
	return GLEW_VERSION_3_0 || GLEW_ARB_texture_float;
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

	/* Compute cache IDs. */
	GpuShaderDesc shaderDesc;
	shaderDesc.setLanguage(GPU_LANGUAGE_GLSL_1_3);
	shaderDesc.setFunctionName("OCIODisplay");
	shaderDesc.setLut3DEdgeLen(LUT3D_EDGE_SIZE);

	std::string lut3dCacheID = ocio_processor->getGpuLut3DCacheID(shaderDesc);
	std::string shaderCacheID = ocio_processor->getGpuShaderTextCacheID(shaderDesc);

	/* Find matching cached shader. */
	OCIO_GLSLShader *shader = NULL;
	for (int i = 0; i < SHADER_CACHE_SIZE; i++) {
		OCIO_GLSLShader *cached_shader = state->shader_cache[i];
		if (cached_shader == NULL) {
			continue;
		}

		if (cached_shader->lut3dCacheID == lut3dCacheID &&
		    cached_shader->shaderCacheID == shaderCacheID &&
		    cached_shader->use_predivide == use_predivide &&
		    cached_shader->use_curve_mapping == use_curve_mapping &&
		    cached_shader->use_dither == use_dither)
		{
			/* LRU cache, so move to front. */
			for (int j = i; j > 0; j--) {
				state->shader_cache[j] = state->shader_cache[j - 1];
			}
			state->shader_cache[0] = cached_shader;

			shader = cached_shader;
			break;
		}
	}

	if (shader == NULL) {
		/* LRU cache, shift other items back so we can insert at the front. */
		OCIO_GLSLShader *last_shader = state->shader_cache[SHADER_CACHE_SIZE - 1];
		if (last_shader) {
			freeGLSLShader(last_shader);
		}
		for (int j = SHADER_CACHE_SIZE - 1; j > 0; j--) {
			state->shader_cache[j] = state->shader_cache[j - 1];
		}

		/* Allocate memory for shader. */
		shader = (OCIO_GLSLShader *) MEM_callocN(sizeof(OCIO_GLSLShader),
		                                         "OCIO GLSL Shader");
		state->shader_cache[0] = shader;

		new (&shader->lut3dCacheID) std::string();
		new (&shader->shaderCacheID) std::string();

        shader->lut3dCacheID = lut3dCacheID;
        shader->shaderCacheID = shaderCacheID;
		shader->use_curve_mapping = use_curve_mapping;
		shader->use_dither = use_dither;
		shader->use_predivide = use_predivide;

		bool valid = true;

		/* Compute 3D LUT. */
		if (valid && ensureLUT3DAllocated(shader)) {
		    ocio_processor->getGpuLut3D(shader->lut3d, shaderDesc);

		    glActiveTexture(GL_TEXTURE1);
		    glBindTexture(GL_TEXTURE_3D, shader->lut3d_texture);
		    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0,
		                    LUT3D_EDGE_SIZE, LUT3D_EDGE_SIZE, LUT3D_EDGE_SIZE,
		                    GL_RGB, GL_FLOAT, shader->lut3d);
		}
		else {
			valid = false;
		}

		/* Allocate curve mapping texture. */
		if (valid && use_curve_mapping) {
			if (!ensureCurveMappingAllocated(shader, curve_mapping_settings)) {
				valid = false;
			}
		}

		if (valid) {
			/* Vertex shader */
			std::ostringstream osv;

			osv << "#version 330\n";
			osv << datatoc_gpu_shader_display_transform_vertex_glsl;

			shader->vert_shader = compileShaderText(GL_VERTEX_SHADER, osv.str().c_str());

			/* Fragment shader */
			std::ostringstream os;

			os << "#version 330\n";

			/* Work around OpenColorIO not supporting latest GLSL yet. */
			os << "#define texture2D texture\n";
			os << "#define texture3D texture\n";

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

			shader->ocio_shader = compileShaderText(GL_FRAGMENT_SHADER, os.str().c_str());

			/* Program */
			if (shader->ocio_shader && shader->vert_shader) {
				shader->program = linkShaders(shader->ocio_shader, shader->vert_shader);
			}

			if (shader->program) {
				if (shader->shader_interface) {
					GWN_shaderinterface_discard(shader->shader_interface);
				}
				shader->shader_interface = GWN_shaderinterface_create(shader->program);
			}
		}
	}

	/* Update curve mapping texture. */
	if (use_curve_mapping && shader->curve_mapping_texture_allocated) {
		if (shader->curve_mapping_cache_id != curve_mapping_settings->cache_id) {
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_1D, shader->curve_mapping_texture);
			glTexSubImage1D(GL_TEXTURE_1D, 0, 0, curve_mapping_settings->lut_size,
			                GL_RGBA, GL_FLOAT, curve_mapping_settings->lut);
		}
	}

	/* Bind Shader. */
	if (shader->program) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D, shader->lut3d_texture);

		if (use_curve_mapping) {
			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_1D, shader->curve_mapping_texture);
		}

		glActiveTexture(GL_TEXTURE0);

		/* IMM needs vertex format even if we don't draw with it.
		 *
		 * NOTE: The only reason why it's here is because of Cycles viewport.
		 * All other areas are managing their own vertex formats.
		 * Doing it here is probably harmless, but kind of stupid.
		 *
		 * TODO(sergey): Look into some nicer solution.
		 */
		Gwn_VertFormat *format = immVertexFormat();
		GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		GWN_vertformat_attr_add(format, "texCoord", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
		immBindProgram(shader->program, shader->shader_interface);

		immUniform1i("image_texture", 0);
		immUniform1i("lut3d_texture", 1);

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

void OCIOImpl::freeGLState(OCIO_GLSLDrawState *state)
{
	for (int i = 0; i < SHADER_CACHE_SIZE; i++) {
		if (state->shader_cache[i]) {
			freeGLSLShader(state->shader_cache[i]);
		}
	}

	MEM_freeN(state);
}

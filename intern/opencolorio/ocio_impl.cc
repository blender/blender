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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Xavier Thomas
 *                 Lukas Toene,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <iostream>
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

#if !defined(WITH_ASSERT_ABORT)
#  define OCIO_abort()
#else
#  include <stdlib.h>
#  define OCIO_abort() abort()
#endif

#if defined(_MSC_VER)
#  define __func__ __FUNCTION__
#endif

#define MEM_NEW(type) new(MEM_mallocN(sizeof(type), __func__)) type()
#define MEM_DELETE(what, type) if(what) { ((type*)(what))->~type(); MEM_freeN(what); } (void)0

static const int LUT3D_EDGE_SIZE = 32;

static void OCIO_reportError(const char *err)
{
	std::cerr << "OpenColorIO Error: " << err << std::endl;

	OCIO_abort();
}

static void OCIO_reportException(Exception &exception)
{
	OCIO_reportError(exception.what());
}

OCIO_ConstConfigRcPtr *OCIOImpl::getCurrentConfig(void)
{
	ConstConfigRcPtr *config = MEM_NEW(ConstConfigRcPtr);

	try {
		*config = GetCurrentConfig();

		if (*config)
			return (OCIO_ConstConfigRcPtr *) config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	MEM_DELETE(config, ConstConfigRcPtr);

	return NULL;
}

void OCIOImpl::setCurrentConfig(const OCIO_ConstConfigRcPtr *config)
{
	try {
		SetCurrentConfig(*(ConstConfigRcPtr *) config);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}
}

OCIO_ConstConfigRcPtr *OCIOImpl::configCreateFromEnv(void)
{
	ConstConfigRcPtr *config = MEM_NEW(ConstConfigRcPtr);

	try {
		*config = Config::CreateFromEnv();

		if (*config)
			return (OCIO_ConstConfigRcPtr *) config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	MEM_DELETE(config, ConstConfigRcPtr);

	return NULL;
}


OCIO_ConstConfigRcPtr *OCIOImpl::configCreateFromFile(const char *filename)
{
	ConstConfigRcPtr *config = MEM_NEW(ConstConfigRcPtr);

	try {
		*config = Config::CreateFromFile(filename);

		if (*config)
			return (OCIO_ConstConfigRcPtr *) config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	MEM_DELETE(config, ConstConfigRcPtr);

	return NULL;
}

void OCIOImpl::configRelease(OCIO_ConstConfigRcPtr *config)
{
	MEM_DELETE((ConstConfigRcPtr *) config, ConstConfigRcPtr);
}

int OCIOImpl::configGetNumColorSpaces(OCIO_ConstConfigRcPtr *config)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getNumColorSpaces();
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return 0;
}

const char *OCIOImpl::configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr *config, int index)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getColorSpaceNameByIndex(index);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

OCIO_ConstColorSpaceRcPtr *OCIOImpl::configGetColorSpace(OCIO_ConstConfigRcPtr *config, const char *name)
{
	ConstColorSpaceRcPtr *cs = MEM_NEW(ConstColorSpaceRcPtr);

	try {
		*cs = (*(ConstConfigRcPtr *) config)->getColorSpace(name);

		if (*cs)
			return (OCIO_ConstColorSpaceRcPtr *) cs;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	MEM_DELETE(cs, ConstColorSpaceRcPtr);

	return NULL;
}

int OCIOImpl::configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getIndexForColorSpace(name);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return -1;
}

const char *OCIOImpl::configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getDefaultDisplay();
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

int OCIOImpl::configGetNumDisplays(OCIO_ConstConfigRcPtr* config)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getNumDisplays();
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return 0;
}

const char *OCIOImpl::configGetDisplay(OCIO_ConstConfigRcPtr *config, int index)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getDisplay(index);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

const char *OCIOImpl::configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getDefaultView(display);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

int OCIOImpl::configGetNumViews(OCIO_ConstConfigRcPtr *config, const char *display)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getNumViews(display);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return 0;
}

const char *OCIOImpl::configGetView(OCIO_ConstConfigRcPtr *config, const char *display, int index)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getView(display, index);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

const char *OCIOImpl::configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr *config, const char *display, const char *view)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getDisplayColorSpaceName(display, view);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

int OCIOImpl::colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr *cs_)
{
	ConstColorSpaceRcPtr *cs = (ConstColorSpaceRcPtr *) cs_;
	const char *family = (*cs)->getFamily();

	if (!strcmp(family, "rrt") || !strcmp(family, "display")) {
		/* assume display and rrt transformations are not invertible
		 * in fact some of them could be, but it doesn't make much sense to allow use them as invertible
		 */
		return false;
	}

	if ((*cs)->isData()) {
		/* data color spaces don't have transformation at all */
		return true;
	}

	if ((*cs)->getTransform(COLORSPACE_DIR_TO_REFERENCE)) {
		/* if there's defined transform to reference space, color space could be converted to scene linear */
		return true;
	}

	return true;
}

int OCIOImpl::colorSpaceIsData(OCIO_ConstColorSpaceRcPtr *cs)
{
	return (*(ConstColorSpaceRcPtr *) cs)->isData();
}

void OCIOImpl::colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs)
{
	MEM_DELETE((ConstColorSpaceRcPtr *) cs, ConstColorSpaceRcPtr);
}

OCIO_ConstProcessorRcPtr *OCIOImpl::configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config, const char *srcName, const char *dstName)
{
	ConstProcessorRcPtr *p = MEM_NEW(ConstProcessorRcPtr);

	try {
		*p = (*(ConstConfigRcPtr *) config)->getProcessor(srcName, dstName);

		if (*p)
			return (OCIO_ConstProcessorRcPtr *) p;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	MEM_DELETE(p, ConstProcessorRcPtr);

	return 0;
}

OCIO_ConstProcessorRcPtr *OCIOImpl::configGetProcessor(OCIO_ConstConfigRcPtr *config, OCIO_ConstTransformRcPtr *transform)
{
	ConstProcessorRcPtr *p = MEM_NEW(ConstProcessorRcPtr);

	try {
		*p = (*(ConstConfigRcPtr *) config)->getProcessor(*(ConstTransformRcPtr *) transform);

		if (*p)
			return (OCIO_ConstProcessorRcPtr *) p;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	MEM_DELETE(p, ConstProcessorRcPtr);

	return NULL;
}

void OCIOImpl::processorApply(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img)
{
	try {
		(*(ConstProcessorRcPtr *) processor)->apply(*(PackedImageDesc *) img);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}
}

void OCIOImpl::processorApply_predivide(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img_)
{
	try {
		PackedImageDesc *img = (PackedImageDesc *) img_;
		int channels = img->getNumChannels();

		if (channels == 4) {
			float *pixels = img->getData();

			int width = img->getWidth();
			int height = img->getHeight();

			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					float *pixel = pixels + 4 * (y * width + x);

					processorApplyRGBA_predivide(processor, pixel);
				}
			}
		}
		else {
			(*(ConstProcessorRcPtr *) processor)->apply(*img);
		}
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}
}

void OCIOImpl::processorApplyRGB(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
	(*(ConstProcessorRcPtr *) processor)->applyRGB(pixel);
}

void OCIOImpl::processorApplyRGBA(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
	(*(ConstProcessorRcPtr *) processor)->applyRGBA(pixel);
}

void OCIOImpl::processorApplyRGBA_predivide(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
	if (pixel[3] == 1.0f || pixel[3] == 0.0f) {
		(*(ConstProcessorRcPtr *) processor)->applyRGBA(pixel);
	}
	else {
		float alpha, inv_alpha;

		alpha = pixel[3];
		inv_alpha = 1.0f / alpha;

		pixel[0] *= inv_alpha;
		pixel[1] *= inv_alpha;
		pixel[2] *= inv_alpha;

		(*(ConstProcessorRcPtr *) processor)->applyRGBA(pixel);

		pixel[0] *= alpha;
		pixel[1] *= alpha;
		pixel[2] *= alpha;
	}
}

void OCIOImpl::processorRelease(OCIO_ConstProcessorRcPtr *p)
{
	MEM_DELETE(p, ConstProcessorRcPtr);
}

const char *OCIOImpl::colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs)
{
	return (*(ConstColorSpaceRcPtr *) cs)->getName();
}

const char *OCIOImpl::colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr *cs)
{
	return (*(ConstColorSpaceRcPtr *) cs)->getDescription();
}

const char *OCIOImpl::colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr *cs)
{
	return (*(ConstColorSpaceRcPtr *)cs)->getFamily();
}

OCIO_DisplayTransformRcPtr *OCIOImpl::createDisplayTransform(void)
{
	DisplayTransformRcPtr *dt = MEM_NEW(DisplayTransformRcPtr);

	*dt = DisplayTransform::Create();

	return (OCIO_DisplayTransformRcPtr *) dt;
}

void OCIOImpl::displayTransformSetInputColorSpaceName(OCIO_DisplayTransformRcPtr *dt, const char *name)
{
	(*(DisplayTransformRcPtr *) dt)->setInputColorSpaceName(name);
}

void OCIOImpl::displayTransformSetDisplay(OCIO_DisplayTransformRcPtr *dt, const char *name)
{
	(*(DisplayTransformRcPtr *) dt)->setDisplay(name);
}

void OCIOImpl::displayTransformSetView(OCIO_DisplayTransformRcPtr *dt, const char *name)
{
	(*(DisplayTransformRcPtr *) dt)->setView(name);
}

void OCIOImpl::displayTransformSetDisplayCC(OCIO_DisplayTransformRcPtr *dt, OCIO_ConstTransformRcPtr *t)
{
	(*(DisplayTransformRcPtr *) dt)->setDisplayCC(* (ConstTransformRcPtr *) t);
}

void OCIOImpl::displayTransformSetLinearCC(OCIO_DisplayTransformRcPtr *dt, OCIO_ConstTransformRcPtr *t)
{
	(*(DisplayTransformRcPtr *) dt)->setLinearCC(*(ConstTransformRcPtr *) t);
}

void OCIOImpl::displayTransformRelease(OCIO_DisplayTransformRcPtr *dt)
{
	MEM_DELETE((DisplayTransformRcPtr *) dt, DisplayTransformRcPtr);
}

OCIO_PackedImageDesc *OCIOImpl::createOCIO_PackedImageDesc(float *data, long width, long height, long numChannels,
                                                           long chanStrideBytes, long xStrideBytes, long yStrideBytes)
{
	try {
		void *mem = MEM_mallocN(sizeof(PackedImageDesc), __func__);
		PackedImageDesc *id = new(mem) PackedImageDesc(data, width, height, numChannels, chanStrideBytes, xStrideBytes, yStrideBytes);

		return (OCIO_PackedImageDesc *) id;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

void OCIOImpl::OCIO_PackedImageDescRelease(OCIO_PackedImageDesc* id)
{
	MEM_DELETE((PackedImageDesc *) id, PackedImageDesc);
}

OCIO_ExponentTransformRcPtr *OCIOImpl::createExponentTransform(void)
{
	ExponentTransformRcPtr *et = MEM_NEW(ExponentTransformRcPtr);

	*et = ExponentTransform::Create();

	return (OCIO_ExponentTransformRcPtr *) et;
}

void OCIOImpl::exponentTransformSetValue(OCIO_ExponentTransformRcPtr *et, const float *exponent)
{
	(*(ExponentTransformRcPtr *) et)->setValue(exponent);
}

void OCIOImpl::exponentTransformRelease(OCIO_ExponentTransformRcPtr *et)
{
	MEM_DELETE((ExponentTransformRcPtr *) et, ExponentTransformRcPtr);
}

OCIO_MatrixTransformRcPtr *OCIOImpl::createMatrixTransform(void)
{
	MatrixTransformRcPtr *mt = MEM_NEW(MatrixTransformRcPtr);

	*mt = MatrixTransform::Create();

	return (OCIO_MatrixTransformRcPtr *) mt;
}

void OCIOImpl::matrixTransformSetValue(OCIO_MatrixTransformRcPtr *mt, const float *m44, const float *offset4)
{
	(*(MatrixTransformRcPtr *) mt)->setValue(m44, offset4);
}

void OCIOImpl::matrixTransformRelease(OCIO_MatrixTransformRcPtr *mt)
{
	MEM_DELETE((MatrixTransformRcPtr *) mt, MatrixTransformRcPtr);
}

void OCIOImpl::matrixTransformScale(float * m44, float * offset4, const float *scale4f)
{
	MatrixTransform::Scale(m44, offset4, scale4f);
}

/* **** OpenGL drawing routines using GLSL for color space transform ***** */

/* Some of the GLSL transform related functions below are adopted from
 * ociodisplay utility of OpenColorIO project which are originally
 *
 * Copyright (c) 2003-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 */

typedef struct OCIO_GLSLDrawState {
	bool lut3d_texture_allocated;  /* boolean flag indicating whether
									* lut texture is allocated
									*/

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

static const char * g_fragShaderText = ""
"\n"
"uniform sampler2D tex1;\n"
"uniform sampler3D tex2;\n"
"\n"
"void main()\n"
"{\n"
"    vec4 col = texture2D(tex1, gl_TexCoord[0].st);\n"
"    gl_FragColor = OCIODisplay(col, tex2);\n"
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
static void ensureLUT3DAllocated(OCIO_GLSLDrawState *state)
{
	int num_3d_entries = 3 * LUT3D_EDGE_SIZE * LUT3D_EDGE_SIZE * LUT3D_EDGE_SIZE;

	if (state->lut3d_texture_allocated)
		return;

	glGenTextures(1, &state->lut3d_texture);

	state->lut3d = (float *) MEM_callocN(sizeof(float) * num_3d_entries, "OCIO GPU 3D LUT");

    glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D, state->lut3d_texture);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB16F_ARB,
	             LUT3D_EDGE_SIZE, LUT3D_EDGE_SIZE, LUT3D_EDGE_SIZE,
	             0, GL_RGB,GL_FLOAT, &state->lut3d);

	state->lut3d_texture_allocated = true;
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
void OCIOImpl::setupGLSLDraw(OCIO_GLSLDrawState **state_r, OCIO_ConstProcessorRcPtr *processor)
{
	ConstProcessorRcPtr ocio_processor = *(ConstProcessorRcPtr *) processor;

	/* Create state if needed. */
	OCIO_GLSLDrawState *state;
	if (!*state_r)
		*state_r = allocateOpenGLState();
	state = *state_r;

	glGetIntegerv(GL_TEXTURE_2D, &state->last_texture);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &state->last_texture_unit);

	ensureLUT3DAllocated(state);

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

		if (state->program)
			glDeleteProgram(state->program);

		state->program = linkShaders(state->fragShader);
	}

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D, state->lut3d_texture);

	glActiveTexture(GL_TEXTURE0);

    glUseProgram(state->program);
    glUniform1i(glGetUniformLocation(state->program, "tex1"), 0);
    glUniform1i(glGetUniformLocation(state->program, "tex2"), 1);
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

	state->lut3dcacheid.~string();
	state->shadercacheid.~string();

	MEM_freeN(state);
}

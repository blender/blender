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
 * Contributor(s): Brecht van Lommel
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>

#include "MEM_guardedalloc.h"
#include "BLI_math_color.h"

#include "ocio_impl.h"

#define CONFIG_DEFAULT           ((OCIO_ConstConfigRcPtr*)1)

#define PROCESSOR_LINEAR_TO_SRGB ((OCIO_ConstProcessorRcPtr*)1)
#define PROCESSOR_SRGB_TO_LINEAR ((OCIO_ConstProcessorRcPtr*)2)
#define PROCESSOR_UNKNOWN        ((OCIO_ConstProcessorRcPtr*)3)

#define COLORSPACE_LINEAR        ((OCIO_ConstColorSpaceRcPtr*)1)
#define COLORSPACE_SRGB          ((OCIO_ConstColorSpaceRcPtr*)2)

typedef struct OCIO_PackedImageDescription {
	float *data;
	long width;
	long height;
	long numChannels;
	long chanStrideBytes;
	long xStrideBytes;
	long yStrideBytes;
} OCIO_PackedImageDescription;

OCIO_ConstConfigRcPtr *FallbackImpl::getCurrentConfig(void)
{
	return CONFIG_DEFAULT;
}

void FallbackImpl::setCurrentConfig(const OCIO_ConstConfigRcPtr * /*config*/)
{
}

OCIO_ConstConfigRcPtr *FallbackImpl::configCreateFromEnv(void)
{
	return CONFIG_DEFAULT;
}

OCIO_ConstConfigRcPtr *FallbackImpl::configCreateFromFile(const char * /*filename*/)
{
	return CONFIG_DEFAULT;
}

void FallbackImpl::configRelease(OCIO_ConstConfigRcPtr * /*config*/)
{
}

int FallbackImpl::configGetNumColorSpaces(OCIO_ConstConfigRcPtr * /*config*/)
{
	return 2;
}

const char *FallbackImpl::configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr * /*config*/, int index)
{
	if (index == 0)
		return "Linear";
	else if (index == 1)
		return "sRGB";
	
	return NULL;
}

OCIO_ConstColorSpaceRcPtr *FallbackImpl::configGetColorSpace(OCIO_ConstConfigRcPtr * /*config*/, const char *name)
{
	if (strcmp(name, "scene_linear") == 0)
		return COLORSPACE_LINEAR;
	else if (strcmp(name, "color_picking") == 0)
		return COLORSPACE_SRGB;
	else if (strcmp(name, "texture_paint") == 0)
		return COLORSPACE_LINEAR;
	else if (strcmp(name, "default_byte") == 0)
		return COLORSPACE_SRGB;
	else if (strcmp(name, "default_float") == 0)
		return COLORSPACE_LINEAR;
	else if (strcmp(name, "default_sequencer") == 0)
		return COLORSPACE_SRGB;
	else if (strcmp(name, "Linear") == 0)
		return COLORSPACE_LINEAR;
	else if (strcmp(name, "sRGB") == 0)
		return COLORSPACE_SRGB;

	return NULL;
}

int FallbackImpl::configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name)
{
	OCIO_ConstColorSpaceRcPtr *cs = configGetColorSpace(config, name);

	if (cs == COLORSPACE_LINEAR)
		return 0;
	else if (cs == COLORSPACE_SRGB)
		return 1;

	return -1;
}

const char *FallbackImpl::configGetDefaultDisplay(OCIO_ConstConfigRcPtr * /*config*/)
{
	return "sRGB";
}

int FallbackImpl::configGetNumDisplays(OCIO_ConstConfigRcPtr * /*config*/)
{
	return 1;
}

const char *FallbackImpl::configGetDisplay(OCIO_ConstConfigRcPtr * /*config*/, int index)
{
	if (index == 0)
		return "sRGB";
	
	return NULL;
}

const char *FallbackImpl::configGetDefaultView(OCIO_ConstConfigRcPtr * /*config*/, const char * /*display*/)
{
	return "Default";
}

int FallbackImpl::configGetNumViews(OCIO_ConstConfigRcPtr * /*config*/, const char * /*display*/)
{
	return 1;
}

const char *FallbackImpl::configGetView(OCIO_ConstConfigRcPtr * /*config*/, const char * /*display*/, int index)
{
	if (index == 0)
		return "Default";

	return NULL;
}

const char *FallbackImpl::configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr * /*config*/, const char * /*display*/, const char * /*view*/)
{
	return "sRGB";
}

int FallbackImpl::configGetNumLooks(OCIO_ConstConfigRcPtr * /*config*/)
{
	return 0;
}

const char *FallbackImpl::configGetLookNameByIndex(OCIO_ConstConfigRcPtr * /*config*/, int /*index*/)
{
	return "";
}

OCIO_ConstLookRcPtr *FallbackImpl::configGetLook(OCIO_ConstConfigRcPtr * /*config*/, const char * /*name*/)
{
	return NULL;
}

const char *FallbackImpl::lookGetProcessSpace(OCIO_ConstLookRcPtr *look)
{
	return NULL;
}

void FallbackImpl::lookRelease(OCIO_ConstLookRcPtr * /*look*/)
{
}

int FallbackImpl::colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr * /*cs*/)
{
	return 1;
}

int FallbackImpl::colorSpaceIsData(OCIO_ConstColorSpaceRcPtr * /*cs*/)
{
	return 0;
}

void FallbackImpl::colorSpaceRelease(OCIO_ConstColorSpaceRcPtr * /*cs*/)
{
}

OCIO_ConstProcessorRcPtr *FallbackImpl::configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config, const char *srcName, const char *dstName)
{
	OCIO_ConstColorSpaceRcPtr *cs_src = configGetColorSpace(config, srcName);
	OCIO_ConstColorSpaceRcPtr *cs_dst = configGetColorSpace(config, dstName);

	if (cs_src == COLORSPACE_LINEAR && cs_dst == COLORSPACE_SRGB)
		return PROCESSOR_LINEAR_TO_SRGB;
	else if (cs_src == COLORSPACE_SRGB && cs_dst == COLORSPACE_LINEAR)
		return PROCESSOR_SRGB_TO_LINEAR;

	return 0;
}

OCIO_ConstProcessorRcPtr *FallbackImpl::configGetProcessor(OCIO_ConstConfigRcPtr * /*config*/, OCIO_ConstTransformRcPtr *tfm)
{
	return (OCIO_ConstProcessorRcPtr*)tfm;
}

void FallbackImpl::processorApply(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img)
{
	/* OCIO_TODO stride not respected, channels must be 3 or 4 */
	OCIO_PackedImageDescription *desc = (OCIO_PackedImageDescription*)img;
	int channels = desc->numChannels;
	float *pixels = desc->data;
	int width = desc->width;
	int height = desc->height;
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			float *pixel = pixels + channels * (y * width + x);

			if (channels == 4)
				processorApplyRGBA(processor, pixel);
			else if (channels == 3)
				processorApplyRGB(processor, pixel);
		}
	}
}

void FallbackImpl::processorApply_predivide(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img)
{
	/* OCIO_TODO stride not respected, channels must be 3 or 4 */
	OCIO_PackedImageDescription *desc = (OCIO_PackedImageDescription*)img;
	int channels = desc->numChannels;
	float *pixels = desc->data;
	int width = desc->width;
	int height = desc->height;
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			float *pixel = pixels + channels * (y * width + x);

			if (channels == 4)
				processorApplyRGBA_predivide(processor, pixel);
			else if (channels == 3)
				processorApplyRGB(processor, pixel);
		}
	}
}

void FallbackImpl::processorApplyRGB(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
	if (processor == PROCESSOR_LINEAR_TO_SRGB)
		linearrgb_to_srgb_v3_v3(pixel, pixel);
	else if (processor == PROCESSOR_SRGB_TO_LINEAR)
		srgb_to_linearrgb_v3_v3(pixel, pixel);
}

void FallbackImpl::processorApplyRGBA(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
	if (processor == PROCESSOR_LINEAR_TO_SRGB)
		linearrgb_to_srgb_v4(pixel, pixel);
	else if (processor == PROCESSOR_SRGB_TO_LINEAR)
		srgb_to_linearrgb_v4(pixel, pixel);
}

void FallbackImpl::processorApplyRGBA_predivide(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
	if (pixel[3] == 1.0f || pixel[3] == 0.0f) {
		processorApplyRGBA(processor, pixel);
	}
	else {
		float alpha, inv_alpha;

		alpha = pixel[3];
		inv_alpha = 1.0f / alpha;

		pixel[0] *= inv_alpha;
		pixel[1] *= inv_alpha;
		pixel[2] *= inv_alpha;

		processorApplyRGBA(processor, pixel);

		pixel[0] *= alpha;
		pixel[1] *= alpha;
		pixel[2] *= alpha;
	}
}

void FallbackImpl::processorRelease(OCIO_ConstProcessorRcPtr * /*p*/)
{
}

const char *FallbackImpl::colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs)
{
	if (cs == COLORSPACE_LINEAR)
		return "Linear";
	else if (cs == COLORSPACE_SRGB)
		return "sRGB";
	
	return NULL;
}

const char *FallbackImpl::colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr * /*cs*/)
{
	return "";
}

const char *FallbackImpl::colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr * /*cs*/)
{
	return "";
}

OCIO_DisplayTransformRcPtr *FallbackImpl::createDisplayTransform(void)
{
	return (OCIO_DisplayTransformRcPtr*)PROCESSOR_LINEAR_TO_SRGB;
}

void FallbackImpl::displayTransformSetInputColorSpaceName(OCIO_DisplayTransformRcPtr * /*dt*/, const char * /*name*/)
{
}

void FallbackImpl::displayTransformSetDisplay(OCIO_DisplayTransformRcPtr * /*dt*/, const char * /*name*/)
{
}

void FallbackImpl::displayTransformSetView(OCIO_DisplayTransformRcPtr * /*dt*/, const char * /*name*/)
{
}

void FallbackImpl::displayTransformSetDisplayCC(OCIO_DisplayTransformRcPtr * /*dt*/, OCIO_ConstTransformRcPtr * /*et*/)
{
}

void FallbackImpl::displayTransformSetLinearCC(OCIO_DisplayTransformRcPtr * /*dt*/, OCIO_ConstTransformRcPtr * /*et*/)
{
}

void FallbackImpl::displayTransformSetLooksOverride(OCIO_DisplayTransformRcPtr * /*dt*/, const char * /*looks*/)
{
}

void FallbackImpl::displayTransformSetLooksOverrideEnabled(OCIO_DisplayTransformRcPtr * /*dt*/, bool /*enabled*/)
{
}

void FallbackImpl::displayTransformRelease(OCIO_DisplayTransformRcPtr * /*dt*/)
{
}

OCIO_PackedImageDesc *FallbackImpl::createOCIO_PackedImageDesc(float *data, long width, long height, long numChannels,
                                                               long chanStrideBytes, long xStrideBytes, long yStrideBytes)
{
	OCIO_PackedImageDescription *desc = (OCIO_PackedImageDescription*)MEM_callocN(sizeof(OCIO_PackedImageDescription), "OCIO_PackedImageDescription");

	desc->data = data;
	desc->width = width;
	desc->height = height;
	desc->numChannels = numChannels;
	desc->chanStrideBytes = chanStrideBytes;
	desc->xStrideBytes = xStrideBytes;
	desc->yStrideBytes = yStrideBytes;

	return (OCIO_PackedImageDesc*)desc;
}

void FallbackImpl::OCIO_PackedImageDescRelease(OCIO_PackedImageDesc* id)
{
	MEM_freeN(id);
}

OCIO_ExponentTransformRcPtr *FallbackImpl::createExponentTransform(void)
{
	return (OCIO_ExponentTransformRcPtr*)PROCESSOR_UNKNOWN;
}

void FallbackImpl::exponentTransformSetValue(OCIO_ExponentTransformRcPtr * /*et*/, const float * /*exponent*/)
{
}

void FallbackImpl::exponentTransformRelease(OCIO_ExponentTransformRcPtr * /*et*/)
{
}

OCIO_MatrixTransformRcPtr *FallbackImpl::createMatrixTransform(void)
{
	return (OCIO_MatrixTransformRcPtr*)PROCESSOR_UNKNOWN;
}

void FallbackImpl::matrixTransformSetValue(OCIO_MatrixTransformRcPtr * /*mt*/, const float * /*m44*/, const float * /*offset4*/)
{
}

void FallbackImpl::matrixTransformRelease(OCIO_MatrixTransformRcPtr * /*mt*/)
{
}

void FallbackImpl::matrixTransformScale(float * /*m44*/, float * /*offset44*/, const float * /*scale4*/)
{
}

bool FallbackImpl::supportGLSLDraw(void)
{
	return false;
}

bool FallbackImpl::setupGLSLDraw(struct OCIO_GLSLDrawState ** /*state_r*/, OCIO_ConstProcessorRcPtr * /*processor*/,
                                 OCIO_CurveMappingSettings * /*curve_mapping_settings*/,
                                 float /*dither*/,  bool /*predivide*/)
{
	return false;
}

void FallbackImpl::finishGLSLDraw(OCIO_GLSLDrawState * /*state*/)
{
}

void FallbackImpl::freeGLState(struct OCIO_GLSLDrawState * /*state_r*/)
{
}

const char *FallbackImpl::getVersionString(void)
{
	return "fallback";
}

int FallbackImpl::getVersionHex(void)
{
	return 0;
}

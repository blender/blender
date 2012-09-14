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

namespace OCIO_NAMESPACE {};

#include "ocio_capi.h"

#define CONFIG_DEFAULT           ((ConstConfigRcPtr*)1)

#define PROCESSOR_LINEAR_TO_SRGB ((ConstProcessorRcPtr*)1)
#define PROCESSOR_SRGB_TO_LINEAR ((ConstProcessorRcPtr*)2)
#define PROCESSOR_UNKNOWN        ((ConstProcessorRcPtr*)3)

#define COLORSPACE_LINEAR        ((ConstColorSpaceRcPtr*)1)
#define COLORSPACE_SRGB          ((ConstColorSpaceRcPtr*)2)

typedef struct PackedImageDescription {
	float *data;
	long width;
	long height;
	long numChannels;
	long chanStrideBytes;
	long xStrideBytes;
	long yStrideBytes;
} PackedImageDescription;

ConstConfigRcPtr *OCIO_getCurrentConfig(void)
{
	return CONFIG_DEFAULT;
}

void OCIO_setCurrentConfig(const ConstConfigRcPtr *)
{
}

ConstConfigRcPtr *OCIO_configCreateFromEnv(void)
{
	return CONFIG_DEFAULT;
}

ConstConfigRcPtr *OCIO_configCreateFromFile(const char *)
{
	return CONFIG_DEFAULT;
}

void OCIO_configRelease(ConstConfigRcPtr *)
{
}

int OCIO_configGetNumColorSpaces(ConstConfigRcPtr *)
{
	return 2;
}

const char *OCIO_configGetColorSpaceNameByIndex(ConstConfigRcPtr *, int index)
{
	if (index == 0)
		return "Linear";
	else if (index == 1)
		return "sRGB";
	
	return NULL;
}

ConstColorSpaceRcPtr *OCIO_configGetColorSpace(ConstConfigRcPtr *, const char *name)
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

int OCIO_configGetIndexForColorSpace(ConstConfigRcPtr *config, const char *name)
{
	ConstColorSpaceRcPtr *cs = OCIO_configGetColorSpace(config, name);

	if (cs == COLORSPACE_LINEAR)
		return 0;
	else if (cs == COLORSPACE_SRGB)
		return 1;

	return -1;
}

const char *OCIO_configGetDefaultDisplay(ConstConfigRcPtr *)
{
	return "sRGB";
}

int OCIO_configGetNumDisplays(ConstConfigRcPtr* config)
{
	return 1;
}

const char *OCIO_configGetDisplay(ConstConfigRcPtr *, int index)
{
	if (index == 0)
		return "sRGB";
	
	return NULL;
}

const char *OCIO_configGetDefaultView(ConstConfigRcPtr *, const char *)
{
	return "Default";
}

int OCIO_configGetNumViews(ConstConfigRcPtr *, const char *)
{
	return 1;
}

const char *OCIO_configGetView(ConstConfigRcPtr *, const char *, int index)
{
	if (index == 0)
		return "Default";

	return NULL;
}

const char *OCIO_configGetDisplayColorSpaceName(ConstConfigRcPtr *, const char *, const char *)
{
	return "sRGB";
}

int OCIO_colorSpaceIsInvertible(ConstColorSpaceRcPtr *cs)
{
	return 1;
}

void OCIO_colorSpaceRelease(ConstColorSpaceRcPtr *cs)
{
}

ConstProcessorRcPtr *OCIO_configGetProcessorWithNames(ConstConfigRcPtr *config, const char *srcName, const char *dstName)
{
	ConstColorSpaceRcPtr *cs_src = OCIO_configGetColorSpace(config, srcName);
	ConstColorSpaceRcPtr *cs_dst = OCIO_configGetColorSpace(config, dstName);

	if (cs_src == COLORSPACE_LINEAR && cs_dst == COLORSPACE_SRGB)
		return PROCESSOR_LINEAR_TO_SRGB;
	else if (cs_src == COLORSPACE_SRGB && cs_dst == COLORSPACE_LINEAR)
		return PROCESSOR_SRGB_TO_LINEAR;

	return 0;
}

ConstProcessorRcPtr *OCIO_configGetProcessor(ConstConfigRcPtr *, ConstTransformRcPtr *tfm)
{
	return (ConstProcessorRcPtr*)tfm;
}

void OCIO_processorApply(ConstProcessorRcPtr *processor, PackedImageDesc *img)
{
	/* OCIO_TODO stride not respected, channels must be 3 or 4 */
	PackedImageDescription *desc = (PackedImageDescription*)img;
	int channels = desc->numChannels;
	float *pixels = desc->data;
	int width = desc->width;
	int height = desc->height;
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			float *pixel = pixels + channels * (y * width + x);

			if (channels == 4)
				OCIO_processorApplyRGBA(processor, pixel);
			else if (channels == 3)
				OCIO_processorApplyRGB(processor, pixel);
		}
	}
}

void OCIO_processorApply_predivide(ConstProcessorRcPtr *processor, PackedImageDesc *img)
{
	/* OCIO_TODO stride not respected, channels must be 3 or 4 */
	PackedImageDescription *desc = (PackedImageDescription*)img;
	int channels = desc->numChannels;
	float *pixels = desc->data;
	int width = desc->width;
	int height = desc->height;
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			float *pixel = pixels + channels * (y * width + x);

			if (channels == 4)
				OCIO_processorApplyRGBA_predivide(processor, pixel);
			else if (channels == 3)
				OCIO_processorApplyRGB(processor, pixel);
		}
	}
}

void OCIO_processorApplyRGB(ConstProcessorRcPtr *processor, float *pixel)
{
	if (processor == PROCESSOR_LINEAR_TO_SRGB)
		linearrgb_to_srgb_v3_v3(pixel, pixel);
	else if (processor == PROCESSOR_SRGB_TO_LINEAR)
		srgb_to_linearrgb_v3_v3(pixel, pixel);
}

void OCIO_processorApplyRGBA(ConstProcessorRcPtr *processor, float *pixel)
{
	if (processor == PROCESSOR_LINEAR_TO_SRGB)
		linearrgb_to_srgb_v4(pixel, pixel);
	else if (processor == PROCESSOR_SRGB_TO_LINEAR)
		srgb_to_linearrgb_v4(pixel, pixel);
}

void OCIO_processorApplyRGBA_predivide(ConstProcessorRcPtr *processor, float *pixel)
{
	if (pixel[3] == 1.0f || pixel[3] == 0.0f) {
		OCIO_processorApplyRGBA(processor, pixel);
	}
	else {
		float alpha, inv_alpha;

		alpha = pixel[3];
		inv_alpha = 1.0f / alpha;

		pixel[0] *= inv_alpha;
		pixel[1] *= inv_alpha;
		pixel[2] *= inv_alpha;

		OCIO_processorApplyRGBA(processor, pixel);

		pixel[0] *= alpha;
		pixel[1] *= alpha;
		pixel[2] *= alpha;
	}
}

void OCIO_processorRelease(ConstProcessorRcPtr *)
{
}

const char *OCIO_colorSpaceGetName(ConstColorSpaceRcPtr *cs)
{
	if (cs == COLORSPACE_LINEAR)
		return "Linear";
	else if (cs == COLORSPACE_SRGB)
		return "sRGB";
	
	return NULL;
}

const char *OCIO_colorSpaceGetDescription(ConstColorSpaceRcPtr *)
{
	return "";
}

const char *OCIO_colorSpaceGetFamily(ConstColorSpaceRcPtr *)
{
	return "";
}

DisplayTransformRcPtr *OCIO_createDisplayTransform(void)
{
	return (DisplayTransformRcPtr*)PROCESSOR_LINEAR_TO_SRGB;
}

void OCIO_displayTransformSetInputColorSpaceName(DisplayTransformRcPtr *, const char *)
{
}

void OCIO_displayTransformSetDisplay(DisplayTransformRcPtr *, const char *)
{
}

void OCIO_displayTransformSetView(DisplayTransformRcPtr *, const char *)
{
}

void OCIO_displayTransformSetDisplayCC(DisplayTransformRcPtr *, ConstTransformRcPtr *)
{
}

void OCIO_displayTransformSetLinearCC(DisplayTransformRcPtr *, ConstTransformRcPtr *)
{
}

void OCIO_displayTransformRelease(DisplayTransformRcPtr *)
{
}

PackedImageDesc *OCIO_createPackedImageDesc(float *data, long width, long height, long numChannels,
                                            long chanStrideBytes, long xStrideBytes, long yStrideBytes)
{
	PackedImageDescription *desc = (PackedImageDescription*)MEM_callocN(sizeof(PackedImageDescription), "PackedImageDescription");

	desc->data = data;
	desc->width = width;
	desc->height = height;
	desc->numChannels = numChannels;
	desc->chanStrideBytes = chanStrideBytes;
	desc->xStrideBytes = xStrideBytes;
	desc->yStrideBytes = yStrideBytes;

	return (PackedImageDesc*)desc;
}

void OCIO_packedImageDescRelease(PackedImageDesc* id)
{
	MEM_freeN(id);
}

ExponentTransformRcPtr *OCIO_createExponentTransform(void)
{
	return (ExponentTransformRcPtr*)PROCESSOR_UNKNOWN;
}

void OCIO_exponentTransformSetValue(ExponentTransformRcPtr *, const float *)
{
}

void OCIO_exponentTransformRelease(ExponentTransformRcPtr *)
{
}

MatrixTransformRcPtr *OCIO_createMatrixTransform(void)
{
	return (MatrixTransformRcPtr*)PROCESSOR_UNKNOWN;
}

void OCIO_matrixTransformSetValue(MatrixTransformRcPtr *, const float *, const float *)
{
}

void OCIO_matrixTransformRelease(MatrixTransformRcPtr *)
{
}

void OCIO_matrixTransformScale(float * , float * , const float *)
{
}


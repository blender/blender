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
 *                 Lukas Toene
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OCIO_CAPI_H__
#define __OCIO_CAPI_H__



#ifdef __cplusplus
using namespace OCIO_NAMESPACE;
extern "C" {
#endif

#define OCIO_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name


#ifndef OCIO_CAPI_IMPLEMENTATION
	#define OCIO_ROLE_SCENE_LINEAR	"scene_linear"
	#define OCIO_ROLE_COLOR_PICKING	"color_picking"
	#define OCIO_ROLE_TEXTURE_PAINT	"texture_paint"

	OCIO_DECLARE_HANDLE(ConstConfigRcPtr);
	OCIO_DECLARE_HANDLE(ConstColorSpaceRcPtr);
	OCIO_DECLARE_HANDLE(ConstProcessorRcPtr);
	OCIO_DECLARE_HANDLE(ConstContextRcPtr);
	OCIO_DECLARE_HANDLE(PackedImageDesc);
	OCIO_DECLARE_HANDLE(DisplayTransformRcPtr);
	OCIO_DECLARE_HANDLE(ConstTransformRcPtr);
	OCIO_DECLARE_HANDLE(ExponentTransformRcPtr);
	OCIO_DECLARE_HANDLE(MatrixTransformRcPtr);
#endif


ConstConfigRcPtr *OCIO_getCurrentConfig(void);
void OCIO_setCurrentConfig(const ConstConfigRcPtr *config);

ConstConfigRcPtr *OCIO_configCreateFromEnv(void);
ConstConfigRcPtr *OCIO_configCreateFromFile(const char* filename);

void OCIO_configRelease(ConstConfigRcPtr *config);

int OCIO_configGetNumColorSpaces(ConstConfigRcPtr *config);
const char *OCIO_configGetColorSpaceNameByIndex(ConstConfigRcPtr *config, int index);
ConstColorSpaceRcPtr *OCIO_configGetColorSpace(ConstConfigRcPtr *config, const char *name);
int OCIO_configGetIndexForColorSpace(ConstConfigRcPtr *config, const char *name);

void OCIO_colorSpaceRelease(ConstColorSpaceRcPtr *cs);

const char *OCIO_configGetDefaultDisplay(ConstConfigRcPtr *config);
int         OCIO_configGetNumDisplays(ConstConfigRcPtr *config);
const char *OCIO_configGetDisplay(ConstConfigRcPtr *config, int index);
const char *OCIO_configGetDefaultView(ConstConfigRcPtr *config, const char *display);
int         OCIO_configGetNumViews(ConstConfigRcPtr *config, const char *display);
const char *OCIO_configGetView(ConstConfigRcPtr *config, const char *display, int index);
const char *OCIO_configGetDisplayColorSpaceName(ConstConfigRcPtr *config, const char *display, const char *view);

ConstProcessorRcPtr *OCIO_configGetProcessorWithNames(ConstConfigRcPtr *config, const char *srcName, const char *dstName);
ConstProcessorRcPtr *OCIO_configGetProcessor(ConstConfigRcPtr *config, ConstTransformRcPtr *transform);

void OCIO_processorApply(ConstProcessorRcPtr *processor, PackedImageDesc *img);
void OCIO_processorApplyRGB(ConstProcessorRcPtr *processor, float *pixel);
void OCIO_processorApplyRGBA(ConstProcessorRcPtr *processor, float *pixel);

void OCIO_processorRelease(ConstProcessorRcPtr *p);


const char *OCIO_colorSpaceGetName(ConstColorSpaceRcPtr *cs);
const char *OCIO_colorSpaceGetFamily(ConstColorSpaceRcPtr *cs);

DisplayTransformRcPtr *OCIO_createDisplayTransform(void);
void OCIO_displayTransformSetInputColorSpaceName(DisplayTransformRcPtr *dt, const char *name);
void OCIO_displayTransformSetDisplay(DisplayTransformRcPtr *dt, const char *name);
void OCIO_displayTransformSetView(DisplayTransformRcPtr *dt, const char *name);
void OCIO_displayTransformSetDisplayCC(DisplayTransformRcPtr *dt, ConstTransformRcPtr *et);
void OCIO_displayTransformSetLinearCC(DisplayTransformRcPtr *dt, ConstTransformRcPtr *et);
void OCIO_displayTransformRelease(DisplayTransformRcPtr *dt);

PackedImageDesc *OCIO_createPackedImageDesc(float *data, long width, long height, long numChannels,
											long chanStrideBytes, long xStrideBytes, long yStrideBytes);

void OCIO_packedImageDescRelease(PackedImageDesc *p);

ExponentTransformRcPtr *OCIO_createExponentTransform(void);
void OCIO_exponentTransformSetValue(ExponentTransformRcPtr *et, const float *exponent);
void OCIO_exponentTransformRelease(ExponentTransformRcPtr *et);

MatrixTransformRcPtr *OCIO_createMatrixTransform(void);
void OCIO_matrixTransformSetValue(MatrixTransformRcPtr *et, const float *m44, const float *offset4);
void OCIO_matrixTransformRelease(MatrixTransformRcPtr *mt);

void OCIO_matrixTransformScale(float * m44, float * offset4, const float * scale4);

#ifdef __cplusplus
}
#endif

#endif //OCIO_CAPI_H

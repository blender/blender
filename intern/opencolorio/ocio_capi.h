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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef OCIO_CAPI_H
#define OCIO_CAPI_H



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


extern ConstConfigRcPtr* OCIO_getCurrentConfig(void);
extern void OCIO_setCurrentConfig(const ConstConfigRcPtr* config);

extern ConstConfigRcPtr* OCIO_configCreateFromEnv(void);
extern ConstConfigRcPtr* OCIO_configCreateFromFile(const char* filename);

extern void OCIO_configRelease(ConstConfigRcPtr* config);

extern int OCIO_configGetNumColorSpaces(ConstConfigRcPtr* config);
extern const char* OCIO_configGetColorSpaceNameByIndex(ConstConfigRcPtr* config, int index);
extern ConstColorSpaceRcPtr* OCIO_configGetColorSpace(ConstConfigRcPtr* config, const char* name);
extern int OCIO_configGetIndexForColorSpace(ConstConfigRcPtr* config, const char* name);

extern void OCIO_colorSpaceRelease(ConstColorSpaceRcPtr* cs);

extern const char* OCIO_configGetDefaultDisplay(ConstConfigRcPtr* config);
extern int         OCIO_configGetNumDisplays(ConstConfigRcPtr* config);
extern const char* OCIO_configGetDisplay(ConstConfigRcPtr* config, int index);
extern const char* OCIO_configGetDefaultView(ConstConfigRcPtr* config, const char* display);
extern int         OCIO_configGetNumViews(ConstConfigRcPtr* config, const char* display);
extern const char* OCIO_configGetView(ConstConfigRcPtr* config, const char* display, int index);
extern const char* OCIO_configGetDisplayColorSpaceName(ConstConfigRcPtr* config, const char* display, const char* view);

extern ConstProcessorRcPtr* OCIO_configGetProcessorWithNames(ConstConfigRcPtr* config, const char* srcName, const char* dstName);
extern ConstProcessorRcPtr* OCIO_configGetProcessor(ConstConfigRcPtr* config, ConstTransformRcPtr* transform);

extern void OCIO_processorApply(ConstProcessorRcPtr* processor, PackedImageDesc* img);
extern void OCIO_processorApplyRGB(ConstProcessorRcPtr* processor, float* pixel);
extern void OCIO_processorApplyRGBA(ConstProcessorRcPtr* processor, float* pixel);

extern void OCIO_processorRelease(ConstProcessorRcPtr* p);


extern const char* OCIO_colorSpaceGetName(ConstColorSpaceRcPtr* cs);
extern const char* OCIO_colorSpaceGetFamily(ConstColorSpaceRcPtr* cs);

extern DisplayTransformRcPtr* OCIO_createDisplayTransform(void);
extern void OCIO_displayTransformSetInputColorSpaceName(DisplayTransformRcPtr* dt, const char * name);
extern void OCIO_displayTransformSetDisplay(DisplayTransformRcPtr* dt, const char * name);
extern void OCIO_displayTransformSetView(DisplayTransformRcPtr* dt, const char * name);
extern void OCIO_displayTransformSetDisplayCC(DisplayTransformRcPtr *dt, ConstTransformRcPtr *et);
extern void OCIO_displayTransformSetLinearCC(DisplayTransformRcPtr *dt, ConstTransformRcPtr *et);
extern void OCIO_displayTransformRelease(DisplayTransformRcPtr* dt);

PackedImageDesc* OCIO_createPackedImageDesc(float * data, long width, long height, long numChannels,
											long chanStrideBytes, long xStrideBytes, long yStrideBytes);

extern void OCIO_packedImageDescRelease(PackedImageDesc* p);

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

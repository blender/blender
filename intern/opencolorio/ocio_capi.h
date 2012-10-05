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
extern "C" {
#endif

#define OCIO_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name

#define OCIO_ROLE_SCENE_LINEAR       "scene_linear"
#define OCIO_ROLE_COLOR_PICKING      "color_picking"
#define OCIO_ROLE_TEXTURE_PAINT      "texture_paint"
#define OCIO_ROLE_DEFAULT_BYTE       "default_byte"
#define OCIO_ROLE_DEFAULT_FLOAT      "default_float"
#define OCIO_ROLE_DEFAULT_SEQUENCER  "default_sequencer"

OCIO_DECLARE_HANDLE(OCIO_ConstConfigRcPtr);
OCIO_DECLARE_HANDLE(OCIO_ConstColorSpaceRcPtr);
OCIO_DECLARE_HANDLE(OCIO_ConstProcessorRcPtr);
OCIO_DECLARE_HANDLE(OCIO_ConstContextRcPtr);
OCIO_DECLARE_HANDLE(OCIO_PackedImageDesc);
OCIO_DECLARE_HANDLE(OCIO_DisplayTransformRcPtr);
OCIO_DECLARE_HANDLE(OCIO_ConstTransformRcPtr);
OCIO_DECLARE_HANDLE(OCIO_ExponentTransformRcPtr);
OCIO_DECLARE_HANDLE(OCIO_MatrixTransformRcPtr);

void OCIO_init(void);
void OCIO_exit(void);

OCIO_ConstConfigRcPtr *OCIO_getCurrentConfig(void);
void OCIO_setCurrentConfig(const OCIO_ConstConfigRcPtr *config);

OCIO_ConstConfigRcPtr *OCIO_configCreateFromEnv(void);
OCIO_ConstConfigRcPtr *OCIO_configCreateFromFile(const char* filename);
OCIO_ConstConfigRcPtr *OCIO_configCreateFallback(void);

void OCIO_configRelease(OCIO_ConstConfigRcPtr *config);

int OCIO_configGetNumColorSpaces(OCIO_ConstConfigRcPtr *config);
const char *OCIO_configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr *config, int index);
OCIO_ConstColorSpaceRcPtr *OCIO_configGetColorSpace(OCIO_ConstConfigRcPtr *config, const char *name);
int OCIO_configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name);

int OCIO_colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr *cs);
int OCIO_colorSpaceIsData(OCIO_ConstColorSpaceRcPtr *cs);

void OCIO_colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs);

const char *OCIO_configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config);
int         OCIO_configGetNumDisplays(OCIO_ConstConfigRcPtr *config);
const char *OCIO_configGetDisplay(OCIO_ConstConfigRcPtr *config, int index);
const char *OCIO_configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display);
int         OCIO_configGetNumViews(OCIO_ConstConfigRcPtr *config, const char *display);
const char *OCIO_configGetView(OCIO_ConstConfigRcPtr *config, const char *display, int index);
const char *OCIO_configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr *config, const char *display, const char *view);

OCIO_ConstProcessorRcPtr *OCIO_configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config, const char *srcName, const char *dstName);
OCIO_ConstProcessorRcPtr *OCIO_configGetProcessor(OCIO_ConstConfigRcPtr *config, OCIO_ConstTransformRcPtr *transform);

void OCIO_processorApply(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img);
void OCIO_processorApply_predivide(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img);
void OCIO_processorApplyRGB(OCIO_ConstProcessorRcPtr *processor, float *pixel);
void OCIO_processorApplyRGBA(OCIO_ConstProcessorRcPtr *processor, float *pixel);
void OCIO_processorApplyRGBA_predivide(OCIO_ConstProcessorRcPtr *processor, float *pixel);

void OCIO_processorRelease(OCIO_ConstProcessorRcPtr *p);

const char *OCIO_colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs);
const char *OCIO_colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr *cs);
const char *OCIO_colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr *cs);

OCIO_DisplayTransformRcPtr *OCIO_createDisplayTransform(void);
void OCIO_displayTransformSetInputColorSpaceName(OCIO_DisplayTransformRcPtr *dt, const char *name);
void OCIO_displayTransformSetDisplay(OCIO_DisplayTransformRcPtr *dt, const char *name);
void OCIO_displayTransformSetView(OCIO_DisplayTransformRcPtr *dt, const char *name);
void OCIO_displayTransformSetDisplayCC(OCIO_DisplayTransformRcPtr *dt, OCIO_ConstTransformRcPtr *et);
void OCIO_displayTransformSetLinearCC(OCIO_DisplayTransformRcPtr *dt, OCIO_ConstTransformRcPtr *et);
void OCIO_displayTransformRelease(OCIO_DisplayTransformRcPtr *dt);

OCIO_PackedImageDesc *OCIO_createOCIO_PackedImageDesc(float *data, long width, long height, long numChannels,
                                            long chanStrideBytes, long xStrideBytes, long yStrideBytes);

void OCIO_OCIO_PackedImageDescRelease(OCIO_PackedImageDesc *p);

OCIO_ExponentTransformRcPtr *OCIO_createExponentTransform(void);
void OCIO_exponentTransformSetValue(OCIO_ExponentTransformRcPtr *et, const float *exponent);
void OCIO_exponentTransformRelease(OCIO_ExponentTransformRcPtr *et);

OCIO_MatrixTransformRcPtr *OCIO_createMatrixTransform(void);
void OCIO_matrixTransformSetValue(OCIO_MatrixTransformRcPtr *et, const float *m44, const float *offset4);
void OCIO_matrixTransformRelease(OCIO_MatrixTransformRcPtr *mt);

void OCIO_matrixTransformScale(float * m44, float * offset4, const float * scale4);

#ifdef __cplusplus
}
#endif

#endif /* OCIO_CAPI_H */

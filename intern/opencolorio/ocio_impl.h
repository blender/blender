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
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __OCIO_IMPL_H__
#define __OCIO_IMPL_H__

#include "ocio_capi.h"

class IOCIOImpl {
public:
	virtual ~IOCIOImpl() {}

	virtual OCIO_ConstConfigRcPtr *getCurrentConfig(void) = 0;
	virtual void setCurrentConfig(const OCIO_ConstConfigRcPtr *config) = 0;

	virtual OCIO_ConstConfigRcPtr *configCreateFromEnv(void) = 0;
	virtual OCIO_ConstConfigRcPtr *configCreateFromFile(const char* filename) = 0;

	virtual void configRelease(OCIO_ConstConfigRcPtr *config) = 0;

	virtual int configGetNumColorSpaces(OCIO_ConstConfigRcPtr *config) = 0;
	virtual const char *configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr *config, int index) = 0;
	virtual OCIO_ConstColorSpaceRcPtr *configGetColorSpace(OCIO_ConstConfigRcPtr *config, const char *name) = 0;
	virtual int configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name) = 0;

	virtual int colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr *cs) = 0;
	virtual int colorSpaceIsData(OCIO_ConstColorSpaceRcPtr *cs) = 0;

	virtual void colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs) = 0;

	virtual const char *configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config) = 0;
	virtual int         configGetNumDisplays(OCIO_ConstConfigRcPtr *config) = 0;
	virtual const char *configGetDisplay(OCIO_ConstConfigRcPtr *config, int index) = 0;
	virtual const char *configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display) = 0;
	virtual int         configGetNumViews(OCIO_ConstConfigRcPtr *config, const char *display) = 0;
	virtual const char *configGetView(OCIO_ConstConfigRcPtr *config, const char *display, int index) = 0;
	virtual const char *configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr *config, const char *display, const char *view) = 0;

	virtual int                  configGetNumLooks(OCIO_ConstConfigRcPtr *config) = 0;
	virtual const char          *configGetLookNameByIndex(OCIO_ConstConfigRcPtr *config, int index) = 0;
	virtual OCIO_ConstLookRcPtr *configGetLook(OCIO_ConstConfigRcPtr *config, const char *name) = 0;

	virtual const char *lookGetProcessSpace(OCIO_ConstLookRcPtr *look) = 0;
	virtual void        lookRelease(OCIO_ConstLookRcPtr *look) = 0;

	virtual OCIO_ConstProcessorRcPtr *configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config, const char *srcName, const char *dstName) = 0;
	virtual OCIO_ConstProcessorRcPtr *configGetProcessor(OCIO_ConstConfigRcPtr *config, OCIO_ConstTransformRcPtr *transform) = 0;

	virtual void processorApply(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img) = 0;
	virtual void processorApply_predivide(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img) = 0;
	virtual void processorApplyRGB(OCIO_ConstProcessorRcPtr *processor, float *pixel) = 0;
	virtual void processorApplyRGBA(OCIO_ConstProcessorRcPtr *processor, float *pixel) = 0;
	virtual void processorApplyRGBA_predivide(OCIO_ConstProcessorRcPtr *processor, float *pixel) = 0;

	virtual void processorRelease(OCIO_ConstProcessorRcPtr *p) = 0;

	virtual const char *colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs) = 0;
	virtual const char *colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr *cs) = 0;
	virtual const char *colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr *cs) = 0;

	virtual OCIO_DisplayTransformRcPtr *createDisplayTransform(void) = 0;
	virtual void displayTransformSetInputColorSpaceName(OCIO_DisplayTransformRcPtr *dt, const char *name) = 0;
	virtual void displayTransformSetDisplay(OCIO_DisplayTransformRcPtr *dt, const char *name) = 0;
	virtual void displayTransformSetView(OCIO_DisplayTransformRcPtr *dt, const char *name) = 0;
	virtual void displayTransformSetDisplayCC(OCIO_DisplayTransformRcPtr *dt, OCIO_ConstTransformRcPtr *et) = 0;
	virtual void displayTransformSetLinearCC(OCIO_DisplayTransformRcPtr *dt, OCIO_ConstTransformRcPtr *et) = 0;
	virtual void displayTransformSetLooksOverride(OCIO_DisplayTransformRcPtr *dt, const char *looks) = 0;
	virtual void displayTransformSetLooksOverrideEnabled(OCIO_DisplayTransformRcPtr *dt, bool enabled) = 0;
	virtual void displayTransformRelease(OCIO_DisplayTransformRcPtr *dt) = 0;

	virtual OCIO_PackedImageDesc *createOCIO_PackedImageDesc(float *data, long width, long height, long numChannels,
	                                               long chanStrideBytes, long xStrideBytes, long yStrideBytes) = 0;

	virtual void OCIO_PackedImageDescRelease(OCIO_PackedImageDesc *p) = 0;

	virtual OCIO_ExponentTransformRcPtr *createExponentTransform(void) = 0;
	virtual void exponentTransformSetValue(OCIO_ExponentTransformRcPtr *et, const float *exponent) = 0;
	virtual void exponentTransformRelease(OCIO_ExponentTransformRcPtr *et) = 0;

	virtual OCIO_MatrixTransformRcPtr *createMatrixTransform(void) = 0;
	virtual void matrixTransformSetValue(OCIO_MatrixTransformRcPtr *mt, const float *m44, const float *offset4) = 0;
	virtual void matrixTransformRelease(OCIO_MatrixTransformRcPtr *mt) = 0;

	virtual void matrixTransformScale(float * m44, float * offset4, const float * scale4) = 0;

	virtual bool supportGLSLDraw(void) = 0;
	virtual bool setupGLSLDraw(struct OCIO_GLSLDrawState **state_r, OCIO_ConstProcessorRcPtr *processor,
	                           OCIO_CurveMappingSettings *curve_mapping_settings, float dither, bool predivide) = 0;
	virtual void finishGLSLDraw(struct OCIO_GLSLDrawState *state) = 0;
	virtual void freeGLState(struct OCIO_GLSLDrawState *state_r) = 0;

	virtual const char *getVersionString(void) = 0;
	virtual int getVersionHex(void) = 0;
};

class FallbackImpl : public IOCIOImpl {
public:
	FallbackImpl() {}

	OCIO_ConstConfigRcPtr *getCurrentConfig(void);
	void setCurrentConfig(const OCIO_ConstConfigRcPtr *config);

	OCIO_ConstConfigRcPtr *configCreateFromEnv(void);
	OCIO_ConstConfigRcPtr *configCreateFromFile(const char* filename);

	void configRelease(OCIO_ConstConfigRcPtr *config);

	int configGetNumColorSpaces(OCIO_ConstConfigRcPtr *config);
	const char *configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr *config, int index);
	OCIO_ConstColorSpaceRcPtr *configGetColorSpace(OCIO_ConstConfigRcPtr *config, const char *name);
	int configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name);

	int colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr *cs);
	int colorSpaceIsData(OCIO_ConstColorSpaceRcPtr *cs);

	void colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs);

	const char *configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config);
	int         configGetNumDisplays(OCIO_ConstConfigRcPtr *config);
	const char *configGetDisplay(OCIO_ConstConfigRcPtr *config, int index);
	const char *configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display);
	int         configGetNumViews(OCIO_ConstConfigRcPtr *config, const char *display);
	const char *configGetView(OCIO_ConstConfigRcPtr *config, const char *display, int index);
	const char *configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr *config, const char *display, const char *view);

	int                  configGetNumLooks(OCIO_ConstConfigRcPtr *config);
	const char          *configGetLookNameByIndex(OCIO_ConstConfigRcPtr *config, int index);
	OCIO_ConstLookRcPtr *configGetLook(OCIO_ConstConfigRcPtr *config, const char *name);

	const char *lookGetProcessSpace(OCIO_ConstLookRcPtr *look);
	void        lookRelease(OCIO_ConstLookRcPtr *look);

	OCIO_ConstProcessorRcPtr *configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config, const char *srcName, const char *dstName);
	OCIO_ConstProcessorRcPtr *configGetProcessor(OCIO_ConstConfigRcPtr *config, OCIO_ConstTransformRcPtr *transform);

	void processorApply(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img);
	void processorApply_predivide(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img);
	void processorApplyRGB(OCIO_ConstProcessorRcPtr *processor, float *pixel);
	void processorApplyRGBA(OCIO_ConstProcessorRcPtr *processor, float *pixel);
	void processorApplyRGBA_predivide(OCIO_ConstProcessorRcPtr *processor, float *pixel);

	void processorRelease(OCIO_ConstProcessorRcPtr *p);

	const char *colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs);
	const char *colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr *cs);
	const char *colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr *cs);

	OCIO_DisplayTransformRcPtr *createDisplayTransform(void);
	void displayTransformSetInputColorSpaceName(OCIO_DisplayTransformRcPtr *dt, const char *name);
	void displayTransformSetDisplay(OCIO_DisplayTransformRcPtr *dt, const char *name);
	void displayTransformSetView(OCIO_DisplayTransformRcPtr *dt, const char *name);
	void displayTransformSetDisplayCC(OCIO_DisplayTransformRcPtr *dt, OCIO_ConstTransformRcPtr *et);
	void displayTransformSetLinearCC(OCIO_DisplayTransformRcPtr *dt, OCIO_ConstTransformRcPtr *et);
	void displayTransformSetLooksOverride(OCIO_DisplayTransformRcPtr *dt, const char *looks);
	void displayTransformSetLooksOverrideEnabled(OCIO_DisplayTransformRcPtr *dt, bool enabled);
	void displayTransformRelease(OCIO_DisplayTransformRcPtr *dt);

	OCIO_PackedImageDesc *createOCIO_PackedImageDesc(float *data, long width, long height, long numChannels,
	                                       long chanStrideBytes, long xStrideBytes, long yStrideBytes);

	void OCIO_PackedImageDescRelease(OCIO_PackedImageDesc *p);

	OCIO_ExponentTransformRcPtr *createExponentTransform(void);
	void exponentTransformSetValue(OCIO_ExponentTransformRcPtr *et, const float *exponent);
	void exponentTransformRelease(OCIO_ExponentTransformRcPtr *et);

	OCIO_MatrixTransformRcPtr *createMatrixTransform(void);
	void matrixTransformSetValue(OCIO_MatrixTransformRcPtr *mt, const float *m44, const float *offset4);
	void matrixTransformRelease(OCIO_MatrixTransformRcPtr *mt);

	void matrixTransformScale(float *m44, float *offset4, const float *scale4);

	bool supportGLSLDraw(void);
	bool setupGLSLDraw(struct OCIO_GLSLDrawState **state_r, OCIO_ConstProcessorRcPtr *processor,
	                   OCIO_CurveMappingSettings *curve_mapping_settings, float dither, bool predivide);
	void finishGLSLDraw(struct OCIO_GLSLDrawState *state);
	void freeGLState(struct OCIO_GLSLDrawState *state_r);

	const char *getVersionString(void);
	int getVersionHex(void);
};

#ifdef WITH_OCIO
class OCIOImpl : public IOCIOImpl {
public:
	OCIOImpl() {};

	OCIO_ConstConfigRcPtr *getCurrentConfig(void);
	void setCurrentConfig(const OCIO_ConstConfigRcPtr *config);

	OCIO_ConstConfigRcPtr *configCreateFromEnv(void);
	OCIO_ConstConfigRcPtr *configCreateFromFile(const char* filename);

	void configRelease(OCIO_ConstConfigRcPtr *config);

	int configGetNumColorSpaces(OCIO_ConstConfigRcPtr *config);
	const char *configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr *config, int index);
	OCIO_ConstColorSpaceRcPtr *configGetColorSpace(OCIO_ConstConfigRcPtr *config, const char *name);
	int configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name);

	int colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr *cs);
	int colorSpaceIsData(OCIO_ConstColorSpaceRcPtr *cs);

	void colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs);

	const char *configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config);
	int         configGetNumDisplays(OCIO_ConstConfigRcPtr *config);
	const char *configGetDisplay(OCIO_ConstConfigRcPtr *config, int index);
	const char *configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display);
	int         configGetNumViews(OCIO_ConstConfigRcPtr *config, const char *display);
	const char *configGetView(OCIO_ConstConfigRcPtr *config, const char *display, int index);
	const char *configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr *config, const char *display, const char *view);

	int                  configGetNumLooks(OCIO_ConstConfigRcPtr *config);
	const char          *configGetLookNameByIndex(OCIO_ConstConfigRcPtr *config, int index);
	OCIO_ConstLookRcPtr *configGetLook(OCIO_ConstConfigRcPtr *config, const char *name);

	const char *lookGetProcessSpace(OCIO_ConstLookRcPtr *look);
	void        lookRelease(OCIO_ConstLookRcPtr *look);

	OCIO_ConstProcessorRcPtr *configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config, const char *srcName, const char *dstName);
	OCIO_ConstProcessorRcPtr *configGetProcessor(OCIO_ConstConfigRcPtr *config, OCIO_ConstTransformRcPtr *transform);

	void processorApply(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img);
	void processorApply_predivide(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img);
	void processorApplyRGB(OCIO_ConstProcessorRcPtr *processor, float *pixel);
	void processorApplyRGBA(OCIO_ConstProcessorRcPtr *processor, float *pixel);
	void processorApplyRGBA_predivide(OCIO_ConstProcessorRcPtr *processor, float *pixel);

	void processorRelease(OCIO_ConstProcessorRcPtr *p);

	const char *colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs);
	const char *colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr *cs);
	const char *colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr *cs);

	OCIO_DisplayTransformRcPtr *createDisplayTransform(void);
	void displayTransformSetInputColorSpaceName(OCIO_DisplayTransformRcPtr *dt, const char *name);
	void displayTransformSetDisplay(OCIO_DisplayTransformRcPtr *dt, const char *name);
	void displayTransformSetView(OCIO_DisplayTransformRcPtr *dt, const char *name);
	void displayTransformSetDisplayCC(OCIO_DisplayTransformRcPtr *dt, OCIO_ConstTransformRcPtr *et);
	void displayTransformSetLinearCC(OCIO_DisplayTransformRcPtr *dt, OCIO_ConstTransformRcPtr *et);
	void displayTransformSetLooksOverride(OCIO_DisplayTransformRcPtr *dt, const char *looks);
	void displayTransformSetLooksOverrideEnabled(OCIO_DisplayTransformRcPtr *dt, bool enabled);
	void displayTransformRelease(OCIO_DisplayTransformRcPtr *dt);

	OCIO_PackedImageDesc *createOCIO_PackedImageDesc(float *data, long width, long height, long numChannels,
	                                       long chanStrideBytes, long xStrideBytes, long yStrideBytes);

	void OCIO_PackedImageDescRelease(OCIO_PackedImageDesc *p);

	OCIO_ExponentTransformRcPtr *createExponentTransform(void);
	void exponentTransformSetValue(OCIO_ExponentTransformRcPtr *et, const float *exponent);
	void exponentTransformRelease(OCIO_ExponentTransformRcPtr *et);

	OCIO_MatrixTransformRcPtr *createMatrixTransform(void);
	void matrixTransformSetValue(OCIO_MatrixTransformRcPtr *mt, const float *m44, const float *offset4);
	void matrixTransformRelease(OCIO_MatrixTransformRcPtr *mt);

	void matrixTransformScale(float * m44, float * offset4, const float * scale4);

	bool supportGLSLDraw(void);
	bool setupGLSLDraw(struct OCIO_GLSLDrawState **state_r, OCIO_ConstProcessorRcPtr *processor,
	                   OCIO_CurveMappingSettings *curve_mapping_settings, float dither, bool predivide);
	void finishGLSLDraw(struct OCIO_GLSLDrawState *state);
	void freeGLState(struct OCIO_GLSLDrawState *state_r);

	const char *getVersionString(void);
	int getVersionHex(void);
};
#endif

#endif /* OCIO_IMPL_H */

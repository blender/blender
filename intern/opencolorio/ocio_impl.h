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
	virtual ~IOCIOImpl() {};

	virtual ConstConfigRcPtr *getCurrentConfig(void) = 0;
	virtual void setCurrentConfig(const ConstConfigRcPtr *config) = 0;

	virtual ConstConfigRcPtr *configCreateFromEnv(void) = 0;
	virtual ConstConfigRcPtr *configCreateFromFile(const char* filename) = 0;

	virtual void configRelease(ConstConfigRcPtr *config) = 0;

	virtual int configGetNumColorSpaces(ConstConfigRcPtr *config) = 0;
	virtual const char *configGetColorSpaceNameByIndex(ConstConfigRcPtr *config, int index) = 0;
	virtual ConstColorSpaceRcPtr *configGetColorSpace(ConstConfigRcPtr *config, const char *name) = 0;
	virtual int configGetIndexForColorSpace(ConstConfigRcPtr *config, const char *name) = 0;

	virtual int colorSpaceIsInvertible(ConstColorSpaceRcPtr *cs) = 0;
	virtual int colorSpaceIsData(ConstColorSpaceRcPtr *cs) = 0;

	virtual void colorSpaceRelease(ConstColorSpaceRcPtr *cs) = 0;

	virtual const char *configGetDefaultDisplay(ConstConfigRcPtr *config) = 0;
	virtual int         configGetNumDisplays(ConstConfigRcPtr *config) = 0;
	virtual const char *configGetDisplay(ConstConfigRcPtr *config, int index) = 0;
	virtual const char *configGetDefaultView(ConstConfigRcPtr *config, const char *display) = 0;
	virtual int         configGetNumViews(ConstConfigRcPtr *config, const char *display) = 0;
	virtual const char *configGetView(ConstConfigRcPtr *config, const char *display, int index) = 0;
	virtual const char *configGetDisplayColorSpaceName(ConstConfigRcPtr *config, const char *display, const char *view) = 0;

	virtual ConstProcessorRcPtr *configGetProcessorWithNames(ConstConfigRcPtr *config, const char *srcName, const char *dstName) = 0;
	virtual ConstProcessorRcPtr *configGetProcessor(ConstConfigRcPtr *config, ConstTransformRcPtr *transform) = 0;

	virtual void processorApply(ConstProcessorRcPtr *processor, PackedImageDesc *img) = 0;
	virtual void processorApply_predivide(ConstProcessorRcPtr *processor, PackedImageDesc *img) = 0;
	virtual void processorApplyRGB(ConstProcessorRcPtr *processor, float *pixel) = 0;
	virtual void processorApplyRGBA(ConstProcessorRcPtr *processor, float *pixel) = 0;
	virtual void processorApplyRGBA_predivide(ConstProcessorRcPtr *processor, float *pixel) = 0;

	virtual void processorRelease(ConstProcessorRcPtr *p) = 0;

	virtual const char *colorSpaceGetName(ConstColorSpaceRcPtr *cs) = 0;
	virtual const char *colorSpaceGetDescription(ConstColorSpaceRcPtr *cs) = 0;
	virtual const char *colorSpaceGetFamily(ConstColorSpaceRcPtr *cs) = 0;

	virtual DisplayTransformRcPtr *createDisplayTransform(void) = 0;
	virtual void displayTransformSetInputColorSpaceName(DisplayTransformRcPtr *dt, const char *name) = 0;
	virtual void displayTransformSetDisplay(DisplayTransformRcPtr *dt, const char *name) = 0;
	virtual void displayTransformSetView(DisplayTransformRcPtr *dt, const char *name) = 0;
	virtual void displayTransformSetDisplayCC(DisplayTransformRcPtr *dt, ConstTransformRcPtr *et) = 0;
	virtual void displayTransformSetLinearCC(DisplayTransformRcPtr *dt, ConstTransformRcPtr *et) = 0;
	virtual void displayTransformRelease(DisplayTransformRcPtr *dt) = 0;

	virtual PackedImageDesc *createPackedImageDesc(float *data, long width, long height, long numChannels,
	                                               long chanStrideBytes, long xStrideBytes, long yStrideBytes) = 0;

	virtual void packedImageDescRelease(PackedImageDesc *p) = 0;

	virtual ExponentTransformRcPtr *createExponentTransform(void) = 0;
	virtual void exponentTransformSetValue(ExponentTransformRcPtr *et, const float *exponent) = 0;
	virtual void exponentTransformRelease(ExponentTransformRcPtr *et) = 0;

	virtual MatrixTransformRcPtr *createMatrixTransform(void) = 0;
	virtual void matrixTransformSetValue(MatrixTransformRcPtr *et, const float *m44, const float *offset4) = 0;
	virtual void matrixTransformRelease(MatrixTransformRcPtr *mt) = 0;

	virtual void matrixTransformScale(float * m44, float * offset4, const float * scale4) = 0;
};

class FallbackImpl : public IOCIOImpl {
public:
	FallbackImpl() {};

	ConstConfigRcPtr *getCurrentConfig(void);
	void setCurrentConfig(const ConstConfigRcPtr *config);

	ConstConfigRcPtr *configCreateFromEnv(void);
	ConstConfigRcPtr *configCreateFromFile(const char* filename);

	void configRelease(ConstConfigRcPtr *config);

	int configGetNumColorSpaces(ConstConfigRcPtr *config);
	const char *configGetColorSpaceNameByIndex(ConstConfigRcPtr *config, int index);
	ConstColorSpaceRcPtr *configGetColorSpace(ConstConfigRcPtr *config, const char *name);
	int configGetIndexForColorSpace(ConstConfigRcPtr *config, const char *name);

	int colorSpaceIsInvertible(ConstColorSpaceRcPtr *cs);
	int colorSpaceIsData(ConstColorSpaceRcPtr *cs);

	void colorSpaceRelease(ConstColorSpaceRcPtr *cs);

	const char *configGetDefaultDisplay(ConstConfigRcPtr *config);
	int         configGetNumDisplays(ConstConfigRcPtr *config);
	const char *configGetDisplay(ConstConfigRcPtr *config, int index);
	const char *configGetDefaultView(ConstConfigRcPtr *config, const char *display);
	int         configGetNumViews(ConstConfigRcPtr *config, const char *display);
	const char *configGetView(ConstConfigRcPtr *config, const char *display, int index);
	const char *configGetDisplayColorSpaceName(ConstConfigRcPtr *config, const char *display, const char *view);

	ConstProcessorRcPtr *configGetProcessorWithNames(ConstConfigRcPtr *config, const char *srcName, const char *dstName);
	ConstProcessorRcPtr *configGetProcessor(ConstConfigRcPtr *config, ConstTransformRcPtr *transform);

	void processorApply(ConstProcessorRcPtr *processor, PackedImageDesc *img);
	void processorApply_predivide(ConstProcessorRcPtr *processor, PackedImageDesc *img);
	void processorApplyRGB(ConstProcessorRcPtr *processor, float *pixel);
	void processorApplyRGBA(ConstProcessorRcPtr *processor, float *pixel);
	void processorApplyRGBA_predivide(ConstProcessorRcPtr *processor, float *pixel);

	void processorRelease(ConstProcessorRcPtr *p);

	const char *colorSpaceGetName(ConstColorSpaceRcPtr *cs);
	const char *colorSpaceGetDescription(ConstColorSpaceRcPtr *cs);
	const char *colorSpaceGetFamily(ConstColorSpaceRcPtr *cs);

	DisplayTransformRcPtr *createDisplayTransform(void);
	void displayTransformSetInputColorSpaceName(DisplayTransformRcPtr *dt, const char *name);
	void displayTransformSetDisplay(DisplayTransformRcPtr *dt, const char *name);
	void displayTransformSetView(DisplayTransformRcPtr *dt, const char *name);
	void displayTransformSetDisplayCC(DisplayTransformRcPtr *dt, ConstTransformRcPtr *et);
	void displayTransformSetLinearCC(DisplayTransformRcPtr *dt, ConstTransformRcPtr *et);
	void displayTransformRelease(DisplayTransformRcPtr *dt);

	PackedImageDesc *createPackedImageDesc(float *data, long width, long height, long numChannels,
	                                       long chanStrideBytes, long xStrideBytes, long yStrideBytes);

	void packedImageDescRelease(PackedImageDesc *p);

	ExponentTransformRcPtr *createExponentTransform(void);
	void exponentTransformSetValue(ExponentTransformRcPtr *et, const float *exponent);
	void exponentTransformRelease(ExponentTransformRcPtr *et);

	MatrixTransformRcPtr *createMatrixTransform(void);
	void matrixTransformSetValue(MatrixTransformRcPtr *et, const float *m44, const float *offset4);
	void matrixTransformRelease(MatrixTransformRcPtr *mt);

	void matrixTransformScale(float * m44, float * offset4, const float * scale4);
};

#ifdef WITH_OCIO
class OCIOImpl : public IOCIOImpl {
public:
	OCIOImpl() {};

	ConstConfigRcPtr *getCurrentConfig(void);
	void setCurrentConfig(const ConstConfigRcPtr *config);

	ConstConfigRcPtr *configCreateFromEnv(void);
	ConstConfigRcPtr *configCreateFromFile(const char* filename);

	void configRelease(ConstConfigRcPtr *config);

	int configGetNumColorSpaces(ConstConfigRcPtr *config);
	const char *configGetColorSpaceNameByIndex(ConstConfigRcPtr *config, int index);
	ConstColorSpaceRcPtr *configGetColorSpace(ConstConfigRcPtr *config, const char *name);
	int configGetIndexForColorSpace(ConstConfigRcPtr *config, const char *name);

	int colorSpaceIsInvertible(ConstColorSpaceRcPtr *cs);
	int colorSpaceIsData(ConstColorSpaceRcPtr *cs);

	void colorSpaceRelease(ConstColorSpaceRcPtr *cs);

	const char *configGetDefaultDisplay(ConstConfigRcPtr *config);
	int         configGetNumDisplays(ConstConfigRcPtr *config);
	const char *configGetDisplay(ConstConfigRcPtr *config, int index);
	const char *configGetDefaultView(ConstConfigRcPtr *config, const char *display);
	int         configGetNumViews(ConstConfigRcPtr *config, const char *display);
	const char *configGetView(ConstConfigRcPtr *config, const char *display, int index);
	const char *configGetDisplayColorSpaceName(ConstConfigRcPtr *config, const char *display, const char *view);

	ConstProcessorRcPtr *configGetProcessorWithNames(ConstConfigRcPtr *config, const char *srcName, const char *dstName);
	ConstProcessorRcPtr *configGetProcessor(ConstConfigRcPtr *config, ConstTransformRcPtr *transform);

	void processorApply(ConstProcessorRcPtr *processor, PackedImageDesc *img);
	void processorApply_predivide(ConstProcessorRcPtr *processor, PackedImageDesc *img);
	void processorApplyRGB(ConstProcessorRcPtr *processor, float *pixel);
	void processorApplyRGBA(ConstProcessorRcPtr *processor, float *pixel);
	void processorApplyRGBA_predivide(ConstProcessorRcPtr *processor, float *pixel);

	void processorRelease(ConstProcessorRcPtr *p);

	const char *colorSpaceGetName(ConstColorSpaceRcPtr *cs);
	const char *colorSpaceGetDescription(ConstColorSpaceRcPtr *cs);
	const char *colorSpaceGetFamily(ConstColorSpaceRcPtr *cs);

	DisplayTransformRcPtr *createDisplayTransform(void);
	void displayTransformSetInputColorSpaceName(DisplayTransformRcPtr *dt, const char *name);
	void displayTransformSetDisplay(DisplayTransformRcPtr *dt, const char *name);
	void displayTransformSetView(DisplayTransformRcPtr *dt, const char *name);
	void displayTransformSetDisplayCC(DisplayTransformRcPtr *dt, ConstTransformRcPtr *et);
	void displayTransformSetLinearCC(DisplayTransformRcPtr *dt, ConstTransformRcPtr *et);
	void displayTransformRelease(DisplayTransformRcPtr *dt);

	PackedImageDesc *createPackedImageDesc(float *data, long width, long height, long numChannels,
	                                       long chanStrideBytes, long xStrideBytes, long yStrideBytes);

	void packedImageDescRelease(PackedImageDesc *p);

	ExponentTransformRcPtr *createExponentTransform(void);
	void exponentTransformSetValue(ExponentTransformRcPtr *et, const float *exponent);
	void exponentTransformRelease(ExponentTransformRcPtr *et);

	MatrixTransformRcPtr *createMatrixTransform(void);
	void matrixTransformSetValue(MatrixTransformRcPtr *et, const float *m44, const float *offset4);
	void matrixTransformRelease(MatrixTransformRcPtr *mt);

	void matrixTransformScale(float * m44, float * offset4, const float * scale4);
};
#endif

#endif /* OCIO_IMPL_H */

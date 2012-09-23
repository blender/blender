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
#include <string.h>

#include <OpenColorIO/OpenColorIO.h>

#include "MEM_guardedalloc.h"

#define OCIO_CAPI_IMPLEMENTATION
#include "ocio_capi.h"

#ifdef NDEBUG
#  define OCIO_abort()
#else
#  include <stdlib.h>
#  define OCIO_abort() abort()
#endif

#if defined(_MSC_VER)
#  define __func__ __FUNCTION__
#endif

#define MEM_NEW(type) new(MEM_mallocN(sizeof(type), __func__)) type()
#define MEM_DELETE(what, type) if(what) { what->~type(); MEM_freeN(what); } (void)0

static void OCIO_reportError(const char *err)
{
	std::cerr << "OpenColorIO Error: " << err << std::endl;

	// OCIO_abort();
}

static void OCIO_reportException(Exception &exception)
{
	OCIO_reportError(exception.what());
}

ConstConfigRcPtr *OCIO_getCurrentConfig(void)
{
	ConstConfigRcPtr *config = MEM_NEW(ConstConfigRcPtr);

	try {
		*config = GetCurrentConfig();

		if(*config)
			return config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	MEM_DELETE(config, ConstConfigRcPtr);

	return NULL;
}

ConstConfigRcPtr *OCIO_getDefaultConfig(void)
{
	return NULL;
}

void OCIO_setCurrentConfig(const ConstConfigRcPtr *config)
{
	try {
		SetCurrentConfig(*config);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}
}

ConstConfigRcPtr *OCIO_configCreateFromEnv(void)
{
	ConstConfigRcPtr *config = MEM_NEW(ConstConfigRcPtr);

	try {
		*config = Config::CreateFromEnv();

		if (*config)
			return config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	MEM_DELETE(config, ConstConfigRcPtr);

	return NULL;
}


ConstConfigRcPtr *OCIO_configCreateFromFile(const char *filename)
{
	ConstConfigRcPtr *config = MEM_NEW(ConstConfigRcPtr);

	try {
		*config = Config::CreateFromFile(filename);

		if (*config)
			return config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	MEM_DELETE(config, ConstConfigRcPtr);

	return NULL;
}

void OCIO_configRelease(ConstConfigRcPtr *config)
{
	MEM_DELETE(config, ConstConfigRcPtr);
}

int OCIO_configGetNumColorSpaces(ConstConfigRcPtr *config)
{
	try {
		return (*config)->getNumColorSpaces();
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return 0;
}

const char *OCIO_configGetColorSpaceNameByIndex(ConstConfigRcPtr *config, int index)
{
	try {
		return (*config)->getColorSpaceNameByIndex(index);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

ConstColorSpaceRcPtr *OCIO_configGetColorSpace(ConstConfigRcPtr *config, const char *name)
{
	ConstColorSpaceRcPtr *cs = MEM_NEW(ConstColorSpaceRcPtr);

	try {
		*cs = (*config)->getColorSpace(name);

		if (*cs)
			return cs;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	MEM_DELETE(cs, ConstColorSpaceRcPtr);

	return NULL;
}

int OCIO_configGetIndexForColorSpace(ConstConfigRcPtr *config, const char *name)
{
	try {
		return (*config)->getIndexForColorSpace(name);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return -1;
}

const char *OCIO_configGetDefaultDisplay(ConstConfigRcPtr *config)
{
	try {
		return (*config)->getDefaultDisplay();
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

int OCIO_configGetNumDisplays(ConstConfigRcPtr* config)
{
	try {
		return (*config)->getNumDisplays();
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return 0;
}

const char *OCIO_configGetDisplay(ConstConfigRcPtr *config, int index)
{
	try {
		return (*config)->getDisplay(index);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

const char *OCIO_configGetDefaultView(ConstConfigRcPtr *config, const char *display)
{
	try {
		return (*config)->getDefaultView(display);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

int OCIO_configGetNumViews(ConstConfigRcPtr *config, const char *display)
{
	try {
		return (*config)->getNumViews(display);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return 0;
}

const char *OCIO_configGetView(ConstConfigRcPtr *config, const char *display, int index)
{
	try {
		return (*config)->getView(display, index);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

const char *OCIO_configGetDisplayColorSpaceName(ConstConfigRcPtr *config, const char *display, const char *view)
{
	try {
		return (*config)->getDisplayColorSpaceName(display, view);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

int OCIO_colorSpaceIsInvertible(ConstColorSpaceRcPtr *cs)
{
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

int OCIO_colorSpaceIsData(ConstColorSpaceRcPtr *cs)
{
	return ((*cs)->isData());
}

void OCIO_colorSpaceRelease(ConstColorSpaceRcPtr *cs)
{
	MEM_DELETE(cs, ConstColorSpaceRcPtr);
}

ConstProcessorRcPtr *OCIO_configGetProcessorWithNames(ConstConfigRcPtr *config, const char *srcName, const char *dstName)
{
	ConstProcessorRcPtr *p = MEM_NEW(ConstProcessorRcPtr);

	try {
		*p = (*config)->getProcessor(srcName, dstName);

		if (*p)
			return p;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	MEM_DELETE(p, ConstProcessorRcPtr);

	return 0;
}

ConstProcessorRcPtr *OCIO_configGetProcessor(ConstConfigRcPtr *config, ConstTransformRcPtr *transform)
{
	ConstProcessorRcPtr *p = MEM_NEW(ConstProcessorRcPtr);

	try {
		*p = (*config)->getProcessor(*transform);

		if (*p)
			return p;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	MEM_DELETE(p, ConstProcessorRcPtr);

	return NULL;
}

void OCIO_processorApply(ConstProcessorRcPtr *processor, PackedImageDesc *img)
{
	try {
		(*processor)->apply(*img);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}
}

void OCIO_processorApply_predivide(ConstProcessorRcPtr *processor, PackedImageDesc *img)
{
	try {
		int channels = img->getNumChannels();

		if (channels == 4) {
			float *pixels = img->getData();

			int width = img->getWidth();
			int height = img->getHeight();

			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
					float *pixel = pixels + 4 * (y * width + x);

					OCIO_processorApplyRGBA_predivide(processor, pixel);
				}
			}
		}
		else {
			(*processor)->apply(*img);
		}
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}
}

void OCIO_processorApplyRGB(ConstProcessorRcPtr *processor, float *pixel)
{
	(*processor)->applyRGB(pixel);
}

void OCIO_processorApplyRGBA(ConstProcessorRcPtr *processor, float *pixel)
{
	(*processor)->applyRGBA(pixel);
}

void OCIO_processorApplyRGBA_predivide(ConstProcessorRcPtr *processor, float *pixel)
{
	if (pixel[3] == 1.0f || pixel[3] == 0.0f) {
		(*processor)->applyRGBA(pixel);
	}
	else {
		float alpha, inv_alpha;

		alpha = pixel[3];
		inv_alpha = 1.0f / alpha;

		pixel[0] *= inv_alpha;
		pixel[1] *= inv_alpha;
		pixel[2] *= inv_alpha;

		(*processor)->applyRGBA(pixel);

		pixel[0] *= alpha;
		pixel[1] *= alpha;
		pixel[2] *= alpha;
	}
}

void OCIO_processorRelease(ConstProcessorRcPtr *p)
{
	p->~ConstProcessorRcPtr();
	MEM_freeN(p);
}

const char *OCIO_colorSpaceGetName(ConstColorSpaceRcPtr *cs)
{
	return (*cs)->getName();
}

const char *OCIO_colorSpaceGetDescription(ConstColorSpaceRcPtr *cs)
{
	return (*cs)->getDescription();
}

const char *OCIO_colorSpaceGetFamily(ConstColorSpaceRcPtr *cs)
{
	return (*cs)->getFamily();
}

DisplayTransformRcPtr *OCIO_createDisplayTransform(void)
{
	DisplayTransformRcPtr *dt = MEM_NEW(DisplayTransformRcPtr);

	*dt = DisplayTransform::Create();

	return dt;
}

void OCIO_displayTransformSetInputColorSpaceName(DisplayTransformRcPtr *dt, const char *name)
{
	(*dt)->setInputColorSpaceName(name);
}

void OCIO_displayTransformSetDisplay(DisplayTransformRcPtr *dt, const char *name)
{
	(*dt)->setDisplay(name);
}

void OCIO_displayTransformSetView(DisplayTransformRcPtr *dt, const char *name)
{
	(*dt)->setView(name);
}

void OCIO_displayTransformSetDisplayCC(DisplayTransformRcPtr *dt, ConstTransformRcPtr *t)
{
	(*dt)->setDisplayCC(*t);
}

void OCIO_displayTransformSetLinearCC(DisplayTransformRcPtr *dt, ConstTransformRcPtr *t)
{
	(*dt)->setLinearCC(*t);
}

void OCIO_displayTransformRelease(DisplayTransformRcPtr *dt)
{
	MEM_DELETE(dt, DisplayTransformRcPtr);
}

PackedImageDesc *OCIO_createPackedImageDesc(float *data, long width, long height, long numChannels,
                                            long chanStrideBytes, long xStrideBytes, long yStrideBytes)
{
	try {
		void *mem = MEM_mallocN(sizeof(PackedImageDesc), __func__);
		PackedImageDesc *id = new(mem) PackedImageDesc(data, width, height, numChannels, chanStrideBytes, xStrideBytes, yStrideBytes);

		return id;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

void OCIO_packedImageDescRelease(PackedImageDesc* id)
{
	MEM_DELETE(id, PackedImageDesc);
}

ExponentTransformRcPtr *OCIO_createExponentTransform(void)
{
	ExponentTransformRcPtr *et = MEM_NEW(ExponentTransformRcPtr);

	*et = ExponentTransform::Create();

	return et;
}

void OCIO_exponentTransformSetValue(ExponentTransformRcPtr *et, const float *exponent)
{
	(*et)->setValue(exponent);
}

void OCIO_exponentTransformRelease(ExponentTransformRcPtr *et)
{
	MEM_DELETE(et, ExponentTransformRcPtr);
}

MatrixTransformRcPtr *OCIO_createMatrixTransform(void)
{
	MatrixTransformRcPtr *mt = MEM_NEW(MatrixTransformRcPtr);

	*mt = MatrixTransform::Create();

	return mt;
}

void OCIO_matrixTransformSetValue(MatrixTransformRcPtr *mt, const float *m44, const float *offset4)
{
	(*mt)->setValue(m44, offset4);
}

void OCIO_matrixTransformRelease(MatrixTransformRcPtr *mt)
{
	MEM_DELETE(mt, MatrixTransformRcPtr);
}

void OCIO_matrixTransformScale(float * m44, float * offset4, const float *scale4f)
{
	MatrixTransform::Scale(m44, offset4, scale4f);
}

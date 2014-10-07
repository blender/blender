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
	ConstConfigRcPtr *config = OBJECT_GUARDED_NEW(ConstConfigRcPtr);

	try {
		*config = GetCurrentConfig();

		if (*config)
			return (OCIO_ConstConfigRcPtr *) config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	OBJECT_GUARDED_DELETE(config, ConstConfigRcPtr);

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
	ConstConfigRcPtr *config = OBJECT_GUARDED_NEW(ConstConfigRcPtr);

	try {
		*config = Config::CreateFromEnv();

		if (*config)
			return (OCIO_ConstConfigRcPtr *) config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	OBJECT_GUARDED_DELETE(config, ConstConfigRcPtr);

	return NULL;
}


OCIO_ConstConfigRcPtr *OCIOImpl::configCreateFromFile(const char *filename)
{
	ConstConfigRcPtr *config = OBJECT_GUARDED_NEW(ConstConfigRcPtr);

	try {
		*config = Config::CreateFromFile(filename);

		if (*config)
			return (OCIO_ConstConfigRcPtr *) config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	OBJECT_GUARDED_DELETE(config, ConstConfigRcPtr);

	return NULL;
}

void OCIOImpl::configRelease(OCIO_ConstConfigRcPtr *config)
{
	OBJECT_GUARDED_DELETE((ConstConfigRcPtr *) config, ConstConfigRcPtr);
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
	ConstColorSpaceRcPtr *cs = OBJECT_GUARDED_NEW(ConstColorSpaceRcPtr);

	try {
		*cs = (*(ConstConfigRcPtr *) config)->getColorSpace(name);

		if (*cs)
			return (OCIO_ConstColorSpaceRcPtr *) cs;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	OBJECT_GUARDED_DELETE(cs, ConstColorSpaceRcPtr);

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

int OCIOImpl::configGetNumLooks(OCIO_ConstConfigRcPtr *config)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getNumLooks();
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return 0;
}

const char *OCIOImpl::configGetLookNameByIndex(OCIO_ConstConfigRcPtr *config, int index)
{
	try {
		return (*(ConstConfigRcPtr *) config)->getLookNameByIndex(index);
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

OCIO_ConstLookRcPtr *OCIOImpl::configGetLook(OCIO_ConstConfigRcPtr *config, const char *name)
{
	ConstLookRcPtr *look = OBJECT_GUARDED_NEW(ConstLookRcPtr);

	try {
		*look = (*(ConstConfigRcPtr *) config)->getLook(name);

		if (*look)
			return (OCIO_ConstLookRcPtr *) look;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	OBJECT_GUARDED_DELETE(look, ConstLookRcPtr);

	return NULL;
}

const char *OCIOImpl::lookGetProcessSpace(OCIO_ConstLookRcPtr *look)
{
	return (*(ConstLookRcPtr *) look)->getProcessSpace();
}

void OCIOImpl::lookRelease(OCIO_ConstLookRcPtr *look)
{
	OBJECT_GUARDED_DELETE((ConstLookRcPtr *) look, ConstLookRcPtr);
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
	OBJECT_GUARDED_DELETE((ConstColorSpaceRcPtr *) cs, ConstColorSpaceRcPtr);
}

OCIO_ConstProcessorRcPtr *OCIOImpl::configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config, const char *srcName, const char *dstName)
{
	ConstProcessorRcPtr *p = OBJECT_GUARDED_NEW(ConstProcessorRcPtr);

	try {
		*p = (*(ConstConfigRcPtr *) config)->getProcessor(srcName, dstName);

		if (*p)
			return (OCIO_ConstProcessorRcPtr *) p;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	OBJECT_GUARDED_DELETE(p, ConstProcessorRcPtr);

	return 0;
}

OCIO_ConstProcessorRcPtr *OCIOImpl::configGetProcessor(OCIO_ConstConfigRcPtr *config, OCIO_ConstTransformRcPtr *transform)
{
	ConstProcessorRcPtr *p = OBJECT_GUARDED_NEW(ConstProcessorRcPtr);

	try {
		*p = (*(ConstConfigRcPtr *) config)->getProcessor(*(ConstTransformRcPtr *) transform);

		if (*p)
			return (OCIO_ConstProcessorRcPtr *) p;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	OBJECT_GUARDED_DELETE(p, ConstProcessorRcPtr);

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
	OBJECT_GUARDED_DELETE(p, ConstProcessorRcPtr);
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
	DisplayTransformRcPtr *dt = OBJECT_GUARDED_NEW(DisplayTransformRcPtr);

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

void OCIOImpl::displayTransformSetLooksOverride(OCIO_DisplayTransformRcPtr *dt, const char *looks)
{
	(*(DisplayTransformRcPtr *) dt)->setLooksOverride(looks);
}

void OCIOImpl::displayTransformSetLooksOverrideEnabled(OCIO_DisplayTransformRcPtr *dt, bool enabled)
{
	(*(DisplayTransformRcPtr *) dt)->setLooksOverrideEnabled(enabled);
}

void OCIOImpl::displayTransformRelease(OCIO_DisplayTransformRcPtr *dt)
{
	OBJECT_GUARDED_DELETE((DisplayTransformRcPtr *) dt, DisplayTransformRcPtr);
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
	OBJECT_GUARDED_DELETE((PackedImageDesc *) id, PackedImageDesc);
}

OCIO_ExponentTransformRcPtr *OCIOImpl::createExponentTransform(void)
{
	ExponentTransformRcPtr *et = OBJECT_GUARDED_NEW(ExponentTransformRcPtr);

	*et = ExponentTransform::Create();

	return (OCIO_ExponentTransformRcPtr *) et;
}

void OCIOImpl::exponentTransformSetValue(OCIO_ExponentTransformRcPtr *et, const float *exponent)
{
	(*(ExponentTransformRcPtr *) et)->setValue(exponent);
}

void OCIOImpl::exponentTransformRelease(OCIO_ExponentTransformRcPtr *et)
{
	OBJECT_GUARDED_DELETE((ExponentTransformRcPtr *) et, ExponentTransformRcPtr);
}

OCIO_MatrixTransformRcPtr *OCIOImpl::createMatrixTransform(void)
{
	MatrixTransformRcPtr *mt = OBJECT_GUARDED_NEW(MatrixTransformRcPtr);

	*mt = MatrixTransform::Create();

	return (OCIO_MatrixTransformRcPtr *) mt;
}

void OCIOImpl::matrixTransformSetValue(OCIO_MatrixTransformRcPtr *mt, const float *m44, const float *offset4)
{
	(*(MatrixTransformRcPtr *) mt)->setValue(m44, offset4);
}

void OCIOImpl::matrixTransformRelease(OCIO_MatrixTransformRcPtr *mt)
{
	OBJECT_GUARDED_DELETE((MatrixTransformRcPtr *) mt, MatrixTransformRcPtr);
}

void OCIOImpl::matrixTransformScale(float *m44, float *offset4, const float *scale4f)
{
	MatrixTransform::Scale(m44, offset4, scale4f);
}

const char *OCIOImpl::getVersionString(void)
{
	return GetVersion();
}

int OCIOImpl::getVersionHex(void)
{
	return GetVersionHex();
}

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

#include <iostream>

#include <OpenColorIO/OpenColorIO.h>


#define OCIO_CAPI_IMPLEMENTATION
#include "ocio_capi.h"

#ifdef NDEBUG
#  define OCIO_abort()
#else
#  include <stdlib.h>
#  define OCIO_abort() abort()
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

ConstConfigRcPtr *OCIO_getCurrentConfig(void)
{
	ConstConfigRcPtr *config =  new ConstConfigRcPtr();
	try {
		*config = GetCurrentConfig();

		if(*config)
			return config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

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
	ConstConfigRcPtr *config =  new ConstConfigRcPtr();

	try {
		*config = Config::CreateFromEnv();

		if (*config)
			return config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}


ConstConfigRcPtr *OCIO_configCreateFromFile(const char *filename)
{
	ConstConfigRcPtr *config =  new ConstConfigRcPtr();

	try {
		*config = Config::CreateFromFile(filename);

		if (*config)
			return config;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

void OCIO_configRelease(ConstConfigRcPtr *config)
{
	delete config;
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
	ConstColorSpaceRcPtr *cs =  new ConstColorSpaceRcPtr();

	try {
		*cs = (*config)->getColorSpace(name);

		if (*cs)
			return cs;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
		delete cs;
	}

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

void OCIO_colorSpaceRelease(ConstColorSpaceRcPtr *cs)
{
	delete cs;
}

ConstProcessorRcPtr *OCIO_configGetProcessorWithNames(ConstConfigRcPtr *config, const char *srcName, const char *dstName)
{
	ConstProcessorRcPtr *p =  new ConstProcessorRcPtr();

	try {
		*p = (*config)->getProcessor(srcName, dstName);

		if (*p)
			return p;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return 0;
}

ConstProcessorRcPtr *OCIO_configGetProcessor(ConstConfigRcPtr *config, ConstTransformRcPtr *transform)
{
	ConstProcessorRcPtr *p =  new ConstProcessorRcPtr();

	try {
		*p = (*config)->getProcessor(*transform);

		if (*p)
			return p;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

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

void OCIO_processorApplyRGB(ConstProcessorRcPtr *processor, float *pixel)
{
	(*processor)->applyRGB(pixel);
}

void OCIO_processorApplyRGBA(ConstProcessorRcPtr *processor, float *pixel)
{
	(*processor)->applyRGBA(pixel);
}

void OCIO_processorRelease(ConstProcessorRcPtr *p)
{
	delete p;
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
	DisplayTransformRcPtr *dt =  new DisplayTransformRcPtr();

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
	delete dt;
	dt = NULL;
}

PackedImageDesc *OCIO_createPackedImageDesc(float *data, long width, long height, long numChannels,
											long chanStrideBytes, long xStrideBytes, long yStrideBytes)
{
	try {
		PackedImageDesc *id = new PackedImageDesc(data, width, height, numChannels, chanStrideBytes, xStrideBytes, yStrideBytes);

		return id;
	}
	catch (Exception &exception) {
		OCIO_reportException(exception);
	}

	return NULL;
}

void OCIO_packedImageDescRelease(PackedImageDesc* id)
{
	delete id;
	id = NULL;
}

ExponentTransformRcPtr *OCIO_createExponentTransform(void)
{
	ExponentTransformRcPtr *et =  new ExponentTransformRcPtr();

	*et = ExponentTransform::Create();

	return et;
}

void OCIO_exponentTransformSetValue(ExponentTransformRcPtr *et, const float *exponent)
{
	(*et)->setValue(exponent);
}

void OCIO_exponentTransformRelease(ExponentTransformRcPtr *et)
{
	delete et;
}

MatrixTransformRcPtr *OCIO_createMatrixTransform(void)
{
	MatrixTransformRcPtr *mt = new MatrixTransformRcPtr();

	*mt = MatrixTransform::Create();

	return mt;
}

void OCIO_matrixTransformSetValue(MatrixTransformRcPtr *mt, const float *m44, const float *offset4)
{
	(*mt)->setValue(m44, offset4);
}

void OCIO_matrixTransformRelease(MatrixTransformRcPtr *mt)
{
	delete mt;
}

void OCIO_matrixTransformScale(float * m44, float * offset4, const float *scale4f)
{
	MatrixTransform::Scale(m44, offset4, scale4f);
}

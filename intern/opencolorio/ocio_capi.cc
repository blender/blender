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

#include "MEM_guardedalloc.h"

namespace OCIO_NAMESPACE {};

#include "ocio_impl.h"

static IOCIOImpl *impl = NULL;

void OCIO_init(void)
{
#ifdef WITH_OCIO
	impl = new OCIOImpl();
#else
	impl = new FallbackImpl();
#endif
}

void OCIO_exit(void)
{
	delete impl;
	impl = NULL;
}

ConstConfigRcPtr *OCIO_getCurrentConfig(void)
{
	return impl->getCurrentConfig();
}

ConstConfigRcPtr *OCIO_configCreateFallback(void)
{
	delete impl;
	impl = new FallbackImpl();

	return impl->getCurrentConfig();
}

void OCIO_setCurrentConfig(const ConstConfigRcPtr *config)
{
	impl->setCurrentConfig(config);
}

ConstConfigRcPtr *OCIO_configCreateFromEnv(void)
{
	return impl->configCreateFromEnv();
}

ConstConfigRcPtr *OCIO_configCreateFromFile(const char *filename)
{
	return impl->configCreateFromFile(filename);
}

void OCIO_configRelease(ConstConfigRcPtr *config)
{
	impl->configRelease(config);
}

int OCIO_configGetNumColorSpaces(ConstConfigRcPtr *config)
{
	return impl->configGetNumColorSpaces(config);
}

const char *OCIO_configGetColorSpaceNameByIndex(ConstConfigRcPtr *config, int index)
{
	return impl->configGetColorSpaceNameByIndex(config, index);
}

ConstColorSpaceRcPtr *OCIO_configGetColorSpace(ConstConfigRcPtr *config, const char *name)
{
	return impl->configGetColorSpace(config, name);
}

int OCIO_configGetIndexForColorSpace(ConstConfigRcPtr *config, const char *name)
{
	return impl->configGetIndexForColorSpace(config, name);
}

const char *OCIO_configGetDefaultDisplay(ConstConfigRcPtr *config)
{
	return impl->configGetDefaultDisplay(config);
}

int OCIO_configGetNumDisplays(ConstConfigRcPtr* config)
{
	return impl->configGetNumDisplays(config);
}

const char *OCIO_configGetDisplay(ConstConfigRcPtr *config, int index)
{
	return impl->configGetDisplay(config, index);
}

const char *OCIO_configGetDefaultView(ConstConfigRcPtr *config, const char *display)
{
	return impl->configGetDefaultView(config, display);
}

int OCIO_configGetNumViews(ConstConfigRcPtr *config, const char *display)
{
	return impl->configGetNumViews(config, display);
}

const char *OCIO_configGetView(ConstConfigRcPtr *config, const char *display, int index)
{
	return impl->configGetView(config, display, index);
}

const char *OCIO_configGetDisplayColorSpaceName(ConstConfigRcPtr *config, const char *display, const char *view)
{
	return impl->configGetDisplayColorSpaceName(config, display, view);
}

int OCIO_colorSpaceIsInvertible(ConstColorSpaceRcPtr *cs)
{
	return impl->colorSpaceIsInvertible(cs);
}

int OCIO_colorSpaceIsData(ConstColorSpaceRcPtr *cs)
{
	return impl->colorSpaceIsData(cs);
}

void OCIO_colorSpaceRelease(ConstColorSpaceRcPtr *cs)
{
	impl->colorSpaceRelease(cs);
}

ConstProcessorRcPtr *OCIO_configGetProcessorWithNames(ConstConfigRcPtr *config, const char *srcName, const char *dstName)
{
	return impl->configGetProcessorWithNames(config, srcName, dstName);
}

ConstProcessorRcPtr *OCIO_configGetProcessor(ConstConfigRcPtr *config, ConstTransformRcPtr *transform)
{
	return impl->configGetProcessor(config, transform);
}

void OCIO_processorApply(ConstProcessorRcPtr *processor, PackedImageDesc *img)
{
	impl->processorApply(processor, img);
}

void OCIO_processorApply_predivide(ConstProcessorRcPtr *processor, PackedImageDesc *img)
{
	impl->processorApply_predivide(processor, img);
}

void OCIO_processorApplyRGB(ConstProcessorRcPtr *processor, float *pixel)
{
	impl->processorApplyRGB(processor, pixel);
}

void OCIO_processorApplyRGBA(ConstProcessorRcPtr *processor, float *pixel)
{
	impl->processorApplyRGBA(processor, pixel);
}

void OCIO_processorApplyRGBA_predivide(ConstProcessorRcPtr *processor, float *pixel)
{
	impl->processorApplyRGBA_predivide(processor, pixel);
}

void OCIO_processorRelease(ConstProcessorRcPtr *p)
{
	impl->processorRelease(p);
}

const char *OCIO_colorSpaceGetName(ConstColorSpaceRcPtr *cs)
{
	return impl->colorSpaceGetName(cs);
}

const char *OCIO_colorSpaceGetDescription(ConstColorSpaceRcPtr *cs)
{
	return impl->colorSpaceGetDescription(cs);
}

const char *OCIO_colorSpaceGetFamily(ConstColorSpaceRcPtr *cs)
{
	return impl->colorSpaceGetFamily(cs);
}

DisplayTransformRcPtr *OCIO_createDisplayTransform(void)
{
	return impl->createDisplayTransform();
}

void OCIO_displayTransformSetInputColorSpaceName(DisplayTransformRcPtr *dt, const char *name)
{
	impl->displayTransformSetInputColorSpaceName(dt, name);
}

void OCIO_displayTransformSetDisplay(DisplayTransformRcPtr *dt, const char *name)
{
	impl->displayTransformSetDisplay(dt, name);
}

void OCIO_displayTransformSetView(DisplayTransformRcPtr *dt, const char *name)
{
	impl->displayTransformSetView(dt, name);
}

void OCIO_displayTransformSetDisplayCC(DisplayTransformRcPtr *dt, ConstTransformRcPtr *t)
{
	impl->displayTransformSetDisplayCC(dt, t);
}

void OCIO_displayTransformSetLinearCC(DisplayTransformRcPtr *dt, ConstTransformRcPtr *t)
{
	impl->displayTransformSetLinearCC(dt, t);
}

void OCIO_displayTransformRelease(DisplayTransformRcPtr *dt)
{
	impl->displayTransformRelease(dt);
}

PackedImageDesc *OCIO_createPackedImageDesc(float *data, long width, long height, long numChannels,
                                            long chanStrideBytes, long xStrideBytes, long yStrideBytes)
{
	return impl->createPackedImageDesc(data, width, height, numChannels, chanStrideBytes, xStrideBytes, yStrideBytes);
}

void OCIO_packedImageDescRelease(PackedImageDesc* id)
{
	impl->packedImageDescRelease(id);
}

ExponentTransformRcPtr *OCIO_createExponentTransform(void)
{
	return impl->createExponentTransform();
}

void OCIO_exponentTransformSetValue(ExponentTransformRcPtr *et, const float *exponent)
{
	impl->exponentTransformSetValue(et, exponent);
}

void OCIO_exponentTransformRelease(ExponentTransformRcPtr *et)
{
	impl->exponentTransformRelease(et);
}

MatrixTransformRcPtr *OCIO_createMatrixTransform(void)
{
	return impl->createMatrixTransform();
}

void OCIO_matrixTransformSetValue(MatrixTransformRcPtr *mt, const float *m44, const float *offset4)
{
	impl->matrixTransformSetValue(mt, m44, offset4);
}

void OCIO_matrixTransformRelease(MatrixTransformRcPtr *mt)
{
	impl->matrixTransformRelease(mt);
}

void OCIO_matrixTransformScale(float * m44, float * offset4, const float *scale4f)
{
	impl->matrixTransformScale(m44, offset4, scale4f);
}

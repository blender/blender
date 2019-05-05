/*
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
 */

#include "MEM_guardedalloc.h"

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

OCIO_ConstConfigRcPtr *OCIO_getCurrentConfig(void)
{
  return impl->getCurrentConfig();
}

OCIO_ConstConfigRcPtr *OCIO_configCreateFallback(void)
{
  delete impl;
  impl = new FallbackImpl();

  return impl->getCurrentConfig();
}

void OCIO_setCurrentConfig(const OCIO_ConstConfigRcPtr *config)
{
  impl->setCurrentConfig(config);
}

OCIO_ConstConfigRcPtr *OCIO_configCreateFromEnv(void)
{
  return impl->configCreateFromEnv();
}

OCIO_ConstConfigRcPtr *OCIO_configCreateFromFile(const char *filename)
{
  return impl->configCreateFromFile(filename);
}

void OCIO_configRelease(OCIO_ConstConfigRcPtr *config)
{
  impl->configRelease(config);
}

int OCIO_configGetNumColorSpaces(OCIO_ConstConfigRcPtr *config)
{
  return impl->configGetNumColorSpaces(config);
}

const char *OCIO_configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr *config, int index)
{
  return impl->configGetColorSpaceNameByIndex(config, index);
}

OCIO_ConstColorSpaceRcPtr *OCIO_configGetColorSpace(OCIO_ConstConfigRcPtr *config,
                                                    const char *name)
{
  return impl->configGetColorSpace(config, name);
}

int OCIO_configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name)
{
  return impl->configGetIndexForColorSpace(config, name);
}

const char *OCIO_configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config)
{
  return impl->configGetDefaultDisplay(config);
}

int OCIO_configGetNumDisplays(OCIO_ConstConfigRcPtr *config)
{
  return impl->configGetNumDisplays(config);
}

const char *OCIO_configGetDisplay(OCIO_ConstConfigRcPtr *config, int index)
{
  return impl->configGetDisplay(config, index);
}

const char *OCIO_configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display)
{
  return impl->configGetDefaultView(config, display);
}

int OCIO_configGetNumViews(OCIO_ConstConfigRcPtr *config, const char *display)
{
  return impl->configGetNumViews(config, display);
}

const char *OCIO_configGetView(OCIO_ConstConfigRcPtr *config, const char *display, int index)
{
  return impl->configGetView(config, display, index);
}

const char *OCIO_configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr *config,
                                                const char *display,
                                                const char *view)
{
  return impl->configGetDisplayColorSpaceName(config, display, view);
}

void OCIO_configGetDefaultLumaCoefs(OCIO_ConstConfigRcPtr *config, float *rgb)
{
  impl->configGetDefaultLumaCoefs(config, rgb);
}

void OCIO_configGetXYZtoRGB(OCIO_ConstConfigRcPtr *config, float xyz_to_rgb[3][3])
{
  impl->configGetXYZtoRGB(config, xyz_to_rgb);
}

int OCIO_configGetNumLooks(OCIO_ConstConfigRcPtr *config)
{
  return impl->configGetNumLooks(config);
}

const char *OCIO_configGetLookNameByIndex(OCIO_ConstConfigRcPtr *config, int index)
{
  return impl->configGetLookNameByIndex(config, index);
}

OCIO_ConstLookRcPtr *OCIO_configGetLook(OCIO_ConstConfigRcPtr *config, const char *name)
{
  return impl->configGetLook(config, name);
}

const char *OCIO_lookGetProcessSpace(OCIO_ConstLookRcPtr *look)
{
  return impl->lookGetProcessSpace(look);
}

void OCIO_lookRelease(OCIO_ConstLookRcPtr *look)
{
  impl->lookRelease(look);
}

int OCIO_colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr *cs)
{
  return impl->colorSpaceIsInvertible(cs);
}

int OCIO_colorSpaceIsData(OCIO_ConstColorSpaceRcPtr *cs)
{
  return impl->colorSpaceIsData(cs);
}

void OCIO_colorSpaceIsBuiltin(OCIO_ConstConfigRcPtr *config,
                              OCIO_ConstColorSpaceRcPtr *cs,
                              bool *is_scene_linear,
                              bool *is_srgb)
{
  impl->colorSpaceIsBuiltin(config, cs, *is_scene_linear, *is_srgb);
}

void OCIO_colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs)
{
  impl->colorSpaceRelease(cs);
}

OCIO_ConstProcessorRcPtr *OCIO_configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config,
                                                           const char *srcName,
                                                           const char *dstName)
{
  return impl->configGetProcessorWithNames(config, srcName, dstName);
}

OCIO_ConstProcessorRcPtr *OCIO_configGetProcessor(OCIO_ConstConfigRcPtr *config,
                                                  OCIO_ConstTransformRcPtr *transform)
{
  return impl->configGetProcessor(config, transform);
}

void OCIO_processorApply(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img)
{
  impl->processorApply(processor, img);
}

void OCIO_processorApply_predivide(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img)
{
  impl->processorApply_predivide(processor, img);
}

void OCIO_processorApplyRGB(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
  impl->processorApplyRGB(processor, pixel);
}

void OCIO_processorApplyRGBA(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
  impl->processorApplyRGBA(processor, pixel);
}

void OCIO_processorApplyRGBA_predivide(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
  impl->processorApplyRGBA_predivide(processor, pixel);
}

void OCIO_processorRelease(OCIO_ConstProcessorRcPtr *p)
{
  impl->processorRelease(p);
}

const char *OCIO_colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs)
{
  return impl->colorSpaceGetName(cs);
}

const char *OCIO_colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr *cs)
{
  return impl->colorSpaceGetDescription(cs);
}

const char *OCIO_colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr *cs)
{
  return impl->colorSpaceGetFamily(cs);
}

OCIO_DisplayTransformRcPtr *OCIO_createDisplayTransform(void)
{
  return impl->createDisplayTransform();
}

void OCIO_displayTransformSetInputColorSpaceName(OCIO_DisplayTransformRcPtr *dt, const char *name)
{
  impl->displayTransformSetInputColorSpaceName(dt, name);
}

void OCIO_displayTransformSetDisplay(OCIO_DisplayTransformRcPtr *dt, const char *name)
{
  impl->displayTransformSetDisplay(dt, name);
}

void OCIO_displayTransformSetView(OCIO_DisplayTransformRcPtr *dt, const char *name)
{
  impl->displayTransformSetView(dt, name);
}

void OCIO_displayTransformSetDisplayCC(OCIO_DisplayTransformRcPtr *dt, OCIO_ConstTransformRcPtr *t)
{
  impl->displayTransformSetDisplayCC(dt, t);
}

void OCIO_displayTransformSetLinearCC(OCIO_DisplayTransformRcPtr *dt, OCIO_ConstTransformRcPtr *t)
{
  impl->displayTransformSetLinearCC(dt, t);
}

void OCIO_displayTransformSetLooksOverride(OCIO_DisplayTransformRcPtr *dt, const char *looks)
{
  impl->displayTransformSetLooksOverride(dt, looks);
}

void OCIO_displayTransformSetLooksOverrideEnabled(OCIO_DisplayTransformRcPtr *dt, bool enabled)
{
  impl->displayTransformSetLooksOverrideEnabled(dt, enabled);
}

void OCIO_displayTransformRelease(OCIO_DisplayTransformRcPtr *dt)
{
  impl->displayTransformRelease(dt);
}

OCIO_PackedImageDesc *OCIO_createOCIO_PackedImageDesc(float *data,
                                                      long width,
                                                      long height,
                                                      long numChannels,
                                                      long chanStrideBytes,
                                                      long xStrideBytes,
                                                      long yStrideBytes)
{
  return impl->createOCIO_PackedImageDesc(
      data, width, height, numChannels, chanStrideBytes, xStrideBytes, yStrideBytes);
}

void OCIO_PackedImageDescRelease(OCIO_PackedImageDesc *id)
{
  impl->OCIO_PackedImageDescRelease(id);
}

OCIO_ExponentTransformRcPtr *OCIO_createExponentTransform(void)
{
  return impl->createExponentTransform();
}

void OCIO_exponentTransformSetValue(OCIO_ExponentTransformRcPtr *et, const float *exponent)
{
  impl->exponentTransformSetValue(et, exponent);
}

void OCIO_exponentTransformRelease(OCIO_ExponentTransformRcPtr *et)
{
  impl->exponentTransformRelease(et);
}

OCIO_MatrixTransformRcPtr *OCIO_createMatrixTransform(void)
{
  return impl->createMatrixTransform();
}

void OCIO_matrixTransformSetValue(OCIO_MatrixTransformRcPtr *mt,
                                  const float *m44,
                                  const float *offset4)
{
  impl->matrixTransformSetValue(mt, m44, offset4);
}

void OCIO_matrixTransformRelease(OCIO_MatrixTransformRcPtr *mt)
{
  impl->matrixTransformRelease(mt);
}

void OCIO_matrixTransformScale(float *m44, float *offset4, const float *scale4f)
{
  impl->matrixTransformScale(m44, offset4, scale4f);
}

int OCIO_supportGLSLDraw(void)
{
  return (int)impl->supportGLSLDraw();
}

int OCIO_setupGLSLDraw(struct OCIO_GLSLDrawState **state_r,
                       OCIO_ConstProcessorRcPtr *processor,
                       OCIO_CurveMappingSettings *curve_mapping_settings,
                       float dither,
                       bool predivide)
{
  return (int)impl->setupGLSLDraw(state_r, processor, curve_mapping_settings, dither, predivide);
}

void OCIO_finishGLSLDraw(struct OCIO_GLSLDrawState *state)
{
  impl->finishGLSLDraw(state);
}

void OCIO_freeOGLState(struct OCIO_GLSLDrawState *state)
{
  impl->freeGLState(state);
}

const char *OCIO_getVersionString(void)
{
  return impl->getVersionString();
}

int OCIO_getVersionHex(void)
{
  return impl->getVersionHex();
}

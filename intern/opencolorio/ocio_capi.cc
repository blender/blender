/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "MEM_guardedalloc.h"

#include "ocio_impl.h"

static IOCIOImpl *impl = NULL;

void OCIO_init()
{
#ifdef WITH_OCIO
  impl = new OCIOImpl();
#else
  impl = new FallbackImpl();
#endif
}

void OCIO_exit()
{
  delete impl;
  impl = NULL;
}

OCIO_ConstConfigRcPtr *OCIO_getCurrentConfig()
{
  return impl->getCurrentConfig();
}

OCIO_ConstConfigRcPtr *OCIO_configCreateFallback()
{
  delete impl;
  impl = new FallbackImpl();

  return impl->getCurrentConfig();
}

void OCIO_setCurrentConfig(const OCIO_ConstConfigRcPtr *config)
{
  impl->setCurrentConfig(config);
}

OCIO_ConstConfigRcPtr *OCIO_configCreateFromEnv()
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

void OCIO_configGetXYZtoSceneLinear(OCIO_ConstConfigRcPtr *config, float xyz_to_scene_linear[3][3])
{
  impl->configGetXYZtoSceneLinear(config, xyz_to_scene_linear);
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

void OCIO_processorRelease(OCIO_ConstProcessorRcPtr *processor)
{
  impl->processorRelease(processor);
}

OCIO_ConstCPUProcessorRcPtr *OCIO_processorGetCPUProcessor(OCIO_ConstProcessorRcPtr *processor)
{
  return impl->processorGetCPUProcessor(processor);
}

void OCIO_cpuProcessorApply(OCIO_ConstCPUProcessorRcPtr *cpu_processor, OCIO_PackedImageDesc *img)
{
  impl->cpuProcessorApply(cpu_processor, img);
}

void OCIO_cpuProcessorApply_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                      OCIO_PackedImageDesc *img)
{
  impl->cpuProcessorApply_predivide(cpu_processor, img);
}

void OCIO_cpuProcessorApplyRGB(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel)
{
  impl->cpuProcessorApplyRGB(cpu_processor, pixel);
}

void OCIO_cpuProcessorApplyRGBA(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel)
{
  impl->cpuProcessorApplyRGBA(cpu_processor, pixel);
}

void OCIO_cpuProcessorApplyRGBA_predivide(OCIO_ConstCPUProcessorRcPtr *processor, float *pixel)
{
  impl->cpuProcessorApplyRGBA_predivide(processor, pixel);
}

void OCIO_cpuProcessorRelease(OCIO_ConstCPUProcessorRcPtr *cpu_processor)
{
  impl->cpuProcessorRelease(cpu_processor);
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

int OCIO_colorSpaceGetNumAliases(OCIO_ConstColorSpaceRcPtr *cs)
{
  return impl->colorSpaceGetNumAliases(cs);
}

const char *OCIO_colorSpaceGetAlias(OCIO_ConstColorSpaceRcPtr *cs, const int index)
{
  return impl->colorSpaceGetAlias(cs, index);
}

OCIO_ConstProcessorRcPtr *OCIO_createDisplayProcessor(OCIO_ConstConfigRcPtr *config,
                                                      const char *input,
                                                      const char *view,
                                                      const char *display,
                                                      const char *look,
                                                      const float scale,
                                                      const float exponent,
                                                      const bool inverse)
{
  return impl->createDisplayProcessor(
      config, input, view, display, look, scale, exponent, inverse);
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

bool OCIO_supportGPUShader()
{
  return impl->supportGPUShader();
}

bool OCIO_gpuDisplayShaderBind(OCIO_ConstConfigRcPtr *config,
                               const char *input,
                               const char *view,
                               const char *display,
                               const char *look,
                               OCIO_CurveMappingSettings *curve_mapping_settings,
                               const float scale,
                               const float exponent,
                               const float dither,
                               const bool use_predivide,
                               const bool use_overlay)
{
  return impl->gpuDisplayShaderBind(config,
                                    input,
                                    view,
                                    display,
                                    look,
                                    curve_mapping_settings,
                                    scale,
                                    exponent,
                                    dither,
                                    use_predivide,
                                    use_overlay);
}

void OCIO_gpuDisplayShaderUnbind()
{
  impl->gpuDisplayShaderUnbind();
}

void OCIO_gpuCacheFree()
{
  impl->gpuCacheFree();
}

const char *OCIO_getVersionString()
{
  return impl->getVersionString();
}

int OCIO_getVersionHex()
{
  return impl->getVersionHex();
}

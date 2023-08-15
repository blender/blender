/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __OCIO_IMPL_H__
#define __OCIO_IMPL_H__

#include "ocio_capi.h"

class IOCIOImpl {
 public:
  virtual ~IOCIOImpl() {}

  virtual OCIO_ConstConfigRcPtr *getCurrentConfig(void) = 0;
  virtual void setCurrentConfig(const OCIO_ConstConfigRcPtr *config) = 0;

  virtual OCIO_ConstConfigRcPtr *configCreateFromEnv(void) = 0;
  virtual OCIO_ConstConfigRcPtr *configCreateFromFile(const char *filename) = 0;

  virtual void configRelease(OCIO_ConstConfigRcPtr *config) = 0;

  virtual int configGetNumColorSpaces(OCIO_ConstConfigRcPtr *config) = 0;
  virtual const char *configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr *config, int index) = 0;
  virtual OCIO_ConstColorSpaceRcPtr *configGetColorSpace(OCIO_ConstConfigRcPtr *config,
                                                         const char *name) = 0;
  virtual int configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name) = 0;

  virtual int colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr *cs) = 0;
  virtual int colorSpaceIsData(OCIO_ConstColorSpaceRcPtr *cs) = 0;
  virtual void colorSpaceIsBuiltin(OCIO_ConstConfigRcPtr *config,
                                   OCIO_ConstColorSpaceRcPtr *cs,
                                   bool &is_scene_linear,
                                   bool &is_srgb) = 0;

  virtual void colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs) = 0;

  virtual const char *configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config) = 0;
  virtual int configGetNumDisplays(OCIO_ConstConfigRcPtr *config) = 0;
  virtual const char *configGetDisplay(OCIO_ConstConfigRcPtr *config, int index) = 0;
  virtual const char *configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display) = 0;
  virtual int configGetNumViews(OCIO_ConstConfigRcPtr *config, const char *display) = 0;
  virtual const char *configGetView(OCIO_ConstConfigRcPtr *config,
                                    const char *display,
                                    int index) = 0;
  virtual const char *configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr *config,
                                                     const char *display,
                                                     const char *view) = 0;

  virtual void configGetDefaultLumaCoefs(OCIO_ConstConfigRcPtr *config, float *rgb) = 0;
  virtual void configGetXYZtoSceneLinear(OCIO_ConstConfigRcPtr *config,
                                         float xyz_to_scene_linear[3][3]) = 0;

  virtual int configGetNumLooks(OCIO_ConstConfigRcPtr *config) = 0;
  virtual const char *configGetLookNameByIndex(OCIO_ConstConfigRcPtr *config, int index) = 0;
  virtual OCIO_ConstLookRcPtr *configGetLook(OCIO_ConstConfigRcPtr *config, const char *name) = 0;

  virtual const char *lookGetProcessSpace(OCIO_ConstLookRcPtr *look) = 0;
  virtual void lookRelease(OCIO_ConstLookRcPtr *look) = 0;

  virtual OCIO_ConstProcessorRcPtr *configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config,
                                                                const char *srcName,
                                                                const char *dstName) = 0;
  virtual void processorRelease(OCIO_ConstProcessorRcPtr *processor) = 0;

  virtual OCIO_ConstCPUProcessorRcPtr *processorGetCPUProcessor(OCIO_ConstProcessorRcPtr *p) = 0;
  virtual bool cpuProcessorIsNoOp(OCIO_ConstCPUProcessorRcPtr *cpu_processor) = 0;
  virtual void cpuProcessorApply(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                 OCIO_PackedImageDesc *img) = 0;
  virtual void cpuProcessorApply_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                           OCIO_PackedImageDesc *img) = 0;
  virtual void cpuProcessorApplyRGB(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel) = 0;
  virtual void cpuProcessorApplyRGBA(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel) = 0;
  virtual void cpuProcessorApplyRGBA_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                               float *pixel) = 0;
  virtual void cpuProcessorRelease(OCIO_ConstCPUProcessorRcPtr *cpu_processor) = 0;

  virtual const char *colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs) = 0;
  virtual const char *colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr *cs) = 0;
  virtual const char *colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr *cs) = 0;
  virtual int colorSpaceGetNumAliases(OCIO_ConstColorSpaceRcPtr *cs) = 0;
  virtual const char *colorSpaceGetAlias(OCIO_ConstColorSpaceRcPtr *cs, const int index) = 0;

  virtual OCIO_ConstProcessorRcPtr *createDisplayProcessor(OCIO_ConstConfigRcPtr *config,
                                                           const char *input,
                                                           const char *view,
                                                           const char *display,
                                                           const char *look,
                                                           const float scale,
                                                           const float exponent,
                                                           const bool inverse) = 0;

  virtual OCIO_PackedImageDesc *createOCIO_PackedImageDesc(float *data,
                                                           long width,
                                                           long height,
                                                           long numChannels,
                                                           long chanStrideBytes,
                                                           long xStrideBytes,
                                                           long yStrideBytes) = 0;

  virtual void OCIO_PackedImageDescRelease(OCIO_PackedImageDesc *p) = 0;

  /* Optional GPU support. */
  virtual bool supportGPUShader()
  {
    return false;
  }
  virtual bool gpuDisplayShaderBind(OCIO_ConstConfigRcPtr * /*config*/,
                                    const char * /*input*/,
                                    const char * /*view*/,
                                    const char * /*display*/,
                                    const char * /*look*/,
                                    OCIO_CurveMappingSettings * /*curve_mapping_settings*/,
                                    const float /*scale*/,
                                    const float /*exponent*/,
                                    const float /*dither*/,
                                    const bool /*use_predivide*/,
                                    const bool /*use_overlay*/,
                                    const bool /*use_hdr*/)
  {
    return false;
  }
  virtual void gpuDisplayShaderUnbind(void) {}
  virtual void gpuCacheFree(void) {}

  virtual const char *getVersionString(void) = 0;
  virtual int getVersionHex(void) = 0;
};

class FallbackImpl : public IOCIOImpl {
 public:
  FallbackImpl() {}

  OCIO_ConstConfigRcPtr *getCurrentConfig(void);
  void setCurrentConfig(const OCIO_ConstConfigRcPtr *config);

  OCIO_ConstConfigRcPtr *configCreateFromEnv(void);
  OCIO_ConstConfigRcPtr *configCreateFromFile(const char *filename);

  void configRelease(OCIO_ConstConfigRcPtr *config);

  int configGetNumColorSpaces(OCIO_ConstConfigRcPtr *config);
  const char *configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr *config, int index);
  OCIO_ConstColorSpaceRcPtr *configGetColorSpace(OCIO_ConstConfigRcPtr *config, const char *name);
  int configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name);

  int colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr *cs);
  int colorSpaceIsData(OCIO_ConstColorSpaceRcPtr *cs);
  void colorSpaceIsBuiltin(OCIO_ConstConfigRcPtr *config,
                           OCIO_ConstColorSpaceRcPtr *cs,
                           bool &is_scene_linear,
                           bool &is_srgb);

  void colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs);

  const char *configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config);
  int configGetNumDisplays(OCIO_ConstConfigRcPtr *config);
  const char *configGetDisplay(OCIO_ConstConfigRcPtr *config, int index);
  const char *configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display);
  int configGetNumViews(OCIO_ConstConfigRcPtr *config, const char *display);
  const char *configGetView(OCIO_ConstConfigRcPtr *config, const char *display, int index);
  const char *configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr *config,
                                             const char *display,
                                             const char *view);

  void configGetDefaultLumaCoefs(OCIO_ConstConfigRcPtr *config, float *rgb);
  void configGetXYZtoSceneLinear(OCIO_ConstConfigRcPtr *config, float xyz_to_scene_linear[3][3]);

  int configGetNumLooks(OCIO_ConstConfigRcPtr *config);
  const char *configGetLookNameByIndex(OCIO_ConstConfigRcPtr *config, int index);
  OCIO_ConstLookRcPtr *configGetLook(OCIO_ConstConfigRcPtr *config, const char *name);

  const char *lookGetProcessSpace(OCIO_ConstLookRcPtr *look);
  void lookRelease(OCIO_ConstLookRcPtr *look);

  OCIO_ConstProcessorRcPtr *configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config,
                                                        const char *srcName,
                                                        const char *dstName);
  void processorRelease(OCIO_ConstProcessorRcPtr *processor);

  OCIO_ConstCPUProcessorRcPtr *processorGetCPUProcessor(OCIO_ConstProcessorRcPtr *processor);
  bool cpuProcessorIsNoOp(OCIO_ConstCPUProcessorRcPtr *cpu_processor);
  void cpuProcessorApply(OCIO_ConstCPUProcessorRcPtr *cpu_processor, OCIO_PackedImageDesc *img);
  void cpuProcessorApply_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                   OCIO_PackedImageDesc *img);
  void cpuProcessorApplyRGB(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel);
  void cpuProcessorApplyRGBA(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel);
  void cpuProcessorApplyRGBA_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel);
  void cpuProcessorRelease(OCIO_ConstCPUProcessorRcPtr *cpu_processor);

  const char *colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs);
  const char *colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr *cs);
  const char *colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr *cs);
  int colorSpaceGetNumAliases(OCIO_ConstColorSpaceRcPtr *cs);
  const char *colorSpaceGetAlias(OCIO_ConstColorSpaceRcPtr *cs, const int index);

  OCIO_ConstProcessorRcPtr *createDisplayProcessor(OCIO_ConstConfigRcPtr *config,
                                                   const char *input,
                                                   const char *view,
                                                   const char *display,
                                                   const char *look,
                                                   const float scale,
                                                   const float exponent,
                                                   const bool inverse);

  OCIO_PackedImageDesc *createOCIO_PackedImageDesc(float *data,
                                                   long width,
                                                   long height,
                                                   long numChannels,
                                                   long chanStrideBytes,
                                                   long xStrideBytes,
                                                   long yStrideBytes);

  void OCIO_PackedImageDescRelease(OCIO_PackedImageDesc *p);

  const char *getVersionString(void);
  int getVersionHex(void);
};

#ifdef WITH_OCIO
class OCIOImpl : public IOCIOImpl {
 public:
  OCIOImpl(){};

  OCIO_ConstConfigRcPtr *getCurrentConfig(void);
  void setCurrentConfig(const OCIO_ConstConfigRcPtr *config);

  OCIO_ConstConfigRcPtr *configCreateFromEnv(void);
  OCIO_ConstConfigRcPtr *configCreateFromFile(const char *filename);

  void configRelease(OCIO_ConstConfigRcPtr *config);

  int configGetNumColorSpaces(OCIO_ConstConfigRcPtr *config);
  const char *configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr *config, int index);
  OCIO_ConstColorSpaceRcPtr *configGetColorSpace(OCIO_ConstConfigRcPtr *config, const char *name);
  int configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name);

  int colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr *cs);
  int colorSpaceIsData(OCIO_ConstColorSpaceRcPtr *cs);
  void colorSpaceIsBuiltin(OCIO_ConstConfigRcPtr *config,
                           OCIO_ConstColorSpaceRcPtr *cs,
                           bool &is_scene_linear,
                           bool &is_srgb);

  void colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs);

  const char *configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config);
  int configGetNumDisplays(OCIO_ConstConfigRcPtr *config);
  const char *configGetDisplay(OCIO_ConstConfigRcPtr *config, int index);
  const char *configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display);
  int configGetNumViews(OCIO_ConstConfigRcPtr *config, const char *display);
  const char *configGetView(OCIO_ConstConfigRcPtr *config, const char *display, int index);
  const char *configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr *config,
                                             const char *display,
                                             const char *view);

  void configGetDefaultLumaCoefs(OCIO_ConstConfigRcPtr *config, float *rgb);
  void configGetXYZtoSceneLinear(OCIO_ConstConfigRcPtr *config, float xyz_to_scene_linear[3][3]);

  int configGetNumLooks(OCIO_ConstConfigRcPtr *config);
  const char *configGetLookNameByIndex(OCIO_ConstConfigRcPtr *config, int index);
  OCIO_ConstLookRcPtr *configGetLook(OCIO_ConstConfigRcPtr *config, const char *name);

  const char *lookGetProcessSpace(OCIO_ConstLookRcPtr *look);
  void lookRelease(OCIO_ConstLookRcPtr *look);

  OCIO_ConstProcessorRcPtr *configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config,
                                                        const char *srcName,
                                                        const char *dstName);
  void processorRelease(OCIO_ConstProcessorRcPtr *processor);

  OCIO_ConstCPUProcessorRcPtr *processorGetCPUProcessor(OCIO_ConstProcessorRcPtr *processor);
  bool cpuProcessorIsNoOp(OCIO_ConstCPUProcessorRcPtr *cpu_processor);
  void cpuProcessorApply(OCIO_ConstCPUProcessorRcPtr *cpu_processor, OCIO_PackedImageDesc *img);
  void cpuProcessorApply_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                   OCIO_PackedImageDesc *img);
  void cpuProcessorApplyRGB(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel);
  void cpuProcessorApplyRGBA(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel);
  void cpuProcessorApplyRGBA_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel);
  void cpuProcessorRelease(OCIO_ConstCPUProcessorRcPtr *cpu_processor);

  const char *colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs);
  const char *colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr *cs);
  const char *colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr *cs);
  int colorSpaceGetNumAliases(OCIO_ConstColorSpaceRcPtr *cs);
  const char *colorSpaceGetAlias(OCIO_ConstColorSpaceRcPtr *cs, const int index);

  OCIO_ConstProcessorRcPtr *createDisplayProcessor(OCIO_ConstConfigRcPtr *config,
                                                   const char *input,
                                                   const char *view,
                                                   const char *display,
                                                   const char *look,
                                                   const float scale,
                                                   const float exponent,
                                                   const bool inverse);

  OCIO_PackedImageDesc *createOCIO_PackedImageDesc(float *data,
                                                   long width,
                                                   long height,
                                                   long numChannels,
                                                   long chanStrideBytes,
                                                   long xStrideBytes,
                                                   long yStrideBytes);

  void OCIO_PackedImageDescRelease(OCIO_PackedImageDesc *p);

  bool supportGPUShader();
  bool gpuDisplayShaderBind(OCIO_ConstConfigRcPtr *config,
                            const char *input,
                            const char *view,
                            const char *display,
                            const char *look,
                            OCIO_CurveMappingSettings *curve_mapping_settings,
                            const float scale,
                            const float exponent,
                            const float dither,
                            const bool use_predivide,
                            const bool use_overlay,
                            const bool use_hdr);
  void gpuDisplayShaderUnbind(void);
  void gpuCacheFree(void);

  const char *getVersionString(void);
  int getVersionHex(void);
};
#endif

#endif /* OCIO_IMPL_H */

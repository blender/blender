/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef __OCIO_IMPL_H__
#define __OCIO_IMPL_H__

#include "ocio_capi.h"

class IOCIOImpl {
 public:
  virtual ~IOCIOImpl() = default;

  virtual OCIO_ConstConfigRcPtr *getCurrentConfig() = 0;
  virtual void setCurrentConfig(const OCIO_ConstConfigRcPtr *config) = 0;

  virtual OCIO_ConstConfigRcPtr *configCreateFromEnv() = 0;
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
                                                           const float temperature,
                                                           const float tint,
                                                           const bool use_white_balance,
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
                                    const float /*temperature*/,
                                    const float /*tint*/,
                                    const bool /*use_predivide*/,
                                    const bool /*use_overlay*/,
                                    const bool /*use_hdr*/,
                                    const bool /*use_white_balance*/)
  {
    return false;
  }
  virtual void gpuDisplayShaderUnbind() {}
  virtual void gpuCacheFree() {}

  virtual const char *getVersionString() = 0;
  virtual int getVersionHex() = 0;
};

class FallbackImpl : public IOCIOImpl {
 public:
  FallbackImpl() = default;

  OCIO_ConstConfigRcPtr *getCurrentConfig() override;
  void setCurrentConfig(const OCIO_ConstConfigRcPtr *config) override;

  OCIO_ConstConfigRcPtr *configCreateFromEnv() override;
  OCIO_ConstConfigRcPtr *configCreateFromFile(const char *filename) override;

  void configRelease(OCIO_ConstConfigRcPtr *config) override;

  int configGetNumColorSpaces(OCIO_ConstConfigRcPtr *config) override;
  const char *configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr *config, int index) override;
  OCIO_ConstColorSpaceRcPtr *configGetColorSpace(OCIO_ConstConfigRcPtr *config,
                                                 const char *name) override;
  int configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name) override;

  int colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr *cs) override;
  int colorSpaceIsData(OCIO_ConstColorSpaceRcPtr *cs) override;
  void colorSpaceIsBuiltin(OCIO_ConstConfigRcPtr *config,
                           OCIO_ConstColorSpaceRcPtr *cs,
                           bool &is_scene_linear,
                           bool &is_srgb) override;

  void colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs) override;

  const char *configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config) override;
  int configGetNumDisplays(OCIO_ConstConfigRcPtr *config) override;
  const char *configGetDisplay(OCIO_ConstConfigRcPtr *config, int index) override;
  const char *configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display) override;
  int configGetNumViews(OCIO_ConstConfigRcPtr *config, const char *display) override;
  const char *configGetView(OCIO_ConstConfigRcPtr *config,
                            const char *display,
                            int index) override;
  const char *configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr *config,
                                             const char *display,
                                             const char *view) override;

  void configGetDefaultLumaCoefs(OCIO_ConstConfigRcPtr *config, float *rgb) override;
  void configGetXYZtoSceneLinear(OCIO_ConstConfigRcPtr *config,
                                 float xyz_to_scene_linear[3][3]) override;

  int configGetNumLooks(OCIO_ConstConfigRcPtr *config) override;
  const char *configGetLookNameByIndex(OCIO_ConstConfigRcPtr *config, int index) override;
  OCIO_ConstLookRcPtr *configGetLook(OCIO_ConstConfigRcPtr *config, const char *name) override;

  const char *lookGetProcessSpace(OCIO_ConstLookRcPtr *look) override;
  void lookRelease(OCIO_ConstLookRcPtr *look) override;

  OCIO_ConstProcessorRcPtr *configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config,
                                                        const char *srcName,
                                                        const char *dstName) override;
  void processorRelease(OCIO_ConstProcessorRcPtr *processor) override;

  OCIO_ConstCPUProcessorRcPtr *processorGetCPUProcessor(
      OCIO_ConstProcessorRcPtr *processor) override;
  bool cpuProcessorIsNoOp(OCIO_ConstCPUProcessorRcPtr *cpu_processor) override;
  void cpuProcessorApply(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                         OCIO_PackedImageDesc *img) override;
  void cpuProcessorApply_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                   OCIO_PackedImageDesc *img) override;
  void cpuProcessorApplyRGB(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel) override;
  void cpuProcessorApplyRGBA(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel) override;
  void cpuProcessorApplyRGBA_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                       float *pixel) override;
  void cpuProcessorRelease(OCIO_ConstCPUProcessorRcPtr *cpu_processor) override;

  const char *colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs) override;
  const char *colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr *cs) override;
  const char *colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr *cs) override;
  int colorSpaceGetNumAliases(OCIO_ConstColorSpaceRcPtr *cs) override;
  const char *colorSpaceGetAlias(OCIO_ConstColorSpaceRcPtr *cs, const int index) override;

  OCIO_ConstProcessorRcPtr *createDisplayProcessor(OCIO_ConstConfigRcPtr *config,
                                                   const char *input,
                                                   const char *view,
                                                   const char *display,
                                                   const char *look,
                                                   const float scale,
                                                   const float exponent,
                                                   const float temperature,
                                                   const float tint,
                                                   const bool use_white_balance,
                                                   const bool inverse) override;

  OCIO_PackedImageDesc *createOCIO_PackedImageDesc(float *data,
                                                   long width,
                                                   long height,
                                                   long numChannels,
                                                   long chanStrideBytes,
                                                   long xStrideBytes,
                                                   long yStrideBytes) override;

  void OCIO_PackedImageDescRelease(OCIO_PackedImageDesc *id) override;

  const char *getVersionString() override;
  int getVersionHex() override;
};

#ifdef WITH_OCIO
class OCIOImpl : public IOCIOImpl {
 public:
  OCIOImpl() = default;

  OCIO_ConstConfigRcPtr *getCurrentConfig() override;
  void setCurrentConfig(const OCIO_ConstConfigRcPtr *config) override;

  OCIO_ConstConfigRcPtr *configCreateFromEnv() override;
  OCIO_ConstConfigRcPtr *configCreateFromFile(const char *filename) override;

  void configRelease(OCIO_ConstConfigRcPtr *config) override;

  int configGetNumColorSpaces(OCIO_ConstConfigRcPtr *config) override;
  const char *configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr *config, int index) override;
  OCIO_ConstColorSpaceRcPtr *configGetColorSpace(OCIO_ConstConfigRcPtr *config,
                                                 const char *name) override;
  int configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name) override;

  int colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr *cs) override;
  int colorSpaceIsData(OCIO_ConstColorSpaceRcPtr *cs) override;
  void colorSpaceIsBuiltin(OCIO_ConstConfigRcPtr *config,
                           OCIO_ConstColorSpaceRcPtr *cs,
                           bool &is_scene_linear,
                           bool &is_srgb) override;

  void colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs) override;

  const char *configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config) override;
  int configGetNumDisplays(OCIO_ConstConfigRcPtr *config) override;
  const char *configGetDisplay(OCIO_ConstConfigRcPtr *config, int index) override;
  const char *configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display) override;
  int configGetNumViews(OCIO_ConstConfigRcPtr *config, const char *display) override;
  const char *configGetView(OCIO_ConstConfigRcPtr *config,
                            const char *display,
                            int index) override;
  const char *configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr *config,
                                             const char *display,
                                             const char *view) override;

  void configGetDefaultLumaCoefs(OCIO_ConstConfigRcPtr *config, float *rgb) override;
  void configGetXYZtoSceneLinear(OCIO_ConstConfigRcPtr *config,
                                 float xyz_to_scene_linear[3][3]) override;

  int configGetNumLooks(OCIO_ConstConfigRcPtr *config) override;
  const char *configGetLookNameByIndex(OCIO_ConstConfigRcPtr *config, int index) override;
  OCIO_ConstLookRcPtr *configGetLook(OCIO_ConstConfigRcPtr *config, const char *name) override;

  const char *lookGetProcessSpace(OCIO_ConstLookRcPtr *look) override;
  void lookRelease(OCIO_ConstLookRcPtr *look) override;

  OCIO_ConstProcessorRcPtr *configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config,
                                                        const char *srcName,
                                                        const char *dstName) override;
  void processorRelease(OCIO_ConstProcessorRcPtr *processor) override;

  OCIO_ConstCPUProcessorRcPtr *processorGetCPUProcessor(
      OCIO_ConstProcessorRcPtr *processor) override;
  bool cpuProcessorIsNoOp(OCIO_ConstCPUProcessorRcPtr *cpu_processor) override;
  void cpuProcessorApply(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                         OCIO_PackedImageDesc *img) override;
  void cpuProcessorApply_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                   OCIO_PackedImageDesc *img) override;
  void cpuProcessorApplyRGB(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel) override;
  void cpuProcessorApplyRGBA(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel) override;
  void cpuProcessorApplyRGBA_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                       float *pixel) override;
  void cpuProcessorRelease(OCIO_ConstCPUProcessorRcPtr *cpu_processor) override;

  const char *colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs) override;
  const char *colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr *cs) override;
  const char *colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr *cs) override;
  int colorSpaceGetNumAliases(OCIO_ConstColorSpaceRcPtr *cs) override;
  const char *colorSpaceGetAlias(OCIO_ConstColorSpaceRcPtr *cs, const int index) override;

  OCIO_ConstProcessorRcPtr *createDisplayProcessor(OCIO_ConstConfigRcPtr *config,
                                                   const char *input,
                                                   const char *view,
                                                   const char *display,
                                                   const char *look,
                                                   const float scale,
                                                   const float exponent,
                                                   const float temperature,
                                                   const float tint,
                                                   const bool use_white_balance,
                                                   const bool inverse) override;

  OCIO_PackedImageDesc *createOCIO_PackedImageDesc(float *data,
                                                   long width,
                                                   long height,
                                                   long numChannels,
                                                   long chanStrideBytes,
                                                   long xStrideBytes,
                                                   long yStrideBytes) override;

  void OCIO_PackedImageDescRelease(OCIO_PackedImageDesc *id) override;

  bool supportGPUShader() override;
  bool gpuDisplayShaderBind(OCIO_ConstConfigRcPtr *config,
                            const char *input,
                            const char *view,
                            const char *display,
                            const char *look,
                            OCIO_CurveMappingSettings *curve_mapping_settings,
                            const float scale,
                            const float exponent,
                            const float dither,
                            const float temperature,
                            const float tint,
                            const bool use_predivide,
                            const bool use_overlay,
                            const bool use_hdr,
                            const bool use_white_balance) override;
  void gpuDisplayShaderUnbind() override;
  void gpuCacheFree() override;

  const char *getVersionString() override;
  int getVersionHex() override;
};
#endif

#endif /* OCIO_IMPL_H */

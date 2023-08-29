/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cassert>
#include <iostream>
#include <math.h>
#include <sstream>
#include <string.h>

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4251 4275)
#endif
#include <OpenColorIO/OpenColorIO.h>
#ifdef _MSC_VER
#  pragma warning(pop)
#endif

using namespace OCIO_NAMESPACE;

#include "MEM_guardedalloc.h"

#include "BLI_math_color.h"
#include "BLI_math_matrix.h"

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
  ConstConfigRcPtr *config = MEM_new<ConstConfigRcPtr>(__func__);

  try {
    *config = GetCurrentConfig();

    if (*config)
      return (OCIO_ConstConfigRcPtr *)config;
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  MEM_delete(config);

  return NULL;
}

void OCIOImpl::setCurrentConfig(const OCIO_ConstConfigRcPtr *config)
{
  try {
    SetCurrentConfig(*(ConstConfigRcPtr *)config);
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }
}

OCIO_ConstConfigRcPtr *OCIOImpl::configCreateFromEnv(void)
{
  ConstConfigRcPtr *config = MEM_new<ConstConfigRcPtr>(__func__);

  try {
    *config = Config::CreateFromEnv();

    if (*config)
      return (OCIO_ConstConfigRcPtr *)config;
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  MEM_delete(config);

  return NULL;
}

OCIO_ConstConfigRcPtr *OCIOImpl::configCreateFromFile(const char *filename)
{
  ConstConfigRcPtr *config = MEM_new<ConstConfigRcPtr>(__func__);

  try {
    *config = Config::CreateFromFile(filename);

    if (*config)
      return (OCIO_ConstConfigRcPtr *)config;
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  MEM_delete(config);

  return NULL;
}

void OCIOImpl::configRelease(OCIO_ConstConfigRcPtr *config)
{
  MEM_delete((ConstConfigRcPtr *)config);
}

int OCIOImpl::configGetNumColorSpaces(OCIO_ConstConfigRcPtr *config)
{
  try {
    return (*(ConstConfigRcPtr *)config)->getNumColorSpaces();
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return 0;
}

const char *OCIOImpl::configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr *config, int index)
{
  try {
    return (*(ConstConfigRcPtr *)config)->getColorSpaceNameByIndex(index);
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return NULL;
}

OCIO_ConstColorSpaceRcPtr *OCIOImpl::configGetColorSpace(OCIO_ConstConfigRcPtr *config,
                                                         const char *name)
{
  ConstColorSpaceRcPtr *cs = MEM_new<ConstColorSpaceRcPtr>(__func__);

  try {
    *cs = (*(ConstConfigRcPtr *)config)->getColorSpace(name);

    if (*cs)
      return (OCIO_ConstColorSpaceRcPtr *)cs;
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  MEM_delete(cs);

  return NULL;
}

int OCIOImpl::configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name)
{
  try {
    return (*(ConstConfigRcPtr *)config)->getIndexForColorSpace(name);
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return -1;
}

const char *OCIOImpl::configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config)
{
  try {
    return (*(ConstConfigRcPtr *)config)->getDefaultDisplay();
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return NULL;
}

int OCIOImpl::configGetNumDisplays(OCIO_ConstConfigRcPtr *config)
{
  try {
    return (*(ConstConfigRcPtr *)config)->getNumDisplays();
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return 0;
}

const char *OCIOImpl::configGetDisplay(OCIO_ConstConfigRcPtr *config, int index)
{
  try {
    return (*(ConstConfigRcPtr *)config)->getDisplay(index);
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return NULL;
}

const char *OCIOImpl::configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display)
{
  try {
    return (*(ConstConfigRcPtr *)config)->getDefaultView(display);
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return NULL;
}

int OCIOImpl::configGetNumViews(OCIO_ConstConfigRcPtr *config, const char *display)
{
  try {
    return (*(ConstConfigRcPtr *)config)->getNumViews(display);
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return 0;
}

const char *OCIOImpl::configGetView(OCIO_ConstConfigRcPtr *config, const char *display, int index)
{
  try {
    return (*(ConstConfigRcPtr *)config)->getView(display, index);
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return NULL;
}

const char *OCIOImpl::configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr *config,
                                                     const char *display,
                                                     const char *view)
{
  try {
    const char *name = (*(ConstConfigRcPtr *)config)->getDisplayViewColorSpaceName(display, view);
    /* OpenColorIO does not resolve this token for us, so do it ourselves. */
    if (strcasecmp(name, "<USE_DISPLAY_NAME>") == 0) {
      return display;
    }
    return name;
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return NULL;
}

void OCIOImpl::configGetDefaultLumaCoefs(OCIO_ConstConfigRcPtr *config, float *rgb)
{
  try {
    double rgb_double[3];
    (*(ConstConfigRcPtr *)config)->getDefaultLumaCoefs(rgb_double);
    rgb[0] = rgb_double[0];
    rgb[1] = rgb_double[1];
    rgb[2] = rgb_double[2];
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }
}

static bool to_scene_linear_matrix(ConstConfigRcPtr &config,
                                   const char *colorspace,
                                   float to_scene_linear[3][3])
{
  ConstProcessorRcPtr processor;
  try {
    processor = config->getProcessor(colorspace, ROLE_SCENE_LINEAR);
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
    return false;
  }

  if (!processor) {
    return false;
  }

  ConstCPUProcessorRcPtr cpu_processor = processor->getDefaultCPUProcessor();
  if (!cpu_processor) {
    return false;
  }

  unit_m3(to_scene_linear);
  cpu_processor->applyRGB(to_scene_linear[0]);
  cpu_processor->applyRGB(to_scene_linear[1]);
  cpu_processor->applyRGB(to_scene_linear[2]);
  return true;
}

void OCIOImpl::configGetXYZtoSceneLinear(OCIO_ConstConfigRcPtr *config_,
                                         float xyz_to_scene_linear[3][3])
{
  ConstConfigRcPtr config = (*(ConstConfigRcPtr *)config_);

  /* Default to ITU-BT.709 in case no appropriate transform found.
   * Note XYZ is defined here as having a D65 white point. */
  memcpy(xyz_to_scene_linear, OCIO_XYZ_TO_REC709, sizeof(OCIO_XYZ_TO_REC709));

  /* Get from OpenColorO config if it has the required roles. */
  if (!config->hasRole(ROLE_SCENE_LINEAR)) {
    return;
  }

  if (config->hasRole("aces_interchange")) {
    /* Standard OpenColorIO role, defined as ACES AP0 (ACES2065-1). */
    float aces_to_scene_linear[3][3];
    if (to_scene_linear_matrix(config, "aces_interchange", aces_to_scene_linear)) {
      float xyz_to_aces[3][3];
      invert_m3_m3(xyz_to_aces, OCIO_ACES_TO_XYZ);

      mul_m3_m3m3(xyz_to_scene_linear, aces_to_scene_linear, xyz_to_aces);
    }
  }
  else if (config->hasRole("XYZ")) {
    /* Custom role used before the standard existed. */
    to_scene_linear_matrix(config, "XYZ", xyz_to_scene_linear);
  }
}

int OCIOImpl::configGetNumLooks(OCIO_ConstConfigRcPtr *config)
{
  try {
    return (*(ConstConfigRcPtr *)config)->getNumLooks();
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return 0;
}

const char *OCIOImpl::configGetLookNameByIndex(OCIO_ConstConfigRcPtr *config, int index)
{
  try {
    return (*(ConstConfigRcPtr *)config)->getLookNameByIndex(index);
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return NULL;
}

OCIO_ConstLookRcPtr *OCIOImpl::configGetLook(OCIO_ConstConfigRcPtr *config, const char *name)
{
  ConstLookRcPtr *look = MEM_new<ConstLookRcPtr>(__func__);

  try {
    *look = (*(ConstConfigRcPtr *)config)->getLook(name);

    if (*look)
      return (OCIO_ConstLookRcPtr *)look;
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  MEM_delete(look);

  return NULL;
}

const char *OCIOImpl::lookGetProcessSpace(OCIO_ConstLookRcPtr *look)
{
  return (*(ConstLookRcPtr *)look)->getProcessSpace();
}

void OCIOImpl::lookRelease(OCIO_ConstLookRcPtr *look)
{
  MEM_delete((ConstLookRcPtr *)look);
}

int OCIOImpl::colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr *cs_)
{
  ConstColorSpaceRcPtr *cs = (ConstColorSpaceRcPtr *)cs_;
  const char *family = (*cs)->getFamily();

  if (!strcmp(family, "rrt") || !strcmp(family, "display")) {
    /* assume display and rrt transformations are not invertible in fact some of them could be,
     * but it doesn't make much sense to allow use them as invertible. */
    return false;
  }

  if ((*cs)->isData()) {
    /* data color spaces don't have transformation at all */
    return true;
  }

  if ((*cs)->getTransform(COLORSPACE_DIR_TO_REFERENCE)) {
    /* if there's defined transform to reference space,
     * color space could be converted to scene linear. */
    return true;
  }

  return true;
}

int OCIOImpl::colorSpaceIsData(OCIO_ConstColorSpaceRcPtr *cs)
{
  return (*(ConstColorSpaceRcPtr *)cs)->isData();
}

static float compare_floats(float a, float b, float abs_diff, int ulp_diff)
{
  /* Returns true if the absolute difference is smaller than abs_diff (for numbers near zero)
   * or their relative difference is less than ulp_diff ULPs. Based on:
   * https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/ */
  if (fabsf(a - b) < abs_diff) {
    return true;
  }

  if ((a < 0.0f) != (b < 0.0f)) {
    return false;
  }

  return (abs((*(int *)&a) - (*(int *)&b)) < ulp_diff);
}

void OCIOImpl::colorSpaceIsBuiltin(OCIO_ConstConfigRcPtr *config_,
                                   OCIO_ConstColorSpaceRcPtr *cs_,
                                   bool &is_scene_linear,
                                   bool &is_srgb)
{
  ConstConfigRcPtr *config = (ConstConfigRcPtr *)config_;
  ConstColorSpaceRcPtr *cs = (ConstColorSpaceRcPtr *)cs_;
  ConstProcessorRcPtr processor;

  try {
    processor = (*config)->getProcessor((*cs)->getName(), "scene_linear");
  }
  catch (Exception &) {
    /* Silently ignore if no conversion possible, then it's not scene linear or sRGB. */
    is_scene_linear = false;
    is_srgb = false;
    return;
  }

  ConstCPUProcessorRcPtr cpu_processor = processor->getDefaultCPUProcessor();

  is_scene_linear = true;
  is_srgb = true;
  for (int i = 0; i < 256; i++) {
    float v = i / 255.0f;

    float cR[3] = {v, 0, 0};
    float cG[3] = {0, v, 0};
    float cB[3] = {0, 0, v};
    float cW[3] = {v, v, v};
    cpu_processor->applyRGB(cR);
    cpu_processor->applyRGB(cG);
    cpu_processor->applyRGB(cB);
    cpu_processor->applyRGB(cW);

    /* Make sure that there is no channel crosstalk. */
    if (fabsf(cR[1]) > 1e-5f || fabsf(cR[2]) > 1e-5f || fabsf(cG[0]) > 1e-5f ||
        fabsf(cG[2]) > 1e-5f || fabsf(cB[0]) > 1e-5f || fabsf(cB[1]) > 1e-5f)
    {
      is_scene_linear = false;
      is_srgb = false;
      break;
    }
    /* Make sure that the three primaries combine linearly. */
    if (!compare_floats(cR[0], cW[0], 1e-6f, 64) || !compare_floats(cG[1], cW[1], 1e-6f, 64) ||
        !compare_floats(cB[2], cW[2], 1e-6f, 64))
    {
      is_scene_linear = false;
      is_srgb = false;
      break;
    }
    /* Make sure that the three channels behave identically. */
    if (!compare_floats(cW[0], cW[1], 1e-6f, 64) || !compare_floats(cW[1], cW[2], 1e-6f, 64)) {
      is_scene_linear = false;
      is_srgb = false;
      break;
    }

    float out_v = (cW[0] + cW[1] + cW[2]) * (1.0f / 3.0f);
    if (!compare_floats(v, out_v, 1e-6f, 64)) {
      is_scene_linear = false;
    }
    if (!compare_floats(srgb_to_linearrgb(v), out_v, 1e-4f, 64)) {
      is_srgb = false;
    }
  }
}

void OCIOImpl::colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs)
{
  MEM_delete((ConstColorSpaceRcPtr *)cs);
}

OCIO_ConstProcessorRcPtr *OCIOImpl::configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config,
                                                                const char *srcName,
                                                                const char *dstName)
{
  ConstProcessorRcPtr *processor = MEM_new<ConstProcessorRcPtr>(__func__);

  try {
    *processor = (*(ConstConfigRcPtr *)config)->getProcessor(srcName, dstName);

    if (*processor)
      return (OCIO_ConstProcessorRcPtr *)processor;
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  MEM_delete(processor);

  return 0;
}

void OCIOImpl::processorRelease(OCIO_ConstProcessorRcPtr *processor)
{
  MEM_delete(processor);
}

OCIO_ConstCPUProcessorRcPtr *OCIOImpl::processorGetCPUProcessor(
    OCIO_ConstProcessorRcPtr *processor)
{
  ConstCPUProcessorRcPtr *cpu_processor = MEM_new<ConstCPUProcessorRcPtr>(__func__);
  *cpu_processor = (*(ConstProcessorRcPtr *)processor)->getDefaultCPUProcessor();
  return (OCIO_ConstCPUProcessorRcPtr *)cpu_processor;
}

void OCIOImpl::cpuProcessorApply(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                 OCIO_PackedImageDesc *img)
{
  try {
    (*(ConstCPUProcessorRcPtr *)cpu_processor)->apply(*(PackedImageDesc *)img);
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }
}

void OCIOImpl::cpuProcessorApply_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                           OCIO_PackedImageDesc *img_)
{
  try {
    PackedImageDesc *img = (PackedImageDesc *)img_;
    int channels = img->getNumChannels();

    if (channels == 4) {
      assert(img->isFloat());
      float *pixels = (float *)img->getData();

      size_t width = img->getWidth();
      size_t height = img->getHeight();

      for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
          float *pixel = pixels + 4 * (y * width + x);

          cpuProcessorApplyRGBA_predivide(cpu_processor, pixel);
        }
      }
    }
    else {
      (*(ConstCPUProcessorRcPtr *)cpu_processor)->apply(*img);
    }
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }
}

bool OCIOImpl::cpuProcessorIsNoOp(OCIO_ConstCPUProcessorRcPtr *cpu_processor)
{
  return (*(ConstCPUProcessorRcPtr *)cpu_processor)->isNoOp();
}

void OCIOImpl::cpuProcessorApplyRGB(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel)
{
  (*(ConstCPUProcessorRcPtr *)cpu_processor)->applyRGB(pixel);
}

void OCIOImpl::cpuProcessorApplyRGBA(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel)
{
  (*(ConstCPUProcessorRcPtr *)cpu_processor)->applyRGBA(pixel);
}

void OCIOImpl::cpuProcessorApplyRGBA_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                               float *pixel)
{
  if (pixel[3] == 1.0f || pixel[3] == 0.0f) {
    (*(ConstCPUProcessorRcPtr *)cpu_processor)->applyRGBA(pixel);
  }
  else {
    float alpha, inv_alpha;

    alpha = pixel[3];
    inv_alpha = 1.0f / alpha;

    pixel[0] *= inv_alpha;
    pixel[1] *= inv_alpha;
    pixel[2] *= inv_alpha;

    (*(ConstCPUProcessorRcPtr *)cpu_processor)->applyRGBA(pixel);

    pixel[0] *= alpha;
    pixel[1] *= alpha;
    pixel[2] *= alpha;
  }
}

void OCIOImpl::cpuProcessorRelease(OCIO_ConstCPUProcessorRcPtr *cpu_processor)
{
  MEM_delete(cpu_processor);
}

const char *OCIOImpl::colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs)
{
  return (*(ConstColorSpaceRcPtr *)cs)->getName();
}

const char *OCIOImpl::colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr *cs)
{
  return (*(ConstColorSpaceRcPtr *)cs)->getDescription();
}

const char *OCIOImpl::colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr *cs)
{
  return (*(ConstColorSpaceRcPtr *)cs)->getFamily();
}

int OCIOImpl::colorSpaceGetNumAliases(OCIO_ConstColorSpaceRcPtr *cs)
{
  return (*(ConstColorSpaceRcPtr *)cs)->getNumAliases();
}
const char *OCIOImpl::colorSpaceGetAlias(OCIO_ConstColorSpaceRcPtr *cs, const int index)
{
  return (*(ConstColorSpaceRcPtr *)cs)->getAlias(index);
}

OCIO_ConstProcessorRcPtr *OCIOImpl::createDisplayProcessor(OCIO_ConstConfigRcPtr *config_,
                                                           const char *input,
                                                           const char *view,
                                                           const char *display,
                                                           const char *look,
                                                           const float scale,
                                                           const float exponent,
                                                           const bool inverse)

{
  ConstConfigRcPtr config = *(ConstConfigRcPtr *)config_;
  GroupTransformRcPtr group = GroupTransform::Create();

  /* Exposure. */
  if (scale != 1.0f) {
    /* Always apply exposure in scene linear. */
    ColorSpaceTransformRcPtr ct = ColorSpaceTransform::Create();
    ct->setSrc(input);
    ct->setDst(ROLE_SCENE_LINEAR);
    group->appendTransform(ct);

    /* Make further transforms aware of the color space change. */
    input = ROLE_SCENE_LINEAR;

    /* Apply scale. */
    MatrixTransformRcPtr mt = MatrixTransform::Create();
    const double matrix[16] = {
        scale, 0.0, 0.0, 0.0, 0.0, scale, 0.0, 0.0, 0.0, 0.0, scale, 0.0, 0.0, 0.0, 0.0, 1.0};
    mt->setMatrix(matrix);
    group->appendTransform(mt);
  }

  /* Add look transform. */
  bool use_look = (look != nullptr && look[0] != 0);
  if (use_look) {
    const char *look_output = LookTransform::GetLooksResultColorSpace(
        config, config->getCurrentContext(), look);

    if (look_output != nullptr && look_output[0] != 0) {
      LookTransformRcPtr lt = LookTransform::Create();
      lt->setSrc(input);
      lt->setDst(look_output);
      lt->setLooks(look);
      group->appendTransform(lt);

      /* Make further transforms aware of the color space change. */
      input = look_output;
    }
    else {
      /* For empty looks, no output color space is returned. */
      use_look = false;
    }
  }

  /* Add view and display transform. */
  DisplayViewTransformRcPtr dvt = DisplayViewTransform::Create();
  dvt->setSrc(input);
  dvt->setLooksBypass(use_look);
  dvt->setView(view);
  dvt->setDisplay(display);
  group->appendTransform(dvt);

  /* Gamma. */
  if (exponent != 1.0f) {
    ExponentTransformRcPtr et = ExponentTransform::Create();
    const double value[4] = {exponent, exponent, exponent, 1.0};
    et->setValue(value);
    group->appendTransform(et);
  }

  if (inverse) {
    group->setDirection(TRANSFORM_DIR_INVERSE);
  }

  /* Create processor from transform. This is the moment were OCIO validates
   * the entire transform, no need to check for the validity of inputs above. */
  ConstProcessorRcPtr *p = MEM_new<ConstProcessorRcPtr>(__func__);

  try {
    *p = config->getProcessor(group);

    if (*p)
      return (OCIO_ConstProcessorRcPtr *)p;
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  MEM_delete(p);
  return NULL;
}

OCIO_PackedImageDesc *OCIOImpl::createOCIO_PackedImageDesc(float *data,
                                                           long width,
                                                           long height,
                                                           long numChannels,
                                                           long chanStrideBytes,
                                                           long xStrideBytes,
                                                           long yStrideBytes)
{
  try {
    void *mem = MEM_mallocN(sizeof(PackedImageDesc), __func__);
    PackedImageDesc *id = new (mem) PackedImageDesc(data,
                                                    width,
                                                    height,
                                                    numChannels,
                                                    BIT_DEPTH_F32,
                                                    chanStrideBytes,
                                                    xStrideBytes,
                                                    yStrideBytes);

    return (OCIO_PackedImageDesc *)id;
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return NULL;
}

void OCIOImpl::OCIO_PackedImageDescRelease(OCIO_PackedImageDesc *id)
{
  MEM_delete((PackedImageDesc *)id);
}

const char *OCIOImpl::getVersionString(void)
{
  return GetVersion();
}

int OCIOImpl::getVersionHex(void)
{
  return GetVersionHex();
}

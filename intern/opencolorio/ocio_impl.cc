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

/* NOTE: This is because OCIO 1.1.0 has a bug which makes default
 * display to be the one which is first alphabetically.
 *
 * Fix has been submitted as a patch
 *   https://github.com/imageworks/OpenColorIO/pull/638
 *
 * For until then we use first usable display instead. */
#define DEFAULT_DISPLAY_WORKAROUND
#ifdef DEFAULT_DISPLAY_WORKAROUND
#  include <algorithm>
#  include <map>
#  include <mutex>
#  include <vector>
#  include <string>
#  include <set>
using std::map;
using std::set;
using std::string;
using std::vector;
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
      return (OCIO_ConstConfigRcPtr *)config;
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
    SetCurrentConfig(*(ConstConfigRcPtr *)config);
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
      return (OCIO_ConstConfigRcPtr *)config;
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
      return (OCIO_ConstConfigRcPtr *)config;
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  OBJECT_GUARDED_DELETE(config, ConstConfigRcPtr);

  return NULL;
}

void OCIOImpl::configRelease(OCIO_ConstConfigRcPtr *config)
{
  OBJECT_GUARDED_DELETE((ConstConfigRcPtr *)config, ConstConfigRcPtr);
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
  ConstColorSpaceRcPtr *cs = OBJECT_GUARDED_NEW(ConstColorSpaceRcPtr);

  try {
    *cs = (*(ConstConfigRcPtr *)config)->getColorSpace(name);

    if (*cs)
      return (OCIO_ConstColorSpaceRcPtr *)cs;
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
    return (*(ConstConfigRcPtr *)config)->getIndexForColorSpace(name);
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return -1;
}

const char *OCIOImpl::configGetDefaultDisplay(OCIO_ConstConfigRcPtr *config)
{
#ifdef DEFAULT_DISPLAY_WORKAROUND
  if (getenv("OCIO_ACTIVE_DISPLAYS") == NULL) {
    const char *active_displays = (*(ConstConfigRcPtr *)config)->getActiveDisplays();
    if (active_displays[0] != '\0') {
      const char *separator_pos = strchr(active_displays, ',');
      if (separator_pos == NULL) {
        return active_displays;
      }
      static std::string active_display;
      /* NOTE: Configuration is shared and is never changed during
       * runtime, so we only guarantee two threads don't initialize at the
       * same. */
      static std::mutex mutex;
      mutex.lock();
      if (active_display.empty()) {
        active_display = active_displays;
        active_display[separator_pos - active_displays] = '\0';
      }
      mutex.unlock();
      return active_display.c_str();
    }
  }
#endif

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

#ifdef DEFAULT_DISPLAY_WORKAROUND
namespace {

void splitStringEnvStyle(vector<string> *tokens, const string &str)
{
  tokens->clear();
  const int len = str.length();
  int token_start = 0, token_length = 0;
  for (int i = 0; i < len; ++i) {
    const char ch = str[i];
    if (ch != ',' && ch != ':') {
      /* Append non-separator char to a token. */
      ++token_length;
    }
    else {
      /* Append current token to the list (if any). */
      if (token_length > 0) {
        string token = str.substr(token_start, token_length);
        tokens->push_back(token);
      }
      /* Re-set token pointers. */
      token_start = i + 1;
      token_length = 0;
    }
  }
  /* Append token which might be at the end of the string. */
  if (token_length != 0) {
    string token = str.substr(token_start, token_length);
    tokens->push_back(token);
  }
}

string stringToLower(const string &str)
{
  string lower = str;
  std::transform(lower.begin(), lower.end(), lower.begin(), tolower);
  return lower;
}

}  // namespace
#endif

const char *OCIOImpl::configGetDefaultView(OCIO_ConstConfigRcPtr *config, const char *display)
{
#ifdef DEFAULT_DISPLAY_WORKAROUND
  /* NOTE: We assume that first active view always exists for a default
   * display. */
  if (getenv("OCIO_ACTIVE_VIEWS") == NULL) {
    ConstConfigRcPtr config_ptr = *((ConstConfigRcPtr *)config);
    const char *active_views_encoded = config_ptr->getActiveViews();
    if (active_views_encoded[0] != '\0') {
      const string display_lower = stringToLower(display);
      static map<string, string> default_display_views;
      static std::mutex mutex;
      mutex.lock();
      /* Check if the view is already known. */
      map<string, string>::const_iterator it = default_display_views.find(display_lower);
      if (it != default_display_views.end()) {
        mutex.unlock();
        return it->second.c_str();
      }
      /* Active views. */
      vector<string> active_views;
      splitStringEnvStyle(&active_views, active_views_encoded);
      /* Get all views supported by tge display. */
      set<string> display_views;
      const int num_display_views = config_ptr->getNumViews(display);
      for (int view_index = 0; view_index < num_display_views; ++view_index) {
        const char *view = config_ptr->getView(display, view_index);
        display_views.insert(stringToLower(view));
      }
      /* Get first view which is supported by tge display. */
      for (const string &view : active_views) {
        const string view_lower = stringToLower(view);
        if (display_views.find(view_lower) != display_views.end()) {
          default_display_views[display_lower] = view;
          mutex.unlock();
          return default_display_views[display_lower].c_str();
        }
      }
      mutex.unlock();
    }
  }
#endif
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
    return (*(ConstConfigRcPtr *)config)->getDisplayColorSpaceName(display, view);
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return NULL;
}

void OCIOImpl::configGetDefaultLumaCoefs(OCIO_ConstConfigRcPtr *config, float *rgb)
{
  try {
    (*(ConstConfigRcPtr *)config)->getDefaultLumaCoefs(rgb);
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }
}

void OCIOImpl::configGetXYZtoRGB(OCIO_ConstConfigRcPtr *config_, float xyz_to_rgb[3][3])
{
  ConstConfigRcPtr config = (*(ConstConfigRcPtr *)config_);

  /* Default to ITU-BT.709 in case no appropriate transform found. */
  memcpy(xyz_to_rgb, OCIO_XYZ_TO_LINEAR_SRGB, sizeof(OCIO_XYZ_TO_LINEAR_SRGB));

  /* Auto estimate from XYZ and scene_linear roles, assumed to be a linear transform. */
  if (config->hasRole("XYZ") && config->hasRole("scene_linear")) {
    ConstProcessorRcPtr to_rgb_processor = config->getProcessor("XYZ", "scene_linear");
    if (to_rgb_processor) {
      xyz_to_rgb[0][0] = 1.0f;
      xyz_to_rgb[0][1] = 0.0f;
      xyz_to_rgb[0][2] = 0.0f;
      xyz_to_rgb[1][0] = 0.0f;
      xyz_to_rgb[1][1] = 1.0f;
      xyz_to_rgb[1][2] = 0.0f;
      xyz_to_rgb[2][0] = 0.0f;
      xyz_to_rgb[2][1] = 0.0f;
      xyz_to_rgb[2][2] = 1.0f;
      to_rgb_processor->applyRGB(xyz_to_rgb[0]);
      to_rgb_processor->applyRGB(xyz_to_rgb[1]);
      to_rgb_processor->applyRGB(xyz_to_rgb[2]);
    }
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
  ConstLookRcPtr *look = OBJECT_GUARDED_NEW(ConstLookRcPtr);

  try {
    *look = (*(ConstConfigRcPtr *)config)->getLook(name);

    if (*look)
      return (OCIO_ConstLookRcPtr *)look;
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  OBJECT_GUARDED_DELETE(look, ConstLookRcPtr);

  return NULL;
}

const char *OCIOImpl::lookGetProcessSpace(OCIO_ConstLookRcPtr *look)
{
  return (*(ConstLookRcPtr *)look)->getProcessSpace();
}

void OCIOImpl::lookRelease(OCIO_ConstLookRcPtr *look)
{
  OBJECT_GUARDED_DELETE((ConstLookRcPtr *)look, ConstLookRcPtr);
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

  is_scene_linear = true;
  is_srgb = true;
  for (int i = 0; i < 256; i++) {
    float v = i / 255.0f;

    float cR[3] = {v, 0, 0};
    float cG[3] = {0, v, 0};
    float cB[3] = {0, 0, v};
    float cW[3] = {v, v, v};
    processor->applyRGB(cR);
    processor->applyRGB(cG);
    processor->applyRGB(cB);
    processor->applyRGB(cW);

    /* Make sure that there is no channel crosstalk. */
    if (fabsf(cR[1]) > 1e-5f || fabsf(cR[2]) > 1e-5f || fabsf(cG[0]) > 1e-5f ||
        fabsf(cG[2]) > 1e-5f || fabsf(cB[0]) > 1e-5f || fabsf(cB[1]) > 1e-5f) {
      is_scene_linear = false;
      is_srgb = false;
      break;
    }
    /* Make sure that the three primaries combine linearly. */
    if (!compare_floats(cR[0], cW[0], 1e-6f, 64) || !compare_floats(cG[1], cW[1], 1e-6f, 64) ||
        !compare_floats(cB[2], cW[2], 1e-6f, 64)) {
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
    if (!compare_floats(srgb_to_linearrgb(v), out_v, 1e-6f, 64)) {
      is_srgb = false;
    }
  }
}

void OCIOImpl::colorSpaceRelease(OCIO_ConstColorSpaceRcPtr *cs)
{
  OBJECT_GUARDED_DELETE((ConstColorSpaceRcPtr *)cs, ConstColorSpaceRcPtr);
}

OCIO_ConstProcessorRcPtr *OCIOImpl::configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config,
                                                                const char *srcName,
                                                                const char *dstName)
{
  ConstProcessorRcPtr *p = OBJECT_GUARDED_NEW(ConstProcessorRcPtr);

  try {
    *p = (*(ConstConfigRcPtr *)config)->getProcessor(srcName, dstName);

    if (*p)
      return (OCIO_ConstProcessorRcPtr *)p;
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  OBJECT_GUARDED_DELETE(p, ConstProcessorRcPtr);

  return 0;
}

OCIO_ConstProcessorRcPtr *OCIOImpl::configGetProcessor(OCIO_ConstConfigRcPtr *config,
                                                       OCIO_ConstTransformRcPtr *transform)
{
  ConstProcessorRcPtr *p = OBJECT_GUARDED_NEW(ConstProcessorRcPtr);

  try {
    *p = (*(ConstConfigRcPtr *)config)->getProcessor(*(ConstTransformRcPtr *)transform);

    if (*p)
      return (OCIO_ConstProcessorRcPtr *)p;
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
    (*(ConstProcessorRcPtr *)processor)->apply(*(PackedImageDesc *)img);
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }
}

void OCIOImpl::processorApply_predivide(OCIO_ConstProcessorRcPtr *processor,
                                        OCIO_PackedImageDesc *img_)
{
  try {
    PackedImageDesc *img = (PackedImageDesc *)img_;
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
      (*(ConstProcessorRcPtr *)processor)->apply(*img);
    }
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }
}

void OCIOImpl::processorApplyRGB(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
  (*(ConstProcessorRcPtr *)processor)->applyRGB(pixel);
}

void OCIOImpl::processorApplyRGBA(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
  (*(ConstProcessorRcPtr *)processor)->applyRGBA(pixel);
}

void OCIOImpl::processorApplyRGBA_predivide(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
  if (pixel[3] == 1.0f || pixel[3] == 0.0f) {
    (*(ConstProcessorRcPtr *)processor)->applyRGBA(pixel);
  }
  else {
    float alpha, inv_alpha;

    alpha = pixel[3];
    inv_alpha = 1.0f / alpha;

    pixel[0] *= inv_alpha;
    pixel[1] *= inv_alpha;
    pixel[2] *= inv_alpha;

    (*(ConstProcessorRcPtr *)processor)->applyRGBA(pixel);

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

OCIO_DisplayTransformRcPtr *OCIOImpl::createDisplayTransform(void)
{
  DisplayTransformRcPtr *dt = OBJECT_GUARDED_NEW(DisplayTransformRcPtr);

  *dt = DisplayTransform::Create();

  return (OCIO_DisplayTransformRcPtr *)dt;
}

void OCIOImpl::displayTransformSetInputColorSpaceName(OCIO_DisplayTransformRcPtr *dt,
                                                      const char *name)
{
  (*(DisplayTransformRcPtr *)dt)->setInputColorSpaceName(name);
}

void OCIOImpl::displayTransformSetDisplay(OCIO_DisplayTransformRcPtr *dt, const char *name)
{
  (*(DisplayTransformRcPtr *)dt)->setDisplay(name);
}

void OCIOImpl::displayTransformSetView(OCIO_DisplayTransformRcPtr *dt, const char *name)
{
  (*(DisplayTransformRcPtr *)dt)->setView(name);
}

void OCIOImpl::displayTransformSetDisplayCC(OCIO_DisplayTransformRcPtr *dt,
                                            OCIO_ConstTransformRcPtr *t)
{
  (*(DisplayTransformRcPtr *)dt)->setDisplayCC(*(ConstTransformRcPtr *)t);
}

void OCIOImpl::displayTransformSetLinearCC(OCIO_DisplayTransformRcPtr *dt,
                                           OCIO_ConstTransformRcPtr *t)
{
  (*(DisplayTransformRcPtr *)dt)->setLinearCC(*(ConstTransformRcPtr *)t);
}

void OCIOImpl::displayTransformSetLooksOverride(OCIO_DisplayTransformRcPtr *dt, const char *looks)
{
  (*(DisplayTransformRcPtr *)dt)->setLooksOverride(looks);
}

void OCIOImpl::displayTransformSetLooksOverrideEnabled(OCIO_DisplayTransformRcPtr *dt,
                                                       bool enabled)
{
  (*(DisplayTransformRcPtr *)dt)->setLooksOverrideEnabled(enabled);
}

void OCIOImpl::displayTransformRelease(OCIO_DisplayTransformRcPtr *dt)
{
  OBJECT_GUARDED_DELETE((DisplayTransformRcPtr *)dt, DisplayTransformRcPtr);
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
    PackedImageDesc *id = new (mem) PackedImageDesc(
        data, width, height, numChannels, chanStrideBytes, xStrideBytes, yStrideBytes);

    return (OCIO_PackedImageDesc *)id;
  }
  catch (Exception &exception) {
    OCIO_reportException(exception);
  }

  return NULL;
}

void OCIOImpl::OCIO_PackedImageDescRelease(OCIO_PackedImageDesc *id)
{
  OBJECT_GUARDED_DELETE((PackedImageDesc *)id, PackedImageDesc);
}

OCIO_ExponentTransformRcPtr *OCIOImpl::createExponentTransform(void)
{
  ExponentTransformRcPtr *et = OBJECT_GUARDED_NEW(ExponentTransformRcPtr);

  *et = ExponentTransform::Create();

  return (OCIO_ExponentTransformRcPtr *)et;
}

void OCIOImpl::exponentTransformSetValue(OCIO_ExponentTransformRcPtr *et, const float *exponent)
{
  (*(ExponentTransformRcPtr *)et)->setValue(exponent);
}

void OCIOImpl::exponentTransformRelease(OCIO_ExponentTransformRcPtr *et)
{
  OBJECT_GUARDED_DELETE((ExponentTransformRcPtr *)et, ExponentTransformRcPtr);
}

OCIO_MatrixTransformRcPtr *OCIOImpl::createMatrixTransform(void)
{
  MatrixTransformRcPtr *mt = OBJECT_GUARDED_NEW(MatrixTransformRcPtr);

  *mt = MatrixTransform::Create();

  return (OCIO_MatrixTransformRcPtr *)mt;
}

void OCIOImpl::matrixTransformSetValue(OCIO_MatrixTransformRcPtr *mt,
                                       const float *m44,
                                       const float *offset4)
{
  (*(MatrixTransformRcPtr *)mt)->setValue(m44, offset4);
}

void OCIOImpl::matrixTransformRelease(OCIO_MatrixTransformRcPtr *mt)
{
  OBJECT_GUARDED_DELETE((MatrixTransformRcPtr *)mt, MatrixTransformRcPtr);
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

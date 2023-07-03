/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>
#include <cstring>
#include <vector>

#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "MEM_guardedalloc.h"

#include "ocio_impl.h"

using std::max;

#define CONFIG_DEFAULT ((OCIO_ConstConfigRcPtr *)1)

enum TransformType {
  TRANSFORM_LINEAR_TO_SRGB,
  TRANSFORM_SRGB_TO_LINEAR,
  TRANSFORM_SCALE,
  TRANSFORM_EXPONENT,
  TRANSFORM_NONE,
  TRANSFORM_UNKNOWN,
};

#define COLORSPACE_LINEAR ((OCIO_ConstColorSpaceRcPtr *)1)
#define COLORSPACE_SRGB ((OCIO_ConstColorSpaceRcPtr *)2)
#define COLORSPACE_DATA ((OCIO_ConstColorSpaceRcPtr *)3)

struct OCIO_PackedImageDescription {
  float *data;
  long width;
  long height;
  long numChannels;
  long chanStrideBytes;
  long xStrideBytes;
  long yStrideBytes;
};

struct FallbackTransform {
  FallbackTransform() : type(TRANSFORM_UNKNOWN), scale(1.0f), exponent(1.0f) {}

  virtual ~FallbackTransform() {}

  void applyRGB(float *pixel)
  {
    if (type == TRANSFORM_LINEAR_TO_SRGB) {
      pixel[0] *= scale;
      pixel[1] *= scale;
      pixel[2] *= scale;

      linearrgb_to_srgb_v3_v3(pixel, pixel);

      pixel[0] = powf(max(0.0f, pixel[0]), exponent);
      pixel[1] = powf(max(0.0f, pixel[1]), exponent);
      pixel[2] = powf(max(0.0f, pixel[2]), exponent);
    }
    else if (type == TRANSFORM_SRGB_TO_LINEAR) {
      srgb_to_linearrgb_v3_v3(pixel, pixel);
    }
    else if (type == TRANSFORM_EXPONENT) {
      pixel[0] = powf(max(0.0f, pixel[0]), exponent);
      pixel[1] = powf(max(0.0f, pixel[1]), exponent);
      pixel[2] = powf(max(0.0f, pixel[2]), exponent);
    }
    else if (type == TRANSFORM_SCALE) {
      pixel[0] *= scale;
      pixel[1] *= scale;
      pixel[2] *= scale;
    }
  }

  void applyRGBA(float *pixel)
  {
    applyRGB(pixel);
  }

  TransformType type;
  /* Scale transform. */
  float scale;
  /* Exponent transform. */
  float exponent;

  MEM_CXX_CLASS_ALLOC_FUNCS("FallbackTransform");
};

struct FallbackProcessor {
  FallbackProcessor(const FallbackTransform &transform) : transform(transform) {}

  void applyRGB(float *pixel)
  {
    transform.applyRGB(pixel);
  }

  void applyRGBA(float *pixel)
  {
    transform.applyRGBA(pixel);
  }

  FallbackTransform transform;

  MEM_CXX_CLASS_ALLOC_FUNCS("FallbackProcessor");
};

OCIO_ConstConfigRcPtr *FallbackImpl::getCurrentConfig()
{
  return CONFIG_DEFAULT;
}

void FallbackImpl::setCurrentConfig(const OCIO_ConstConfigRcPtr * /*config*/) {}

OCIO_ConstConfigRcPtr *FallbackImpl::configCreateFromEnv()
{
  return NULL;
}

OCIO_ConstConfigRcPtr *FallbackImpl::configCreateFromFile(const char * /*filename*/)
{
  return CONFIG_DEFAULT;
}

void FallbackImpl::configRelease(OCIO_ConstConfigRcPtr * /*config*/) {}

int FallbackImpl::configGetNumColorSpaces(OCIO_ConstConfigRcPtr * /*config*/)
{
  return 2;
}

const char *FallbackImpl::configGetColorSpaceNameByIndex(OCIO_ConstConfigRcPtr * /*config*/,
                                                         int index)
{
  if (index == 0)
    return "Linear";
  else if (index == 1)
    return "sRGB";

  return NULL;
}

OCIO_ConstColorSpaceRcPtr *FallbackImpl::configGetColorSpace(OCIO_ConstConfigRcPtr * /*config*/,
                                                             const char *name)
{
  if (strcmp(name, "scene_linear") == 0)
    return COLORSPACE_LINEAR;
  else if (strcmp(name, "color_picking") == 0)
    return COLORSPACE_SRGB;
  else if (strcmp(name, "texture_paint") == 0)
    return COLORSPACE_LINEAR;
  else if (strcmp(name, "default_byte") == 0)
    return COLORSPACE_SRGB;
  else if (strcmp(name, "default_float") == 0)
    return COLORSPACE_LINEAR;
  else if (strcmp(name, "default_sequencer") == 0)
    return COLORSPACE_SRGB;
  else if (strcmp(name, "Linear") == 0)
    return COLORSPACE_LINEAR;
  else if (strcmp(name, "sRGB") == 0)
    return COLORSPACE_SRGB;
  else if (strcmp(name, "data") == 0)
    return COLORSPACE_DATA;

  return NULL;
}

int FallbackImpl::configGetIndexForColorSpace(OCIO_ConstConfigRcPtr *config, const char *name)
{
  OCIO_ConstColorSpaceRcPtr *cs = configGetColorSpace(config, name);

  if (cs == COLORSPACE_LINEAR) {
    return 0;
  }
  else if (cs == COLORSPACE_SRGB) {
    return 1;
  }
  else if (cs == COLORSPACE_DATA) {
    return 2;
  }
  return -1;
}

const char *FallbackImpl::configGetDefaultDisplay(OCIO_ConstConfigRcPtr * /*config*/)
{
  return "sRGB";
}

int FallbackImpl::configGetNumDisplays(OCIO_ConstConfigRcPtr * /*config*/)
{
  return 1;
}

const char *FallbackImpl::configGetDisplay(OCIO_ConstConfigRcPtr * /*config*/, int index)
{
  if (index == 0) {
    return "sRGB";
  }
  return NULL;
}

const char *FallbackImpl::configGetDefaultView(OCIO_ConstConfigRcPtr * /*config*/,
                                               const char * /*display*/)
{
  return "Standard";
}

int FallbackImpl::configGetNumViews(OCIO_ConstConfigRcPtr * /*config*/, const char * /*display*/)
{
  return 1;
}

const char *FallbackImpl::configGetView(OCIO_ConstConfigRcPtr * /*config*/,
                                        const char * /*display*/,
                                        int index)
{
  if (index == 0) {
    return "Standard";
  }
  return NULL;
}

const char *FallbackImpl::configGetDisplayColorSpaceName(OCIO_ConstConfigRcPtr * /*config*/,
                                                         const char * /*display*/,
                                                         const char * /*view*/)
{
  return "sRGB";
}

void FallbackImpl::configGetDefaultLumaCoefs(OCIO_ConstConfigRcPtr * /*config*/, float *rgb)
{
  /* Here we simply use the older Blender assumed primaries of
   * ITU-BT.709 / sRGB, or 0.2126729 0.7151522 0.0721750. Brute
   * force stupid, but only plausible option given no color management
   * system in place.
   */

  rgb[0] = 0.2126f;
  rgb[1] = 0.7152f;
  rgb[2] = 0.0722f;
}

void FallbackImpl::configGetXYZtoSceneLinear(OCIO_ConstConfigRcPtr * /*config*/,
                                             float xyz_to_scene_linear[3][3])
{
  /* Default to ITU-BT.709. */
  memcpy(xyz_to_scene_linear, OCIO_XYZ_TO_REC709, sizeof(OCIO_XYZ_TO_REC709));
}

int FallbackImpl::configGetNumLooks(OCIO_ConstConfigRcPtr * /*config*/)
{
  return 0;
}

const char *FallbackImpl::configGetLookNameByIndex(OCIO_ConstConfigRcPtr * /*config*/,
                                                   int /*index*/)
{
  return "";
}

OCIO_ConstLookRcPtr *FallbackImpl::configGetLook(OCIO_ConstConfigRcPtr * /*config*/,
                                                 const char * /*name*/)
{
  return NULL;
}

const char *FallbackImpl::lookGetProcessSpace(OCIO_ConstLookRcPtr * /*look*/)
{
  return NULL;
}

void FallbackImpl::lookRelease(OCIO_ConstLookRcPtr * /*look*/) {}

int FallbackImpl::colorSpaceIsInvertible(OCIO_ConstColorSpaceRcPtr * /*cs*/)
{
  return 1;
}

int FallbackImpl::colorSpaceIsData(OCIO_ConstColorSpaceRcPtr * /*cs*/)
{
  return 0;
}

void FallbackImpl::colorSpaceIsBuiltin(OCIO_ConstConfigRcPtr * /*config*/,
                                       OCIO_ConstColorSpaceRcPtr *cs,
                                       bool &is_scene_linear,
                                       bool &is_srgb)
{
  if (cs == COLORSPACE_LINEAR) {
    is_scene_linear = true;
    is_srgb = false;
  }
  else if (cs == COLORSPACE_SRGB) {
    is_scene_linear = false;
    is_srgb = true;
  }
  else {
    is_scene_linear = false;
    is_srgb = false;
  }
}

void FallbackImpl::colorSpaceRelease(OCIO_ConstColorSpaceRcPtr * /*cs*/) {}

OCIO_ConstProcessorRcPtr *FallbackImpl::configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config,
                                                                    const char *srcName,
                                                                    const char *dstName)
{
  OCIO_ConstColorSpaceRcPtr *cs_src = configGetColorSpace(config, srcName);
  OCIO_ConstColorSpaceRcPtr *cs_dst = configGetColorSpace(config, dstName);
  FallbackTransform transform;
  if (cs_src == COLORSPACE_DATA || cs_dst == COLORSPACE_DATA) {
    transform.type = TRANSFORM_NONE;
  }
  else if (cs_src == COLORSPACE_LINEAR && cs_dst == COLORSPACE_SRGB) {
    transform.type = TRANSFORM_LINEAR_TO_SRGB;
  }
  else if (cs_src == COLORSPACE_SRGB && cs_dst == COLORSPACE_LINEAR) {
    transform.type = TRANSFORM_SRGB_TO_LINEAR;
  }
  else {
    transform.type = TRANSFORM_UNKNOWN;
  }
  return (OCIO_ConstProcessorRcPtr *)new FallbackProcessor(transform);
}

OCIO_ConstCPUProcessorRcPtr *FallbackImpl::processorGetCPUProcessor(
    OCIO_ConstProcessorRcPtr *processor)
{
  /* Just make a copy of the processor so that we are compatible with OCIO
   * which does need it as a separate object. */
  FallbackProcessor *fallback_processor = (FallbackProcessor *)processor;
  return (OCIO_ConstCPUProcessorRcPtr *)new FallbackProcessor(*fallback_processor);
}

void FallbackImpl::processorRelease(OCIO_ConstProcessorRcPtr *processor)
{
  delete (FallbackProcessor *)(processor);
}

void FallbackImpl::cpuProcessorApply(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                     OCIO_PackedImageDesc *img)
{
  /* OCIO_TODO stride not respected, channels must be 3 or 4 */
  OCIO_PackedImageDescription *desc = (OCIO_PackedImageDescription *)img;
  int channels = desc->numChannels;
  float *pixels = desc->data;
  int width = desc->width;
  int height = desc->height;
  int x, y;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      float *pixel = pixels + channels * (y * width + x);

      if (channels == 4)
        cpuProcessorApplyRGBA(cpu_processor, pixel);
      else if (channels == 3)
        cpuProcessorApplyRGB(cpu_processor, pixel);
    }
  }
}

void FallbackImpl::cpuProcessorApply_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                               OCIO_PackedImageDesc *img)
{
  /* OCIO_TODO stride not respected, channels must be 3 or 4 */
  OCIO_PackedImageDescription *desc = (OCIO_PackedImageDescription *)img;
  int channels = desc->numChannels;
  float *pixels = desc->data;
  int width = desc->width;
  int height = desc->height;
  int x, y;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      float *pixel = pixels + channels * (y * width + x);

      if (channels == 4)
        cpuProcessorApplyRGBA_predivide(cpu_processor, pixel);
      else if (channels == 3)
        cpuProcessorApplyRGB(cpu_processor, pixel);
    }
  }
}

void FallbackImpl::cpuProcessorApplyRGB(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel)
{
  ((FallbackProcessor *)cpu_processor)->applyRGB(pixel);
}

void FallbackImpl::cpuProcessorApplyRGBA(OCIO_ConstCPUProcessorRcPtr *cpu_processor, float *pixel)
{
  ((FallbackProcessor *)cpu_processor)->applyRGBA(pixel);
}

void FallbackImpl::cpuProcessorApplyRGBA_predivide(OCIO_ConstCPUProcessorRcPtr *cpu_processor,
                                                   float *pixel)
{
  if (pixel[3] == 1.0f || pixel[3] == 0.0f) {
    cpuProcessorApplyRGBA(cpu_processor, pixel);
  }
  else {
    float alpha, inv_alpha;

    alpha = pixel[3];
    inv_alpha = 1.0f / alpha;

    pixel[0] *= inv_alpha;
    pixel[1] *= inv_alpha;
    pixel[2] *= inv_alpha;

    cpuProcessorApplyRGBA(cpu_processor, pixel);

    pixel[0] *= alpha;
    pixel[1] *= alpha;
    pixel[2] *= alpha;
  }
}

void FallbackImpl::cpuProcessorRelease(OCIO_ConstCPUProcessorRcPtr *cpu_processor)
{
  delete (FallbackProcessor *)(cpu_processor);
}

const char *FallbackImpl::colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs)
{
  if (cs == COLORSPACE_LINEAR) {
    return "Linear";
  }
  else if (cs == COLORSPACE_SRGB) {
    return "sRGB";
  }
  else if (cs == COLORSPACE_DATA) {
    return "data";
  }
  return NULL;
}

const char *FallbackImpl::colorSpaceGetDescription(OCIO_ConstColorSpaceRcPtr * /*cs*/)
{
  return "";
}

const char *FallbackImpl::colorSpaceGetFamily(OCIO_ConstColorSpaceRcPtr * /*cs*/)
{
  return "";
}

int FallbackImpl::colorSpaceGetNumAliases(OCIO_ConstColorSpaceRcPtr * /*cs*/)
{
  return 0;
}
const char *FallbackImpl::colorSpaceGetAlias(OCIO_ConstColorSpaceRcPtr * /*cs*/,
                                             const int /*index*/)
{
  return "";
}

OCIO_ConstProcessorRcPtr *FallbackImpl::createDisplayProcessor(OCIO_ConstConfigRcPtr * /*config*/,
                                                               const char * /*input*/,
                                                               const char * /*view*/,
                                                               const char * /*display*/,
                                                               const char * /*look*/,
                                                               const float scale,
                                                               const float exponent,
                                                               const bool inverse)
{
  FallbackTransform transform;
  transform.type = (inverse) ? TRANSFORM_SRGB_TO_LINEAR : TRANSFORM_LINEAR_TO_SRGB;
  transform.scale = (inverse && scale != 0.0f) ? 1.0f / scale : scale;
  transform.exponent = (inverse && exponent != 0.0f) ? 1.0f / exponent : exponent;

  return (OCIO_ConstProcessorRcPtr *)new FallbackProcessor(transform);
}

OCIO_PackedImageDesc *FallbackImpl::createOCIO_PackedImageDesc(float *data,
                                                               long width,
                                                               long height,
                                                               long numChannels,
                                                               long chanStrideBytes,
                                                               long xStrideBytes,
                                                               long yStrideBytes)
{
  OCIO_PackedImageDescription *desc = MEM_cnew<OCIO_PackedImageDescription>(
      "OCIO_PackedImageDescription");
  desc->data = data;
  desc->width = width;
  desc->height = height;
  desc->numChannels = numChannels;
  desc->chanStrideBytes = chanStrideBytes;
  desc->xStrideBytes = xStrideBytes;
  desc->yStrideBytes = yStrideBytes;
  return (OCIO_PackedImageDesc *)desc;
}

void FallbackImpl::OCIO_PackedImageDescRelease(OCIO_PackedImageDesc *id)
{
  MEM_freeN(id);
}

const char *FallbackImpl::getVersionString()
{
  return "fallback";
}

int FallbackImpl::getVersionHex()
{
  return 0;
}

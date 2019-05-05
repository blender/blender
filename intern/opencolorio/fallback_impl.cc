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

#include <algorithm>
#include <cstring>

#include "MEM_guardedalloc.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"

#include "ocio_impl.h"

using std::max;

#define CONFIG_DEFAULT ((OCIO_ConstConfigRcPtr *)1)

enum TransformType {
  TRANSFORM_LINEAR_TO_SRGB,
  TRANSFORM_SRGB_TO_LINEAR,
  TRANSFORM_MATRIX,
  TRANSFORM_EXPONENT,
  TRANSFORM_UNKNOWN,
};

#define COLORSPACE_LINEAR ((OCIO_ConstColorSpaceRcPtr *)1)
#define COLORSPACE_SRGB ((OCIO_ConstColorSpaceRcPtr *)2)

typedef struct OCIO_PackedImageDescription {
  float *data;
  long width;
  long height;
  long numChannels;
  long chanStrideBytes;
  long xStrideBytes;
  long yStrideBytes;
} OCIO_PackedImageDescription;

struct FallbackTransform {
  FallbackTransform() : type(TRANSFORM_UNKNOWN), linear_transform(NULL), display_transform(NULL)
  {
  }

  ~FallbackTransform()
  {
    delete linear_transform;
    delete display_transform;
  }

  void applyRGB(float *pixel)
  {
    if (type == TRANSFORM_LINEAR_TO_SRGB) {
      applyLinearRGB(pixel);
      linearrgb_to_srgb_v3_v3(pixel, pixel);
      applyDisplayRGB(pixel);
    }
    else if (type == TRANSFORM_SRGB_TO_LINEAR) {
      srgb_to_linearrgb_v3_v3(pixel, pixel);
    }
    else if (type == TRANSFORM_EXPONENT) {
      pixel[0] = powf(max(0.0f, pixel[0]), exponent[0]);
      pixel[1] = powf(max(0.0f, pixel[1]), exponent[1]);
      pixel[2] = powf(max(0.0f, pixel[2]), exponent[2]);
    }
    else if (type == TRANSFORM_MATRIX) {
      float r = pixel[0];
      float g = pixel[1];
      float b = pixel[2];
      pixel[0] = r * matrix[0] + g * matrix[1] + b * matrix[2];
      pixel[1] = r * matrix[4] + g * matrix[5] + b * matrix[6];
      pixel[2] = r * matrix[8] + g * matrix[9] + b * matrix[10];
      pixel[0] += offset[0];
      pixel[1] += offset[1];
      pixel[2] += offset[2];
    }
  }

  void applyRGBA(float *pixel)
  {
    if (type == TRANSFORM_LINEAR_TO_SRGB) {
      applyLinearRGBA(pixel);
      linearrgb_to_srgb_v4(pixel, pixel);
      applyDisplayRGBA(pixel);
    }
    else if (type == TRANSFORM_SRGB_TO_LINEAR) {
      srgb_to_linearrgb_v4(pixel, pixel);
    }
    else if (type == TRANSFORM_EXPONENT) {
      pixel[0] = powf(max(0.0f, pixel[0]), exponent[0]);
      pixel[1] = powf(max(0.0f, pixel[1]), exponent[1]);
      pixel[2] = powf(max(0.0f, pixel[2]), exponent[2]);
      pixel[3] = powf(max(0.0f, pixel[3]), exponent[3]);
    }
    else if (type == TRANSFORM_MATRIX) {
      float r = pixel[0];
      float g = pixel[1];
      float b = pixel[2];
      float a = pixel[3];
      pixel[0] = r * matrix[0] + g * matrix[1] + b * matrix[2] + a * matrix[3];
      pixel[1] = r * matrix[4] + g * matrix[5] + b * matrix[6] + a * matrix[7];
      pixel[2] = r * matrix[8] + g * matrix[9] + b * matrix[10] + a * matrix[11];
      pixel[3] = r * matrix[12] + g * matrix[13] + b * matrix[14] + a * matrix[15];
      pixel[0] += offset[0];
      pixel[1] += offset[1];
      pixel[2] += offset[2];
      pixel[3] += offset[3];
    }
  }

  void applyLinearRGB(float *pixel)
  {
    if (linear_transform != NULL) {
      linear_transform->applyRGB(pixel);
    }
  }

  void applyLinearRGBA(float *pixel)
  {
    if (linear_transform != NULL) {
      linear_transform->applyRGBA(pixel);
    }
  }

  void applyDisplayRGB(float *pixel)
  {
    if (display_transform != NULL) {
      display_transform->applyRGB(pixel);
    }
  }

  void applyDisplayRGBA(float *pixel)
  {
    if (display_transform != NULL) {
      display_transform->applyRGBA(pixel);
    }
  }

  TransformType type;
  FallbackTransform *linear_transform;
  FallbackTransform *display_transform;
  /* Exponent transform. */
  float exponent[4];
  /* Matrix transform. */
  float matrix[16];
  float offset[4];

  MEM_CXX_CLASS_ALLOC_FUNCS("FallbackProcessor");
};

struct FallbackProcessor {
  FallbackProcessor() : transform(NULL)
  {
  }

  ~FallbackProcessor()
  {
    delete transform;
  }

  void applyRGB(float *pixel)
  {
    transform->applyRGB(pixel);
  }

  void applyRGBA(float *pixel)
  {
    transform->applyRGBA(pixel);
  }

  FallbackTransform *transform;

  MEM_CXX_CLASS_ALLOC_FUNCS("FallbackProcessor");
};

OCIO_ConstConfigRcPtr *FallbackImpl::getCurrentConfig(void)
{
  return CONFIG_DEFAULT;
}

void FallbackImpl::setCurrentConfig(const OCIO_ConstConfigRcPtr * /*config*/)
{
}

OCIO_ConstConfigRcPtr *FallbackImpl::configCreateFromEnv(void)
{
  return NULL;
}

OCIO_ConstConfigRcPtr *FallbackImpl::configCreateFromFile(const char * /*filename*/)
{
  return CONFIG_DEFAULT;
}

void FallbackImpl::configRelease(OCIO_ConstConfigRcPtr * /*config*/)
{
}

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
  return "Default";
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
    return "Default";
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

void FallbackImpl::configGetXYZtoRGB(OCIO_ConstConfigRcPtr * /*config*/, float xyz_to_rgb[3][3])
{
  /* Default to ITU-BT.709. */
  memcpy(xyz_to_rgb, OCIO_XYZ_TO_LINEAR_SRGB, sizeof(OCIO_XYZ_TO_LINEAR_SRGB));
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

void FallbackImpl::lookRelease(OCIO_ConstLookRcPtr * /*look*/)
{
}

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

void FallbackImpl::colorSpaceRelease(OCIO_ConstColorSpaceRcPtr * /*cs*/)
{
}

OCIO_ConstProcessorRcPtr *FallbackImpl::configGetProcessorWithNames(OCIO_ConstConfigRcPtr *config,
                                                                    const char *srcName,
                                                                    const char *dstName)
{
  OCIO_ConstColorSpaceRcPtr *cs_src = configGetColorSpace(config, srcName);
  OCIO_ConstColorSpaceRcPtr *cs_dst = configGetColorSpace(config, dstName);
  FallbackTransform *transform = new FallbackTransform();
  if (cs_src == COLORSPACE_LINEAR && cs_dst == COLORSPACE_SRGB) {
    transform->type = TRANSFORM_LINEAR_TO_SRGB;
  }
  else if (cs_src == COLORSPACE_SRGB && cs_dst == COLORSPACE_LINEAR) {
    transform->type = TRANSFORM_SRGB_TO_LINEAR;
  }
  else {
    transform->type = TRANSFORM_UNKNOWN;
  }
  FallbackProcessor *processor = new FallbackProcessor();
  processor->transform = transform;
  return (OCIO_ConstProcessorRcPtr *)processor;
}

OCIO_ConstProcessorRcPtr *FallbackImpl::configGetProcessor(OCIO_ConstConfigRcPtr * /*config*/,
                                                           OCIO_ConstTransformRcPtr *transform)
{
  FallbackProcessor *processor = new FallbackProcessor();
  processor->transform = (FallbackTransform *)transform;
  return (OCIO_ConstProcessorRcPtr *)processor;
}

void FallbackImpl::processorApply(OCIO_ConstProcessorRcPtr *processor, OCIO_PackedImageDesc *img)
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
        processorApplyRGBA(processor, pixel);
      else if (channels == 3)
        processorApplyRGB(processor, pixel);
    }
  }
}

void FallbackImpl::processorApply_predivide(OCIO_ConstProcessorRcPtr *processor,
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
        processorApplyRGBA_predivide(processor, pixel);
      else if (channels == 3)
        processorApplyRGB(processor, pixel);
    }
  }
}

void FallbackImpl::processorApplyRGB(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
  ((FallbackProcessor *)processor)->applyRGB(pixel);
}

void FallbackImpl::processorApplyRGBA(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
  ((FallbackProcessor *)processor)->applyRGBA(pixel);
}

void FallbackImpl::processorApplyRGBA_predivide(OCIO_ConstProcessorRcPtr *processor, float *pixel)
{
  if (pixel[3] == 1.0f || pixel[3] == 0.0f) {
    processorApplyRGBA(processor, pixel);
  }
  else {
    float alpha, inv_alpha;

    alpha = pixel[3];
    inv_alpha = 1.0f / alpha;

    pixel[0] *= inv_alpha;
    pixel[1] *= inv_alpha;
    pixel[2] *= inv_alpha;

    processorApplyRGBA(processor, pixel);

    pixel[0] *= alpha;
    pixel[1] *= alpha;
    pixel[2] *= alpha;
  }
}

void FallbackImpl::processorRelease(OCIO_ConstProcessorRcPtr *processor)
{
  delete (FallbackProcessor *)(processor);
}

const char *FallbackImpl::colorSpaceGetName(OCIO_ConstColorSpaceRcPtr *cs)
{
  if (cs == COLORSPACE_LINEAR) {
    return "Linear";
  }
  else if (cs == COLORSPACE_SRGB) {
    return "sRGB";
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

OCIO_DisplayTransformRcPtr *FallbackImpl::createDisplayTransform(void)
{
  FallbackTransform *transform = new FallbackTransform();
  transform->type = TRANSFORM_LINEAR_TO_SRGB;
  return (OCIO_DisplayTransformRcPtr *)transform;
}

void FallbackImpl::displayTransformSetInputColorSpaceName(OCIO_DisplayTransformRcPtr * /*dt*/,
                                                          const char * /*name*/)
{
}

void FallbackImpl::displayTransformSetDisplay(OCIO_DisplayTransformRcPtr * /*dt*/,
                                              const char * /*name*/)
{
}

void FallbackImpl::displayTransformSetView(OCIO_DisplayTransformRcPtr * /*dt*/,
                                           const char * /*name*/)
{
}

void FallbackImpl::displayTransformSetDisplayCC(OCIO_DisplayTransformRcPtr *dt,
                                                OCIO_ConstTransformRcPtr *et)
{
  FallbackTransform *transform = (FallbackTransform *)dt;
  transform->display_transform = (FallbackTransform *)et;
}

void FallbackImpl::displayTransformSetLinearCC(OCIO_DisplayTransformRcPtr *dt,
                                               OCIO_ConstTransformRcPtr *et)
{
  FallbackTransform *transform = (FallbackTransform *)dt;
  transform->linear_transform = (FallbackTransform *)et;
}

void FallbackImpl::displayTransformSetLooksOverride(OCIO_DisplayTransformRcPtr * /*dt*/,
                                                    const char * /*looks*/)
{
}

void FallbackImpl::displayTransformSetLooksOverrideEnabled(OCIO_DisplayTransformRcPtr * /*dt*/,
                                                           bool /*enabled*/)
{
}

void FallbackImpl::displayTransformRelease(OCIO_DisplayTransformRcPtr * /*dt*/)
{
}

OCIO_PackedImageDesc *FallbackImpl::createOCIO_PackedImageDesc(float *data,
                                                               long width,
                                                               long height,
                                                               long numChannels,
                                                               long chanStrideBytes,
                                                               long xStrideBytes,
                                                               long yStrideBytes)
{
  OCIO_PackedImageDescription *desc = (OCIO_PackedImageDescription *)MEM_callocN(
      sizeof(OCIO_PackedImageDescription), "OCIO_PackedImageDescription");
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

OCIO_ExponentTransformRcPtr *FallbackImpl::createExponentTransform(void)
{
  FallbackTransform *transform = new FallbackTransform();
  transform->type = TRANSFORM_EXPONENT;
  return (OCIO_ExponentTransformRcPtr *)transform;
}

void FallbackImpl::exponentTransformSetValue(OCIO_ExponentTransformRcPtr *et,
                                             const float *exponent)
{
  FallbackTransform *transform = (FallbackTransform *)et;
  copy_v4_v4(transform->exponent, exponent);
}

void FallbackImpl::exponentTransformRelease(OCIO_ExponentTransformRcPtr * /*et*/)
{
}

OCIO_MatrixTransformRcPtr *FallbackImpl::createMatrixTransform(void)
{
  FallbackTransform *transform = new FallbackTransform();
  transform->type = TRANSFORM_MATRIX;
  return (OCIO_MatrixTransformRcPtr *)transform;
}

void FallbackImpl::matrixTransformSetValue(OCIO_MatrixTransformRcPtr *mt,
                                           const float *m44,
                                           const float *offset4)
{
  FallbackTransform *transform = (FallbackTransform *)mt;
  copy_m4_m4((float(*)[4])transform->matrix, (float(*)[4])m44);
  copy_v4_v4(transform->offset, offset4);
}

void FallbackImpl::matrixTransformRelease(OCIO_MatrixTransformRcPtr * /*mt*/)
{
}

void FallbackImpl::matrixTransformScale(float *m44, float *offset4, const float *scale4)
{
  if (scale4 == NULL) {
    return;
  }
  if (m44 != NULL) {
    memset(m44, 0, 16 * sizeof(float));
    m44[0] = scale4[0];
    m44[5] = scale4[1];
    m44[10] = scale4[2];
    m44[15] = scale4[3];
  }
  if (offset4 != NULL) {
    offset4[0] = 0.0f;
    offset4[1] = 0.0f;
    offset4[2] = 0.0f;
    offset4[3] = 0.0f;
  }
}

bool FallbackImpl::supportGLSLDraw(void)
{
  return false;
}

bool FallbackImpl::setupGLSLDraw(struct OCIO_GLSLDrawState ** /*state_r*/,
                                 OCIO_ConstProcessorRcPtr * /*processor*/,
                                 OCIO_CurveMappingSettings * /*curve_mapping_settings*/,
                                 float /*dither*/,
                                 bool /*predivide*/)
{
  return false;
}

void FallbackImpl::finishGLSLDraw(OCIO_GLSLDrawState * /*state*/)
{
}

void FallbackImpl::freeGLState(struct OCIO_GLSLDrawState * /*state_r*/)
{
}

const char *FallbackImpl::getVersionString(void)
{
  return "fallback";
}

int FallbackImpl::getVersionHex(void)
{
  return 0;
}

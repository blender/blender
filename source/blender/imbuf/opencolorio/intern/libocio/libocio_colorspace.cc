/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "libocio_colorspace.hh"

#if defined(WITH_OPENCOLORIO)

#  include <cmath>

#  include "BLI_math_color.h"

#  include "../description.hh"
#  include "error_handling.hh"
#  include "libocio_cpu_processor.hh"
#  include "libocio_processor.hh"

namespace blender::ocio {

static bool compare_floats(float a, float b, float abs_diff, int ulp_diff)
{
  /* Returns true if the absolute difference is smaller than abs_diff (for numbers near zero)
   * or their relative difference is less than ulp_diff ULPs. Based on:
   * https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
   */
  if (fabsf(a - b) < abs_diff) {
    return true;
  }

  if ((a < 0.0f) != (b < 0.0f)) {
    return false;
  }

  return (abs((*(int *)&a) - (*(int *)&b)) < ulp_diff);
}

static bool color_space_is_invertible(const OCIO_NAMESPACE::ConstColorSpaceRcPtr &ocio_color_space)
{
  const StringRefNull family = ocio_color_space->getFamily();

  if (ELEM(family, "rrt", "display")) {
    /* assume display and rrt transformations are not invertible in fact some of them could be,
     * but it doesn't make much sense to allow use them as invertible. */
    return false;
  }

  if (ocio_color_space->isData()) {
    /* Data color spaces don't have transformation at all. */
    return true;
  }

  if (ocio_color_space->getTransform(OCIO_NAMESPACE::COLORSPACE_DIR_TO_REFERENCE)) {
    /* if there's defined transform to reference space,  color space could be converted to scene
     * linear. */
    return true;
  }

  return true;
}

static void color_space_is_builtin(const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config,
                                   const OCIO_NAMESPACE::ConstColorSpaceRcPtr &ocio_color_space,
                                   bool &is_scene_linear,
                                   bool &is_srgb)
{
  OCIO_NAMESPACE::ConstProcessorRcPtr processor = create_ocio_processor_silent(
      ocio_config, ocio_color_space->getName(), OCIO_NAMESPACE::ROLE_SCENE_LINEAR);
  if (!processor) {
    /* Silently ignore if no conversion possible, then it's not scene linear or sRGB. */
    is_scene_linear = false;
    is_srgb = false;
    return;
  }

  OCIO_NAMESPACE::ConstCPUProcessorRcPtr cpu_processor = processor->getDefaultCPUProcessor();

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

LibOCIOColorSpace::LibOCIOColorSpace(const int index,
                                     const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config,
                                     const OCIO_NAMESPACE::ConstColorSpaceRcPtr &ocio_color_space)
    : ocio_config_(ocio_config),
      ocio_color_space_(ocio_color_space),
      clean_description_(cleanup_description(ocio_color_space->getDescription()))
{
  this->index = index;

  is_invertible_ = color_space_is_invertible(ocio_color_space);
}

bool LibOCIOColorSpace::is_scene_linear() const
{
  ensure_srgb_scene_linear_info();
  return is_scene_linear_;
}

bool LibOCIOColorSpace::is_srgb() const
{
  ensure_srgb_scene_linear_info();
  return is_srgb_;
}

const CPUProcessor *LibOCIOColorSpace::get_to_scene_linear_cpu_processor() const
{
  return to_scene_linear_cpu_processor_.get([&]() -> std::unique_ptr<CPUProcessor> {
    OCIO_NAMESPACE::ConstProcessorRcPtr ocio_processor = create_ocio_processor(
        ocio_config_, ocio_color_space_->getName(), OCIO_NAMESPACE::ROLE_SCENE_LINEAR);
    if (!ocio_processor) {
      return nullptr;
    }
    return std::make_unique<LibOCIOCPUProcessor>(ocio_processor->getDefaultCPUProcessor());
  });
}

const CPUProcessor *LibOCIOColorSpace::get_from_scene_linear_cpu_processor() const
{
  return from_scene_linear_cpu_processor_.get([&]() -> std::unique_ptr<CPUProcessor> {
    OCIO_NAMESPACE::ConstProcessorRcPtr ocio_processor = create_ocio_processor(
        ocio_config_, OCIO_NAMESPACE::ROLE_SCENE_LINEAR, ocio_color_space_->getName());
    if (!ocio_processor) {
      return nullptr;
    }
    return std::make_unique<LibOCIOCPUProcessor>(ocio_processor->getDefaultCPUProcessor());
  });
}

void LibOCIOColorSpace::ensure_srgb_scene_linear_info() const
{
  if (is_info_cached_) {
    return;
  }
  color_space_is_builtin(ocio_config_, ocio_color_space_, is_scene_linear_, is_srgb_);
  is_info_cached_ = true;
}

}  // namespace blender::ocio

#endif

/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_assert.h"
#include "BLI_math_color.h"

#include "OCIO_cpu_processor.hh"
#include "OCIO_packed_image.hh"

namespace blender::ocio {

/**
 * CPU processor implementation that does not perform any pixel modification.
 */
class FallbackNOOPCPUProcessor : public CPUProcessor {
 public:
  bool is_noop() const override
  {
    return true;
  }

  void apply_rgb(float /*rgb*/[3]) const override {}
  void apply_rgba(float /*rgba*/[4]) const override {}

  void apply_rgba_predivide(float /*rgba*/[4]) const override {}

  void apply(const PackedImage & /*image*/) const override {}
  void apply_predivide(const PackedImage & /*image*/) const override {}
};

/**
 * Processor which applies templated pixel_processor for every pixel that is to be converted.
 */
template<void (*pixel_processor)(float dst[3], const float src[3])>
class FallbackCustomCPUProcessor : public CPUProcessor {
 public:
  bool is_noop() const override
  {
    return false;
  }

  void apply_rgb(float rgb[3]) const override
  {
    pixel_processor(rgb, rgb);
  }
  void apply_rgba(float rgba[4]) const override
  {
    pixel_processor(rgba, rgba);
  }

  void apply_rgba_predivide(float rgba[4]) const override
  {
    if (rgba[3] == 1.0f || rgba[3] == 0.0f) {
      pixel_processor(rgba, rgba);
      return;
    }

    const float alpha = rgba[3];
    const float inv_alpha = 1.0f / alpha;

    rgba[0] *= inv_alpha;
    rgba[1] *= inv_alpha;
    rgba[2] *= inv_alpha;

    pixel_processor(rgba, rgba);

    rgba[0] *= alpha;
    rgba[1] *= alpha;
    rgba[2] *= alpha;
  }

  void apply(const PackedImage &image) const override
  {
    /* TODO(sergey): Stride not respected, channels must be 3 or 4, bit depth is float32. */

    BLI_assert(image.get_num_channels() >= 3);
    BLI_assert(image.get_bit_depth() == BitDepth::BIT_DEPTH_F32);

    const int num_channels = image.get_num_channels();
    const size_t width = image.get_width();
    const size_t height = image.get_height();

    float *pixels = static_cast<float *>(image.get_data());

    for (size_t y = 0; y < height; y++) {
      for (size_t x = 0; x < width; x++) {
        float *pixel = pixels + num_channels * (y * width + x);
        pixel_processor(pixel, pixel);
      }
    }
  }

  void apply_predivide(const PackedImage &image) const override
  {
    /* TODO(sergey): Stride not respected, channels must be 3 or 4, bit depth is float32. */

    BLI_assert(image.get_num_channels() >= 3);
    BLI_assert(image.get_bit_depth() == BitDepth::BIT_DEPTH_F32);

    const int num_channels = image.get_num_channels();
    if (num_channels < 4) {
      apply(image);
      return;
    }

    const size_t width = image.get_width();
    const size_t height = image.get_height();

    float *pixels = static_cast<float *>(image.get_data());

    for (size_t y = 0; y < height; y++) {
      for (size_t x = 0; x < width; x++) {
        float *pixel = pixels + num_channels * (y * width + x);
        apply_rgba_predivide(pixel);
      }
    }
  }
};

using FallbackLinearRGBToSRGBCPUProcessor = FallbackCustomCPUProcessor<linearrgb_to_srgb_v3_v3>;
using FallbackSRGBToLinearRGBCPUProcessor = FallbackCustomCPUProcessor<srgb_to_linearrgb_v3_v3>;

}  // namespace blender::ocio

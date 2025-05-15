/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "fallback_display_cpu_processor.hh"

#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"

#include "OCIO_config.hh"
#include "OCIO_cpu_processor.hh"
#include "OCIO_packed_image.hh"

#include "../white_point.hh"

namespace blender::ocio {

namespace {

using PixelSpaceProcessor3 = void (*)(float dst[3], const float src[3]);

class NOOPDisplayCPUProcessor : public CPUProcessor {
 public:
  static std::shared_ptr<const CPUProcessor> get()
  {
    static auto processor = std::make_shared<NOOPDisplayCPUProcessor>();
    return processor;
  }

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

class BaseDisplayCPUProcessor : public CPUProcessor {
 public:
  /* Matrix transform which is applied in the linear space.
   *
   * NOTE: The matrix is inversed when the processor is configured to go from display space to
   * linear. */
  float3x3 matrix = float3x3::identity();

  float exponent = 1.0f;
};

template<PixelSpaceProcessor3 pixel_space_processor, bool is_inverse>
class DisplayCPUProcessor : public BaseDisplayCPUProcessor {
 public:
  bool is_noop() const override
  {
    return false;
  }

  void apply_rgb(float rgb[3]) const override
  {
    process_rgb(rgb);
  }
  void apply_rgba(float rgba[4]) const override
  {
    process_rgb(rgba);
  }

  void apply_rgba_predivide(float rgba[4]) const override
  {
    if (ELEM(rgba[3], 1.0f, 0.0f)) {
      process_rgb(rgba);
      return;
    }

    const float alpha = rgba[3];
    const float inv_alpha = 1.0f / alpha;

    rgba[0] *= inv_alpha;
    rgba[1] *= inv_alpha;
    rgba[2] *= inv_alpha;

    process_rgb(rgba);

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
        process_rgb(pixel);
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

 private:
  void process_rgb(float rgb[3]) const
  {
    if constexpr (is_inverse) {
      if (exponent != 0) {
        const float inv_exponent = 1.0f / exponent;
        rgb[0] = rgb[0] != 0.0f ? math::pow(rgb[0], inv_exponent) : 0.0f;
        rgb[1] = rgb[1] != 0.0f ? math::pow(rgb[1], inv_exponent) : 0.0f;
        rgb[2] = rgb[2] != 0.0f ? math::pow(rgb[2], inv_exponent) : 0.0f;
      }
      else {
        rgb[0] = 0;
        rgb[1] = 0;
        rgb[2] = 0;
      }

      pixel_space_processor(rgb, rgb);

      mul_v3_m3v3(rgb, this->matrix.ptr(), rgb);
    }
    else {
      mul_v3_m3v3(rgb, this->matrix.ptr(), rgb);

      pixel_space_processor(rgb, rgb);

      rgb[0] = math::pow(math::max(0.0f, rgb[0]), exponent);
      rgb[1] = math::pow(math::max(0.0f, rgb[1]), exponent);
      rgb[2] = math::pow(math::max(0.0f, rgb[2]), exponent);
    }
  }
};

}  // namespace

std::shared_ptr<const CPUProcessor> create_fallback_display_cpu_processor(
    const Config &config, const DisplayParameters &display_parameters)
{
  if (display_parameters.display != "sRGB") {
    return NOOPDisplayCPUProcessor::get();
  }

  if (display_parameters.view != "Standard") {
    return NOOPDisplayCPUProcessor::get();
  }
  if (!ELEM(display_parameters.look, "", "None")) {
    return NOOPDisplayCPUProcessor::get();
  }

  if (display_parameters.from_colorspace == "Non-Color") {
    return NOOPDisplayCPUProcessor::get();
  }

  std::shared_ptr<BaseDisplayCPUProcessor> processor;

  if (display_parameters.from_colorspace == "Linear") {
    if (display_parameters.inverse) {
      processor = std::make_shared<DisplayCPUProcessor<srgb_to_linearrgb_v3_v3, true>>();
    }
    else {
      processor = std::make_shared<DisplayCPUProcessor<linearrgb_to_srgb_v3_v3, false>>();
    }
  }
  else if (display_parameters.from_colorspace == "sRGB") {
    if (display_parameters.inverse) {
      processor = std::make_shared<DisplayCPUProcessor<linearrgb_to_srgb_v3_v3, true>>();
    }
    else {
      processor = std::make_shared<DisplayCPUProcessor<srgb_to_linearrgb_v3_v3, false>>();
    }
  }
  else {
    return NOOPDisplayCPUProcessor::get();
  }

  processor->matrix = float3x3::identity() * display_parameters.scale;
  processor->exponent = display_parameters.exponent;

  /* Apply white balance. */
  if (display_parameters.use_white_balance) {
    const float3x3 matrix = calculate_white_point_matrix(
        config, display_parameters.temperature, display_parameters.tint);
    processor->matrix *= matrix;
  }

  if (display_parameters.inverse) {
    processor->matrix = math::invert(processor->matrix);
  }

  return processor;
}

}  // namespace blender::ocio

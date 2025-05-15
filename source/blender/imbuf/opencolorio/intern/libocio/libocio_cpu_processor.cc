/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "libocio_cpu_processor.hh"

#if defined(WITH_OPENCOLORIO)

#  include "OCIO_packed_image.hh"

#  include "BLI_assert.h"

#  include "error_handling.hh"

namespace blender::ocio {

LibOCIOCPUProcessor::LibOCIOCPUProcessor(
    const OCIO_NAMESPACE::ConstCPUProcessorRcPtr &ocio_cpu_processor)
    : ocio_cpu_processor_(ocio_cpu_processor)
{
  BLI_assert(ocio_cpu_processor_);
}

void LibOCIOCPUProcessor::apply_rgb(float rgb[3]) const
{
  ocio_cpu_processor_->applyRGB(rgb);
}

void LibOCIOCPUProcessor::apply_rgba(float rgba[4]) const
{
  ocio_cpu_processor_->applyRGBA(rgba);
}

void LibOCIOCPUProcessor::apply_rgba_predivide(float rgba[4]) const
{
  if (ELEM(rgba[3], 1.0f, 0.0f)) {
    apply_rgba(rgba);
    return;
  }

  const float alpha = rgba[3];
  const float inv_alpha = 1.0f / alpha;

  rgba[0] *= inv_alpha;
  rgba[1] *= inv_alpha;
  rgba[2] *= inv_alpha;

  apply_rgba(rgba);

  rgba[0] *= alpha;
  rgba[1] *= alpha;
  rgba[2] *= alpha;
}

void LibOCIOCPUProcessor::apply(const PackedImage &image) const
{
  /* TODO(sergey): Support other bit depths. */
  if (image.get_bit_depth() != BitDepth::BIT_DEPTH_F32) {
    return;
  }

  try {
    ocio_cpu_processor_->apply(image);
  }
  catch (OCIO_NAMESPACE::Exception &exception) {
    report_exception(exception);
  }
}

void LibOCIOCPUProcessor::apply_predivide(const PackedImage &image) const
{
  /* TODO(sergey): Support other bit depths. */
  if (image.get_bit_depth() != BitDepth::BIT_DEPTH_F32) {
    return;
  }

  if (image.get_num_channels() == 4) {
    /* Convert from premultiplied alpha to straight alpha. */
    float *pixel = reinterpret_cast<float *>(image.get_data());
    const size_t pixel_count = image.get_width() * image.get_height();
    for (size_t i = 0; i < pixel_count; i++, pixel += 4) {
      const float alpha = pixel[3];
      if (!ELEM(alpha, 0.0f, 1.0f)) {
        const float inv_alpha = 1.0f / alpha;
        pixel[0] *= inv_alpha;
        pixel[1] *= inv_alpha;
        pixel[2] *= inv_alpha;
      }
    }
  }

  apply(image);

  if (image.get_num_channels() == 4) {
    /* Back to premultiplied alpha. */
    float *pixel = reinterpret_cast<float *>(image.get_data());
    const size_t pixel_count = image.get_width() * image.get_height();
    for (size_t i = 0; i < pixel_count; i++, pixel += 4) {
      const float alpha = pixel[3];
      if (!ELEM(alpha, 0.0f, 1.0f)) {
        pixel[0] *= alpha;
        pixel[1] *= alpha;
        pixel[2] *= alpha;
      }
    }
  }
}

}  // namespace blender::ocio

#endif

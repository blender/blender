/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_HueSaturationValueCorrectOperation.h"

#include "BLI_math_vector.h"

#include "BKE_colortools.hh"

namespace blender::compositor {

HueSaturationValueCorrectOperation::HueSaturationValueCorrectOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);

  input_program_ = nullptr;
}
void HueSaturationValueCorrectOperation::init_execution()
{
  CurveBaseOperation::init_execution();
  input_program_ = this->get_input_socket_reader(0);
}

void HueSaturationValueCorrectOperation::execute_pixel_sampled(float output[4],
                                                               float x,
                                                               float y,
                                                               PixelSampler sampler)
{
  float hsv[4], f;

  input_program_->read_sampled(hsv, x, y, sampler);

  /* adjust hue, scaling returned default 0.5 up to 1 */
  f = BKE_curvemapping_evaluateF(curve_mapping_, 0, hsv[0]);
  hsv[0] += f - 0.5f;

  /* adjust saturation, scaling returned default 0.5 up to 1 */
  f = BKE_curvemapping_evaluateF(curve_mapping_, 1, hsv[0]);
  hsv[1] *= (f * 2.0f);

  /* adjust value, scaling returned default 0.5 up to 1 */
  f = BKE_curvemapping_evaluateF(curve_mapping_, 2, hsv[0]);
  hsv[2] *= (f * 2.0f);

  hsv[0] = hsv[0] - floorf(hsv[0]); /* mod 1.0 */
  CLAMP(hsv[1], 0.0f, 1.0f);

  output[0] = hsv[0];
  output[1] = hsv[1];
  output[2] = hsv[2];
  output[3] = hsv[3];
}

void HueSaturationValueCorrectOperation::deinit_execution()
{
  CurveBaseOperation::deinit_execution();
  input_program_ = nullptr;
}

void HueSaturationValueCorrectOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                      const rcti &area,
                                                                      Span<MemoryBuffer *> inputs)
{
  float hsv[4];
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    copy_v4_v4(hsv, it.in(0));

    /* Adjust hue, scaling returned default 0.5 up to 1. */
    float f = BKE_curvemapping_evaluateF(curve_mapping_, 0, hsv[0]);
    hsv[0] += f - 0.5f;

    /* Adjust saturation, scaling returned default 0.5 up to 1. */
    f = BKE_curvemapping_evaluateF(curve_mapping_, 1, hsv[0]);
    hsv[1] *= (f * 2.0f);

    /* Adjust value, scaling returned default 0.5 up to 1. */
    f = BKE_curvemapping_evaluateF(curve_mapping_, 2, hsv[0]);
    hsv[2] *= (f * 2.0f);

    hsv[0] = hsv[0] - floorf(hsv[0]); /* Mod 1.0. */
    CLAMP(hsv[1], 0.0f, 1.0f);

    copy_v4_v4(it.out, hsv);
  }
}

}  // namespace blender::compositor

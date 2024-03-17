/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_BrightnessOperation.h"

#include "BLI_math_color.h"

namespace blender::compositor {

BrightnessOperation::BrightnessOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  use_premultiply_ = false;
  flags_.can_be_constant = true;
}

void BrightnessOperation::set_use_premultiply(bool use_premultiply)
{
  use_premultiply_ = use_premultiply;
}

void BrightnessOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  float tmp_color[4];
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *in_color = it.in(0);
    const float brightness = *it.in(1) / 100.0f;
    const float contrast = *it.in(2);
    float delta = contrast / 200.0f;
    /*
     * The algorithm is by Werner D. Streidt
     * (http://visca.com/ffactory/archives/5-99/msg00021.html)
     * Extracted of OpenCV `demhist.c`.
     */
    float a, b;
    if (contrast > 0) {
      a = 1.0f - delta * 2.0f;
      a = 1.0f / max_ff(a, FLT_EPSILON);
      b = a * (brightness - delta);
    }
    else {
      delta *= -1;
      a = max_ff(1.0f - delta * 2.0f, 0.0f);
      b = a * brightness + delta;
    }
    const float *color;
    if (use_premultiply_) {
      premul_to_straight_v4_v4(tmp_color, in_color);
      color = tmp_color;
    }
    else {
      color = in_color;
    }
    it.out[0] = a * color[0] + b;
    it.out[1] = a * color[1] + b;
    it.out[2] = a * color[2] + b;
    it.out[3] = color[3];
    if (use_premultiply_) {
      straight_to_premul_v4(it.out);
    }
  }
}

}  // namespace blender::compositor

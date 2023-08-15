/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_BrightnessOperation.h"

namespace blender::compositor {

BrightnessOperation::BrightnessOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  input_program_ = nullptr;
  use_premultiply_ = false;
  flags_.can_be_constant = true;
}

void BrightnessOperation::set_use_premultiply(bool use_premultiply)
{
  use_premultiply_ = use_premultiply;
}

void BrightnessOperation::init_execution()
{
  input_program_ = this->get_input_socket_reader(0);
  input_brightness_program_ = this->get_input_socket_reader(1);
  input_contrast_program_ = this->get_input_socket_reader(2);
}

void BrightnessOperation::execute_pixel_sampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float input_value[4];
  float a, b;
  float input_brightness[4];
  float input_contrast[4];
  input_program_->read_sampled(input_value, x, y, sampler);
  input_brightness_program_->read_sampled(input_brightness, x, y, sampler);
  input_contrast_program_->read_sampled(input_contrast, x, y, sampler);
  float brightness = input_brightness[0];
  float contrast = input_contrast[0];
  brightness /= 100.0f;
  float delta = contrast / 200.0f;
  /*
   * The algorithm is by Werner D. Streidt
   * (http://visca.com/ffactory/archives/5-99/msg00021.html)
   * Extracted of OpenCV `demhist.c`.
   */
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
  if (use_premultiply_) {
    premul_to_straight_v4(input_value);
  }
  output[0] = a * input_value[0] + b;
  output[1] = a * input_value[1] + b;
  output[2] = a * input_value[2] + b;
  output[3] = input_value[3];
  if (use_premultiply_) {
    straight_to_premul_v4(output);
  }
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

void BrightnessOperation::deinit_execution()
{
  input_program_ = nullptr;
  input_brightness_program_ = nullptr;
  input_contrast_program_ = nullptr;
}

}  // namespace blender::compositor

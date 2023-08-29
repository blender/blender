/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ChangeHSVOperation.h"

namespace blender::compositor {

ChangeHSVOperation::ChangeHSVOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  input_operation_ = nullptr;
  flags_.can_be_constant = true;
}

void ChangeHSVOperation::init_execution()
{
  input_operation_ = get_input_socket_reader(0);
  hue_operation_ = get_input_socket_reader(1);
  saturation_operation_ = get_input_socket_reader(2);
  value_operation_ = get_input_socket_reader(3);
}

void ChangeHSVOperation::deinit_execution()
{
  input_operation_ = nullptr;
  hue_operation_ = nullptr;
  saturation_operation_ = nullptr;
  value_operation_ = nullptr;
}

void ChangeHSVOperation::execute_pixel_sampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float input_color1[4];
  float hue[4], saturation[4], value[4];

  input_operation_->read_sampled(input_color1, x, y, sampler);
  hue_operation_->read_sampled(hue, x, y, sampler);
  saturation_operation_->read_sampled(saturation, x, y, sampler);
  value_operation_->read_sampled(value, x, y, sampler);

  output[0] = input_color1[0] + (hue[0] - 0.5f);
  if (output[0] > 1.0f) {
    output[0] -= 1.0f;
  }
  else if (output[0] < 0.0f) {
    output[0] += 1.0f;
  }
  output[1] = input_color1[1] * saturation[0];
  output[2] = input_color1[2] * value[0];
  output[3] = input_color1[3];
}

void ChangeHSVOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color = it.in(0);
    const float hue = *it.in(1);
    it.out[0] = color[0] + (hue - 0.5f);
    if (it.out[0] > 1.0f) {
      it.out[0] -= 1.0f;
    }
    else if (it.out[0] < 0.0f) {
      it.out[0] += 1.0f;
    }
    const float saturation = *it.in(2);
    const float value = *it.in(3);
    it.out[1] = color[1] * saturation;
    it.out[2] = color[2] * value;
    it.out[3] = color[3];
  }
}

}  // namespace blender::compositor

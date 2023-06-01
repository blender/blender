/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_PosterizeOperation.h"

namespace blender::compositor {

PosterizeOperation::PosterizeOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  input_program_ = nullptr;
  input_steps_program_ = nullptr;
  flags_.can_be_constant = true;
}

void PosterizeOperation::init_execution()
{
  input_program_ = this->get_input_socket_reader(0);
  input_steps_program_ = this->get_input_socket_reader(1);
}

void PosterizeOperation::execute_pixel_sampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float input_value[4];
  float input_steps[4];

  input_program_->read_sampled(input_value, x, y, sampler);
  input_steps_program_->read_sampled(input_steps, x, y, sampler);
  CLAMP(input_steps[0], 2.0f, 1024.0f);
  const float steps_inv = 1.0f / input_steps[0];

  output[0] = floor(input_value[0] / steps_inv) * steps_inv;
  output[1] = floor(input_value[1] / steps_inv) * steps_inv;
  output[2] = floor(input_value[2] / steps_inv) * steps_inv;
  output[3] = input_value[3];
}

void PosterizeOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *in_value = it.in(0);
    const float *in_steps = it.in(1);
    float steps = in_steps[0];
    CLAMP(steps, 2.0f, 1024.0f);
    const float steps_inv = 1.0f / steps;

    it.out[0] = floor(in_value[0] / steps_inv) * steps_inv;
    it.out[1] = floor(in_value[1] / steps_inv) * steps_inv;
    it.out[2] = floor(in_value[2] / steps_inv) * steps_inv;
    it.out[3] = in_value[3];
  }
}

void PosterizeOperation::deinit_execution()
{
  input_program_ = nullptr;
  input_steps_program_ = nullptr;
}

}  // namespace blender::compositor

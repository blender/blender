/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GammaCorrectOperation.h"

namespace blender::compositor {

GammaCorrectOperation::GammaCorrectOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  input_program_ = nullptr;
  flags_.can_be_constant = true;
}
void GammaCorrectOperation::init_execution()
{
  input_program_ = this->get_input_socket_reader(0);
}

void GammaCorrectOperation::execute_pixel_sampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float input_color[4];
  input_program_->read_sampled(input_color, x, y, sampler);
  if (input_color[3] > 0.0f) {
    input_color[0] /= input_color[3];
    input_color[1] /= input_color[3];
    input_color[2] /= input_color[3];
  }

  /* Check for negative to avoid NAN's. */
  output[0] = input_color[0] > 0.0f ? input_color[0] * input_color[0] : 0.0f;
  output[1] = input_color[1] > 0.0f ? input_color[1] * input_color[1] : 0.0f;
  output[2] = input_color[2] > 0.0f ? input_color[2] * input_color[2] : 0.0f;
  output[3] = input_color[3];

  if (input_color[3] > 0.0f) {
    output[0] *= input_color[3];
    output[1] *= input_color[3];
    output[2] *= input_color[3];
  }
}

void GammaCorrectOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                         const rcti &area,
                                                         Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    float color[4];
    input->read_elem(it.x, it.y, color);
    if (color[3] > 0.0f) {
      color[0] /= color[3];
      color[1] /= color[3];
      color[2] /= color[3];
    }

    /* Check for negative to avoid NAN's. */
    it.out[0] = color[0] > 0.0f ? color[0] * color[0] : 0.0f;
    it.out[1] = color[1] > 0.0f ? color[1] * color[1] : 0.0f;
    it.out[2] = color[2] > 0.0f ? color[2] * color[2] : 0.0f;
    it.out[3] = color[3];

    if (color[3] > 0.0f) {
      it.out[0] *= color[3];
      it.out[1] *= color[3];
      it.out[2] *= color[3];
    }
  }
}

void GammaCorrectOperation::deinit_execution()
{
  input_program_ = nullptr;
}

GammaUncorrectOperation::GammaUncorrectOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  input_program_ = nullptr;
  flags_.can_be_constant = true;
}
void GammaUncorrectOperation::init_execution()
{
  input_program_ = this->get_input_socket_reader(0);
}

void GammaUncorrectOperation::execute_pixel_sampled(float output[4],
                                                    float x,
                                                    float y,
                                                    PixelSampler sampler)
{
  float input_color[4];
  input_program_->read_sampled(input_color, x, y, sampler);

  if (input_color[3] > 0.0f) {
    input_color[0] /= input_color[3];
    input_color[1] /= input_color[3];
    input_color[2] /= input_color[3];
  }

  output[0] = input_color[0] > 0.0f ? sqrtf(input_color[0]) : 0.0f;
  output[1] = input_color[1] > 0.0f ? sqrtf(input_color[1]) : 0.0f;
  output[2] = input_color[2] > 0.0f ? sqrtf(input_color[2]) : 0.0f;
  output[3] = input_color[3];

  if (input_color[3] > 0.0f) {
    output[0] *= input_color[3];
    output[1] *= input_color[3];
    output[2] *= input_color[3];
  }
}

void GammaUncorrectOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    float color[4];
    input->read_elem(it.x, it.y, color);
    if (color[3] > 0.0f) {
      color[0] /= color[3];
      color[1] /= color[3];
      color[2] /= color[3];
    }

    it.out[0] = color[0] > 0.0f ? sqrtf(color[0]) : 0.0f;
    it.out[1] = color[1] > 0.0f ? sqrtf(color[1]) : 0.0f;
    it.out[2] = color[2] > 0.0f ? sqrtf(color[2]) : 0.0f;
    it.out[3] = color[3];

    if (color[3] > 0.0f) {
      it.out[0] *= color[3];
      it.out[1] *= color[3];
      it.out[2] *= color[3];
    }
  }
}

void GammaUncorrectOperation::deinit_execution()
{
  input_program_ = nullptr;
}

}  // namespace blender::compositor

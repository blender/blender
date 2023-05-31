/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DotproductOperation.h"

namespace blender::compositor {

DotproductOperation::DotproductOperation()
{
  this->add_input_socket(DataType::Vector);
  this->add_input_socket(DataType::Vector);
  this->add_output_socket(DataType::Value);
  this->set_canvas_input_index(0);
  input1Operation_ = nullptr;
  input2Operation_ = nullptr;
  flags_.can_be_constant = true;
}
void DotproductOperation::init_execution()
{
  input1Operation_ = this->get_input_socket_reader(0);
  input2Operation_ = this->get_input_socket_reader(1);
}

void DotproductOperation::deinit_execution()
{
  input1Operation_ = nullptr;
  input2Operation_ = nullptr;
}

/** \todo current implementation is the inverse of a dot-product. not 'logically' correct
 */
void DotproductOperation::execute_pixel_sampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float input1[4];
  float input2[4];
  input1Operation_->read_sampled(input1, x, y, sampler);
  input2Operation_->read_sampled(input2, x, y, sampler);
  output[0] = -(input1[0] * input2[0] + input1[1] * input2[1] + input1[2] * input2[2]);
}

void DotproductOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *input1 = it.in(0);
    const float *input2 = it.in(1);
    *it.out = -(input1[0] * input2[0] + input1[1] * input2[1] + input1[2] * input2[2]);
  }
}

}  // namespace blender::compositor

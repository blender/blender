/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_PixelateOperation.h"

namespace blender::compositor {

PixelateOperation::PixelateOperation(DataType data_type)
{
  this->add_input_socket(data_type);
  this->add_output_socket(data_type);
  this->set_canvas_input_index(0);
  input_operation_ = nullptr;
}

void PixelateOperation::init_execution()
{
  input_operation_ = this->get_input_socket_reader(0);
}

void PixelateOperation::deinit_execution()
{
  input_operation_ = nullptr;
}

void PixelateOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float nx = round(x);
  float ny = round(y);
  input_operation_->read_sampled(output, nx, ny, sampler);
}

}  // namespace blender::compositor

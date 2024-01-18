/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ConvertColorProfileOperation.h"

#include "IMB_imbuf.hh"

namespace blender::compositor {

ConvertColorProfileOperation::ConvertColorProfileOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  input_operation_ = nullptr;
  predivided_ = false;
}

void ConvertColorProfileOperation::init_execution()
{
  input_operation_ = this->get_input_socket_reader(0);
}

void ConvertColorProfileOperation::execute_pixel_sampled(float output[4],
                                                         float x,
                                                         float y,
                                                         PixelSampler sampler)
{
  float color[4];
  input_operation_->read_sampled(color, x, y, sampler);
  IMB_buffer_float_from_float(
      output, color, 4, to_profile_, from_profile_, predivided_, 1, 1, 0, 0);
}

void ConvertColorProfileOperation::deinit_execution()
{
  input_operation_ = nullptr;
}

}  // namespace blender::compositor

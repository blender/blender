/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

#include "COM_ConvertColorProfileOperation.h"

#include "IMB_imbuf.h"

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

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
 * Copyright 2020, Blender Foundation.
 */

#include "COM_ColorExposureOperation.h"

namespace blender::compositor {

ExposureOperation::ExposureOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  input_program_ = nullptr;
  flags_.can_be_constant = true;
}

void ExposureOperation::init_execution()
{
  input_program_ = this->get_input_socket_reader(0);
  input_exposure_program_ = this->get_input_socket_reader(1);
}

void ExposureOperation::execute_pixel_sampled(float output[4],
                                              float x,
                                              float y,
                                              PixelSampler sampler)
{
  float input_value[4];
  float input_exposure[4];
  input_program_->read_sampled(input_value, x, y, sampler);
  input_exposure_program_->read_sampled(input_exposure, x, y, sampler);
  const float exposure = pow(2, input_exposure[0]);

  output[0] = input_value[0] * exposure;
  output[1] = input_value[1] * exposure;
  output[2] = input_value[2] * exposure;

  output[3] = input_value[3];
}

void ExposureOperation::update_memory_buffer_row(PixelCursor &p)
{
  for (; p.out < p.row_end; p.next()) {
    const float *in_value = p.ins[0];
    const float *in_exposure = p.ins[1];
    const float exposure = pow(2, in_exposure[0]);
    p.out[0] = in_value[0] * exposure;
    p.out[1] = in_value[1] * exposure;
    p.out[2] = in_value[2] * exposure;
    p.out[3] = in_value[3];
  }
}

void ExposureOperation::deinit_execution()
{
  input_program_ = nullptr;
  input_exposure_program_ = nullptr;
}

}  // namespace blender::compositor

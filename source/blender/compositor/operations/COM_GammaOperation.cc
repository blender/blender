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

#include "COM_GammaOperation.h"

namespace blender::compositor {

GammaOperation::GammaOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  input_program_ = nullptr;
  input_gamma_program_ = nullptr;
  flags_.can_be_constant = true;
}
void GammaOperation::init_execution()
{
  input_program_ = this->get_input_socket_reader(0);
  input_gamma_program_ = this->get_input_socket_reader(1);
}

void GammaOperation::execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler)
{
  float input_value[4];
  float input_gamma[4];

  input_program_->read_sampled(input_value, x, y, sampler);
  input_gamma_program_->read_sampled(input_gamma, x, y, sampler);
  const float gamma = input_gamma[0];
  /* check for negative to avoid nan's */
  output[0] = input_value[0] > 0.0f ? powf(input_value[0], gamma) : input_value[0];
  output[1] = input_value[1] > 0.0f ? powf(input_value[1], gamma) : input_value[1];
  output[2] = input_value[2] > 0.0f ? powf(input_value[2], gamma) : input_value[2];

  output[3] = input_value[3];
}

void GammaOperation::update_memory_buffer_row(PixelCursor &p)
{
  for (; p.out < p.row_end; p.next()) {
    const float *in_value = p.ins[0];
    const float *in_gamma = p.ins[1];
    const float gamma = in_gamma[0];
    /* Check for negative to avoid nan's. */
    p.out[0] = in_value[0] > 0.0f ? powf(in_value[0], gamma) : in_value[0];
    p.out[1] = in_value[1] > 0.0f ? powf(in_value[1], gamma) : in_value[1];
    p.out[2] = in_value[2] > 0.0f ? powf(in_value[2], gamma) : in_value[2];
    p.out[3] = in_value[3];
  }
}

void GammaOperation::deinit_execution()
{
  input_program_ = nullptr;
  input_gamma_program_ = nullptr;
}

}  // namespace blender::compositor

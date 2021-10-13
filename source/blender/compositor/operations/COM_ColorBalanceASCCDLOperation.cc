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

#include "COM_ColorBalanceASCCDLOperation.h"

namespace blender::compositor {

inline float colorbalance_cdl(float in, float offset, float power, float slope)
{
  float x = in * slope + offset;

  /* prevent NaN */
  if (x < 0.0f) {
    x = 0.0f;
  }

  return powf(x, power);
}

ColorBalanceASCCDLOperation::ColorBalanceASCCDLOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Color);
  this->add_output_socket(DataType::Color);
  input_value_operation_ = nullptr;
  input_color_operation_ = nullptr;
  this->set_canvas_input_index(1);
  flags_.can_be_constant = true;
}

void ColorBalanceASCCDLOperation::init_execution()
{
  input_value_operation_ = this->get_input_socket_reader(0);
  input_color_operation_ = this->get_input_socket_reader(1);
}

void ColorBalanceASCCDLOperation::execute_pixel_sampled(float output[4],
                                                        float x,
                                                        float y,
                                                        PixelSampler sampler)
{
  float input_color[4];
  float value[4];

  input_value_operation_->read_sampled(value, x, y, sampler);
  input_color_operation_->read_sampled(input_color, x, y, sampler);

  float fac = value[0];
  fac = MIN2(1.0f, fac);
  const float mfac = 1.0f - fac;

  output[0] = mfac * input_color[0] +
              fac * colorbalance_cdl(input_color[0], offset_[0], power_[0], slope_[0]);
  output[1] = mfac * input_color[1] +
              fac * colorbalance_cdl(input_color[1], offset_[1], power_[1], slope_[1]);
  output[2] = mfac * input_color[2] +
              fac * colorbalance_cdl(input_color[2], offset_[2], power_[2], slope_[2]);
  output[3] = input_color[3];
}

void ColorBalanceASCCDLOperation::update_memory_buffer_row(PixelCursor &p)
{
  for (; p.out < p.row_end; p.next()) {
    const float *in_factor = p.ins[0];
    const float *in_color = p.ins[1];
    const float fac = MIN2(1.0f, in_factor[0]);
    const float fac_m = 1.0f - fac;
    p.out[0] = fac_m * in_color[0] +
               fac * colorbalance_cdl(in_color[0], offset_[0], power_[0], slope_[0]);
    p.out[1] = fac_m * in_color[1] +
               fac * colorbalance_cdl(in_color[1], offset_[1], power_[1], slope_[1]);
    p.out[2] = fac_m * in_color[2] +
               fac * colorbalance_cdl(in_color[2], offset_[2], power_[2], slope_[2]);
    p.out[3] = in_color[3];
  }
}

void ColorBalanceASCCDLOperation::deinit_execution()
{
  input_value_operation_ = nullptr;
  input_color_operation_ = nullptr;
}

}  // namespace blender::compositor

/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  this->set_canvas_input_index(1);
  flags_.can_be_constant = true;
}

void ColorBalanceASCCDLOperation::update_memory_buffer_row(PixelCursor &p)
{
  for (; p.out < p.row_end; p.next()) {
    const float *in_factor = p.ins[0];
    const float *in_color = p.ins[1];
    const float fac = std::min(1.0f, in_factor[0]);
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

}  // namespace blender::compositor

/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_AlphaOverMixedOperation.h"

namespace blender::compositor {

AlphaOverMixedOperation::AlphaOverMixedOperation()
{
  x_ = 0.0f;
  flags_.can_be_constant = true;
}

void AlphaOverMixedOperation::update_memory_buffer_row(PixelCursor &p)
{
  for (; p.out < p.row_end; p.next()) {
    const float *color1 = p.color1;
    const float *over_color = p.color2;
    const float value = *p.value;

    if (over_color[3] <= 0.0f) {
      copy_v4_v4(p.out, color1);
    }
    else if (value == 1.0f && over_color[3] >= 1.0f) {
      copy_v4_v4(p.out, over_color);
    }
    else {
      const float addfac = 1.0f - x_ + over_color[3] * x_;
      const float premul = value * addfac;
      const float mul = 1.0f - value * over_color[3];

      p.out[0] = (mul * color1[0]) + premul * over_color[0];
      p.out[1] = (mul * color1[1]) + premul * over_color[1];
      p.out[2] = (mul * color1[2]) + premul * over_color[2];
      p.out[3] = (mul * color1[3]) + value * over_color[3];
    }
  }
}

}  // namespace blender::compositor

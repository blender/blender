/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GammaOperation.h"

namespace blender::compositor {

GammaOperation::GammaOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  flags_.can_be_constant = true;
}

void GammaOperation::update_memory_buffer_row(PixelCursor &p)
{
  for (; p.out < p.row_end; p.next()) {
    const float *in_value = p.ins[0];
    const float *in_gamma = p.ins[1];
    const float gamma = in_gamma[0];
    /* Check for negative to avoid NAN's. */
    p.out[0] = in_value[0] > 0.0f ? powf(in_value[0], gamma) : in_value[0];
    p.out[1] = in_value[1] > 0.0f ? powf(in_value[1], gamma) : in_value[1];
    p.out[2] = in_value[2] > 0.0f ? powf(in_value[2], gamma) : in_value[2];
    p.out[3] = in_value[3];
  }
}

}  // namespace blender::compositor

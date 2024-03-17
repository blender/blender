/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ColorExposureOperation.h"

namespace blender::compositor {

ExposureOperation::ExposureOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);
  flags_.can_be_constant = true;
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

}  // namespace blender::compositor

/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DotproductOperation.h"

namespace blender::compositor {

DotproductOperation::DotproductOperation()
{
  this->add_input_socket(DataType::Vector);
  this->add_input_socket(DataType::Vector);
  this->add_output_socket(DataType::Value);
  this->set_canvas_input_index(0);
  flags_.can_be_constant = true;
}

void DotproductOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *input1 = it.in(0);
    const float *input2 = it.in(1);
    *it.out = -(input1[0] * input2[0] + input1[1] * input2[1] + input1[2] * input2[2]);
  }
}

}  // namespace blender::compositor

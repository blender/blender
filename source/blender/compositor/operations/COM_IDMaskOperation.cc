/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_IDMaskOperation.h"

namespace blender::compositor {

IDMaskOperation::IDMaskOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  flags_.can_be_constant = true;
}

void IDMaskOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                   const rcti &area,
                                                   Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input = inputs[0];
  const int width = BLI_rcti_size_x(&area);
  for (int y = area.ymin; y < area.ymax; y++) {
    float *out = output->get_elem(area.xmin, y);
    const float *in = input->get_elem(area.xmin, y);
    const float *row_end = out + width * output->elem_stride;
    while (out < row_end) {
      out[0] = (roundf(in[0]) == object_index_) ? 1.0f : 0.0f;
      in += input->elem_stride;
      out += output->elem_stride;
    }
  }
}

}  // namespace blender::compositor

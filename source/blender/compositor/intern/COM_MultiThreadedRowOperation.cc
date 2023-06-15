/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MultiThreadedRowOperation.h"

namespace blender::compositor {

MultiThreadedRowOperation::PixelCursor::PixelCursor(const int num_inputs)
    : out(nullptr), out_stride(0), row_end(nullptr), ins(num_inputs), in_strides(num_inputs)
{
}

void MultiThreadedRowOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                             const rcti &area,
                                                             Span<MemoryBuffer *> inputs)
{
  BLI_assert(output != nullptr);
  const int width = BLI_rcti_size_x(&area);
  PixelCursor p(inputs.size());
  p.out_stride = output->elem_stride;
  for (int i = 0; i < p.in_strides.size(); i++) {
    p.in_strides[i] = inputs[i]->elem_stride;
  }

  for (int y = area.ymin; y < area.ymax; y++) {
    p.out = output->get_elem(area.xmin, y);
    for (int i = 0; i < p.ins.size(); i++) {
      p.ins[i] = inputs[i]->get_elem(area.xmin, y);
    }
    p.row_end = p.out + width * p.out_stride;
    update_memory_buffer_row(p);
  }
}

}  // namespace blender::compositor

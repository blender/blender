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
 * Copyright 2021, Blender Foundation.
 */

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

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

#include "COM_IDMaskOperation.h"

namespace blender::compositor {

IDMaskOperation::IDMaskOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  flags_.complex = true;
  flags_.can_be_constant = true;
}

void *IDMaskOperation::initialize_tile_data(rcti *rect)
{
  void *buffer = get_input_operation(0)->initialize_tile_data(rect);
  return buffer;
}

void IDMaskOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  MemoryBuffer *input_buffer = (MemoryBuffer *)data;
  const int buffer_width = input_buffer->get_width();
  float *buffer = input_buffer->get_buffer();
  int buffer_index = (y * buffer_width + x);
  output[0] = (roundf(buffer[buffer_index]) == object_index_) ? 1.0f : 0.0f;
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

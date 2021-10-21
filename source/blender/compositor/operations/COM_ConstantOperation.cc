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

#include "COM_ConstantOperation.h"

namespace blender::compositor {

ConstantOperation::ConstantOperation()
{
  needs_canvas_to_get_constant_ = false;
  flags_.is_constant_operation = true;
  flags_.is_fullframe_operation = true;
}

bool ConstantOperation::can_get_constant_elem() const
{
  return !needs_canvas_to_get_constant_ || flags_.is_canvas_set;
}

void ConstantOperation::update_memory_buffer(MemoryBuffer *output,
                                             const rcti &area,
                                             Span<MemoryBuffer *> UNUSED(inputs))
{
  BLI_assert(output->is_a_single_elem());
  const float *constant = get_constant_elem();
  float *out = output->get_elem(area.xmin, area.ymin);
  memcpy(out, constant, output->get_elem_bytes_len());
}

}  // namespace blender::compositor

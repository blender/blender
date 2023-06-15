/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
                                             Span<MemoryBuffer *> /*inputs*/)
{
  BLI_assert(output->is_a_single_elem());
  const float *constant = get_constant_elem();
  float *out = output->get_elem(area.xmin, area.ymin);
  memcpy(out, constant, output->get_elem_bytes_len());
}

}  // namespace blender::compositor

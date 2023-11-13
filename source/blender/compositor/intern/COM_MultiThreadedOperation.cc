/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MultiThreadedOperation.h"
#include "COM_ExecutionSystem.h"

namespace blender::compositor {

MultiThreadedOperation::MultiThreadedOperation()
{
  num_passes_ = 1;
  current_pass_ = 0;
  flags_.is_fullframe_operation = true;
}

void MultiThreadedOperation::update_memory_buffer(MemoryBuffer *output,
                                                  const rcti &area,
                                                  Span<MemoryBuffer *> inputs)
{
  for (current_pass_ = 0; current_pass_ < num_passes_; current_pass_++) {
    update_memory_buffer_started(output, area, inputs);
    exec_system_->execute_work(area, [=](const rcti &split_rect) {
      update_memory_buffer_partial(output, split_rect, inputs);
    });
    update_memory_buffer_finished(output, area, inputs);
  }
}

}  // namespace blender::compositor

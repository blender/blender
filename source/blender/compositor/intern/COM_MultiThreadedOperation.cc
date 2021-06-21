#include "COM_MultiThreadedOperation.h"
#include "COM_ExecutionSystem.h"

namespace blender::compositor {

MultiThreadedOperation::MultiThreadedOperation()
{
  m_num_passes = 1;
  current_pass_ = 0;
  flags.is_fullframe_operation = true;
}

void MultiThreadedOperation::update_memory_buffer(MemoryBuffer *output,
                                                  const rcti &output_area,
                                                  blender::Span<MemoryBuffer *> inputs,
                                                  ExecutionSystem &exec_system)
{
  for (current_pass_ = 0; current_pass_ < m_num_passes; current_pass_++) {
    update_memory_buffer_started(output, output_area, inputs, exec_system);
    exec_system.execute_work(output_area, [=, &exec_system](const rcti &split_rect) {
      update_memory_buffer_partial(output, split_rect, inputs, exec_system);
    });
    update_memory_buffer_finished(output, output_area, inputs, exec_system);
  }
}

}  // namespace blender::compositor

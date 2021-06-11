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

#pragma once

#include "COM_ExecutionModel.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace blender::compositor {

/* Forward declarations. */
class ExecutionGroup;

/**
 * Fully renders operations in order from inputs to outputs.
 */
class FullFrameExecutionModel : public ExecutionModel {
 private:
  /**
   * Contains operations active buffers data. Buffers will be disposed once reader operations are
   * finished.
   */
  SharedOperationBuffers &active_buffers_;

  /**
   * Number of operations finished.
   */
  int num_operations_finished_;

  /**
   * Order of priorities for output operations execution.
   */
  Vector<eCompositorPriority> priorities_;

  ThreadMutex work_mutex_;
  ThreadCondition work_finished_cond_;

 public:
  FullFrameExecutionModel(CompositorContext &context,
                          SharedOperationBuffers &shared_buffers,
                          Span<NodeOperation *> operations);
  ~FullFrameExecutionModel();

  void execute(ExecutionSystem &exec_system) override;

  void execute_work(const rcti &work_rect,
                    std::function<void(const rcti &split_rect)> work_func) override;

 private:
  void determine_areas_to_render_and_reads();
  void render_operations(ExecutionSystem &exec_system);
  void render_output_dependencies(NodeOperation *output_op, ExecutionSystem &exec_system);
  Vector<MemoryBuffer *> get_input_buffers(NodeOperation *op);
  MemoryBuffer *create_operation_buffer(NodeOperation *op);
  void render_operation(NodeOperation *op, ExecutionSystem &exec_system);

  void operation_finished(NodeOperation *operation);

  void get_output_render_area(NodeOperation *output_op, rcti &r_area);
  void determine_areas_to_render(NodeOperation *output_op, const rcti &output_area);
  void determine_reads(NodeOperation *output_op);

  void update_progress_bar();

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:FullFrameExecutionModel")
#endif
};

}  // namespace blender::compositor

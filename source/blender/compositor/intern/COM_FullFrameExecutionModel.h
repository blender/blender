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

#include "BLI_vector.hh"

#include "COM_Enums.h"
#include "COM_ExecutionModel.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace blender::compositor {

/* Forward declarations. */
class CompositorContext;
class ExecutionSystem;
class MemoryBuffer;
class NodeOperation;
class SharedOperationBuffers;

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

 public:
  FullFrameExecutionModel(CompositorContext &context,
                          SharedOperationBuffers &shared_buffers,
                          Span<NodeOperation *> operations);

  void execute(ExecutionSystem &exec_system) override;

 private:
  void determine_areas_to_render_and_reads();
  void render_operations();
  void render_output_dependencies(NodeOperation *output_op);
  Vector<MemoryBuffer *> get_input_buffers(NodeOperation *op,
                                           const int output_x,
                                           const int output_y);
  MemoryBuffer *create_operation_buffer(NodeOperation *op, const int output_x, const int output_y);
  void render_operation(NodeOperation *op);

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

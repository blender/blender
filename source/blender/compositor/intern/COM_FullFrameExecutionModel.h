/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
   * Contains operations active buffers data.
   * Buffers will be disposed once reader operations are finished.
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
  /**
   * Render output operations in order of priority.
   */
  void render_operations();
  void render_output_dependencies(NodeOperation *output_op);
  /**
   * Returns input buffers with an offset relative to given output coordinates.
   * Returned memory buffers must be deleted.
   */
  Vector<MemoryBuffer *> get_input_buffers(NodeOperation *op, int output_x, int output_y);
  MemoryBuffer *create_operation_buffer(NodeOperation *op, int output_x, int output_y);
  void render_operation(NodeOperation *op);

  void operation_finished(NodeOperation *operation);

  /**
   * Calculates given output operation area to be rendered taking into account viewer and render
   * borders.
   */
  void get_output_render_area(NodeOperation *output_op, rcti &r_area);
  /**
   * Determines all operations areas needed to render given output area.
   */
  void determine_areas_to_render(NodeOperation *output_op, const rcti &output_area);
  /**
   * Determines reads to receive by operations in output operation tree (i.e: Number of dependent
   * operations each operation has).
   */
  void determine_reads(NodeOperation *output_op);

  void update_progress_bar();

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:FullFrameExecutionModel")
#endif
};

}  // namespace blender::compositor

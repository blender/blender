/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_defines.h"

namespace blender::compositor {

class NodeOperation;
class NodeOperationBuilder;
class ConstantOperation;
class MemoryBuffer;

/**
 * Evaluates all operations with constant elements into primitive constant operations
 * (Value/Vector/Color).
 */
class ConstantFolder {
 private:
  NodeOperationBuilder &operations_builder_;

  /** Constant operations buffers. */
  Map<ConstantOperation *, MemoryBuffer *> constant_buffers_;

  rcti max_area_;
  rcti first_elem_area_;

 public:
  /**
   * \param operations_builder: Contains all operations to fold.
   * \param exec_system: Execution system.
   */
  ConstantFolder(NodeOperationBuilder &operations_builder);
  /**
   * Evaluate operations with constant elements into primitive constant operations.
   */
  int fold_operations();

 private:
  /** Returns constant operations resulted from folded operations. */
  Vector<ConstantOperation *> try_fold_operations(Span<NodeOperation *> operations);
  ConstantOperation *fold_operation(NodeOperation *operation);

  MemoryBuffer *create_constant_buffer(DataType data_type);
  Vector<MemoryBuffer *> get_constant_input_buffers(NodeOperation *operation);
  void delete_constant_buffers();

  void get_operation_output_operations(NodeOperation *operation,
                                       Vector<NodeOperation *> &r_outputs);
};

}  // namespace blender::compositor

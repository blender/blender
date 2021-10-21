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
  ConstantFolder(NodeOperationBuilder &operations_builder);
  int fold_operations();

 private:
  Vector<ConstantOperation *> try_fold_operations(Span<NodeOperation *> operations);
  ConstantOperation *fold_operation(NodeOperation *operation);

  MemoryBuffer *create_constant_buffer(DataType data_type);
  Vector<MemoryBuffer *> get_constant_input_buffers(NodeOperation *operation);
  void delete_constant_buffers();

  void get_operation_output_operations(NodeOperation *operation,
                                       Vector<NodeOperation *> &r_outputs);
};

}  // namespace blender::compositor

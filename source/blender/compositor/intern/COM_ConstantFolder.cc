/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_map.hh"
#include "BLI_set.hh"

#include "COM_ConstantFolder.h"
#include "COM_NodeOperationBuilder.h"
#include "COM_SetColorOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_WorkScheduler.h"

namespace blender::compositor {

ConstantFolder::ConstantFolder(NodeOperationBuilder &operations_builder)
    : operations_builder_(operations_builder)
{
  BLI_rcti_init(&max_area_, INT_MIN, INT_MAX, INT_MIN, INT_MAX);
  BLI_rcti_init(&first_elem_area_, 0, 1, 0, 1);
}

static bool is_constant_foldable(NodeOperation *operation)
{
  if (operation->get_flags().can_be_constant && !operation->get_flags().is_constant_operation) {
    for (int i = 0; i < operation->get_number_of_input_sockets(); i++) {
      NodeOperation *input = operation->get_input_operation(i);
      if (!input->get_flags().is_constant_operation ||
          !static_cast<ConstantOperation *>(input)->can_get_constant_elem())
      {
        return false;
      }
    }
    return true;
  }
  return false;
}

static Set<NodeOperation *> find_constant_foldable_operations(Span<NodeOperation *> operations)
{
  Set<NodeOperation *> foldable_ops;
  for (NodeOperation *op : operations) {
    if (is_constant_foldable(op)) {
      foldable_ops.add(op);
    }
  }
  return foldable_ops;
}

static ConstantOperation *create_constant_operation(DataType data_type, const float *constant_elem)
{
  switch (data_type) {
    case DataType::Color: {
      SetColorOperation *color_op = new SetColorOperation();
      color_op->set_channels(constant_elem);
      return color_op;
    }
    case DataType::Vector: {
      SetVectorOperation *vector_op = new SetVectorOperation();
      vector_op->set_vector(constant_elem);
      return vector_op;
    }
    case DataType::Value: {
      SetValueOperation *value_op = new SetValueOperation();
      value_op->set_value(*constant_elem);
      return value_op;
    }
    default: {
      BLI_assert_msg(0, "Non implemented data type");
      return nullptr;
    }
  }
}

ConstantOperation *ConstantFolder::fold_operation(NodeOperation *operation)
{
  const DataType data_type = operation->get_output_socket()->get_data_type();
  MemoryBuffer fold_buf(data_type, first_elem_area_);
  Vector<MemoryBuffer *> input_bufs = get_constant_input_buffers(operation);
  operation->init_data();
  operation->render(&fold_buf, {first_elem_area_}, input_bufs);

  MemoryBuffer *constant_buf = create_constant_buffer(data_type);
  constant_buf->copy_from(&fold_buf, first_elem_area_);
  ConstantOperation *constant_op = create_constant_operation(data_type,
                                                             constant_buf->get_buffer());
  operations_builder_.replace_operation_with_constant(operation, constant_op);
  constant_buffers_.add_new(constant_op, constant_buf);
  return constant_op;
}

MemoryBuffer *ConstantFolder::create_constant_buffer(const DataType data_type)
{
  /* Create a single elem buffer with maximum area possible so readers can read any coordinate
   * returning always same element. */
  return new MemoryBuffer(data_type, max_area_, true);
}

Vector<MemoryBuffer *> ConstantFolder::get_constant_input_buffers(NodeOperation *operation)
{
  const int num_inputs = operation->get_number_of_input_sockets();
  Vector<MemoryBuffer *> inputs_bufs(num_inputs);
  for (int i = 0; i < num_inputs; i++) {
    BLI_assert(operation->get_input_operation(i)->get_flags().is_constant_operation);
    ConstantOperation *constant_op = static_cast<ConstantOperation *>(
        operation->get_input_operation(i));
    MemoryBuffer *constant_buf = constant_buffers_.lookup_or_add_cb(constant_op, [=] {
      MemoryBuffer *buf = create_constant_buffer(
          constant_op->get_output_socket()->get_data_type());
      constant_op->render(buf, {first_elem_area_}, {});
      return buf;
    });
    inputs_bufs[i] = constant_buf;
  }
  return inputs_bufs;
}

Vector<ConstantOperation *> ConstantFolder::try_fold_operations(Span<NodeOperation *> operations)
{
  Set<NodeOperation *> foldable_ops = find_constant_foldable_operations(operations);
  if (foldable_ops.is_empty()) {
    return Vector<ConstantOperation *>();
  }

  Vector<ConstantOperation *> new_folds;
  for (NodeOperation *op : foldable_ops) {
    ConstantOperation *constant_op = fold_operation(op);
    new_folds.append(constant_op);
  }
  return new_folds;
}

int ConstantFolder::fold_operations()
{
  WorkScheduler::start(operations_builder_.context());
  Vector<ConstantOperation *> last_folds = try_fold_operations(
      operations_builder_.get_operations());
  int folds_count = last_folds.size();
  while (last_folds.size() > 0) {
    Vector<NodeOperation *> ops_to_fold;
    for (ConstantOperation *fold : last_folds) {
      get_operation_output_operations(fold, ops_to_fold);
    }
    last_folds = try_fold_operations(ops_to_fold);
    folds_count += last_folds.size();
  }
  WorkScheduler::stop();

  delete_constant_buffers();

  return folds_count;
}

void ConstantFolder::delete_constant_buffers()
{
  for (MemoryBuffer *buf : constant_buffers_.values()) {
    delete buf;
  }
  constant_buffers_.clear();
}

void ConstantFolder::get_operation_output_operations(NodeOperation *operation,
                                                     Vector<NodeOperation *> &r_outputs)
{
  const Vector<NodeOperationBuilder::Link> &links = operations_builder_.get_links();
  for (const NodeOperationBuilder::Link &link : links) {
    if (&link.from()->get_operation() == operation) {
      r_outputs.append(&link.to()->get_operation());
    }
  }
}

}  // namespace blender::compositor

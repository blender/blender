/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "COM_context.hh"
#include "COM_node_operation.hh"
#include "COM_result.hh"
#include "COM_undefined_node_operation.hh"

namespace blender::compositor {

/* A node operation that allocates all of its outputs as invalid. */
class UndefinedNodeOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    this->allocate_default_remaining_outputs();
  }
};

NodeOperation *get_undefined_node_operation(Context &context, const bNode &node)
{
  return new UndefinedNodeOperation(context, node);
}

}  // namespace blender::compositor

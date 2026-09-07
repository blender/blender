/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "DNA_node_types.h"

#include "BKE_node.hh"

#include "COM_context.hh"
#include "COM_node_operation.hh"
#include "COM_operation.hh"
#include "COM_pixel_operation.hh"
#include "COM_result.hh"

namespace blender {
class ComputeContext;
}  // namespace blender

namespace blender::compositor {

/* ------------------------------------------------------------------------------------------------
 * Node Group Operation
 *
 * The node group operation represents and evaluates a node group. */
class NodeGroupOperation : public Operation {
 private:
  /* The node group that this operation represents. */
  const bNodeTree &node_group_;
  /* A compute context that identifies the particular group node that uses this node group or the
   * scene for the top-level compositor node tree. */
  const ComputeContext &compute_context_;

 public:
  /* Populate the output results based on the node group interface outputs and populate the input
   * descriptors based on the node group interface inputs. */
  NodeGroupOperation(Context &context,
                     const bNodeTree &node_group,
                     const ComputeContext &compute_context);

  /* Compile and evaluate the node group. */
  void execute() override;

  /* An accessors for node_group_. */
  const bNodeTree &node_group() const;

  /* An accessors for compute_context_. */
  const ComputeContext &compute_context() const;
};

}  // namespace blender::compositor

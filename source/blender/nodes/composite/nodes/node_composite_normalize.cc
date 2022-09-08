/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** NORMALIZE single channel, useful for Z buffer ******************** */

namespace blender::nodes::node_composite_normalize_cc {

static void cmp_node_normalize_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Value")).default_value(1.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Float>(N_("Value"));
}

using namespace blender::realtime_compositor;

class NormalizeOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Value").pass_through(get_result("Value"));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new NormalizeOperation(context, node);
}

}  // namespace blender::nodes::node_composite_normalize_cc

void register_node_type_cmp_normalize()
{
  namespace file_ns = blender::nodes::node_composite_normalize_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_NORMALIZE, "Normalize", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::cmp_node_normalize_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}

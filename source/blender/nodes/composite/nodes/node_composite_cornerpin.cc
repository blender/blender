/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_cornerpin_cc {

static void cmp_node_cornerpin_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Vector>(N_("Upper Left"))
      .default_value({0.0f, 1.0f, 0.0f})
      .min(0.0f)
      .max(1.0f);
  b.add_input<decl::Vector>(N_("Upper Right"))
      .default_value({1.0f, 1.0f, 0.0f})
      .min(0.0f)
      .max(1.0f);
  b.add_input<decl::Vector>(N_("Lower Left"))
      .default_value({0.0f, 0.0f, 0.0f})
      .min(0.0f)
      .max(1.0f);
  b.add_input<decl::Vector>(N_("Lower Right"))
      .default_value({1.0f, 0.0f, 0.0f})
      .min(0.0f)
      .max(1.0f);
  b.add_output<decl::Color>(N_("Image"));
  b.add_output<decl::Float>(N_("Plane"));
}

using namespace blender::realtime_compositor;

class CornerPinOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Image").pass_through(get_result("Image"));
    get_result("Plane").allocate_invalid();
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new CornerPinOperation(context, node);
}

}  // namespace blender::nodes::node_composite_cornerpin_cc

void register_node_type_cmp_cornerpin()
{
  namespace file_ns = blender::nodes::node_composite_cornerpin_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CORNERPIN, "Corner Pin", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_cornerpin_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Displace  ******************** */

namespace blender::nodes::node_composite_displace_cc {

static void cmp_node_displace_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Vector>(N_("Vector"))
      .default_value({1.0f, 1.0f, 1.0f})
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_TRANSLATION);
  b.add_input<decl::Float>(N_("X Scale")).default_value(0.0f).min(-1000.0f).max(1000.0f);
  b.add_input<decl::Float>(N_("Y Scale")).default_value(0.0f).min(-1000.0f).max(1000.0f);
  b.add_output<decl::Color>(N_("Image"));
}

using namespace blender::realtime_compositor;

class DisplaceOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Image").pass_through(get_result("Image"));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DisplaceOperation(context, node);
}

}  // namespace blender::nodes::node_composite_displace_cc

void register_node_type_cmp_displace()
{
  namespace file_ns = blender::nodes::node_composite_displace_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DISPLACE, "Displace", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_displace_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}

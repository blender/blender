/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Pixelate ******************** */

namespace blender::nodes::node_composite_pixelate_cc {

static void cmp_node_pixelate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Color"));
  b.add_output<decl::Color>(N_("Color"));
}

using namespace blender::realtime_compositor;

class PixelateOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    get_input("Color").pass_through(get_result("Color"));
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new PixelateOperation(context, node);
}

}  // namespace blender::nodes::node_composite_pixelate_cc

void register_node_type_cmp_pixelate()
{
  namespace file_ns = blender::nodes::node_composite_pixelate_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_PIXELATE, "Pixelate", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_pixelate_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector_types.hh"

#include "DNA_node_types.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** RGB ******************** */

namespace blender::nodes::node_composite_rgb_cc {

static void cmp_node_rgb_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>(N_("RGBA")).default_value({0.5f, 0.5f, 0.5f, 1.0f});
}

using namespace blender::realtime_compositor;

class RGBOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &result = get_result("RGBA");
    result.allocate_single_value();

    const bNodeSocket *socket = static_cast<const bNodeSocket *>(bnode().outputs.first);
    float4 color = float4(static_cast<const bNodeSocketValueRGBA *>(socket->default_value)->value);

    result.set_color_value(color);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new RGBOperation(context, node);
}

}  // namespace blender::nodes::node_composite_rgb_cc

void register_node_type_cmp_rgb()
{
  namespace file_ns = blender::nodes::node_composite_rgb_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_RGB, "RGB", NODE_CLASS_INPUT);
  ntype.declare = file_ns::cmp_node_rgb_declare;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::DEFAULT);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}

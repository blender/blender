/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"
#include "BLI_math_matrix.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Rotate  ******************** */

namespace blender::nodes::node_composite_rotate_cc {

static void cmp_node_rotate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Degr")
      .default_value(0.0f)
      .min(-10000.0f)
      .max(10000.0f)
      .subtype(PROP_ANGLE)
      .compositor_expects_single_value();
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_rotate(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = 1; /* Bilinear Filter. */
}

static void node_composit_buts_rotate(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "filter_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::realtime_compositor;

class RotateOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input = get_input("Image");
    Result &result = get_result("Image");
    input.pass_through(result);

    const math::AngleRadian rotation = get_input("Degr").get_float_value_default(0.0f);

    const float3x3 transformation = math::from_rotation<float3x3>(rotation);

    result.transform(transformation);
    result.get_realization_options().interpolation = get_interpolation();
  }

  Interpolation get_interpolation()
  {
    switch (bnode().custom1) {
      case 0:
        return Interpolation::Nearest;
      case 1:
        return Interpolation::Bilinear;
      case 2:
        return Interpolation::Bicubic;
    }

    BLI_assert_unreachable();
    return Interpolation::Nearest;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new RotateOperation(context, node);
}

}  // namespace blender::nodes::node_composite_rotate_cc

void register_node_type_cmp_rotate()
{
  namespace file_ns = blender::nodes::node_composite_rotate_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_ROTATE, "Rotate", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_rotate_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_rotate;
  ntype.initfunc = file_ns::node_composit_init_rotate;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}

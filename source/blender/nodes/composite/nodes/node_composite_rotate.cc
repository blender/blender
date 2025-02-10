/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"
#include "BLI_math_angle_types.hh"
#include "BLI_math_matrix.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Rotate  ******************** */

namespace blender::nodes::node_composite_rotate_cc {

static void cmp_node_rotate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_realization_mode(CompositorInputRealizationMode::None)
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
  node->custom1 = CMP_NODE_INTERPOLATION_BILINEAR;
}

static void node_composit_buts_rotate(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "filter_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::compositor;

class RotateOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input = this->get_input("Image");
    Result &output = this->get_result("Image");

    const math::AngleRadian rotation = this->get_input("Degr").get_single_value_default(0.0f);
    const float3x3 transformation = math::from_rotation<float3x3>(rotation);

    input.pass_through(output);
    output.transform(transformation);
    output.get_realization_options().interpolation = this->get_interpolation();
  }

  Interpolation get_interpolation()
  {
    switch (static_cast<CMPNodeInterpolation>(bnode().custom1)) {
      case CMP_NODE_INTERPOLATION_NEAREST:
        return Interpolation::Nearest;
      case CMP_NODE_INTERPOLATION_BILINEAR:
        return Interpolation::Bilinear;
      case CMP_NODE_INTERPOLATION_BICUBIC:
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

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeRotate", CMP_NODE_ROTATE);
  ntype.ui_name = "Rotate";
  ntype.ui_description = "Rotate image by specified angle";
  ntype.enum_name_legacy = "ROTATE";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = file_ns::cmp_node_rotate_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_rotate;
  ntype.initfunc = file_ns::node_composit_init_rotate;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}

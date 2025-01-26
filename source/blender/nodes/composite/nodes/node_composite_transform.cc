/* SPDX-FileCopyrightText: 2011 Blender Authors
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

/* **************** Transform  ******************** */

namespace blender::nodes::node_composite_transform_cc {

static void cmp_node_transform_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .compositor_domain_priority(0)
      .compositor_realization_mode(CompositorInputRealizationMode::None);
  b.add_input<decl::Float>("X")
      .default_value(0.0f)
      .min(-10000.0f)
      .max(10000.0f)
      .compositor_expects_single_value();
  b.add_input<decl::Float>("Y")
      .default_value(0.0f)
      .min(-10000.0f)
      .max(10000.0f)
      .compositor_expects_single_value();
  b.add_input<decl::Float>("Angle")
      .default_value(0.0f)
      .min(-10000.0f)
      .max(10000.0f)
      .subtype(PROP_ANGLE)
      .compositor_expects_single_value();
  b.add_input<decl::Float>("Scale")
      .default_value(1.0f)
      .min(0.0001f)
      .max(CMP_SCALE_MAX)
      .compositor_expects_single_value();
  b.add_output<decl::Color>("Image");
}

static void node_composit_buts_transform(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "filter_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::compositor;

class TransformOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input = this->get_input("Image");
    Result &output = this->get_result("Image");

    const float2 translation = float2(this->get_input("X").get_single_value_default(0.0f),
                                      this->get_input("Y").get_single_value_default(0.0f));
    const math::AngleRadian rotation = this->get_input("Angle").get_single_value_default(0.0f);
    const float2 scale = float2(this->get_input("Scale").get_single_value_default(1.0f));
    const float3x3 transformation = math::from_loc_rot_scale<float3x3>(
        translation, rotation, scale);

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
  return new TransformOperation(context, node);
}

}  // namespace blender::nodes::node_composite_transform_cc

void register_node_type_cmp_transform()
{
  namespace file_ns = blender::nodes::node_composite_transform_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeTransform", CMP_NODE_TRANSFORM);
  ntype.ui_name = "Transform";
  ntype.ui_description = "Scale, translate and rotate an image";
  ntype.enum_name_legacy = "TRANSFORM";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = file_ns::cmp_node_transform_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_transform;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}

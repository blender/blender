/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BKE_node.hh"

#include "BLI_assert.h"
#include "BLI_math_angle_types.hh"
#include "BLI_math_matrix.hh"

#include "COM_node_operation.hh"

#include "DNA_node_types.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "MEM_guardedalloc.h"

#include "node_composite_util.hh"

/* **************** Transform  ******************** */

namespace blender::nodes::node_composite_transform_cc {

NODE_STORAGE_FUNCS(NodeTransformData)

static void cmp_node_init_transform(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTransformData *data = MEM_callocN<NodeTransformData>(__func__);
  data->interpolation = CMP_NODE_INTERPOLATION_NEAREST;
  data->extension_x = CMP_NODE_EXTENSION_MODE_CLIP;
  data->extension_y = CMP_NODE_EXTENSION_MODE_CLIP;
  node->storage = data;
}

static void cmp_node_transform_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .compositor_realization_mode(CompositorInputRealizationMode::None)
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Float>("X").default_value(0.0f).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Y").default_value(0.0f).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Angle").default_value(0.0f).min(-10000.0f).max(10000.0f).subtype(
      PROP_ANGLE);
  b.add_input<decl::Float>("Scale").default_value(1.0f).min(0.0001f).max(CMP_SCALE_MAX);
  b.add_output<decl::Color>("Image");
}

static void node_composit_buts_transform(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout &column = layout->column(true);
  column.prop(ptr, "interpolation", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiLayout &row = column.row(true);
  row.prop(ptr, "extension_x", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  row.prop(ptr, "extension_y", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::compositor;

class TransformOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const float2 translation = float2(this->get_input("X").get_single_value_default(0.0f),
                                      this->get_input("Y").get_single_value_default(0.0f));
    const math::AngleRadian rotation = this->get_input("Angle").get_single_value_default(0.0f);
    const float2 scale = float2(this->get_input("Scale").get_single_value_default(1.0f));
    const float3x3 transformation = math::from_loc_rot_scale<float3x3>(
        translation, rotation, scale);

    const Result &input = this->get_input("Image");
    Result &output = this->get_result("Image");
    output.share_data(input);
    output.transform(transformation);
    output.get_realization_options().interpolation = this->get_interpolation();
    output.get_realization_options().extension_x = this->get_extension_mode_x();
    output.get_realization_options().extension_y = this->get_extension_mode_y();
  }

  ExtensionMode get_extension_mode_x()
  {
    switch (static_cast<CMPExtensionMode>(node_storage(bnode()).extension_x)) {
      case CMP_NODE_EXTENSION_MODE_CLIP:
        return ExtensionMode::Clip;
      case CMP_NODE_EXTENSION_MODE_REPEAT:
        return ExtensionMode::Repeat;
      case CMP_NODE_EXTENSION_MODE_EXTEND:
        return ExtensionMode::Extend;
    }

    BLI_assert_unreachable();
    return ExtensionMode::Clip;
  }

  ExtensionMode get_extension_mode_y()
  {
    switch (static_cast<CMPExtensionMode>(node_storage(bnode()).extension_y)) {
      case CMP_NODE_EXTENSION_MODE_CLIP:
        return ExtensionMode::Clip;
      case CMP_NODE_EXTENSION_MODE_REPEAT:
        return ExtensionMode::Repeat;
      case CMP_NODE_EXTENSION_MODE_EXTEND:
        return ExtensionMode::Extend;
    }

    BLI_assert_unreachable();
    return ExtensionMode::Clip;
  }

  Interpolation get_interpolation()
  {
    switch (static_cast<CMPNodeInterpolation>(node_storage(bnode()).interpolation)) {
      case CMP_NODE_INTERPOLATION_NEAREST:
        return Interpolation::Nearest;
      case CMP_NODE_INTERPOLATION_BILINEAR:
        return Interpolation::Bilinear;
      case CMP_NODE_INTERPOLATION_ANISOTROPIC:
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

static void register_node_type_cmp_transform()
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
  ntype.initfunc = file_ns::cmp_node_init_transform;
  blender::bke::node_type_storage(
      ntype, "NodeTransformData", node_free_standard_storage, node_copy_standard_storage);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_transform)

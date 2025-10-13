/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_angle_types.hh"
#include "BLI_math_matrix.hh"

#include "DNA_node_types.h"

#include "RNA_enum_types.hh"

#include "BKE_node.hh"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_transform_cc {

static void cmp_node_transform_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Color>("Image")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .hide_value()
      .compositor_realization_mode(CompositorInputRealizationMode::None)
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Float>("X").default_value(0.0f).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Y").default_value(0.0f).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Angle").default_value(0.0f).min(-10000.0f).max(10000.0f).subtype(
      PROP_ANGLE);
  b.add_input<decl::Float>("Scale").default_value(1.0f).min(0.0001f).max(CMP_SCALE_MAX);

  PanelDeclarationBuilder &sampling_panel = b.add_panel("Sampling").default_closed(true);
  sampling_panel.add_input<decl::Menu>("Interpolation")
      .default_value(CMP_NODE_INTERPOLATION_BILINEAR)
      .static_items(rna_enum_node_compositor_interpolation_items)
      .optional_label()
      .description("Interpolation method");
  sampling_panel.add_input<decl::Menu>("Extension X")
      .default_value(CMP_NODE_EXTENSION_MODE_CLIP)
      .static_items(rna_enum_node_compositor_extension_items)
      .optional_label()
      .description("The extension mode applied to the X axis");
  sampling_panel.add_input<decl::Menu>("Extension Y")
      .default_value(CMP_NODE_EXTENSION_MODE_CLIP)
      .static_items(rna_enum_node_compositor_extension_items)
      .optional_label()
      .description("The extension mode applied to the Y axis");
}

static void cmp_node_init_transform(bNodeTree * /*ntree*/, bNode *node)
{
  /* Unused, kept for forward compatibility. */
  NodeTransformData *data = MEM_callocN<NodeTransformData>(__func__);
  node->storage = data;
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

  Interpolation get_interpolation()
  {
    const Result &input = this->get_input("Interpolation");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_INTERPOLATION_BILINEAR);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    const CMPNodeInterpolation interpolation = static_cast<CMPNodeInterpolation>(menu_value.value);
    switch (interpolation) {
      case CMP_NODE_INTERPOLATION_NEAREST:
        return Interpolation::Nearest;
      case CMP_NODE_INTERPOLATION_BILINEAR:
        return Interpolation::Bilinear;
      case CMP_NODE_INTERPOLATION_ANISOTROPIC:
      case CMP_NODE_INTERPOLATION_BICUBIC:
        return Interpolation::Bicubic;
    }

    return Interpolation::Nearest;
  }

  ExtensionMode get_extension_mode_x()
  {
    const Result &input = this->get_input("Extension X");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_EXTENSION_MODE_CLIP);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    const CMPExtensionMode extension_x = static_cast<CMPExtensionMode>(menu_value.value);
    switch (extension_x) {
      case CMP_NODE_EXTENSION_MODE_CLIP:
        return ExtensionMode::Clip;
      case CMP_NODE_EXTENSION_MODE_REPEAT:
        return ExtensionMode::Repeat;
      case CMP_NODE_EXTENSION_MODE_EXTEND:
        return ExtensionMode::Extend;
    }

    return ExtensionMode::Clip;
  }

  ExtensionMode get_extension_mode_y()
  {
    const Result &input = this->get_input("Extension Y");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_EXTENSION_MODE_CLIP);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    const CMPExtensionMode extension_y = static_cast<CMPExtensionMode>(menu_value.value);
    switch (extension_y) {
      case CMP_NODE_EXTENSION_MODE_CLIP:
        return ExtensionMode::Clip;
      case CMP_NODE_EXTENSION_MODE_REPEAT:
        return ExtensionMode::Repeat;
      case CMP_NODE_EXTENSION_MODE_EXTEND:
        return ExtensionMode::Extend;
    }

    return ExtensionMode::Clip;
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
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.initfunc = file_ns::cmp_node_init_transform;
  blender::bke::node_type_storage(
      ntype, "NodeTransformData", node_free_standard_storage, node_copy_standard_storage);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_transform)

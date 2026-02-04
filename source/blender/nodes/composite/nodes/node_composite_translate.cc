/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"

#include "DNA_node_types.h"

#include "RNA_enum_types.hh"

#include "COM_domain.hh"
#include "COM_node_operation.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_translate_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .compositor_realization_mode(CompositorInputRealizationMode::None)
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Float>("X").default_value(0.0f).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Y").default_value(0.0f).min(-10000.0f).max(10000.0f);

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

static void node_init(bNodeTree * /*ntree*/, bNode *node)
{
  /* Unused, kept for forward compatibility. */
  NodeTranslateData *data = MEM_new<NodeTranslateData>(__func__);
  node->storage = data;
}

using namespace blender::compositor;

class TranslateOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Image");

    float x = this->get_input("X").get_single_value_default<float>();
    float y = this->get_input("Y").get_single_value_default<float>();
    const float2 translation = float2(x, y);

    Result &output = this->get_result("Image");
    output.share_data(input);
    output.transform(math::from_location<float3x3>(translation));
    output.get_realization_options().interpolation = this->get_interpolation();
    output.get_realization_options().extension_x = this->get_extension_mode_x();
    output.get_realization_options().extension_y = this->get_extension_mode_y();
  }

  Interpolation get_interpolation()
  {
    const CMPNodeInterpolation interpolation = CMPNodeInterpolation(
        this->get_input("Interpolation").get_single_value_default<MenuValue>().value);
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

  Extension get_extension_mode_x()
  {
    const CMPExtensionMode extension_x = CMPExtensionMode(
        this->get_input("Extension X").get_single_value_default<MenuValue>().value);
    switch (extension_x) {
      case CMP_NODE_EXTENSION_MODE_CLIP:
        return Extension::Clip;
      case CMP_NODE_EXTENSION_MODE_REPEAT:
        return Extension::Repeat;
      case CMP_NODE_EXTENSION_MODE_EXTEND:
        return Extension::Extend;
    }

    return Extension::Clip;
  }

  Extension get_extension_mode_y()
  {
    const CMPExtensionMode extension_y = CMPExtensionMode(
        this->get_input("Extension Y").get_single_value_default<MenuValue>().value);
    switch (extension_y) {
      case CMP_NODE_EXTENSION_MODE_CLIP:
        return Extension::Clip;
      case CMP_NODE_EXTENSION_MODE_REPEAT:
        return Extension::Repeat;
      case CMP_NODE_EXTENSION_MODE_EXTEND:
        return Extension::Extend;
    }

    return Extension::Clip;
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new TranslateOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeTranslate", CMP_NODE_TRANSLATE);
  ntype.ui_name = "Translate";
  ntype.ui_description = "Offset an image";
  ntype.enum_name_legacy = "TRANSLATE";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  bke::node_type_storage(
      ntype, "NodeTranslateData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = get_compositor_operation;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_translate_cc

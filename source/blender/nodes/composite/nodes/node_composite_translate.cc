/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"
#include "BLI_math_matrix.hh"

#include "DNA_node_types.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "COM_domain.hh"
#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Translate ******************** */

namespace blender::nodes::node_composite_translate_cc {

NODE_STORAGE_FUNCS(NodeTranslateData)

static void cmp_node_translate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_realization_mode(CompositorInputRealizationMode::None)
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Float>("X").default_value(0.0f).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Y").default_value(0.0f).min(-10000.0f).max(10000.0f);

  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic);
}

static void node_composit_init_translate(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTranslateData *data = MEM_callocN<NodeTranslateData>(__func__);
  node->storage = data;
}

static void node_composit_buts_translate(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout &column = layout->column(true);
  column.prop(ptr, "interpolation", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiLayout &row = column.row(true);
  row.prop(ptr, "extension_x", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  row.prop(ptr, "extension_y", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::compositor;

class TranslateOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Image");

    float x = this->get_input("X").get_single_value_default(0.0f);
    float y = this->get_input("Y").get_single_value_default(0.0f);
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
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new TranslateOperation(context, node);
}

}  // namespace blender::nodes::node_composite_translate_cc

static void register_node_type_cmp_translate()
{
  namespace file_ns = blender::nodes::node_composite_translate_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeTranslate", CMP_NODE_TRANSLATE);
  ntype.ui_name = "Translate";
  ntype.ui_description = "Offset an image";
  ntype.enum_name_legacy = "TRANSLATE";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = file_ns::cmp_node_translate_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_translate;
  ntype.initfunc = file_ns::node_composit_init_translate;
  blender::bke::node_type_storage(
      ntype, "NodeTranslateData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_translate)

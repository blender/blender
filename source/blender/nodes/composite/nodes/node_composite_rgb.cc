/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_vector_types.hh"

#include "DNA_node_types.h"

#include "COM_node_operation.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_composite_util.hh"

/* **************** RGB ******************** */

namespace blender::nodes::node_composite_rgb_cc {

static void cmp_node_rgb_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Color")
      .default_value({0.5f, 0.5f, 0.5f, 1.0f})
      .custom_draw([](CustomSocketDrawParams &params) {
        params.layout.alignment_set(ui::LayoutAlign::Expand);
        uiLayout &col = params.layout.column(false);
        uiTemplateColorPicker(
            &col, &params.socket_ptr, "default_value", true, false, false, false);
        col.prop(&params.socket_ptr,
                 "default_value",
                 UI_ITEM_R_SLIDER | UI_ITEM_R_SPLIT_EMPTY_NAME,
                 "",
                 ICON_NONE);
      });
}

using namespace blender::compositor;

class RGBOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &result = get_result("Color");
    result.allocate_single_value();

    const bNodeSocket *socket = static_cast<const bNodeSocket *>(bnode().outputs.first);
    Color color = Color(static_cast<const bNodeSocketValueRGBA *>(socket->default_value)->value);

    result.set_single_value(color);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new RGBOperation(context, node);
}

}  // namespace blender::nodes::node_composite_rgb_cc

static void register_node_type_cmp_rgb()
{
  namespace file_ns = blender::nodes::node_composite_rgb_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeRGB", CMP_NODE_RGB);
  ntype.ui_name = "Color";
  ntype.ui_description = "A color picker";
  ntype.enum_name_legacy = "RGB";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::cmp_node_rgb_declare;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Default);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_rgb)

/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_types.h"

#include "COM_node_operation.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_color_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Color")
      .default_value({0.5f, 0.5f, 0.5f, 1.0f})
      .custom_draw([](CustomSocketDrawParams &params) {
        params.layout.alignment_set(ui::LayoutAlign::Expand);
        ui::Layout &col = params.layout.column(false);
        template_color_picker(
            &col, &params.socket_ptr, "default_value", true, false, false, false);
        col.prop(&params.socket_ptr,
                 "default_value",
                 ui::ITEM_R_SLIDER | ui::ITEM_R_SPLIT_EMPTY_NAME,
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

    const bNodeSocket *socket = static_cast<const bNodeSocket *>(node().outputs.first);
    Color color = Color(static_cast<const bNodeSocketValueRGBA *>(socket->default_value)->value);

    result.set_single_value(color);
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new RGBOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeRGB", CMP_NODE_RGB);
  ntype.ui_name = "Color";
  ntype.ui_description = "A color picker";
  ntype.enum_name_legacy = "RGB";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  bke::node_type_size_preset(ntype, bke::eNodeSizePreset::Default);
  ntype.get_compositor_operation = get_compositor_operation;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_color_cc

/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_uv_along_stroke_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("UV");
}

static void node_shader_buts_uvalongstroke(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "use_tips", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

}  // namespace blender::nodes::node_shader_uv_along_stroke_cc

/* node type definition */
void register_node_type_sh_uvalongstroke()
{
  namespace file_ns = blender::nodes::node_shader_uv_along_stroke_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeUVAlongStroke", SH_NODE_UVALONGSTROKE);
  ntype.ui_name = "UV Along Stroke";
  ntype.ui_description = "UV coordinates that map a texture along the stroke length";
  ntype.enum_name_legacy = "UVALONGSTROKE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = line_style_shader_nodes_poll;
  ntype.draw_buttons = file_ns::node_shader_buts_uvalongstroke;

  blender::bke::node_register_type(ntype);
}

/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_ies_light_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector").hide_value();
  b.add_input<decl::Float>("Strength")
      .default_value(1.0f)
      .min(0.0f)
      .max(1000000.0f)
      .description("Strength of the light source")
      .translation_context(BLT_I18NCONTEXT_AMOUNT);
  b.add_output<decl::Float>("Factor", "Fac");
}

static void node_shader_buts_ies(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *row;

  row = &layout->row(false);
  row->prop(ptr, "mode", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);

  row = &layout->row(true);

  if (RNA_enum_get(ptr, "mode") == NODE_IES_INTERNAL) {
    row->prop(ptr, "ies", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  }
  else {
    row->prop(ptr, "filepath", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  }
}

static void node_shader_init_tex_ies(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderTexIES *tex = MEM_callocN<NodeShaderTexIES>("NodeShaderIESLight");
  node->storage = tex;
}

}  // namespace blender::nodes::node_shader_ies_light_cc

/* node type definition */
void register_node_type_sh_tex_ies()
{
  namespace file_ns = blender::nodes::node_shader_ies_light_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeTexIES", SH_NODE_TEX_IES);
  ntype.ui_name = "IES Texture";
  ntype.ui_description =
      "Match real world lights with IES files, which store the directional intensity distribution "
      "of light sources";
  ntype.enum_name_legacy = "TEX_IES";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_ies;
  ntype.initfunc = file_ns::node_shader_init_tex_ies;
  blender::bke::node_type_storage(
      ntype, "NodeShaderTexIES", node_free_standard_storage, node_copy_standard_storage);

  blender::bke::node_register_type(ntype);
}

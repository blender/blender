/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_rgb_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Color")
      .default_value({0.5f, 0.5f, 0.5f, 1.0f})
      .custom_draw([](CustomSocketDrawParams &params) {
        params.layout.alignment_set(ui::LayoutAlign::Expand);
        uiLayout &col = params.layout.column(false);
        uiTemplateColorPicker(
            &col, &params.socket_ptr, "default_value", true, false, false, false);
        col.prop(&params.socket_ptr, "default_value", UI_ITEM_R_SLIDER, "", ICON_NONE);
      });
}

static int gpu_shader_rgb(GPUMaterial *mat,
                          bNode *node,
                          bNodeExecData * /*execdata*/,
                          GPUNodeStack * /*in*/,
                          GPUNodeStack *out)
{
  const bNodeSocket *socket = static_cast<bNodeSocket *>(node->outputs.first);
  float *value = static_cast<bNodeSocketValueRGBA *>(socket->default_value)->value;
  return GPU_link(mat, "set_rgba", GPU_uniform(value), &out->link);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem color = get_output_default("Color", NodeItem::Type::Color3);
  return create_node("constant", NodeItem::Type::Color3, {{"value", color}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_rgb_cc

void register_node_type_sh_rgb()
{
  namespace file_ns = blender::nodes::node_shader_rgb_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeRGB", SH_NODE_RGB);
  ntype.ui_name = "Color";
  ntype.ui_description = "A color picker";
  ntype.enum_name_legacy = "RGB";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::gpu_shader_rgb;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}

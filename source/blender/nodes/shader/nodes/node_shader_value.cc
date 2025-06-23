/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_geometry_nodes_gizmos.hh"
#include "NOD_multi_function.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_value_cc {

static void sh_node_value_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Value").custom_draw([](CustomSocketDrawParams &params) {
    uiLayout &row = params.layout.row(true);
    row.prop(&params.socket_ptr, "default_value", UI_ITEM_NONE, "", ICON_NONE);
    if (gizmos::value_node_has_gizmo(params.tree, params.node)) {
      row.prop(&params.socket_ptr, "pin_gizmo", UI_ITEM_NONE, "", ICON_GIZMO);
    }
  });
}

static int gpu_shader_value(GPUMaterial *mat,
                            bNode *node,
                            bNodeExecData * /*execdata*/,
                            GPUNodeStack * /*in*/,
                            GPUNodeStack *out)
{
  const bNodeSocket *socket = static_cast<bNodeSocket *>(node->outputs.first);
  float value = static_cast<bNodeSocketValueFloat *>(socket->default_value)->value;
  return GPU_link(mat, "set_value", GPU_uniform(&value), &out->link);
}

static void sh_node_value_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNodeSocket *bsocket = (bNodeSocket *)builder.node().outputs.first;
  const bNodeSocketValueFloat *value = (const bNodeSocketValueFloat *)bsocket->default_value;
  builder.construct_and_set_matching_fn<mf::CustomMF_Constant<float>>(value->value);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem value = get_output_default("Value", NodeItem::Type::Float);
  return create_node("constant", NodeItem::Type::Float, {{"value", value}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_value_cc

void register_node_type_sh_value()
{
  namespace file_ns = blender::nodes::node_shader_value_cc;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeValue", SH_NODE_VALUE);
  ntype.ui_name = "Value";
  ntype.ui_description = "Input numerical values to other nodes in the tree";
  ntype.enum_name_legacy = "VALUE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::sh_node_value_declare;
  ntype.gpu_fn = file_ns::gpu_shader_value;
  ntype.build_multi_function = file_ns::sh_node_value_build_multi_function;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}

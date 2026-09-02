/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_light_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  const bNodeTree *ntree = b.tree_or_null();
  const bool is_gpu_internal = ntree && (ntree->flag & NTREE_IS_GPU_SHADER_INTERNAL);

  b.add_input<decl::Int>("LightIndex"_ustr).available(is_gpu_internal);

  b.add_output<decl::Color>("Color"_ustr).description("Light color");
  b.add_output<decl::Float>("Power"_ustr).description("Radiant intensity divided by PI");
  b.add_output<decl::Vector>("Position"_ustr).description("Light position");
}

static int node_shader_gpu_light_info(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_light_info", in, out);
}

}  // namespace nodes::node_shader_light_info_cc

/* node type definition */
void register_node_type_sh_light_info()
{
  namespace file_ns = nodes::node_shader_light_info_cc;

  static bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeLightInfo"_ustr, SH_NODE_LIGHT_INFO);
  ntype.ui_name = "Light Info";
  ntype.ui_description =
      "Exposes physical properties of the light being evaluated and computes basic information "
      "relative to the evaluation position";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.gather_link_search_ops = search_link_ops_for_shader_material_lighting_node;
  ntype.gpu_fn = file_ns::node_shader_gpu_light_info;
  // ntype.materialx_fn = file_ns::node_shader_materialx;

  bke::node_register_type(ntype);
}

}  // namespace blender

/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_holdout_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNodeTree *ntree = b.tree_or_null();
  const bool is_gpu_internal = ntree && (ntree->flag & NTREE_IS_GPU_SHADER_INTERNAL);

  b.add_input<decl::Float>("Weight"_ustr).available(is_gpu_internal);
  b.add_output<decl::Shader>("Holdout"_ustr);
}

static int gpu_shader_rgb(GPUMaterial *mat,
                          bNode *node,
                          bNodeExecData * /*execdata*/,
                          GPUNodeStack *in,
                          GPUNodeStack *out)
{
  GPU_material_flag_set(mat, GPU_MATFLAG_HOLDOUT);
  return GPU_stack_link(mat, node, "node_holdout", in, out);
}

}  // namespace nodes::node_shader_holdout_cc

/* node type definition */
void register_node_type_sh_holdout()
{
  namespace file_ns = nodes::node_shader_holdout_cc;

  static bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeHoldout"_ustr, SH_NODE_HOLDOUT);
  ntype.ui_name = "Holdout";
  ntype.ui_description =
      "Create a \"hole\" in the image with zero alpha transparency, which is useful for "
      "compositing.\nNote: the holdout shader can only create alpha when transparency is enabled "
      "in the film settings";
  ntype.enum_name_legacy = "HOLDOUT";
  ntype.nclass = NODE_CLASS_SHADER;
  ntype.add_ui_poll = object_shader_nodes_poll;
  ntype.declare = file_ns::node_declare;
  ntype.gather_link_search_ops = search_link_ops_for_shader_bsdf_node;
  ntype.gpu_fn = file_ns::gpu_shader_rgb;

  bke::node_register_type(ntype);
}

}  // namespace blender

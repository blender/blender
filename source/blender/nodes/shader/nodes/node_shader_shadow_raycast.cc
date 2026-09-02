/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_shadow_raycast_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNodeTree *ntree = b.tree_or_null();
  const bool is_gpu_internal = ntree && (ntree->flag & NTREE_IS_GPU_SHADER_INTERNAL);

  b.add_input<decl::Int>("LightIndex"_ustr).available(is_gpu_internal);

  b.add_input<decl::Vector>("Position"_ustr).hide_value();
  b.add_input<decl::Float>("Softness"_ustr)
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_output<decl::Color>("Color"_ustr);
}

static int node_shader_gpu_shadow_raycast(GPUMaterial *mat,
                                          bNode *node,
                                          bNodeExecData * /*execdata*/,
                                          GPUNodeStack *in,
                                          GPUNodeStack *out)
{
  if (!in[1].link) {
    GPU_link(mat, "world_position_get", &in[1].link);
  }
  else {
    GPU_material_flag_set(mat, GPU_MATFLAG_SHADOW_OFFSET);
  }

  return GPU_stack_link(mat, node, "node_shadow_raycast", in, out);
}

}  // namespace nodes::node_shader_shadow_raycast_cc

/* node type definition */
void register_node_type_sh_shadow_raycast()
{
  namespace file_ns = nodes::node_shader_shadow_raycast_cc;

  static bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeShadowRaycast"_ustr, SH_NODE_SHADOW_RAYCAST);
  ntype.ui_name = "Shadow Raycast";
  ntype.ui_description = "Shadow Raycast";
  ntype.enum_name_legacy = "SHADOW RAYCAST";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.gather_link_search_ops = search_link_ops_for_shader_material_lighting_node;
  ntype.gpu_fn = file_ns::node_shader_gpu_shadow_raycast;
  // ntype.materialx_fn = file_ns::node_shader_materialx;

  bke::node_register_type(ntype);
}

}  // namespace blender

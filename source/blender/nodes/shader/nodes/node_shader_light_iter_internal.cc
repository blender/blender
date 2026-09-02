/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_light_iter_internal_cc {

namespace light_iter_input_node {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("LightIndex"_ustr);
}

static int node_shader_fn(GPUMaterial *mat,
                          bNode *node,
                          bNodeExecData * /*execdata*/,
                          GPUNodeStack *in,
                          GPUNodeStack *out)
{
  int zone_id = *reinterpret_cast<int *>(&node->custom3);
  return GPU_stack_link_zone(mat, node, "LIGHT_ITER_BEGIN", in, out, zone_id, false, 0, 1);
}

}  // namespace light_iter_input_node

namespace light_iter_output_node {

static void node_declare(NodeDeclarationBuilder & /*b*/) {}

static int node_shader_fn(GPUMaterial *mat,
                          bNode *node,
                          bNodeExecData * /*execdata*/,
                          GPUNodeStack *in,
                          GPUNodeStack *out)
{
  int zone_id = node->identifier;
  return GPU_stack_link_zone(mat, node, "LIGHT_ITER_END", in, out, zone_id, true, 0, 0);
}

}  // namespace light_iter_output_node

namespace {
bool add_ui_poll(const bContext * /*C*/)
{
  /* This node is for internal use only. */
  return false;
}
}  // namespace

}  // namespace nodes::node_shader_light_iter_internal_cc

void register_node_type_sh_light_iter_internal_input()
{
  namespace file_ns = nodes::node_shader_light_iter_internal_cc::light_iter_input_node;

  static bke::bNodeType ntype;

  sh_node_type_base(
      &ntype, "ShaderNodeLightIterInternalInput"_ustr, SH_NODE_LIGHT_ITER_INTERNAL_INPUT);
  ntype.ui_name = "Light Iter Internal Input";
  ntype.ui_description = "Light Iter Internal Input";
  ntype.enum_name_legacy = "LIGHT ITER INTERNAL INPUT";
  ntype.nclass = NODE_CLASS_SHADER;
  ntype.declare = file_ns::node_declare;
  ntype.gather_link_search_ops = search_link_ops_for_shader_material_lighting_node;
  ntype.gpu_fn = file_ns::node_shader_fn;
  ntype.add_ui_poll = nodes::node_shader_light_iter_internal_cc::add_ui_poll;

  bke::node_register_type(ntype);
}

void register_node_type_sh_light_iter_internal_output()
{
  namespace file_ns = nodes::node_shader_light_iter_internal_cc::light_iter_output_node;

  static bke::bNodeType ntype;

  sh_node_type_base(
      &ntype, "ShaderNodeLightIterInternalOutput"_ustr, SH_NODE_LIGHT_ITER_INTERNAL_OUTPUT);
  ntype.ui_name = "Light Iter Internal Output";
  ntype.ui_description = "Light Iter Internal Output";
  ntype.nclass = NODE_CLASS_SHADER;
  ntype.declare = file_ns::node_declare;
  ntype.gather_link_search_ops = search_link_ops_for_shader_material_lighting_node;
  ntype.gpu_fn = file_ns::node_shader_fn;
  ntype.add_ui_poll = nodes::node_shader_light_iter_internal_cc::add_ui_poll;

  bke::node_register_type(ntype);
}

}  // namespace blender

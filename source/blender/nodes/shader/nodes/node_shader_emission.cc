/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_emission_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Strength").default_value(1.0f).min(0.0f).max(1000000.0f);
  b.add_input<decl::Float>("Weight").unavailable();
  b.add_output<decl::Shader>("Emission");
}

static int node_shader_gpu_emission(GPUMaterial *mat,
                                    bNode *node,
                                    bNodeExecData * /*execdata*/,
                                    GPUNodeStack *in,
                                    GPUNodeStack *out)
{
  GPU_material_flag_set(mat, GPU_MATFLAG_EMISSION);
  return GPU_stack_link(mat, node, "node_emission", in, out);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  if (to_type_ != NodeItem::Type::EDF) {
    return empty();
  }

  NodeItem color = get_input_value("Color", NodeItem::Type::Color3);
  NodeItem strength = get_input_value("Strength", NodeItem::Type::Float);

  return create_node("uniform_edf", NodeItem::Type::EDF, {{"color", color * strength}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_emission_cc

/* node type definition */
void register_node_type_sh_emission()
{
  namespace file_ns = blender::nodes::node_shader_emission_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_EMISSION, "Emission", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_emission;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::nodeRegisterType(&ntype);
}

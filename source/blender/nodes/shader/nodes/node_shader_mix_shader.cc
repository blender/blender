/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_mix_shader_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Factor", "Fac")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Blend weight to use for mixing two shaders. "
          "At zero it uses the first shader entirely and at one the second shader");
  b.add_input<decl::Shader>("Shader");
  b.add_input<decl::Shader>("Shader", "Shader_001");
  b.add_output<decl::Shader>("Shader");
}

static int node_shader_gpu_mix_shader(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_mix_shader", in, out);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  if (!ELEM(to_type_, NodeItem::Type::BSDF, NodeItem::Type::EDF, NodeItem::Type::SurfaceOpacity)) {
    return empty();
  }

  NodeItem shader1 = get_input_link(1, to_type_);
  NodeItem shader2 = get_input_link(2, to_type_);
  if (!shader1 && !shader2) {
    return empty();
  }

  NodeItem fac = get_input_value(0, NodeItem::Type::Float);

  if (shader1 && !shader2) {
    return shader1 * (val(1.0f) - fac);
  }
  if (!shader1 && shader2) {
    return shader2 * fac;
  }
  return fac.mix(shader1, shader2);
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_mix_shader_cc

/* node type definition */
void register_node_type_sh_mix_shader()
{
  namespace file_ns = blender::nodes::node_shader_mix_shader_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeMixShader", SH_NODE_MIX_SHADER);
  ntype.ui_name = "Mix Shader";
  ntype.ui_description = "Mix two shaders together. Typically used for material layering";
  ntype.enum_name_legacy = "MIX_SHADER";
  ntype.nclass = NODE_CLASS_SHADER;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_mix_shader;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}

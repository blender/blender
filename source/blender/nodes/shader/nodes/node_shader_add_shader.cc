/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_add_shader_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Shader>("Shader");
  b.add_input<decl::Shader>("Shader", "Shader_001");
  b.add_output<decl::Shader>("Shader");
}

static int node_shader_gpu_add_shader(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_add_shader", in, out);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  if (!ELEM(to_type_, NodeItem::Type::BSDF, NodeItem::Type::EDF, NodeItem::Type::SurfaceOpacity)) {
    return empty();
  }

  NodeItem shader1 = get_input_link(0, to_type_);
  NodeItem shader2 = get_input_link(1, to_type_);
  if (!shader1 && !shader2) {
    return empty();
  }

  if (shader1 && !shader2) {
    return shader1;
  }
  if (!shader1 && shader2) {
    return shader2;
  }
  if (to_type_ == NodeItem::Type::SurfaceOpacity) {
    return (shader1 + shader2) * val(0.5f);
  }
  return shader1 + shader2;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_add_shader_cc

/* node type definition */
void register_node_type_sh_add_shader()
{
  namespace file_ns = blender::nodes::node_shader_add_shader_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_ADD_SHADER, "Add Shader", NODE_CLASS_SHADER);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_add_shader;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::nodeRegisterType(&ntype);
}

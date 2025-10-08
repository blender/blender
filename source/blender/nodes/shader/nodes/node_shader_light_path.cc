/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_light_path_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Is Camera Ray");
  b.add_output<decl::Float>("Is Shadow Ray");
  b.add_output<decl::Float>("Is Diffuse Ray");
  b.add_output<decl::Float>("Is Glossy Ray");
  b.add_output<decl::Float>("Is Singular Ray");
  b.add_output<decl::Float>("Is Reflection Ray");
  b.add_output<decl::Float>("Is Transmission Ray");
  b.add_output<decl::Float>("Is Volume Scatter Ray");
  b.add_output<decl::Float>("Ray Length");
  b.add_output<decl::Float>("Ray Depth");
  b.add_output<decl::Float>("Diffuse Depth");
  b.add_output<decl::Float>("Glossy Depth");
  b.add_output<decl::Float>("Transparent Depth");
  b.add_output<decl::Float>("Transmission Depth");
  b.add_output<decl::Float>("Portal Depth");
}

static int node_shader_gpu_light_path(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_light_path", in, out);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* NOTE: This node isn't supported by MaterialX. Only default values returned. */
  if (STREQ(socket_out_->identifier, "Is Camera Ray")) {
    return val(1.0f);
  }
  if (STREQ(socket_out_->identifier, "Ray Length")) {
    return val(1.0f);
  }
  NodeItem res = val(0.0f);
  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_light_path_cc

/* node type definition */
void register_node_type_sh_light_path()
{
  namespace file_ns = blender::nodes::node_shader_light_path_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeLightPath", SH_NODE_LIGHT_PATH);
  ntype.ui_name = "Light Path";
  ntype.ui_description =
      "Retrieve the type of incoming ray for which the shader is being executed.\nTypically used "
      "for non-physically-based tricks";
  ntype.enum_name_legacy = "LIGHT_PATH";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_light_path;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

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
  b.add_output<decl::Float>("Ray Length");
  b.add_output<decl::Float>("Ray Depth");
  b.add_output<decl::Float>("Diffuse Depth");
  b.add_output<decl::Float>("Glossy Depth");
  b.add_output<decl::Float>("Transparent Depth");
  b.add_output<decl::Float>("Transmission Depth");
}

static int node_shader_gpu_light_path(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_light_path", in, out);
}

}  // namespace blender::nodes::node_shader_light_path_cc

/* node type definition */
void register_node_type_sh_light_path()
{
  namespace file_ns = blender::nodes::node_shader_light_path_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_LIGHT_PATH, "Light Path", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::node_shader_gpu_light_path;

  nodeRegisterType(&ntype);
}

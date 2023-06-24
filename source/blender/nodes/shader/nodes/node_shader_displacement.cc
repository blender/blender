/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_displacement_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Height").default_value(0.0f).min(0.0f).max(1000.0f);
  b.add_input<decl::Float>("Midlevel").default_value(0.5f).min(0.0f).max(1000.0f);
  b.add_input<decl::Float>("Scale").default_value(1.0f).min(0.0f).max(1000.0f);
  b.add_input<decl::Vector>("Normal").hide_value();
  b.add_output<decl::Vector>("Displacement");
}

static void node_shader_init_displacement(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = SHD_SPACE_OBJECT; /* space */
}

static int gpu_shader_displacement(GPUMaterial *mat,
                                   bNode *node,
                                   bNodeExecData * /*execdata*/,
                                   GPUNodeStack *in,
                                   GPUNodeStack *out)
{
  if (!in[3].link) {
    GPU_link(mat, "world_normals_get", &in[3].link);
  }

  if (node->custom1 == SHD_SPACE_OBJECT) {
    return GPU_stack_link(mat, node, "node_displacement_object", in, out);
  }

  return GPU_stack_link(mat, node, "node_displacement_world", in, out);
}

}  // namespace blender::nodes::node_shader_displacement_cc

/* node type definition */
void register_node_type_sh_displacement()
{
  namespace file_ns = blender::nodes::node_shader_displacement_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_DISPLACEMENT, "Displacement", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_shader_init_displacement;
  ntype.gpu_fn = file_ns::gpu_shader_displacement;

  nodeRegisterType(&ntype);
}

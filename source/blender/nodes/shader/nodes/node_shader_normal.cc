/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_normal_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Normal")
      .default_value({0.0f, 0.0f, 1.0f})
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_DIRECTION);
  b.add_output<decl::Vector>("Normal")
      .default_value({0.0f, 0.0f, 1.0f})
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_DIRECTION);
  b.add_output<decl::Float>("Dot");
}

static int gpu_shader_normal(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  GPUNodeLink *vec = GPU_uniform(out[0].vec);
  return GPU_stack_link(mat, node, "normal_new_shading", in, out, vec);
}

}  // namespace blender::nodes::node_shader_normal_cc

void register_node_type_sh_normal()
{
  namespace file_ns = blender::nodes::node_shader_normal_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_NORMAL, "Normal", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::gpu_shader_normal;

  nodeRegisterType(&ntype);
}

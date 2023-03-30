/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_rgb_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>(N_("Color")).default_value({0.5f, 0.5f, 0.5f, 1.0f});
}

static int gpu_shader_rgb(GPUMaterial *mat,
                          bNode *node,
                          bNodeExecData * /*execdata*/,
                          GPUNodeStack * /*in*/,
                          GPUNodeStack *out)
{
  const bNodeSocket *socket = static_cast<bNodeSocket *>(node->outputs.first);
  float *value = static_cast<bNodeSocketValueRGBA *>(socket->default_value)->value;
  return GPU_link(mat, "set_rgba", GPU_uniform(value), &out->link);
}

}  // namespace blender::nodes::node_shader_rgb_cc

void register_node_type_sh_rgb()
{
  namespace file_ns = blender::nodes::node_shader_rgb_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_RGB, "RGB", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::gpu_shader_rgb;

  nodeRegisterType(&ntype);
}

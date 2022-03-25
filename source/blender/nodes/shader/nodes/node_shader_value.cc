/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_value_cc {

static void sh_node_value_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Value"));
}

static int gpu_shader_value(GPUMaterial *mat,
                            bNode *node,
                            bNodeExecData *UNUSED(execdata),
                            GPUNodeStack *in,
                            GPUNodeStack *out)
{
  GPUNodeLink *link = GPU_uniformbuf_link_out(mat, node, out, 0);
  return GPU_stack_link(mat, node, "set_value", in, out, link);
}

static void sh_node_value_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNodeSocket *bsocket = (bNodeSocket *)builder.node().outputs.first;
  const bNodeSocketValueFloat *value = (const bNodeSocketValueFloat *)bsocket->default_value;
  builder.construct_and_set_matching_fn<fn::CustomMF_Constant<float>>(value->value);
}

}  // namespace blender::nodes::node_shader_value_cc

void register_node_type_sh_value()
{
  namespace file_ns = blender::nodes::node_shader_value_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_VALUE, "Value", NODE_CLASS_INPUT);
  ntype.declare = file_ns::sh_node_value_declare;
  node_type_gpu(&ntype, file_ns::gpu_shader_value);
  ntype.build_multi_function = file_ns::sh_node_value_build_multi_function;

  nodeRegisterType(&ntype);
}

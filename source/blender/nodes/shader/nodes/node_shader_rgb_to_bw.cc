/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

/** \file
 * \ingroup shdnodes
 */

#include "IMB_colormanagement.h"

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_rgb_to_bw_cc {

static void sh_node_rgbtobw_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Color")).default_value({0.5f, 0.5f, 0.5f, 1.0f});
  b.add_output<decl::Float>(N_("Val"));
}

static int gpu_shader_rgbtobw(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData * /*execdata*/,
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "rgbtobw", in, out);
}

}  // namespace blender::nodes::node_shader_rgb_to_bw_cc

void register_node_type_sh_rgbtobw()
{
  namespace file_ns = blender::nodes::node_shader_rgb_to_bw_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_RGBTOBW, "RGB to BW", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::sh_node_rgbtobw_declare;
  ntype.gpu_fn = file_ns::gpu_shader_rgbtobw;

  nodeRegisterType(&ntype);
}

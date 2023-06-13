/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_brightness_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Bright").default_value(0.0f).min(-100.0f).max(100.0f);
  b.add_input<decl::Float>("Contrast").default_value(0.0f).min(-100.0f).max(100.0f);
  b.add_output<decl::Color>("Color");
}

static int gpu_shader_brightcontrast(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "brightness_contrast", in, out);
}

}  // namespace blender::nodes::node_shader_brightness_cc

void register_node_type_sh_brightcontrast()
{
  namespace file_ns = blender::nodes::node_shader_brightness_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BRIGHTCONTRAST, "Brightness/Contrast", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::gpu_shader_brightcontrast;

  nodeRegisterType(&ntype);
}

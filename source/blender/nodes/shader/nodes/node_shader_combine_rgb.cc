/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_combine_rgb_cc {

static void sh_node_combrgb_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>(N_("R")).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("G")).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("B")).min(0.0f).max(1.0f);
  b.add_output<decl::Color>(N_("Image"));
};

static void node_shader_exec_combrgb(void *UNUSED(data),
                                     int UNUSED(thread),
                                     bNode *UNUSED(node),
                                     bNodeExecData *UNUSED(execdata),
                                     bNodeStack **in,
                                     bNodeStack **out)
{
  float r, g, b;
  nodestack_get_vec(&r, SOCK_FLOAT, in[0]);
  nodestack_get_vec(&g, SOCK_FLOAT, in[1]);
  nodestack_get_vec(&b, SOCK_FLOAT, in[2]);

  out[0]->vec[0] = r;
  out[0]->vec[1] = g;
  out[0]->vec[2] = b;
}

static int gpu_shader_combrgb(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData *UNUSED(execdata),
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "combine_rgb", in, out);
}

static void sh_node_combrgb_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static blender::fn::CustomMF_SI_SI_SI_SO<float, float, float, blender::ColorGeometry4f> fn{
      "Combine RGB",
      [](float r, float g, float b) { return blender::ColorGeometry4f(r, g, b, 1.0f); }};
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes::node_shader_combine_rgb_cc

void register_node_type_sh_combrgb()
{
  namespace file_ns = blender::nodes::node_shader_combine_rgb_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_COMBRGB, "Combine RGB", NODE_CLASS_CONVERTER, 0);
  ntype.declare = file_ns::sh_node_combrgb_declare;
  node_type_exec(&ntype, nullptr, nullptr, file_ns::node_shader_exec_combrgb);
  node_type_gpu(&ntype, file_ns::gpu_shader_combrgb);
  ntype.build_multi_function = file_ns::sh_node_combrgb_build_multi_function;

  nodeRegisterType(&ntype);
}

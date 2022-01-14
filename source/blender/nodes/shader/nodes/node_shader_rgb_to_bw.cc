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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

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
                              bNodeExecData *UNUSED(execdata),
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
  node_type_gpu(&ntype, file_ns::gpu_shader_rgbtobw);

  nodeRegisterType(&ntype);
}

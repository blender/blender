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

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_brightness_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Color")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>(N_("Bright")).default_value(0.0f).min(-100.0f).max(100.0f);
  b.add_input<decl::Float>(N_("Contrast")).default_value(0.0f).min(-100.0f).max(100.0f);
  b.add_output<decl::Color>(N_("Color"));
}

static int gpu_shader_brightcontrast(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
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

  sh_node_type_base(&ntype, SH_NODE_BRIGHTCONTRAST, "Bright/Contrast", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::node_declare;
  node_type_gpu(&ntype, file_ns::gpu_shader_brightcontrast);

  nodeRegisterType(&ntype);
}

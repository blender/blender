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

namespace blender::nodes::node_shader_hueSatVal_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Hue")).default_value(0.5f).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("Saturation")).default_value(1.0f).min(0.0f).max(2.0f);
  b.add_input<decl::Float>(N_("Value")).default_value(1.0f).min(0.0f).max(2.0f);
  b.add_input<decl::Float>(N_("Fac")).default_value(1.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Color>(N_("Color")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_output<decl::Color>(N_("Color"));
}

static int gpu_shader_hue_sat(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData *UNUSED(execdata),
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "hue_sat", in, out);
}

}  // namespace blender::nodes::node_shader_hueSatVal_cc

void register_node_type_sh_hue_sat()
{
  namespace file_ns = blender::nodes::node_shader_hueSatVal_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_HUE_SAT, "Hue Saturation Value", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::node_declare;
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_gpu(&ntype, file_ns::gpu_shader_hue_sat);

  nodeRegisterType(&ntype);
}

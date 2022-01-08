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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_sepcomb_hsv_cc {

/* **************** SEPARATE HSV ******************** */

static void node_declare_sephsv(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Color")).default_value({0.8f, 0.8f, 0.8f, 1.0});
  b.add_output<decl::Float>(N_("H"));
  b.add_output<decl::Float>(N_("S"));
  b.add_output<decl::Float>(N_("V"));
}

static int gpu_shader_sephsv(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData *UNUSED(execdata),
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "separate_hsv", in, out);
}

}  // namespace blender::nodes::node_shader_sepcomb_hsv_cc

void register_node_type_sh_sephsv()
{
  namespace file_ns = blender::nodes::node_shader_sepcomb_hsv_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_SEPHSV, "Separate HSV", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare_sephsv;
  node_type_gpu(&ntype, file_ns::gpu_shader_sephsv);

  nodeRegisterType(&ntype);
}

namespace blender::nodes::node_shader_sepcomb_hsv_cc {

/* **************** COMBINE HSV ******************** */

static void node_declare_combhsv(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("H")).default_value(0.0f).min(0.0f).max(1.0f).subtype(PROP_UNSIGNED);
  b.add_input<decl::Float>(N_("S")).default_value(0.0f).min(0.0f).max(1.0f).subtype(PROP_UNSIGNED);
  b.add_input<decl::Float>(N_("V")).default_value(0.0f).min(0.0f).max(1.0f).subtype(PROP_UNSIGNED);
  b.add_output<decl::Color>(N_("Color"));
}

static int gpu_shader_combhsv(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData *UNUSED(execdata),
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "combine_hsv", in, out);
}

}  // namespace blender::nodes::node_shader_sepcomb_hsv_cc

void register_node_type_sh_combhsv()
{
  namespace file_ns = blender::nodes::node_shader_sepcomb_hsv_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_COMBHSV, "Combine HSV", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare_combhsv;
  node_type_gpu(&ntype, file_ns::gpu_shader_combhsv);

  nodeRegisterType(&ntype);
}

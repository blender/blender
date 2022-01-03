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

#include "node_shader_util.h"

/* **************** BUMP ******************** */

namespace blender::nodes::node_shader_bump_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Strength"))
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Distance")).default_value(1.0f).min(0.0f).max(1000.0f);
  b.add_input<decl::Float>(N_("Height"))
      .default_value(1.0f)
      .min(-1000.0f)
      .max(1000.0f)
      .hide_value();
  b.add_input<decl::Float>(N_("Height_dx")).default_value(1.0f).unavailable();
  b.add_input<decl::Float>(N_("Height_dy")).default_value(1.0f).unavailable();
  b.add_input<decl::Vector>(N_("Normal")).min(-1.0f).max(1.0f).hide_value();
  b.add_output<decl::Vector>(N_("Normal"));
}

static int gpu_shader_bump(GPUMaterial *mat,
                           bNode *node,
                           bNodeExecData *UNUSED(execdata),
                           GPUNodeStack *in,
                           GPUNodeStack *out)
{
  if (!in[5].link) {
    GPU_link(mat, "world_normals_get", &in[5].link);
  }

  float invert = (node->custom1) ? -1.0 : 1.0;

  return GPU_stack_link(
      mat, node, "node_bump", in, out, GPU_builtin(GPU_VIEW_POSITION), GPU_constant(&invert));
}

}  // namespace blender::nodes::node_shader_bump_cc

/* node type definition */
void register_node_type_sh_bump()
{
  namespace file_ns = blender::nodes::node_shader_bump_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_BUMP, "Bump", NODE_CLASS_OP_VECTOR, 0);
  ntype.declare = file_ns::node_declare;
  node_type_gpu(&ntype, file_ns::gpu_shader_bump);

  nodeRegisterType(&ntype);
}

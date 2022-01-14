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

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_displacement_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Height")).default_value(0.0f).min(0.0f).max(1000.0f);
  b.add_input<decl::Float>(N_("Midlevel")).default_value(0.0f).min(0.0f).max(1000.0f);
  b.add_input<decl::Float>(N_("Scale")).default_value(1.0f).min(0.0f).max(1000.0f);
  b.add_input<decl::Vector>(N_("Normal")).hide_value();
  b.add_output<decl::Vector>(N_("Displacement"));
}

static void node_shader_init_displacement(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = SHD_SPACE_OBJECT; /* space */

  /* Set default value here for backwards compatibility. */
  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    if (STREQ(sock->name, "Midlevel")) {
      ((bNodeSocketValueFloat *)sock->default_value)->value = 0.5f;
    }
  }
}

static int gpu_shader_displacement(GPUMaterial *mat,
                                   bNode *node,
                                   bNodeExecData *UNUSED(execdata),
                                   GPUNodeStack *in,
                                   GPUNodeStack *out)
{
  if (!in[3].link) {
    GPU_link(mat,
             "direction_transform_m4v3",
             GPU_builtin(GPU_VIEW_NORMAL),
             GPU_builtin(GPU_INVERSE_VIEW_MATRIX),
             &in[3].link);
  }

  if (node->custom1 == SHD_SPACE_OBJECT) {
    return GPU_stack_link(
        mat, node, "node_displacement_object", in, out, GPU_builtin(GPU_OBJECT_MATRIX));
  }

  return GPU_stack_link(mat, node, "node_displacement_world", in, out);
}

}  // namespace blender::nodes::node_shader_displacement_cc

/* node type definition */
void register_node_type_sh_displacement()
{
  namespace file_ns = blender::nodes::node_shader_displacement_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_DISPLACEMENT, "Displacement", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::node_declare;
  node_type_init(&ntype, file_ns::node_shader_init_displacement);
  node_type_gpu(&ntype, file_ns::gpu_shader_displacement);

  nodeRegisterType(&ntype);
}

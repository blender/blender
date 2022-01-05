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

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_camera_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>(N_("View Vector"));
  b.add_output<decl::Float>(N_("View Z Depth"));
  b.add_output<decl::Float>(N_("View Distance"));
}

static int gpu_shader_camera(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData *UNUSED(execdata),
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  GPUNodeLink *viewvec;

  viewvec = GPU_builtin(GPU_VIEW_POSITION);
  GPU_link(mat, "invert_z", viewvec, &viewvec);
  return GPU_stack_link(mat, node, "camera", in, out, viewvec);
}

}  // namespace blender::nodes::node_shader_camera_cc

void register_node_type_sh_camera()
{
  namespace file_ns = blender::nodes::node_shader_camera_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_CAMERA, "Camera Data", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  node_type_gpu(&ntype, file_ns::gpu_shader_camera);

  nodeRegisterType(&ntype);
}

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

#include "RE_texture.h"

namespace blender::nodes::node_shader_particle_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Index"));
  b.add_output<decl::Float>(N_("Random"));
  b.add_output<decl::Float>(N_("Age"));
  b.add_output<decl::Float>(N_("Lifetime"));
  b.add_output<decl::Vector>(N_("Location"));
#if 0 /* quaternion sockets not yet supported */
  b.add_output<decl::Quaternion>(N_("Rotation"));
#endif
  b.add_output<decl::Float>(N_("Size"));
  b.add_output<decl::Vector>(N_("Velocity"));
  b.add_output<decl::Vector>(N_("Angular Velocity"));
}

static int gpu_shader_particle_info(GPUMaterial *mat,
                                    bNode *node,
                                    bNodeExecData *UNUSED(execdata),
                                    GPUNodeStack *in,
                                    GPUNodeStack *out)
{

  return GPU_stack_link(mat,
                        node,
                        "particle_info",
                        in,
                        out,
                        GPU_builtin(GPU_PARTICLE_SCALAR_PROPS),
                        GPU_builtin(GPU_PARTICLE_LOCATION),
                        GPU_builtin(GPU_PARTICLE_VELOCITY),
                        GPU_builtin(GPU_PARTICLE_ANG_VELOCITY));
}

}  // namespace blender::nodes::node_shader_particle_info_cc

/* node type definition */
void register_node_type_sh_particle_info()
{
  namespace file_ns = blender::nodes::node_shader_particle_info_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_PARTICLE_INFO, "Particle Info", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  node_type_gpu(&ntype, file_ns::gpu_shader_particle_info);

  nodeRegisterType(&ntype);
}

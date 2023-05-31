/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "RE_texture.h"

namespace blender::nodes::node_shader_particle_info_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Index");
  b.add_output<decl::Float>("Random");
  b.add_output<decl::Float>("Age");
  b.add_output<decl::Float>("Lifetime");
  b.add_output<decl::Vector>("Location");
#if 0 /* quaternion sockets not yet supported */
  b.add_output<decl::Quaternion>("Rotation");
#endif
  b.add_output<decl::Float>("Size");
  b.add_output<decl::Vector>("Velocity");
  b.add_output<decl::Vector>("Angular Velocity");
}

static int gpu_shader_particle_info(GPUMaterial *mat,
                                    bNode *node,
                                    bNodeExecData * /*execdata*/,
                                    GPUNodeStack *in,
                                    GPUNodeStack *out)
{
  GPU_material_flag_set(mat, GPU_MATFLAG_OBJECT_INFO);
  /* TODO(fclem) Pass particle data in obinfo. */
  return GPU_stack_link(mat, node, "particle_info", in, out);
}

}  // namespace blender::nodes::node_shader_particle_info_cc

/* node type definition */
void register_node_type_sh_particle_info()
{
  namespace file_ns = blender::nodes::node_shader_particle_info_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_PARTICLE_INFO, "Particle Info", NODE_CLASS_INPUT);
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::gpu_shader_particle_info;

  nodeRegisterType(&ntype);
}

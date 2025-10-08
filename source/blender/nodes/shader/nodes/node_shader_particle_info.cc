/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

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

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* NOTE: This node isn't supported by MaterialX. */
  return get_output_default(socket_out_->identifier, NodeItem::Type::Any);
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_particle_info_cc

/* node type definition */
void register_node_type_sh_particle_info()
{
  namespace file_ns = blender::nodes::node_shader_particle_info_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeParticleInfo", SH_NODE_PARTICLE_INFO);
  ntype.ui_name = "Particle Info";
  ntype.ui_description =
      "Retrieve the data of the particle that spawned the object instance, for example to give "
      "variation to multiple instances of an object";
  ntype.enum_name_legacy = "PARTICLE_INFO";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.gpu_fn = file_ns::gpu_shader_particle_info;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}

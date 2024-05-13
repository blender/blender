/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_output_world_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Shader>("Surface");
  b.add_input<decl::Shader>("Volume").translation_context(BLT_I18NCONTEXT_ID_ID);
}

static int node_shader_gpu_output_world(GPUMaterial *mat,
                                        bNode * /*node*/,
                                        bNodeExecData * /*execdata*/,
                                        GPUNodeStack *in,
                                        GPUNodeStack * /*out*/)
{
  GPUNodeLink *outlink_surface, *outlink_volume;
  if (in[0].link) {
    GPU_link(mat, "node_output_world_surface", in[0].link, &outlink_surface);
    GPU_material_output_surface(mat, outlink_surface);
  }
  if (in[1].link) {
    GPU_link(mat, "node_output_world_volume", in[1].link, &outlink_volume);
    GPU_material_output_volume(mat, outlink_volume);
  }
  return true;
}

}  // namespace blender::nodes::node_shader_output_world_cc

/* node type definition */
void register_node_type_sh_output_world()
{
  namespace file_ns = blender::nodes::node_shader_output_world_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_OUTPUT_WORLD, "World Output", NODE_CLASS_OUTPUT);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = world_shader_nodes_poll;
  ntype.gpu_fn = file_ns::node_shader_gpu_output_world;

  ntype.no_muting = true;

  blender::bke::nodeRegisterType(&ntype);
}

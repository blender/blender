/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_output_light_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Shader>("Surface");
}

static int node_shader_gpu_output_light(GPUMaterial *mat,
                                        bNode * /*node*/,
                                        bNodeExecData * /*execdata*/,
                                        GPUNodeStack *in,
                                        GPUNodeStack * /*out*/)
{
  GPUNodeLink *outlink_surface;
  /* Passthrough node in order to do the right socket conversions. */
  if (in[0].link) {
    /* Reuse material output. */
    GPU_link(mat, "node_output_material_surface", in[0].link, &outlink_surface);
    GPU_material_output_surface(mat, outlink_surface);
  }
  return true;
}

}  // namespace blender::nodes::node_shader_output_light_cc

/* node type definition */
void register_node_type_sh_output_light()
{
  namespace file_ns = blender::nodes::node_shader_output_light_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_OUTPUT_LIGHT, "Light Output", NODE_CLASS_OUTPUT);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_cycles_shader_nodes_poll;
  ntype.gpu_fn = file_ns::node_shader_gpu_output_light;

  ntype.no_muting = true;

  nodeRegisterType(&ntype);
}

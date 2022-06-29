/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_output_light_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Shader>(N_("Surface"));
}

static int node_shader_gpu_output_light(GPUMaterial *mat,
                                        bNode *UNUSED(node),
                                        bNodeExecData *UNUSED(execdata),
                                        GPUNodeStack *in,
                                        GPUNodeStack *UNUSED(out))
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
  node_type_gpu(&ntype, file_ns::node_shader_gpu_output_light);

  ntype.no_muting = true;

  nodeRegisterType(&ntype);
}

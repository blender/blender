/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"
#include "node_util.hh"

namespace blender::nodes::node_shader_npr_refraction_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Image>("Combined Color");
  b.add_output<decl::Image>("Position");
}

static int node_shader_fn(GPUMaterial *mat,
                          bNode *node,
                          bNodeExecData * /*execdata*/,
                          GPUNodeStack *in,
                          GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "npr_refraction", in, out);
}

}  // namespace blender::nodes::node_shader_npr_refraction_cc

void register_node_type_sh_npr_refraction()
{
  namespace file_ns = blender::nodes::node_shader_npr_refraction_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeNPR_Refraction", SH_NODE_NPR_REFRACTION);
  ntype.enum_name_legacy = "NPR_REFRACTION";
  ntype.ui_name = "NPR Refraction";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = npr_shader_nodes_poll;
  ntype.gpu_fn = file_ns::node_shader_fn;

  blender::bke::node_register_type(&ntype);
}

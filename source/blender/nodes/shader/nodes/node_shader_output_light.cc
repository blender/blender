/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_output_light_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Shader>(N_("Surface"));
}

}  // namespace blender::nodes::node_shader_output_light_cc

/* node type definition */
void register_node_type_sh_output_light()
{
  namespace file_ns = blender::nodes::node_shader_output_light_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_OUTPUT_LIGHT, "Light Output", NODE_CLASS_OUTPUT);
  ntype.declare = file_ns::node_declare;
  ntype.no_muting = true;

  nodeRegisterType(&ntype);
}

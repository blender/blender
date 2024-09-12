/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_npr_output_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").hide_value();
}

static int node_shader_fn(GPUMaterial *mat,
                          bNode * /*node*/,
                          bNodeExecData * /*execdata*/,
                          GPUNodeStack *in,
                          GPUNodeStack * /*out*/)
{
  GPUNodeLink *outlink_npr = nullptr;
  /* Passthrough node in order to do the right socket conversions. */
  if (in[0].link) {
    GPU_link(mat, "npr_output", in[0].link, &outlink_npr);
    GPU_material_output_npr(mat, outlink_npr);
  }
  return true;
}

}  // namespace blender::nodes::node_shader_npr_output_cc

void register_node_type_sh_npr_output()
{
  namespace file_ns = blender::nodes::node_shader_npr_output_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_NPR_OUTPUT, "Output", NODE_CLASS_OUTPUT);
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = npr_shader_nodes_poll;
  ntype.gpu_fn = file_ns::node_shader_fn;

  blender::bke::node_register_type(&ntype);
}

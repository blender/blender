/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_sepcomb_hsv_cc {

/* **************** SEPARATE HSV ******************** */

static void node_declare_sephsv(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({0.8f, 0.8f, 0.8f, 1.0});
  b.add_output<decl::Float>("H");
  b.add_output<decl::Float>("S");
  b.add_output<decl::Float>("V");
}

static int gpu_shader_sephsv(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "separate_hsv", in, out);
}

}  // namespace blender::nodes::node_shader_sepcomb_hsv_cc

void register_node_type_sh_sephsv()
{
  namespace file_ns = blender::nodes::node_shader_sepcomb_hsv_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_SEPHSV_LEGACY, "Separate HSV (Legacy)", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare_sephsv;
  ntype.gpu_fn = file_ns::gpu_shader_sephsv;
  ntype.gather_link_search_ops = nullptr;
  ntype.gather_add_node_search_ops = nullptr;

  nodeRegisterType(&ntype);
}

namespace blender::nodes::node_shader_sepcomb_hsv_cc {

/* **************** COMBINE HSV ******************** */

static void node_declare_combhsv(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("H").default_value(0.0f).min(0.0f).max(1.0f).subtype(PROP_UNSIGNED);
  b.add_input<decl::Float>("S").default_value(0.0f).min(0.0f).max(1.0f).subtype(PROP_UNSIGNED);
  b.add_input<decl::Float>("V").default_value(0.0f).min(0.0f).max(1.0f).subtype(PROP_UNSIGNED);
  b.add_output<decl::Color>("Color");
}

static int gpu_shader_combhsv(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData * /*execdata*/,
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "combine_hsv", in, out);
}

}  // namespace blender::nodes::node_shader_sepcomb_hsv_cc

void register_node_type_sh_combhsv()
{
  namespace file_ns = blender::nodes::node_shader_sepcomb_hsv_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_COMBHSV_LEGACY, "Combine HSV (Legacy)", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare_combhsv;
  ntype.gpu_fn = file_ns::gpu_shader_combhsv;
  ntype.gather_link_search_ops = nullptr;
  ntype.gather_add_node_search_ops = nullptr;

  nodeRegisterType(&ntype);
}

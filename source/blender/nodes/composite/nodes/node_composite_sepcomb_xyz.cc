/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** SEPARATE XYZ ******************** */
namespace blender::nodes {

static void cmp_node_separate_xyz_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector").min(-10000.0f).max(10000.0f);
  b.add_output<decl::Float>("X");
  b.add_output<decl::Float>("Y");
  b.add_output<decl::Float>("Z");
}

}  // namespace blender::nodes

void register_node_type_cmp_separate_xyz()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SEPARATE_XYZ, "Separate XYZ", NODE_CLASS_CONVERTER);
  ntype.declare = blender::nodes::cmp_node_separate_xyz_declare;

  nodeRegisterType(&ntype);
}

/* **************** COMBINE XYZ ******************** */

namespace blender::nodes {

static void cmp_node_combine_xyz_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("X").min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Y").min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Z").min(-10000.0f).max(10000.0f);
  b.add_output<decl::Vector>("Vector");
}

}  // namespace blender::nodes

void register_node_type_cmp_combine_xyz()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_COMBINE_XYZ, "Combine XYZ", NODE_CLASS_CONVERTER);
  ntype.declare = blender::nodes::cmp_node_combine_xyz_declare;

  nodeRegisterType(&ntype);
}

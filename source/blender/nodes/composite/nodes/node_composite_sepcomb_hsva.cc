/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** SEPARATE HSVA ******************** */

namespace blender::nodes::node_composite_sepcomb_hsva_cc {

static void cmp_node_sephsva_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Float>(N_("H"));
  b.add_output<decl::Float>(N_("S"));
  b.add_output<decl::Float>(N_("V"));
  b.add_output<decl::Float>(N_("A"));
}

}  // namespace blender::nodes::node_composite_sepcomb_hsva_cc

void register_node_type_cmp_sephsva()
{
  namespace file_ns = blender::nodes::node_composite_sepcomb_hsva_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SEPHSVA_LEGACY, "Separate HSVA", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_sephsva_declare;
  nodeRegisterType(&ntype);
}

/* **************** COMBINE HSVA ******************** */

namespace blender::nodes::node_composite_sepcomb_hsva_cc {

static void cmp_node_combhsva_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("H")).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("S")).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("V")).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("A")).default_value(1.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes::node_composite_sepcomb_hsva_cc

void register_node_type_cmp_combhsva()
{
  namespace file_ns = blender::nodes::node_composite_sepcomb_hsva_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_COMBHSVA_LEGACY, "Combine HSVA", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_combhsva_declare;

  nodeRegisterType(&ntype);
}

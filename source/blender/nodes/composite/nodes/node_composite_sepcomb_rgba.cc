/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** SEPARATE RGBA ******************** */
namespace blender::nodes::node_composite_sepcomb_rgba_cc {

static void cmp_node_seprgba_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Image")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Float>(N_("R"));
  b.add_output<decl::Float>(N_("G"));
  b.add_output<decl::Float>(N_("B"));
  b.add_output<decl::Float>(N_("A"));
}

}  // namespace blender::nodes::node_composite_sepcomb_rgba_cc

void register_node_type_cmp_seprgba()
{
  namespace file_ns = blender::nodes::node_composite_sepcomb_rgba_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_SEPRGBA_LEGACY, "Separate RGBA", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_seprgba_declare;

  nodeRegisterType(&ntype);
}

/* **************** COMBINE RGBA ******************** */

namespace blender::nodes::node_composite_sepcomb_rgba_cc {

static void cmp_node_combrgba_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("R")).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("G")).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("B")).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("A")).default_value(1.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Color>(N_("Image"));
}

}  // namespace blender::nodes::node_composite_sepcomb_rgba_cc

void register_node_type_cmp_combrgba()
{
  namespace file_ns = blender::nodes::node_composite_sepcomb_rgba_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_COMBRGBA_LEGACY, "Combine RGBA", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::cmp_node_combrgba_declare;

  nodeRegisterType(&ntype);
}

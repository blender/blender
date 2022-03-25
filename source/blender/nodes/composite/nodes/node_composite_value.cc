/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** VALUE ******************** */

namespace blender::nodes::node_composite_value_cc {

static void cmp_node_value_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Value")).default_value(0.5f);
}

}  // namespace blender::nodes::node_composite_value_cc

void register_node_type_cmp_value()
{
  namespace file_ns = blender::nodes::node_composite_value_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_VALUE, "Value", NODE_CLASS_INPUT);
  ntype.declare = file_ns::cmp_node_value_declare;
  node_type_size_preset(&ntype, NODE_SIZE_SMALL);

  nodeRegisterType(&ntype);
}

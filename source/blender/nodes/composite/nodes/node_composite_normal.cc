/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** NORMAL  ******************** */

namespace blender::nodes::node_composite_normal_cc {

static void cmp_node_normal_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>(N_("Normal"))
      .default_value({0.0f, 0.0f, 1.0f})
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_DIRECTION);
  b.add_output<decl::Vector>(N_("Normal"))
      .default_value({0.0f, 0.0f, 1.0f})
      .min(-1.0f)
      .max(1.0f)
      .subtype(PROP_DIRECTION);
  b.add_output<decl::Float>(N_("Dot"));
}

}  // namespace blender::nodes::node_composite_normal_cc

void register_node_type_cmp_normal()
{
  namespace file_ns = blender::nodes::node_composite_normal_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_NORMAL, "Normal", NODE_CLASS_OP_VECTOR);
  ntype.declare = file_ns::cmp_node_normal_declare;

  nodeRegisterType(&ntype);
}

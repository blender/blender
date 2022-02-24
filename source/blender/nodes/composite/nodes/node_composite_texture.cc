/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** TEXTURE ******************** */

namespace blender::nodes::node_composite_texture_cc {

static void cmp_node_texture_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>(N_("Offset")).min(-2.0f).max(2.0f).subtype(PROP_TRANSLATION);
  b.add_input<decl::Vector>(N_("Scale"))
      .default_value({1.0f, 1.0f, 1.0f})
      .min(-10.0f)
      .max(10.0f)
      .subtype(PROP_XYZ);
  b.add_output<decl::Float>(N_("Value"));
  b.add_output<decl::Color>(N_("Color"));
}

}  // namespace blender::nodes::node_composite_texture_cc

void register_node_type_cmp_texture()
{
  namespace file_ns = blender::nodes::node_composite_texture_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_TEXTURE, "Texture", NODE_CLASS_INPUT);
  ntype.declare = file_ns::cmp_node_texture_declare;
  ntype.flag |= NODE_PREVIEW;

  nodeRegisterType(&ntype);
}

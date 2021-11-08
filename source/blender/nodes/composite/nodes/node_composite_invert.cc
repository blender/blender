/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

/* **************** INVERT ******************** */

namespace blender::nodes {

static void cmp_node_invert_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Fac")).default_value(1.0f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_input<decl::Color>(N_("Color")).default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_output<decl::Color>(N_("Color"));
}

}  // namespace blender::nodes

static void node_composit_init_invert(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 |= CMP_CHAN_RGB;
}

/* custom1 = mix type */
void register_node_type_cmp_invert(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_INVERT, "Invert", NODE_CLASS_OP_COLOR, 0);
  ntype.declare = blender::nodes::cmp_node_invert_declare;
  node_type_init(&ntype, node_composit_init_invert);

  nodeRegisterType(&ntype);
}

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

/* **************** Z COMBINE ******************** */

namespace blender::nodes {

static void cmp_node_zcombine_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Z").default_value(1.0f).min(0.0f).max(10000.0f);
  b.add_input<decl::Color>("Image", "Image_001").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Z", "Z_001").default_value(1.0f).min(0.0f).max(10000.0f);
  b.add_output<decl::Color>("Image");
  b.add_output<decl::Float>("Z");
}

}  // namespace blender::nodes

/* lazy coder NOTE: node->custom2 is abused to send signal. */
void register_node_type_cmp_zcombine(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_ZCOMBINE, "Z Combine", NODE_CLASS_OP_COLOR, 0);
  ntype.declare = blender::nodes::cmp_node_zcombine_declare;

  nodeRegisterType(&ntype);
}

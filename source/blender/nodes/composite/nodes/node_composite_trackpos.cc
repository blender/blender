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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "node_composite_util.hh"

namespace blender::nodes {

static void cmp_node_trackpos_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("X"));
  b.add_output<decl::Float>(N_("Y"));
  b.add_output<decl::Vector>(N_("Speed")).subtype(PROP_VELOCITY);
}

}  // namespace blender::nodes

static void init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTrackPosData *data = (NodeTrackPosData *)MEM_callocN(sizeof(NodeTrackPosData),
                                                           "node track position data");

  node->storage = data;
}

void register_node_type_cmp_trackpos(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_TRACKPOS, "Track Position", NODE_CLASS_INPUT, 0);
  ntype.declare = blender::nodes::cmp_node_trackpos_declare;
  node_type_init(&ntype, init);
  node_type_storage(
      &ntype, "NodeTrackPosData", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}

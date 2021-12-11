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

#include "../node_composite_util.hh"

/* **************** SCALAR MATH ******************** */

namespace blender::nodes {

static void cmp_node_ellipsemask_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Mask")).default_value(0.0f).min(0.0f).max(1.0f);
  b.add_input<decl::Float>(N_("Value")).default_value(1.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Float>(N_("Mask"));
}

}  // namespace blender::nodes

static void node_composit_init_ellipsemask(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeEllipseMask *data = (NodeEllipseMask *)MEM_callocN(sizeof(NodeEllipseMask),
                                                         "NodeEllipseMask");
  data->x = 0.5;
  data->y = 0.5;
  data->width = 0.2;
  data->height = 0.1;
  data->rotation = 0.0;
  node->storage = data;
}

void register_node_type_cmp_ellipsemask()
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MASK_ELLIPSE, "Ellipse Mask", NODE_CLASS_MATTE, 0);
  ntype.declare = blender::nodes::cmp_node_ellipsemask_declare;
  node_type_size(&ntype, 260, 110, 320);
  node_type_init(&ntype, node_composit_init_ellipsemask);
  node_type_storage(
      &ntype, "NodeEllipseMask", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}

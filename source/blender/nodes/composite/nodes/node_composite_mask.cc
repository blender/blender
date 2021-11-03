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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup cmpnodes
 */

#include "DNA_mask_types.h"

#include "node_composite_util.hh"

/* **************** Mask  ******************** */

namespace blender::nodes {

static void cmp_node_mask_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Mask"));
}

}  // namespace blender::nodes

static void node_composit_init_mask(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeMask *data = (NodeMask *)MEM_callocN(sizeof(NodeMask), "NodeMask");
  data->size_x = data->size_y = 256;
  node->storage = data;

  node->custom2 = 16;   /* samples */
  node->custom3 = 0.5f; /* shutter */
}

static void node_mask_label(bNodeTree *UNUSED(ntree), bNode *node, char *label, int maxlen)
{
  if (node->id != nullptr) {
    BLI_strncpy(label, node->id->name + 2, maxlen);
  }
  else {
    BLI_strncpy(label, IFACE_("Mask"), maxlen);
  }
}

void register_node_type_cmp_mask(void)
{
  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_MASK, "Mask", NODE_CLASS_INPUT, 0);
  ntype.declare = blender::nodes::cmp_node_mask_declare;
  node_type_init(&ntype, node_composit_init_mask);
  node_type_label(&ntype, node_mask_label);

  node_type_storage(&ntype, "NodeMask", node_free_standard_storage, node_copy_standard_storage);

  nodeRegisterType(&ntype);
}

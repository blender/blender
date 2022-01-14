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
 * Juho Vepsäläinen
 */

/** \file
 * \ingroup cmpnodes
 */

#include "DNA_node_types.h"

#include "NOD_common.h"
#include "node_common.h"
#include "node_composite_util.hh"

#include "BKE_node.h"

#include "RNA_access.h"

void register_node_type_cmp_group()
{
  static bNodeType ntype;

  /* NOTE: Cannot use sh_node_type_base for node group, because it would map the node type
   * to the shared NODE_GROUP integer type id. */
  node_type_base_custom(&ntype, "CompositorNodeGroup", "Group", NODE_CLASS_GROUP);
  ntype.type = NODE_GROUP;
  ntype.poll = cmp_node_poll_default;
  ntype.poll_instance = node_group_poll_instance;
  ntype.insert_link = node_insert_link_default;
  ntype.rna_ext.srna = RNA_struct_find("CompositorNodeGroup");
  BLI_assert(ntype.rna_ext.srna != nullptr);
  RNA_struct_blender_type_set(ntype.rna_ext.srna, &ntype);

  node_type_size(&ntype, 140, 60, 400);
  ntype.labelfunc = node_group_label;
  node_type_group_update(&ntype, node_group_update);

  nodeRegisterType(&ntype);
}

void register_node_type_cmp_custom_group(bNodeType *ntype)
{
  /* These methods can be overridden but need a default implementation otherwise. */
  if (ntype->poll == nullptr) {
    ntype->poll = cmp_node_poll_default;
  }
  if (ntype->insert_link == nullptr) {
    ntype->insert_link = node_insert_link_default;
  }
}

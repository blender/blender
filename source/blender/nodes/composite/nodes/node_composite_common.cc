/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

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
  ntype.declare_dynamic = blender::nodes::node_group_declare_dynamic;

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

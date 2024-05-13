/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node.hh"

#include "NOD_geometry.hh"
#include "NOD_node_declaration.hh"

#include "NOD_common.h"
#include "node_common.h"
#include "node_geometry_util.hh"

#include "RNA_access.hh"

namespace blender::nodes {

static void register_node_type_geo_group()
{
  static blender::bke::bNodeType ntype;

  bke::node_type_base_custom(&ntype, "GeometryNodeGroup", "Group", "GROUP", NODE_CLASS_GROUP);
  ntype.type = NODE_GROUP;
  ntype.poll = geo_node_poll_default;
  ntype.poll_instance = node_group_poll_instance;
  ntype.insert_link = node_insert_link_default;
  ntype.ui_class = node_group_ui_class;
  ntype.ui_description_fn = node_group_ui_description;
  ntype.rna_ext.srna = RNA_struct_find("GeometryNodeGroup");
  BLI_assert(ntype.rna_ext.srna != nullptr);
  RNA_struct_blender_type_set(ntype.rna_ext.srna, &ntype);

  bke::node_type_size(&ntype, 140, 60, 400);
  ntype.labelfunc = node_group_label;
  ntype.declare = node_group_declare;

  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(register_node_type_geo_group)

}  // namespace blender::nodes

void register_node_type_geo_custom_group(blender::bke::bNodeType *ntype)
{
  /* These methods can be overridden but need a default implementation otherwise. */
  if (ntype->poll == nullptr) {
    ntype->poll = geo_node_poll_default;
  }
  if (ntype->insert_link == nullptr) {
    ntype->insert_link = node_insert_link_default;
  }
  ntype->declare = blender::nodes::node_group_declare;
}

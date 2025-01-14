/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <optional>

#include "node_function_util.hh"
#include "node_util.hh"

#include "NOD_socket_search_link.hh"

static bool fn_node_poll_default(const blender::bke::bNodeType * /*ntype*/,
                                 const bNodeTree *ntree,
                                 const char **r_disabled_hint)
{
  /* Function nodes are only supported in simulation node trees so far. */
  if (!STREQ(ntree->idname, "GeometryNodeTree")) {
    *r_disabled_hint = RPT_("Not a geometry node tree");
    return false;
  }
  return true;
}

void fn_node_type_base(blender::bke::bNodeType *ntype,
                       std::string idname,
                       const std::optional<int16_t> legacy_type)
{
  blender::bke::node_type_base(ntype, idname, legacy_type);
  ntype->poll = fn_node_poll_default;
  ntype->insert_link = node_insert_link_default;
  ntype->gather_link_search_ops = blender::nodes::search_link_ops_for_basic_node;
}

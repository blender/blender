/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"
#include "node_util.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_pointcloud.h"

#include "NOD_add_node_search.hh"
#include "NOD_socket_search_link.hh"

namespace blender::nodes {

std::optional<eCustomDataType> node_data_type_to_custom_data_type(const eNodeSocketDatatype type)
{
  switch (type) {
    case SOCK_FLOAT:
      return CD_PROP_FLOAT;
    case SOCK_VECTOR:
      return CD_PROP_FLOAT3;
    case SOCK_RGBA:
      return CD_PROP_COLOR;
    case SOCK_BOOLEAN:
      return CD_PROP_BOOL;
    case SOCK_ROTATION:
      return CD_PROP_QUATERNION;
    case SOCK_INT:
      return CD_PROP_INT32;
    case SOCK_STRING:
      return CD_PROP_STRING;
    default:
      return {};
  }
}

std::optional<eCustomDataType> node_socket_to_custom_data_type(const bNodeSocket &socket)
{
  return node_data_type_to_custom_data_type(eNodeSocketDatatype(socket.type));
}

bool check_tool_context_and_error(GeoNodeExecParams &params)
{
  if (!params.user_data()->operator_data) {
    params.error_message_add(NodeWarningType::Error, "Node must be run as tool");
    params.set_default_remaining_outputs();
    return false;
  }
  return true;
}

void search_link_ops_for_for_tool_node(GatherAddNodeSearchParams &params)
{
  const SpaceNode &snode = *CTX_wm_space_node(&params.context());
  if (snode.geometry_nodes_type == SNODE_GEOMETRY_TOOL) {
    search_node_add_ops_for_basic_node(params);
  }
}
void search_link_ops_for_tool_node(GatherLinkSearchOpParams &params)
{
  if (params.space_node().geometry_nodes_type == SNODE_GEOMETRY_TOOL) {
    search_link_ops_for_basic_node(params);
  }
}

}  // namespace blender::nodes

bool geo_node_poll_default(const bNodeType * /*ntype*/,
                           const bNodeTree *ntree,
                           const char **r_disabled_hint)
{
  if (!STREQ(ntree->idname, "GeometryNodeTree")) {
    *r_disabled_hint = TIP_("Not a geometry node tree");
    return false;
  }
  return true;
}

void geo_node_type_base(bNodeType *ntype, int type, const char *name, short nclass)
{
  blender::bke::node_type_base(ntype, type, name, nclass);
  ntype->poll = geo_node_poll_default;
  ntype->insert_link = node_insert_link_default;
  ntype->gather_link_search_ops = blender::nodes::search_link_ops_for_basic_node;
  ntype->gather_add_node_search_ops = blender::nodes::search_node_add_ops_for_basic_node;
}

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <optional>

#include "BLI_string.h"

#include "node_geometry_util.hh"
#include "node_util.hh"

#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BKE_node.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

namespace blender::nodes {

bool check_tool_context_and_error(GeoNodeExecParams &params)
{
  if (!params.user_data()->call_data->operator_data) {
    params.error_message_add(NodeWarningType::Error, TIP_("Node must be run as tool"));
    params.set_default_remaining_outputs();
    return false;
  }
  return true;
}

void search_link_ops_for_tool_node(GatherLinkSearchOpParams &params)
{
  if (params.space_node().node_tree_sub_type == SNODE_GEOMETRY_TOOL) {
    search_link_ops_for_basic_node(params);
  }
}

void node_geo_sdf_grid_error_not_levelset(GeoNodeExecParams &params)
{
  params.error_message_add(
      NodeWarningType::Error,
      "Input grid is not a valid level set. Use a signed distance field grid as input");
  params.set_default_remaining_outputs();
}

namespace enums {

const EnumPropertyItem *attribute_type_type_with_socket_fn(bContext * /*C*/,
                                                           PointerRNA * /*ptr*/,
                                                           PropertyRNA * /*prop*/,
                                                           bool *r_free)
{
  *r_free = true;
  return enum_items_filter(
      rna_enum_attribute_type_items, [](const EnumPropertyItem &item) -> bool {
        return generic_attribute_type_supported(item) &&
               !ELEM(item.value, CD_PROP_INT8, CD_PROP_BYTE_COLOR, CD_PROP_FLOAT2);
      });
}

bool generic_attribute_type_supported(const EnumPropertyItem &item)
{
  return ELEM(item.value,
              CD_PROP_FLOAT,
              CD_PROP_FLOAT2,
              CD_PROP_FLOAT3,
              CD_PROP_COLOR,
              CD_PROP_BOOL,
              CD_PROP_INT8,
              CD_PROP_INT32,
              CD_PROP_BYTE_COLOR,
              CD_PROP_QUATERNION,
              CD_PROP_FLOAT4X4);
}

}  // namespace enums

const EnumPropertyItem *grid_data_type_socket_items_filter_fn(bContext * /*C*/,
                                                              PointerRNA * /*ptr*/,
                                                              PropertyRNA * /*prop*/,
                                                              bool *r_free)
{
  *r_free = true;
  return enum_items_filter(
      rna_enum_volume_grid_data_type_items, [](const EnumPropertyItem &item) -> bool {
        return bke::grid_type_to_socket_type(VolumeGridType(item.value)).has_value();
      });
}

const EnumPropertyItem *grid_socket_type_items_filter_fn(bContext * /*C*/,
                                                         PointerRNA * /*ptr*/,
                                                         PropertyRNA * /*prop*/,
                                                         bool *r_free)
{
  *r_free = true;
  return enum_items_filter(rna_enum_node_socket_data_type_items,
                           [](const EnumPropertyItem &item) -> bool {
                             return socket_type_supports_grids(eNodeSocketDatatype(item.value));
                           });
}

void node_geo_exec_with_missing_openvdb(GeoNodeExecParams &params)
{
  params.set_default_remaining_outputs();
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenVDB"));
}

void node_geo_exec_with_too_old_openvdb(GeoNodeExecParams &params)
{
  params.set_default_remaining_outputs();
  params.error_message_add(NodeWarningType::Error, TIP_("Disabled, OpenVDB version is too old"));
}

}  // namespace blender::nodes

bool geo_node_poll_default(const blender::bke::bNodeType * /*ntype*/,
                           const bNodeTree *ntree,
                           const char **r_disabled_hint)
{
  if (!STREQ(ntree->idname, "GeometryNodeTree")) {
    *r_disabled_hint = RPT_("Not a geometry node tree");
    return false;
  }
  return true;
}

void geo_node_type_base(blender::bke::bNodeType *ntype,
                        std::string idname,
                        const std::optional<int16_t> legacy_type)
{
  blender::bke::node_type_base(*ntype, idname, legacy_type);
  ntype->poll = geo_node_poll_default;
  ntype->insert_link = node_insert_link_default;
  ntype->gather_link_search_ops = blender::nodes::search_link_ops_for_basic_node;
}

static bool geo_cmp_node_poll_default(const blender::bke::bNodeType * /*ntype*/,
                                      const bNodeTree *ntree,
                                      const char **r_disabled_hint)
{
  if (!STR_ELEM(ntree->idname, "GeometryNodeTree", "CompositorNodeTree")) {
    *r_disabled_hint = RPT_("Not a geometry or compositor node tree");
    return false;
  }
  return true;
}

void geo_cmp_node_type_base(blender::bke::bNodeType *ntype,
                            std::string idname,
                            const std::optional<int16_t> legacy_type)
{
  blender::bke::node_type_base(*ntype, idname, legacy_type);
  ntype->poll = geo_cmp_node_poll_default;
  ntype->insert_link = node_insert_link_default;
  ntype->gather_link_search_ops = blender::nodes::search_link_ops_for_basic_node;
}

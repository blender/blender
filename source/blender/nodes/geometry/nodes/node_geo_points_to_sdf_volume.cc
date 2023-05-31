/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENVDB
#  include <openvdb/Types.h>
#endif

#include "node_geometry_util.hh"

#include "GEO_points_to_volume.hh"

#include "NOD_add_node_search.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_geo_points_to_sdf_volume_cc {

NODE_STORAGE_FUNCS(NodeGeometryPointsToVolume)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Points");
  b.add_input<decl::Float>("Voxel Size")
      .default_value(0.3f)
      .min(0.01f)
      .subtype(PROP_DISTANCE)
      .make_available([](bNode &node) {
        node_storage(node).resolution_mode = GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE;
      });
  b.add_input<decl::Float>("Voxel Amount")
      .default_value(64.0f)
      .min(0.0f)
      .make_available([](bNode &node) {
        node_storage(node).resolution_mode = GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT;
      });
  b.add_input<decl::Float>("Radius")
      .default_value(0.5f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .field_on_all();
  b.add_output<decl::Geometry>("Volume").translation_context(BLT_I18NCONTEXT_ID_ID);
}

static void search_node_add_ops(GatherAddNodeSearchParams &params)
{
  if (U.experimental.use_new_volume_nodes) {
    blender::nodes::search_node_add_ops_for_basic_node(params);
  }
}

static void search_link_ops(GatherLinkSearchOpParams &params)
{
  if (U.experimental.use_new_volume_nodes) {
    blender::nodes::search_link_ops_for_basic_node(params);
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "resolution_mode", 0, IFACE_("Resolution"), ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryPointsToVolume *data = MEM_cnew<NodeGeometryPointsToVolume>(__func__);
  data->resolution_mode = GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryPointsToVolume &storage = node_storage(*node);
  bNodeSocket *voxel_size_socket = nodeFindSocket(node, SOCK_IN, "Voxel Size");
  bNodeSocket *voxel_amount_socket = nodeFindSocket(node, SOCK_IN, "Voxel Amount");
  bke::nodeSetSocketAvailability(ntree,
                                 voxel_amount_socket,
                                 storage.resolution_mode ==
                                     GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT);
  bke::nodeSetSocketAvailability(ntree,
                                 voxel_size_socket,
                                 storage.resolution_mode ==
                                     GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE);
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");
  const NodeGeometryPointsToVolume &storage = node_storage(params.node());
  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    initialize_volume_component_from_points(
        params, storage, geometry_set, openvdb::GRID_LEVEL_SET);
  });
  params.set_output("Volume", std::move(geometry_set));
#else
  params.set_default_remaining_outputs();
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenVDB"));
#endif
}

}  // namespace blender::nodes::node_geo_points_to_sdf_volume_cc

void register_node_type_geo_points_to_sdf_volume()
{
  namespace file_ns = blender::nodes::node_geo_points_to_sdf_volume_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_POINTS_TO_SDF_VOLUME, "Points to SDF Volume", NODE_CLASS_GEOMETRY);
  node_type_storage(&ntype,
                    "NodeGeometryPointsToVolume",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  blender::bke::node_type_size(&ntype, 170, 120, 700);
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.gather_add_node_search_ops = file_ns::search_node_add_ops;
  ntype.gather_link_search_ops = file_ns::search_link_ops;
  nodeRegisterType(&ntype);
}

/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_volume_grid.hh"
#include "BKE_volume_to_mesh.hh"

#include "GEO_randomize.hh"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_grid_to_mesh_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Grid").hide_value();
  b.add_input<decl::Float>("Threshold")
      .default_value(0.1f)
      .description("Values larger than the threshold are inside the generated mesh");
  b.add_input<decl::Float>("Adaptivity").min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_output<decl::Geometry>("Mesh");
}

static void node_gather_link_search_ops(GatherLinkSearchOpParams &params)
{
  if (U.experimental.use_new_volume_nodes) {
    nodes::search_link_ops_for_basic_node(params);
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const bke::VolumeGrid<float> grid = params.extract_input<bke::VolumeGrid<float>>("Grid");
  if (!grid) {
    params.set_default_remaining_outputs();
    return;
  }
  bke::VolumeTreeAccessToken tree_token;
  Mesh *mesh = bke::volume_grid_to_mesh(grid.get().grid(tree_token),
                                        params.extract_input<float>("Threshold"),
                                        params.extract_input<float>("Adaptivity"));
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  geometry::debug_randomize_mesh_order(mesh);
  params.set_output("Mesh", GeometrySet::from_mesh(mesh));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_GRID_TO_MESH, "Grid to Mesh", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grid_to_mesh_cc

/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"

#include "NOD_geometry_nodes_list.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_get_grid_names_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Volume"_ustr).description("Volume to list the grid names of");
  b.add_output<decl::String>("Names"_ustr)
      .structure_type(StructureType::List)
      .description("Names of all grids stored in the volume");
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const GeometrySet geometry_set = params.extract_input<GeometrySet>("Volume"_ustr);
  Vector<std::string> names;
  if (const Volume *volume = geometry_set.get_volume()) {
    const int grids_num = BKE_volume_num_grids(volume);
    names.reserve(grids_num);
    for (const int i : IndexRange(grids_num)) {
      if (const bke::VolumeGridData *grid = BKE_volume_grid_get(volume, i)) {
        names.append(grid->name());
      }
    }
    std::ranges::sort(names);
  }
  params.set_output("Names"_ustr, GList::from_container(std::move(names)));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGetGridNames"_ustr);
  ntype.ui_name = "Get Grid Names";
  ntype.ui_description =
      "Retrieves the names of all grids stored in a volume as a list of strings";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_get_grid_names_cc

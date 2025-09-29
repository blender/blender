/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_lib_id.hh"
#include "BKE_volume.hh"
#include "BKE_volume_grid_file_cache.hh"
#include "BKE_volume_openvdb.hh"

#include "DNA_volume_types.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_import_vdb {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("Path")
      .subtype(PROP_FILEPATH)
      .path_filter("*.vdb")
      .optional_label()
      .description("Path to a OpenVDB file");

  b.add_output<decl::Geometry>("Volume");
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const std::optional<std::string> path = params.ensure_absolute_path(
      params.extract_input<std::string>("Path"));
  if (!path) {
    params.set_default_remaining_outputs();
    return;
  }

  bke::volume_grid::file_cache::GridsFromFile grids_from_file =
      bke::volume_grid::file_cache::get_all_grids_from_file(*path);
  if (!grids_from_file.error_message.empty()) {
    params.error_message_add(NodeWarningType::Error, grids_from_file.error_message);
    params.set_default_remaining_outputs();
    return;
  }

  Volume *volume = static_cast<Volume *>(BKE_id_new_nomain(ID_VO, "Volume"));
  BKE_volume_metadata_set(*volume, grids_from_file.file_meta_data);
  for (bke::GVolumeGrid &grid : grids_from_file.grids) {
    grid->add_user();
    BKE_volume_grid_add(volume, grid.get());
  }

  params.set_output("Volume", GeometrySet::from_volume(volume));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeImportVDB");
  ntype.ui_name = "Import VDB";
  ntype.ui_description = "Import volume data from a .vdb file";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_import_vdb

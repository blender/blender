/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BKE_volume_grid_fields.hh"

namespace blender::nodes::node_geo_input_voxel_index_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_output<decl::Int>("X").field_source().description(
      "X coordinate of the voxel in index space, or the minimum X coordinate of a tile");
  b.add_output<decl::Int>("Y").field_source().description(
      "Y coordinate of the voxel in index space, or the minimum Y coordinate of a tile");
  b.add_output<decl::Int>("Z").field_source().description(
      "Z coordinate of the voxel in index space, or the minimum Z coordinate of a tile");
  auto &panel = b.add_panel("Tile").default_closed(true);
  panel.add_output<decl::Bool>("Is Tile").field_source().description(
      "True if the field is evaluated on a tile, i.e. on multiple voxels at once. If this is "
      "false, the extent is always 1");
  panel.add_output<decl::Int>("Extent X")
      .field_source()
      .description(
          "Number of voxels in the X direction of the tile, or 1 if the field is evaluated on a "
          "voxel");
  panel.add_output<decl::Int>("Extent Y")
      .field_source()
      .description(
          "Number of voxels in the Y direction of the tile, or 1 if the field is evaluated on a "
          "voxel");
  panel.add_output<decl::Int>("Extent Z")
      .field_source()
      .description(
          "Number of voxels in the Z direction of the tile, or 1 if the field is evaluated on a "
          "voxel");
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  if (params.output_is_required("X")) {
    params.set_output("X",
                      fn::GField(std::make_shared<bke::VoxelCoordinateFieldInput>(math::Axis::X)));
  }
  if (params.output_is_required("Y")) {
    params.set_output("Y",
                      fn::GField(std::make_shared<bke::VoxelCoordinateFieldInput>(math::Axis::Y)));
  }
  if (params.output_is_required("Z")) {
    params.set_output("Z",
                      fn::GField(std::make_shared<bke::VoxelCoordinateFieldInput>(math::Axis::Z)));
  }
  if (params.output_is_required("Is Tile")) {
    params.set_output("Is Tile", fn::GField(std::make_shared<bke::IsTileFieldInput>()));
  }
  if (params.output_is_required("Extent X")) {
    params.set_output("Extent X",
                      fn::GField(std::make_shared<bke::VoxelExtentFieldInput>(math::Axis::X)));
  }
  if (params.output_is_required("Extent Y")) {
    params.set_output("Extent Y",
                      fn::GField(std::make_shared<bke::VoxelExtentFieldInput>(math::Axis::Y)));
  }
  if (params.output_is_required("Extent Z")) {
    params.set_output("Extent Z",
                      fn::GField(std::make_shared<bke::VoxelExtentFieldInput>(math::Axis::Z)));
  }
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeInputVoxelIndex");
  ntype.ui_name = "Voxel Index";
  ntype.ui_description =
      "Retrieve the integer coordinates of the voxel that the field is evaluated on";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_voxel_index_cc

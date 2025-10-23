/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "DNA_mesh_types.h"
#include "DNA_volume_types.h"

#include "BKE_lib_id.hh"

#include "GEO_foreach_geometry.hh"
#include "GEO_mesh_to_volume.hh"

#include "NOD_rna_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_geo_mesh_to_volume_cc {

NODE_STORAGE_FUNCS(NodeGeometryMeshToVolume)

static EnumPropertyItem resolution_mode_items[] = {
    {MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT,
     "VOXEL_AMOUNT",
     0,
     CTX_N_(BLT_I18NCONTEXT_COUNTABLE, "Amount"),
     N_("Desired number of voxels along one axis")},
    {MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_SIZE,
     "VOXEL_SIZE",
     0,
     CTX_N_(BLT_I18NCONTEXT_COUNTABLE, "Size"),
     N_("Desired voxel side length")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh")
      .supported_type(GeometryComponent::Type::Mesh)
      .description("Mesh to convert the inner volume to a fog volume geometry");
  b.add_input<decl::Float>("Density").default_value(1.0f).min(0.01f).max(FLT_MAX);
  b.add_input<decl::Menu>("Resolution Mode")
      .static_items(resolution_mode_items)
      .optional_label()
      .description("How the voxel size is specified")
      .translation_context(BLT_I18NCONTEXT_COUNTABLE);
  b.add_input<decl::Float>("Voxel Size")
      .default_value(0.3f)
      .min(0.01f)
      .max(FLT_MAX)
      .subtype(PROP_DISTANCE)
      .usage_by_single_menu(MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_SIZE);
  b.add_input<decl::Float>("Voxel Amount")
      .default_value(64.0f)
      .min(0.0f)
      .max(FLT_MAX)
      .usage_by_single_menu(MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT);
  b.add_input<decl::Float>("Interior Band Width")
      .default_value(0.2f)
      .min(0.0001f)
      .max(FLT_MAX)
      .subtype(PROP_DISTANCE)
      .description("Width of the gradient inside of the mesh");
  b.add_output<decl::Geometry>("Volume").translation_context(BLT_I18NCONTEXT_ID_ID);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  /* Still used for forward compatibility. */
  node->storage = MEM_callocN<NodeGeometryMeshToVolume>(__func__);
}

#ifdef WITH_OPENVDB

static Volume *create_volume_from_mesh(const Mesh &mesh, GeoNodeExecParams &params)
{
  const float density = params.get_input<float>("Density");
  const float interior_band_width = params.get_input<float>("Interior Band Width");
  const auto mode = params.get_input<MeshToVolumeModifierResolutionMode>("Resolution Mode");

  geometry::MeshToVolumeResolution resolution;
  resolution.mode = mode;
  if (resolution.mode == MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT) {
    resolution.settings.voxel_amount = params.get_input<float>("Voxel Amount");
    if (resolution.settings.voxel_amount <= 0.0f) {
      return nullptr;
    }
  }
  else if (resolution.mode == MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_SIZE) {
    resolution.settings.voxel_size = params.get_input<float>("Voxel Size");
    if (resolution.settings.voxel_size <= 0.0f) {
      return nullptr;
    }
  }

  if (mesh.verts_num == 0 || mesh.faces_num == 0) {
    return nullptr;
  }

  const float4x4 mesh_to_volume_space_transform = float4x4::identity();

  const float voxel_size = geometry::volume_compute_voxel_size(
      params.depsgraph(),
      [&]() { return *mesh.bounds_min_max(); },
      resolution,
      0.0f,
      mesh_to_volume_space_transform);

  Volume *volume = BKE_id_new_nomain<Volume>(nullptr);

  /* Convert mesh to grid and add to volume. */
  geometry::fog_volume_grid_add_from_mesh(volume,
                                          "density",
                                          mesh.vert_positions(),
                                          mesh.corner_verts(),
                                          mesh.corner_tris(),
                                          mesh_to_volume_space_transform,
                                          voxel_size,
                                          interior_band_width,
                                          density);

  return volume;
}

#endif /* WITH_OPENVDB */

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  GeometrySet geometry_set(params.extract_input<GeometrySet>("Mesh"));
  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
    if (geometry_set.has_mesh()) {
      Volume *volume = create_volume_from_mesh(*geometry_set.get_mesh(), params);
      geometry_set.replace_volume(volume);
      geometry_set.keep_only({GeometryComponent::Type::Volume, GeometryComponent::Type::Edit});
    }
  });
  params.set_output("Volume", std::move(geometry_set));
#else
  node_geo_exec_with_missing_openvdb(params);
  return;
#endif
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeMeshToVolume", GEO_NODE_MESH_TO_VOLUME);
  ntype.ui_name = "Mesh to Volume";
  ntype.ui_description = "Create a fog volume with the shape of the input mesh's surface";
  ntype.enum_name_legacy = "MESH_TO_VOLUME";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  bke::node_type_size(ntype, 200, 120, 700);
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryMeshToVolume", node_free_standard_storage, node_copy_standard_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_to_volume_cc

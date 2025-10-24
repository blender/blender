/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENVDB
#  include <openvdb/tools/GridTransformer.h>
#  include <openvdb/tools/VolumeToMesh.h>
#endif

#include "node_geometry_util.hh"

#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"
#include "BKE_volume_to_mesh.hh"

#include "GEO_foreach_geometry.hh"
#include "GEO_randomize.hh"

namespace blender::nodes::node_geo_volume_to_mesh_cc {

NODE_STORAGE_FUNCS(NodeGeometryVolumeToMesh)

static EnumPropertyItem resolution_mode_items[] = {
    {VOLUME_TO_MESH_RESOLUTION_MODE_GRID,
     "GRID",
     0,
     CTX_N_(BLT_I18NCONTEXT_COUNTABLE, "Grid"),
     N_("Use resolution of the volume grid")},
    {VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT,
     "VOXEL_AMOUNT",
     0,
     CTX_N_(BLT_I18NCONTEXT_COUNTABLE, "Amount"),
     N_("Desired number of voxels along one axis")},
    {VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE,
     "VOXEL_SIZE",
     0,
     CTX_N_(BLT_I18NCONTEXT_COUNTABLE, "Size"),
     N_("Desired voxel side length")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Volume")
      .supported_type(GeometryComponent::Type::Volume)
      .translation_context(BLT_I18NCONTEXT_ID_ID)
      .is_default_link_socket()
      .description("Volume to convert to a mesh");
  b.add_input<decl::Menu>("Resolution Mode")
      .static_items(resolution_mode_items)
      .optional_label()
      .description("How the voxel size is specified")
      .translation_context(BLT_I18NCONTEXT_COUNTABLE);
  b.add_input<decl::Float>("Voxel Size")
      .default_value(0.3f)
      .min(0.01f)
      .subtype(PROP_DISTANCE)
      .usage_by_single_menu(VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE);
  b.add_input<decl::Float>("Voxel Amount")
      .default_value(64.0f)
      .min(0.0f)
      .usage_by_single_menu(VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT);
  b.add_input<decl::Float>("Threshold")
      .default_value(0.1f)
      .description("Values larger than the threshold are inside the generated mesh");
  b.add_input<decl::Float>("Adaptivity").min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_output<decl::Geometry>("Mesh");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  /* Still used for forward compatibility. */
  node->storage = MEM_callocN<NodeGeometryVolumeToMesh>(__func__);
}

#ifdef WITH_OPENVDB

static bke::VolumeToMeshResolution get_resolution_param(const GeoNodeExecParams &params)
{
  bke::VolumeToMeshResolution resolution;
  resolution.mode = params.get_input<VolumeToMeshResolutionMode>("Resolution Mode");
  if (resolution.mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT) {
    resolution.settings.voxel_amount = std::max(params.get_input<float>("Voxel Amount"), 0.0f);
  }
  else if (resolution.mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE) {
    resolution.settings.voxel_size = std::max(params.get_input<float>("Voxel Size"), 0.0f);
  }

  return resolution;
}

static Mesh *create_mesh_from_volume_grids(Span<const openvdb::GridBase *> grids,
                                           GeoNodeExecParams &params,
                                           const float threshold,
                                           const float adaptivity,
                                           const bke::VolumeToMeshResolution &resolution)
{
  Array<bke::VolumeToMeshDataResult> mesh_data(grids.size());
  for (const int i : grids.index_range()) {
    bke::VolumeToMeshDataResult &result = mesh_data[i];
    result = bke::volume_to_mesh_data(*grids[i], resolution, threshold, adaptivity);
    if (!result.error.empty()) {
      params.error_message_add(NodeWarningType::Error, result.error);
    }
  }

  int vert_offset = 0;
  int face_offset = 0;
  int loop_offset = 0;
  Array<int> vert_offsets(mesh_data.size());
  Array<int> face_offsets(mesh_data.size());
  Array<int> loop_offsets(mesh_data.size());
  for (const int i : grids.index_range()) {
    const bke::OpenVDBMeshData &data = mesh_data[i].data;
    vert_offsets[i] = vert_offset;
    face_offsets[i] = face_offset;
    loop_offsets[i] = loop_offset;
    vert_offset += data.verts.size();
    face_offset += (data.tris.size() + data.quads.size());
    loop_offset += (3 * data.tris.size() + 4 * data.quads.size());
  }

  Mesh *mesh = BKE_mesh_new_nomain(vert_offset, 0, face_offset, loop_offset);
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  MutableSpan<float3> positions = mesh->vert_positions_for_write();
  MutableSpan<int> dst_face_offsets = mesh->face_offsets_for_write();
  MutableSpan<int> corner_verts = mesh->corner_verts_for_write();

  for (const int i : grids.index_range()) {
    const bke::OpenVDBMeshData &data = mesh_data[i].data;
    bke::fill_mesh_from_openvdb_data(data.verts,
                                     data.tris,
                                     data.quads,
                                     vert_offsets[i],
                                     face_offsets[i],
                                     loop_offsets[i],
                                     positions,
                                     dst_face_offsets,
                                     corner_verts);
  }

  bke::mesh_calc_edges(*mesh, false, false);
  bke::mesh_smooth_set(*mesh, false);

  mesh->tag_overlapping_none();

  geometry::debug_randomize_mesh_order(mesh);

  return mesh;
}

static Mesh *create_mesh_from_volume(GeometrySet &geometry_set, GeoNodeExecParams &params)
{
  const Volume *volume = geometry_set.get_volume();
  if (volume == nullptr) {
    return nullptr;
  }

  const bke::VolumeToMeshResolution resolution = get_resolution_param(params);

  if (resolution.mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE &&
      resolution.settings.voxel_size <= 0.0f)
  {
    return nullptr;
  }
  if (resolution.mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT &&
      resolution.settings.voxel_amount <= 0)
  {
    return nullptr;
  }

  BKE_volume_load(volume, params.bmain());

  Vector<bke::VolumeTreeAccessToken> tree_tokens;
  Vector<const openvdb::GridBase *> grids;
  for (const int i : IndexRange(BKE_volume_num_grids(volume))) {
    const bke::VolumeGridData *volume_grid = BKE_volume_grid_get(volume, i);
    tree_tokens.append_as();
    grids.append(&volume_grid->grid(tree_tokens.last()));
  }

  if (grids.is_empty()) {
    return nullptr;
  }

  return create_mesh_from_volume_grids(grids,
                                       params,
                                       params.get_input<float>("Threshold"),
                                       params.get_input<float>("Adaptivity"),
                                       resolution);
}

#endif /* WITH_OPENVDB */

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Volume");
  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
    Mesh *mesh = create_mesh_from_volume(geometry_set, params);
    geometry_set.replace_mesh(mesh);
    geometry_set.keep_only({GeometryComponent::Type::Mesh, GeometryComponent::Type::Edit});
  });
  params.set_output("Mesh", std::move(geometry_set));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeVolumeToMesh", GEO_NODE_VOLUME_TO_MESH);
  ntype.ui_name = "Volume to Mesh";
  ntype.ui_description = "Generate a mesh on the \"surface\" of a volume";
  ntype.enum_name_legacy = "VOLUME_TO_MESH";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryVolumeToMesh", node_free_standard_storage, node_copy_standard_storage);
  blender::bke::node_type_size(ntype, 170, 120, 700);
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_volume_to_mesh_cc

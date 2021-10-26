/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "DEG_depsgraph_query.h"
#ifdef WITH_OPENVDB
#  include <openvdb/tools/GridTransformer.h>
#  include <openvdb/tools/VolumeToMesh.h>
#endif

#include "node_geometry_util.hh"

#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_volume.h"
#include "BKE_volume_to_mesh.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes {

static void geo_node_volume_to_mesh_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Volume").supported_type(GEO_COMPONENT_TYPE_VOLUME);
  b.add_input<decl::Float>("Voxel Size").default_value(0.3f).min(0.01f).subtype(PROP_DISTANCE);
  b.add_input<decl::Float>("Voxel Amount").default_value(64.0f).min(0.0f);
  b.add_input<decl::Float>("Threshold").default_value(0.1f).min(0.0f);
  b.add_input<decl::Float>("Adaptivity").min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_output<decl::Geometry>("Mesh");
}

static void geo_node_volume_to_mesh_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "resolution_mode", 0, IFACE_("Resolution"), ICON_NONE);
}

static void geo_node_volume_to_mesh_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryVolumeToMesh *data = (NodeGeometryVolumeToMesh *)MEM_callocN(
      sizeof(NodeGeometryVolumeToMesh), __func__);
  data->resolution_mode = VOLUME_TO_MESH_RESOLUTION_MODE_GRID;
  node->storage = data;
}

static void geo_node_volume_to_mesh_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryVolumeToMesh *data = (NodeGeometryVolumeToMesh *)node->storage;

  bNodeSocket *voxel_size_socket = nodeFindSocket(node, SOCK_IN, "Voxel Size");
  bNodeSocket *voxel_amount_socket = nodeFindSocket(node, SOCK_IN, "Voxel Amount");
  nodeSetSocketAvailability(voxel_amount_socket,
                            data->resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT);
  nodeSetSocketAvailability(voxel_size_socket,
                            data->resolution_mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE);
}

#ifdef WITH_OPENVDB

static bke::VolumeToMeshResolution get_resolution_param(const GeoNodeExecParams &params)
{
  const NodeGeometryVolumeToMesh &storage =
      *(const NodeGeometryVolumeToMesh *)params.node().storage;

  bke::VolumeToMeshResolution resolution;
  resolution.mode = (VolumeToMeshResolutionMode)storage.resolution_mode;
  if (resolution.mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT) {
    resolution.settings.voxel_amount = std::max(params.get_input<float>("Voxel Amount"), 0.0f);
  }
  else if (resolution.mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE) {
    resolution.settings.voxel_size = std::max(params.get_input<float>("Voxel Size"), 0.0f);
  }

  return resolution;
}

static Mesh *create_mesh_from_volume_grids(Span<openvdb::GridBase::ConstPtr> grids,
                                           const float threshold,
                                           const float adaptivity,
                                           const bke::VolumeToMeshResolution &resolution)
{
  Array<bke::OpenVDBMeshData> mesh_data(grids.size());
  for (const int i : grids.index_range()) {
    mesh_data[i] = bke::volume_to_mesh_data(*grids[i], resolution, threshold, adaptivity);
  }

  int vert_offset = 0;
  int poly_offset = 0;
  int loop_offset = 0;
  Array<int> vert_offsets(mesh_data.size());
  Array<int> poly_offsets(mesh_data.size());
  Array<int> loop_offsets(mesh_data.size());
  for (const int i : grids.index_range()) {
    const bke::OpenVDBMeshData &data = mesh_data[i];
    vert_offsets[i] = vert_offset;
    poly_offsets[i] = poly_offset;
    loop_offsets[i] = loop_offset;
    vert_offset += data.verts.size();
    poly_offset += (data.tris.size() + data.quads.size());
    loop_offset += (3 * data.tris.size() + 4 * data.quads.size());
  }

  Mesh *mesh = BKE_mesh_new_nomain(vert_offset, 0, 0, loop_offset, poly_offset);
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  MutableSpan<MVert> verts{mesh->mvert, mesh->totvert};
  MutableSpan<MLoop> loops{mesh->mloop, mesh->totloop};
  MutableSpan<MPoly> polys{mesh->mpoly, mesh->totpoly};

  for (const int i : grids.index_range()) {
    const bke::OpenVDBMeshData &data = mesh_data[i];
    bke::fill_mesh_from_openvdb_data(data.verts,
                                     data.tris,
                                     data.quads,
                                     vert_offsets[i],
                                     poly_offsets[i],
                                     loop_offsets[i],
                                     verts,
                                     polys,
                                     loops);
  }

  BKE_mesh_calc_edges(mesh, false, false);
  BKE_mesh_normals_tag_dirty(mesh);

  return mesh;
}

static Mesh *create_mesh_from_volume(GeometrySet &geometry_set, GeoNodeExecParams &params)
{
  const Volume *volume = geometry_set.get_volume_for_read();
  if (volume == nullptr) {
    return nullptr;
  }

  const bke::VolumeToMeshResolution resolution = get_resolution_param(params);
  const Main *bmain = DEG_get_bmain(params.depsgraph());
  BKE_volume_load(volume, bmain);

  Vector<openvdb::GridBase::ConstPtr> grids;
  for (const int i : IndexRange(BKE_volume_num_grids(volume))) {
    const VolumeGrid *volume_grid = BKE_volume_grid_get_for_read(volume, i);
    openvdb::GridBase::ConstPtr grid = BKE_volume_grid_openvdb_for_read(volume, volume_grid);
    grids.append(std::move(grid));
  }

  if (grids.is_empty()) {
    return nullptr;
  }

  return create_mesh_from_volume_grids(grids,
                                       params.get_input<float>("Threshold"),
                                       params.get_input<float>("Adaptivity"),
                                       resolution);
}

#endif /* WITH_OPENVDB */

static void geo_node_volume_to_mesh_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Volume");

#ifdef WITH_OPENVDB
  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    Mesh *mesh = create_mesh_from_volume(geometry_set, params);
    geometry_set.replace_mesh(mesh);
    geometry_set.keep_only({GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_INSTANCES});
  });
#else
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenVDB"));
#endif

  params.set_output("Mesh", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_volume_to_mesh()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_VOLUME_TO_MESH, "Volume to Mesh", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_volume_to_mesh_declare;
  node_type_storage(
      &ntype, "NodeGeometryVolumeToMesh", node_free_standard_storage, node_copy_standard_storage);
  node_type_size(&ntype, 170, 120, 700);
  node_type_init(&ntype, blender::nodes::geo_node_volume_to_mesh_init);
  node_type_update(&ntype, blender::nodes::geo_node_volume_to_mesh_update);
  ntype.geometry_node_execute = blender::nodes::geo_node_volume_to_mesh_exec;
  ntype.draw_buttons = blender::nodes::geo_node_volume_to_mesh_layout;
  nodeRegisterType(&ntype);
}

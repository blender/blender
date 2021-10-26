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
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::String>("Density");
  b.add_input<decl::Float>("Voxel Size").default_value(0.3f).min(0.01f).subtype(PROP_DISTANCE);
  b.add_input<decl::Float>("Voxel Amount").default_value(64.0f).min(0.0f);
  b.add_input<decl::Float>("Threshold").default_value(0.1f).min(0.0f);
  b.add_input<decl::Float>("Adaptivity").min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_output<decl::Geometry>("Geometry");
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

  bNodeSocket *grid_socket = nodeFindSocket(node, SOCK_IN, "Density");
  bNodeSocketValueString *grid_socket_value = (bNodeSocketValueString *)grid_socket->default_value;
  STRNCPY(grid_socket_value->value, "density");

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

static void create_mesh_from_volume(GeometrySet &geometry_set_in,
                                    GeometrySet &geometry_set_out,
                                    GeoNodeExecParams &params)
{
  if (!geometry_set_in.has<VolumeComponent>()) {
    return;
  }

  const NodeGeometryVolumeToMesh &storage =
      *(const NodeGeometryVolumeToMesh *)params.node().storage;

  bke::VolumeToMeshResolution resolution;
  resolution.mode = (VolumeToMeshResolutionMode)storage.resolution_mode;
  if (resolution.mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_AMOUNT) {
    resolution.settings.voxel_amount = params.get_input<float>("Voxel Amount");
    if (resolution.settings.voxel_amount <= 0.0f) {
      return;
    }
  }
  else if (resolution.mode == VOLUME_TO_MESH_RESOLUTION_MODE_VOXEL_SIZE) {
    resolution.settings.voxel_size = params.get_input<float>("Voxel Size");
    if (resolution.settings.voxel_size <= 0.0f) {
      return;
    }
  }

  const VolumeComponent *component = geometry_set_in.get_component_for_read<VolumeComponent>();
  const Volume *volume = component->get_for_read();
  if (volume == nullptr) {
    return;
  }

  const Main *bmain = DEG_get_bmain(params.depsgraph());
  BKE_volume_load(volume, bmain);

  const std::string grid_name = params.get_input<std::string>("Density");
  const VolumeGrid *volume_grid = BKE_volume_grid_find_for_read(volume, grid_name.c_str());
  if (volume_grid == nullptr) {
    return;
  }

  float threshold = params.get_input<float>("Threshold");
  float adaptivity = params.get_input<float>("Adaptivity");

  const openvdb::GridBase::ConstPtr grid = BKE_volume_grid_openvdb_for_read(volume, volume_grid);
  Mesh *mesh = bke::volume_to_mesh(*grid, resolution, threshold, adaptivity);
  if (mesh == nullptr) {
    return;
  }
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  MeshComponent &dst_component = geometry_set_out.get_component_for_write<MeshComponent>();
  dst_component.replace(mesh);
}

#endif /* WITH_OPENVDB */

static void geo_node_volume_to_mesh_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set_in = params.extract_input<GeometrySet>("Geometry");
  GeometrySet geometry_set_out;

#ifdef WITH_OPENVDB
  create_mesh_from_volume(geometry_set_in, geometry_set_out, params);
#else
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenVDB"));
#endif

  params.set_output("Geometry", geometry_set_out);
}

}  // namespace blender::nodes

void register_node_type_geo_legacy_volume_to_mesh()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_VOLUME_TO_MESH, "Volume to Mesh", NODE_CLASS_GEOMETRY, 0);
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

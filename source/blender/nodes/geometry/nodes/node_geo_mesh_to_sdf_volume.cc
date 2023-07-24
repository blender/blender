/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DEG_depsgraph_query.h"
#include "node_geometry_util.hh"

#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_wrapper.h"
#include "BKE_object.h"
#include "BKE_volume.h"

#include "GEO_mesh_to_volume.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "NOD_add_node_search.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_geo_mesh_to_sdf_volume_cc {

NODE_STORAGE_FUNCS(NodeGeometryMeshToVolume)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh").supported_type(GeometryComponent::Type::Mesh);
  b.add_input<decl::Float>("Voxel Size")
      .default_value(0.3f)
      .min(0.01f)
      .max(FLT_MAX)
      .subtype(PROP_DISTANCE);
  b.add_input<decl::Float>("Voxel Amount").default_value(64.0f).min(0.0f).max(FLT_MAX);
  b.add_input<decl::Float>("Half-Band Width")
      .description("Half the width of the narrow band in voxel units")
      .default_value(3.0f)
      .min(1.01f)
      .max(10.0f);
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
  NodeGeometryMeshToVolume *data = MEM_cnew<NodeGeometryMeshToVolume>(__func__);
  data->resolution_mode = MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  NodeGeometryMeshToVolume &data = node_storage(*node);

  bNodeSocket *voxel_size_socket = nodeFindSocket(node, SOCK_IN, "Voxel Size");
  bNodeSocket *voxel_amount_socket = nodeFindSocket(node, SOCK_IN, "Voxel Amount");
  bke::nodeSetSocketAvailability(ntree,
                                 voxel_amount_socket,
                                 data.resolution_mode ==
                                     MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT);
  bke::nodeSetSocketAvailability(
      ntree, voxel_size_socket, data.resolution_mode == MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_SIZE);
}

#ifdef WITH_OPENVDB

static Volume *create_volume_from_mesh(const Mesh &mesh, GeoNodeExecParams &params)
{
  const NodeGeometryMeshToVolume &storage = node_storage(params.node());

  const float half_band_width = params.get_input<float>("Half-Band Width");

  geometry::MeshToVolumeResolution resolution;
  resolution.mode = (MeshToVolumeModifierResolutionMode)storage.resolution_mode;
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

  if (mesh.faces_num == 0) {
    return nullptr;
  }

  const float4x4 mesh_to_volume_space_transform = float4x4::identity();

  auto bounds_fn = [&](float3 &r_min, float3 &r_max) {
    float3 min{std::numeric_limits<float>::max()};
    float3 max{-std::numeric_limits<float>::max()};
    BKE_mesh_wrapper_minmax(&mesh, min, max);
    r_min = min;
    r_max = max;
  };

  const float voxel_size = geometry::volume_compute_voxel_size(
      params.depsgraph(), bounds_fn, resolution, half_band_width, mesh_to_volume_space_transform);

  if (voxel_size < 1e-5f) {
    /* The voxel size is too small. */
    return nullptr;
  }

  Volume *volume = reinterpret_cast<Volume *>(BKE_id_new_nomain(ID_VO, nullptr));

  /* Convert mesh to grid and add to volume. */
  geometry::sdf_volume_grid_add_from_mesh(volume, "distance", mesh, voxel_size, half_band_width);

  return volume;
}

#endif /* WITH_OPENVDB */

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  GeometrySet geometry_set(params.extract_input<GeometrySet>("Mesh"));
  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_mesh()) {
      Volume *volume = create_volume_from_mesh(*geometry_set.get_mesh_for_read(), params);
      geometry_set.replace_volume(volume);
      geometry_set.keep_only_during_modify({GeometryComponent::Type::Volume});
    }
  });
  params.set_output("Volume", std::move(geometry_set));
#else
  params.set_default_remaining_outputs();
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenVDB"));
  return;
#endif
}

}  // namespace blender::nodes::node_geo_mesh_to_sdf_volume_cc

void register_node_type_geo_mesh_to_sdf_volume()
{
  namespace file_ns = blender::nodes::node_geo_mesh_to_sdf_volume_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_MESH_TO_SDF_VOLUME, "Mesh to SDF Volume", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  blender::bke::node_type_size(&ntype, 180, 120, 300);
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.gather_add_node_search_ops = file_ns::search_node_add_ops;
  ntype.gather_link_search_ops = file_ns::search_link_ops;
  node_type_storage(
      &ntype, "NodeGeometryMeshToVolume", node_free_standard_storage, node_copy_standard_storage);
  nodeRegisterType(&ntype);
}

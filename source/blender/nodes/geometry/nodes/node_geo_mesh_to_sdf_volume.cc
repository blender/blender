/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DEG_depsgraph_query.h"
#include "node_geometry_util.hh"

#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_object.h"
#include "BKE_volume.h"

#include "GEO_mesh_to_volume.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "NOD_socket_search_link.hh"

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

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
  uiItemR(layout, ptr, "resolution_mode", UI_ITEM_NONE, IFACE_("Resolution"), ICON_NONE);
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
      Volume *volume = create_volume_from_mesh(*geometry_set.get_mesh(), params);
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

static void node_rna(StructRNA *srna)
{
  static EnumPropertyItem resolution_mode_items[] = {
      {MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT,
       "VOXEL_AMOUNT",
       0,
       "Amount",
       "Desired number of voxels along one axis"},
      {MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_SIZE,
       "VOXEL_SIZE",
       0,
       "Size",
       "Desired voxel side length"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "resolution_mode",
                    "Resolution Mode",
                    "How the voxel size is specified",
                    resolution_mode_items,
                    NOD_storage_enum_accessors(resolution_mode),
                    MESH_TO_VOLUME_RESOLUTION_MODE_VOXEL_AMOUNT);
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_MESH_TO_SDF_VOLUME, "Mesh to SDF Volume", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  blender::bke::node_type_size(&ntype, 180, 120, 300);
  ntype.initfunc = node_init;
  ntype.updatefunc = node_update;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.gather_link_search_ops = search_link_ops;
  node_type_storage(
      &ntype, "NodeGeometryMeshToVolume", node_free_standard_storage, node_copy_standard_storage);
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_to_sdf_volume_cc

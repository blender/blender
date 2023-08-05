/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/LevelSetUtil.h>
#  include <openvdb/tools/ParticlesToLevelSet.h>
#endif

#include "BLI_bounds.hh"

#include "node_geometry_util.hh"

#include "GEO_points_to_volume.hh"

#include "BKE_lib_id.h"
#include "BKE_volume.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

#ifdef WITH_OPENVDB
namespace blender::nodes {
static void gather_point_data_from_component(Field<float> radius_field,
                                             const GeometryComponent &component,
                                             Vector<float3> &r_positions,
                                             Vector<float> &r_radii)
{
  if (component.is_empty()) {
    return;
  }
  const VArray<float3> positions = *component.attributes()->lookup<float3>("position");

  const bke::GeometryFieldContext field_context{component, ATTR_DOMAIN_POINT};
  const int domain_num = component.attribute_domain_size(ATTR_DOMAIN_POINT);

  r_positions.resize(r_positions.size() + domain_num);
  positions.materialize(r_positions.as_mutable_span().take_back(domain_num));

  r_radii.resize(r_radii.size() + domain_num);
  fn::FieldEvaluator evaluator{field_context, domain_num};
  evaluator.add_with_destination(radius_field, r_radii.as_mutable_span().take_back(domain_num));
  evaluator.evaluate();
}

static float compute_voxel_size_from_amount(const float voxel_amount,
                                            Span<float3> positions,
                                            const float radius)
{
  if (positions.is_empty()) {
    return 0.0f;
  }

  if (voxel_amount <= 1) {
    return 0.0f;
  }

  const Bounds<float3> bounds = *bounds::min_max(positions);

  /* The voxel size adapts to the final size of the volume. */
  const float diagonal = math::distance(bounds.min, bounds.max);
  const float extended_diagonal = diagonal + 2.0f * radius;
  const float voxel_size = extended_diagonal / voxel_amount;
  return voxel_size;
}

static void convert_to_grid_index_space(const float voxel_size,
                                        MutableSpan<float3> positions,
                                        MutableSpan<float> radii)
{
  const float voxel_size_inv = 1.0f / voxel_size;
  for (const int i : positions.index_range()) {
    positions[i] *= voxel_size_inv;
    /* Better align generated grid with source points. */
    positions[i] -= float3(0.5f);
    radii[i] *= voxel_size_inv;
  }
}

void initialize_volume_component_from_points(GeoNodeExecParams &params,
                                             const NodeGeometryPointsToVolume &storage,
                                             GeometrySet &r_geometry_set,
                                             openvdb::GridClass gridClass)
{
  Vector<float3> positions;
  Vector<float> radii;
  Field<float> radius_field = params.get_input<Field<float>>("Radius");

  for (const GeometryComponent::Type type : {GeometryComponent::Type::Mesh,
                                             GeometryComponent::Type::PointCloud,
                                             GeometryComponent::Type::Curve})
  {
    if (r_geometry_set.has(type)) {
      gather_point_data_from_component(
          radius_field, *r_geometry_set.get_component(type), positions, radii);
    }
  }

  if (positions.is_empty()) {
    return;
  }

  float voxel_size = 0.0f;
  if (storage.resolution_mode == GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE) {
    voxel_size = params.get_input<float>("Voxel Size");
  }
  else if (storage.resolution_mode == GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT) {
    const float voxel_amount = params.get_input<float>("Voxel Amount");
    const float max_radius = *std::max_element(radii.begin(), radii.end());
    voxel_size = compute_voxel_size_from_amount(voxel_amount, positions, max_radius);
  }
  else {
    BLI_assert_msg(0, "Unknown volume resolution mode");
  }

  const double determinant = std::pow(double(voxel_size), 3.0);
  if (!BKE_volume_grid_determinant_valid(determinant)) {
    return;
  }

  Volume *volume = reinterpret_cast<Volume *>(BKE_id_new_nomain(ID_VO, nullptr));

  convert_to_grid_index_space(voxel_size, positions, radii);

  if (gridClass == openvdb::GRID_FOG_VOLUME) {
    const float density = params.get_input<float>("Density");
    blender::geometry::fog_volume_grid_add_from_points(
        volume, "density", positions, radii, voxel_size, density);
  }
  else if (gridClass == openvdb::GRID_LEVEL_SET) {
    blender::geometry::sdf_volume_grid_add_from_points(
        volume, "distance", positions, radii, voxel_size);
  }
  r_geometry_set.keep_only_during_modify({GeometryComponent::Type::Volume});
  r_geometry_set.replace_volume(volume);
}
}  // namespace blender::nodes
#endif

namespace blender::nodes::node_geo_points_to_volume_cc {

NODE_STORAGE_FUNCS(NodeGeometryPointsToVolume)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Points");
  b.add_input<decl::Float>("Density").default_value(1.0f).min(0.0f);
  b.add_input<decl::Float>("Voxel Size")
      .default_value(0.3f)
      .min(0.01f)
      .subtype(PROP_DISTANCE)
      .make_available([](bNode &node) {
        node_storage(node).resolution_mode = GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE;
      });
  b.add_input<decl::Float>("Voxel Amount")
      .default_value(64.0f)
      .min(0.0f)
      .make_available([](bNode &node) {
        node_storage(node).resolution_mode = GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT;
      });
  b.add_input<decl::Float>("Radius")
      .default_value(0.5f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .field_on_all();
  b.add_output<decl::Geometry>("Volume").translation_context(BLT_I18NCONTEXT_ID_ID);
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "resolution_mode", UI_ITEM_NONE, IFACE_("Resolution"), ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryPointsToVolume *data = MEM_cnew<NodeGeometryPointsToVolume>(__func__);
  data->resolution_mode = GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryPointsToVolume &storage = node_storage(*node);
  bNodeSocket *voxel_size_socket = nodeFindSocket(node, SOCK_IN, "Voxel Size");
  bNodeSocket *voxel_amount_socket = nodeFindSocket(node, SOCK_IN, "Voxel Amount");
  bke::nodeSetSocketAvailability(ntree,
                                 voxel_amount_socket,
                                 storage.resolution_mode ==
                                     GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT);
  bke::nodeSetSocketAvailability(ntree,
                                 voxel_size_socket,
                                 storage.resolution_mode ==
                                     GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE);
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");
  const NodeGeometryPointsToVolume &storage = node_storage(params.node());
  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    initialize_volume_component_from_points(
        params, storage, geometry_set, openvdb::GRID_FOG_VOLUME);
  });
  params.set_output("Volume", std::move(geometry_set));
#else
  params.set_default_remaining_outputs();
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenVDB"));
#endif
}

}  // namespace blender::nodes::node_geo_points_to_volume_cc

void register_node_type_geo_points_to_volume()
{
  namespace file_ns = blender::nodes::node_geo_points_to_volume_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_POINTS_TO_VOLUME, "Points to Volume", NODE_CLASS_GEOMETRY);
  node_type_storage(&ntype,
                    "NodeGeometryPointsToVolume",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  blender::bke::node_type_size(&ntype, 170, 120, 700);
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}

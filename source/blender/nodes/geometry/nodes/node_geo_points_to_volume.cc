/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/LevelSetUtil.h>
#  include <openvdb/tools/ParticlesToLevelSet.h>
#endif

#include "BLI_bounds.hh"

#include "node_geometry_util.hh"

#include "GEO_foreach_geometry.hh"
#include "GEO_points_to_volume.hh"

#include "BKE_lib_id.hh"
#include "BKE_volume.hh"

namespace blender::nodes::node_geo_points_to_volume_cc {

#ifdef WITH_OPENVDB

static void gather_point_data_from_component(Field<float> radius_field,
                                             const GeometryComponent &component,
                                             Vector<float3> &r_positions,
                                             Vector<float> &r_radii)
{
  if (component.is_empty()) {
    return;
  }
  const VArray<float3> positions = *component.attributes()->lookup<float3>("position");

  const bke::GeometryFieldContext field_context{component, AttrDomain::Point};
  const int domain_num = component.attribute_domain_size(AttrDomain::Point);

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

/**
 * Initializes the VolumeComponent of a GeometrySet with a new Volume from points.
 * The grid class should be either openvdb::GRID_FOG_VOLUME or openvdb::GRID_LEVEL_SET.
 */
static void initialize_volume_component_from_points(GeoNodeExecParams &params,
                                                    GeometrySet &r_geometry_set)
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

  const auto resolution_mode = params.get_input<GeometryNodePointsToVolumeResolutionMode>(
      "Resolution Mode");

  float voxel_size = 0.0f;
  if (resolution_mode == GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE) {
    voxel_size = params.get_input<float>("Voxel Size");
  }
  else if (resolution_mode == GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT) {
    const float voxel_amount = params.get_input<float>("Voxel Amount");
    const float max_radius = *std::max_element(radii.begin(), radii.end());
    voxel_size = compute_voxel_size_from_amount(voxel_amount, positions, max_radius);
  }
  else {
    BLI_assert_msg(0, "Unknown volume resolution mode");
  }

  if (!BKE_volume_voxel_size_valid(float3(voxel_size))) {
    return;
  }

  Volume *volume = BKE_id_new_nomain<Volume>(nullptr);

  const float density = params.get_input<float>("Density");
  blender::geometry::fog_volume_grid_add_from_points(
      volume, "density", positions, radii, voxel_size, density);

  r_geometry_set.keep_only({GeometryComponent::Type::Volume, GeometryComponent::Type::Edit});
  r_geometry_set.replace_volume(volume);
}

#endif /* WITH_OPENVDB */

NODE_STORAGE_FUNCS(NodeGeometryPointsToVolume)

static EnumPropertyItem resolution_mode_items[] = {
    {GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT,
     "VOXEL_AMOUNT",
     0,
     CTX_N_(BLT_I18NCONTEXT_COUNTABLE, "Amount"),
     N_("Specify the approximate number of voxels along the diagonal")},
    {GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE,
     "VOXEL_SIZE",
     0,

     CTX_N_(BLT_I18NCONTEXT_COUNTABLE, "Size"),
     N_("Specify the voxel side length")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Points").is_default_link_socket().description(
      "Points which are converted to a volume");
  b.add_input<decl::Float>("Density").default_value(1.0f).min(0.0f);
  b.add_input<decl::Menu>("Resolution Mode")
      .static_items(resolution_mode_items)
      .optional_label()
      .description("How the voxel size is specified")
      .translation_context(BLT_I18NCONTEXT_COUNTABLE);
  b.add_input<decl::Float>("Voxel Size")
      .default_value(0.3f)
      .min(0.01f)
      .subtype(PROP_DISTANCE)
      .usage_by_single_menu(GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_SIZE);
  b.add_input<decl::Float>("Voxel Amount")
      .default_value(64.0f)
      .min(0.0f)
      .usage_by_single_menu(GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT);
  b.add_input<decl::Float>("Radius")
      .default_value(0.5f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .field_on_all();
  b.add_output<decl::Geometry>("Volume").translation_context(BLT_I18NCONTEXT_ID_ID);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  /* Still used for forward compatibility. */
  node->storage = MEM_callocN<NodeGeometryPointsToVolume>(__func__);
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");
  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
    initialize_volume_component_from_points(params, geometry_set);
  });
  params.set_output("Volume", std::move(geometry_set));
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodePointsToVolume", GEO_NODE_POINTS_TO_VOLUME);
  ntype.ui_name = "Points to Volume";
  ntype.ui_description = "Generate a fog volume sphere around every point";
  ntype.enum_name_legacy = "POINTS_TO_VOLUME";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryPointsToVolume", node_free_standard_storage, node_copy_standard_storage);
  bke::node_type_size(ntype, 170, 120, 700);
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_points_to_volume_cc

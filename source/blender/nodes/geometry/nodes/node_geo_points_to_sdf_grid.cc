/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"

#include "GEO_points_to_volume.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_points_to_sdf_grid_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Points").description(
      "Points whose volume is converted to a signed distance field grid");
  b.add_input<decl::Float>("Radius")
      .default_value(0.5f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .field_on_all();
  b.add_input<decl::Float>("Voxel Size").default_value(0.3f).min(0.01f).subtype(PROP_DISTANCE);
  b.add_output<decl::Float>("SDF Grid").structure_type(StructureType::Grid);
}

#ifdef WITH_OPENVDB

static void gather_positions_from_component(const GeometryComponent &component,
                                            Vector<float3> &r_positions)
{
  if (component.is_empty()) {
    return;
  }
  const VArray<float3> positions = *component.attributes()->lookup<float3>("position");
  r_positions.resize(r_positions.size() + positions.size());
  positions.materialize(r_positions.as_mutable_span().take_back(positions.size()));
}

static void gather_radii_from_component(const GeometryComponent &component,
                                        const Field<float> radius_field,
                                        Vector<float> &r_radii)
{
  if (component.is_empty()) {
    return;
  }

  const bke::GeometryFieldContext field_context{component, AttrDomain::Point};
  const int domain_num = component.attribute_domain_size(AttrDomain::Point);

  r_radii.resize(r_radii.size() + domain_num);
  fn::FieldEvaluator evaluator{field_context, domain_num};
  evaluator.add_with_destination(radius_field, r_radii.as_mutable_span().take_back(domain_num));
  evaluator.evaluate();
}

/**
 * Initializes the VolumeComponent of a GeometrySet with a new Volume from points.
 * The grid class should be either openvdb::GRID_FOG_VOLUME or openvdb::GRID_LEVEL_SET.
 */
static bke::VolumeGrid<float> points_to_grid(const GeometrySet &geometry_set,
                                             const Field<float> &radius_field,
                                             const float voxel_size)
{
  if (!BKE_volume_voxel_size_valid(float3(voxel_size))) {
    return {};
  }

  Vector<float3> positions;
  Vector<float> radii;
  for (const GeometryComponent::Type type : {GeometryComponent::Type::Mesh,
                                             GeometryComponent::Type::PointCloud,
                                             GeometryComponent::Type::Curve})
  {
    if (const GeometryComponent *component = geometry_set.get_component(type)) {
      gather_positions_from_component(*component, positions);
      gather_radii_from_component(*component, radius_field, radii);
    }
  }

  if (positions.is_empty()) {
    return {};
  }

  return geometry::points_to_sdf_grid(positions, radii, voxel_size);
}

#endif /* WITH_OPENVDB */

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  bke::VolumeGrid<float> grid = points_to_grid(params.extract_input<GeometrySet>("Points"),
                                               params.extract_input<Field<float>>("Radius"),
                                               params.extract_input<float>("Voxel Size"));
  if (grid) {
    params.set_output("SDF Grid", std::move(grid));
  }

  params.set_default_remaining_outputs();
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodePointsToSDFGrid", GEO_NODE_POINTS_TO_SDF_GRID);
  ntype.ui_name = "Points to SDF Grid";
  ntype.ui_description = "Create a signed distance volume grid from points";
  ntype.enum_name_legacy = "POINTS_TO_SDF_GRID";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_points_to_sdf_grid_cc

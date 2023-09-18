/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"

#include "BLI_array_utils.hh"

#include "DNA_pointcloud_types.h"

#include "BLI_sort.hh"
#include "BLI_task.hh"

#include "BKE_geometry_set.hh"

namespace blender::nodes::node_geo_points_to_curves_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Points")
      .supported_type(GeometryComponent::Type::PointCloud)
      .description("Points to generate curves from");
  b.add_input<decl::Int>("Curve Group ID")
      .field_on_all()
      .hide_value()
      .description(
          "A curve is created for every distinct group ID. All points with the same ID are put "
          "into the same curve");
  b.add_input<decl::Float>("Weight").field_on_all().hide_value().description(
      "Determines the order of points in each curve");

  b.add_output<decl::Geometry>("Curves").propagate_all();
}

static void grouped_sort(const OffsetIndices<int> offsets,
                         const Span<float> weights,
                         MutableSpan<int> indices)
{
  const auto comparator = [&](const int index_a, const int index_b) {
    const float weight_a = weights[index_a];
    const float weight_b = weights[index_b];
    if (UNLIKELY(weight_a == weight_b)) {
      /* Approach to make it stable. */
      return index_a < index_b;
    }
    return weight_a < weight_b;
  };

  threading::parallel_for(offsets.index_range(), 250, [&](const IndexRange range) {
    for (const int group_index : range) {
      MutableSpan<int> group = indices.slice(offsets[group_index]);
      parallel_sort(group.begin(), group.end(), comparator);
    }
  });
}

static void find_points_by_group_index(const Span<int> indices_of_curves,
                                       MutableSpan<int> r_offsets,
                                       MutableSpan<int> r_indices)
{
  offset_indices::build_reverse_offsets(indices_of_curves, r_offsets);
  Array<int> counts(r_offsets.size(), 0);

  for (const int64_t index : indices_of_curves.index_range()) {
    const int curve_index = indices_of_curves[index];
    r_indices[r_offsets[curve_index] + counts[curve_index]] = int(index);
    counts[curve_index]++;
  }
}

static int identifiers_to_indices(MutableSpan<int> r_identifiers_to_indices)
{
  const VectorSet<int> deduplicated_groups(r_identifiers_to_indices);
  threading::parallel_for(
      r_identifiers_to_indices.index_range(), 2048, [&](const IndexRange range) {
        for (int &value : r_identifiers_to_indices.slice(range)) {
          value = deduplicated_groups.index_of(value);
        }
      });
  return deduplicated_groups.size();
}

static Curves *curve_from_points(const AttributeAccessor attributes,
                                 const VArray<float> &weights_varray,
                                 const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  const int domain_size = weights_varray.size();
  Curves *curves_id = bke::curves_new_nomain_single(domain_size, CURVE_TYPE_POLY);
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  if (weights_varray.is_single()) {
    bke::copy_attributes(
        attributes, ATTR_DOMAIN_POINT, propagation_info, {}, curves.attributes_for_write());
    return curves_id;
  }
  Array<int> indices(domain_size);
  std::iota(indices.begin(), indices.end(), 0);
  const VArraySpan<float> weights(weights_varray);
  grouped_sort(OffsetIndices<int>({0, domain_size}), weights, indices);
  bke::gather_attributes(
      attributes, ATTR_DOMAIN_POINT, propagation_info, {}, indices, curves.attributes_for_write());
  return curves_id;
}

static Curves *curves_from_points(const PointCloud &points,
                                  const Field<int> &group_id_field,
                                  const Field<float> &weight_field,
                                  const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  const int domain_size = points.totpoint;

  const bke::PointCloudFieldContext context(points);
  fn::FieldEvaluator evaluator(context, domain_size);
  evaluator.add(group_id_field);
  evaluator.add(weight_field);
  evaluator.evaluate();

  const VArray<int> group_ids_varray = evaluator.get_evaluated<int>(0);
  const VArray<float> weights_varray = evaluator.get_evaluated<float>(1);

  if (group_ids_varray.is_single()) {
    return curve_from_points(points.attributes(), weights_varray, propagation_info);
  }

  Array<int> group_ids(domain_size);
  group_ids_varray.materialize(group_ids.as_mutable_span());
  const int total_curves = identifiers_to_indices(group_ids);
  if (total_curves == 1) {
    return curve_from_points(points.attributes(), weights_varray, propagation_info);
  }

  Curves *curves_id = bke::curves_new_nomain(domain_size, total_curves);
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  curves.fill_curve_types(CURVE_TYPE_POLY);
  MutableSpan<int> offset = curves.offsets_for_write();
  offset.fill(0);

  Array<int> indices(domain_size);
  find_points_by_group_index(group_ids, offset, indices.as_mutable_span());

  if (!weights_varray.is_single()) {
    const VArraySpan<float> weights(weights_varray);
    grouped_sort(OffsetIndices<int>(offset), weights, indices);
  }
  bke::gather_attributes(points.attributes(),
                         ATTR_DOMAIN_POINT,
                         propagation_info,
                         {},
                         indices,
                         curves.attributes_for_write());
  return curves_id;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");
  const Field<int> group_id_field = params.extract_input<Field<int>>("Curve Group ID");
  const Field<float> weight_field = params.extract_input<Field<float>>("Weight");

  const bke::AnonymousAttributePropagationInfo propagation_info =
      params.get_output_propagation_info("Curves");
  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    geometry_set.replace_curves(nullptr);
    if (const PointCloud *points = geometry_set.get_pointcloud()) {
      Curves *curves_id = curves_from_points(
          *points, group_id_field, weight_field, propagation_info);
      geometry_set.replace_curves(curves_id);
    }
    geometry_set.keep_only_during_modify({GeometryComponent::Type::Curve});
  });

  params.set_output("Curves", std::move(geometry_set));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_POINTS_TO_CURVES, "Points to Curves", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_points_to_curves_cc

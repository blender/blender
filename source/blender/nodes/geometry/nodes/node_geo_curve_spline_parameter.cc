/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_spline_parameter_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Factor").field_source().description(
      "For points, the portion of the spline's total length at the control point. For "
      "Splines, the factor of that spline within the entire curve");
  b.add_output<decl::Float>("Length").field_source().description(
      "For points, the distance along the control point's spline, For splines, the "
      "distance along the entire curve");
  b.add_output<decl::Int>("Index").field_source().description(
      "Each control point's index on its spline");
}

/**
 * For lengths on the curve domain, a basic interpolation from the point domain would be useless,
 * since the average parameter for each curve would just be 0.5, or close to it. Instead, the
 * value for each curve is defined as the portion of the total length of all curves at its start.
 */
static Array<float> accumulated_lengths_curve_domain(const bke::CurvesGeometry &curves)
{
  curves.ensure_evaluated_lengths();

  Array<float> lengths(curves.curves_num());
  const VArray<bool> cyclic = curves.cyclic();
  float length = 0.0f;
  for (const int i : curves.curves_range()) {
    lengths[i] = length;
    length += curves.evaluated_length_total_for_curve(i, cyclic[i]);
  }

  return lengths;
}

static Array<float> calculate_curve_parameters(const bke::CurvesGeometry &curves)
{
  const VArray<bool> cyclic = curves.cyclic();
  Array<float> lengths = accumulated_lengths_curve_domain(curves);

  const int last_index = curves.curves_num() - 1;
  const float total_length = lengths.last() + curves.evaluated_length_total_for_curve(
                                                  last_index, cyclic[last_index]);
  if (total_length > 0.0f) {
    const float factor = 1.0f / total_length;
    for (float &value : lengths) {
      value *= factor;
    }
  }
  else {
    /* It is arbitrary what to do in those rare cases when all the points are
     * in the same position. In this case we are just arbitrarily giving a valid
     * value in the range based on the curve index. */
    for (const int i : lengths.index_range()) {
      lengths[i] = i / (lengths.size() - 1.0f);
    }
  }
  return lengths;
}

/**
 * Return the length of each control point along each curve, starting at zero for the first point.
 * Importantly, this is different than the length at each evaluated point. The implementation is
 * different for every curve type:
 *  - Catmull Rom Curves: Use the resolution to find the evaluated point for each control point.
 *  - Poly Curves: Copy the evaluated lengths, but we need to add a zero to the front of the array.
 *  - Bezier Curves: Use the evaluated offsets to find the evaluated point for each control point.
 *  - NURBS Curves: Treat the control points as if they were a poly curve, because there
 *    is no obvious mapping from each control point to a specific evaluated point.
 */
static Array<float> calculate_point_lengths(
    const bke::CurvesGeometry &curves,
    const FunctionRef<void(MutableSpan<float>, float)> postprocess_lengths_for_curve)
{
  curves.ensure_evaluated_lengths();
  const OffsetIndices points_by_curve = curves.points_by_curve();
  const VArray<int8_t> types = curves.curve_types();
  const VArray<int> resolutions = curves.resolution();
  const VArray<bool> cyclic = curves.cyclic();

  Array<float> result(curves.points_num());

  threading::parallel_for(curves.curves_range(), 128, [&](IndexRange range) {
    for (const int i_curve : range) {
      const IndexRange points = points_by_curve[i_curve];
      const bool is_cyclic = cyclic[i_curve];
      const Span<float> evaluated_lengths = curves.evaluated_lengths_for_curve(i_curve, is_cyclic);
      MutableSpan<float> lengths = result.as_mutable_span().slice(points);
      lengths.first() = 0.0f;
      const float last_evaluated_length = evaluated_lengths.is_empty() ? 0.0f :
                                                                         evaluated_lengths.last();

      float total;
      switch (types[i_curve]) {
        case CURVE_TYPE_CATMULL_ROM: {
          const int resolution = resolutions[i_curve];
          for (const int i : IndexRange(points.size()).drop_back(1)) {
            lengths[i + 1] = evaluated_lengths[resolution * (i + 1) - 1];
          }
          total = last_evaluated_length;
          break;
        }
        case CURVE_TYPE_POLY:
          lengths.drop_front(1).copy_from(evaluated_lengths.take_front(lengths.size() - 1));
          total = last_evaluated_length;
          break;
        case CURVE_TYPE_BEZIER: {
          const Span<int> offsets = curves.bezier_evaluated_offsets_for_curve(i_curve);
          for (const int i : IndexRange(points.size()).drop_back(1)) {
            lengths[i + 1] = evaluated_lengths[offsets[i + 1] - 1];
          }
          total = last_evaluated_length;
          break;
        }
        case CURVE_TYPE_NURBS: {
          const Span<float3> positions = curves.positions().slice(points);
          float length = 0.0f;
          for (const int i : positions.index_range().drop_back(1)) {
            lengths[i] = length;
            length += math::distance(positions[i], positions[i + 1]);
          }
          lengths.last() = length;
          if (is_cyclic) {
            length += math::distance(positions.first(), positions.last());
          }
          total = length;
          break;
        }
      }
      postprocess_lengths_for_curve(lengths, total);
    }
  });
  return result;
}

static void convert_lengths_to_factors(MutableSpan<float> lengths, const float total_curve_length)
{
  if (total_curve_length > 0.0f) {
    const float factor = 1.0f / total_curve_length;
    for (float &value : lengths.drop_front(1)) {
      value *= factor;
    }
  }
  else if (lengths.size() == 1) {
    /* The curve is a single point. */
    lengths[0] = 0.0f;
  }
  else {
    /* It is arbitrary what to do in those rare cases when all the
     * points are in the same position. In this case we are just
     * arbitrarily giving a valid
     * value in the range based on the point index. */
    for (const int i : lengths.index_range()) {
      lengths[i] = i / (lengths.size() - 1.0f);
    }
  }
}
static Array<float> calculate_point_parameters(const bke::CurvesGeometry &curves)
{
  return calculate_point_lengths(curves, convert_lengths_to_factors);
}

class CurveParameterFieldInput final : public bke::CurvesFieldInput {
 public:
  CurveParameterFieldInput() : bke::CurvesFieldInput(CPPType::get<float>(), "Curve Parameter node")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const bke::CurvesGeometry &curves,
                                 const eAttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    switch (domain) {
      case ATTR_DOMAIN_POINT:
        return VArray<float>::ForContainer(calculate_point_parameters(curves));
      case ATTR_DOMAIN_CURVE:
        return VArray<float>::ForContainer(calculate_curve_parameters(curves));
      default:
        BLI_assert_unreachable();
        return {};
    }
  }

  uint64_t hash() const override
  {
    return 29837456298;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const CurveParameterFieldInput *>(&other) != nullptr;
  }
};

class CurveLengthParameterFieldInput final : public bke::CurvesFieldInput {
 public:
  CurveLengthParameterFieldInput()
      : bke::CurvesFieldInput(CPPType::get<float>(), "Curve Length node")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const bke::CurvesGeometry &curves,
                                 const eAttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    switch (domain) {
      case ATTR_DOMAIN_POINT:
        return VArray<float>::ForContainer(calculate_point_lengths(
            curves, [](MutableSpan<float> /*lengths*/, const float /*total*/) {}));
      case ATTR_DOMAIN_CURVE:
        return VArray<float>::ForContainer(accumulated_lengths_curve_domain(curves));
      default:
        BLI_assert_unreachable();
        return {};
    }
  }

  uint64_t hash() const override
  {
    return 345634563454;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const CurveLengthParameterFieldInput *>(&other) != nullptr;
  }
};

class IndexOnSplineFieldInput final : public bke::CurvesFieldInput {
 public:
  IndexOnSplineFieldInput() : bke::CurvesFieldInput(CPPType::get<int>(), "Spline Index")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const bke::CurvesGeometry &curves,
                                 const eAttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    if (domain != ATTR_DOMAIN_POINT) {
      return {};
    }
    Array<int> result(curves.points_num());
    const OffsetIndices points_by_curve = curves.points_by_curve();
    threading::parallel_for(curves.curves_range(), 1024, [&](IndexRange range) {
      for (const int i_curve : range) {
        MutableSpan<int> indices = result.as_mutable_span().slice(points_by_curve[i_curve]);
        for (const int i : indices.index_range()) {
          indices[i] = i;
        }
      }
    });
    return VArray<int>::ForContainer(std::move(result));
  }

  uint64_t hash() const override
  {
    return 4536246522;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const IndexOnSplineFieldInput *>(&other) != nullptr;
  }

  std::optional<eAttrDomain> preferred_domain(const CurvesGeometry & /*curves*/) const
  {
    return ATTR_DOMAIN_POINT;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float> parameter_field{std::make_shared<CurveParameterFieldInput>()};
  Field<float> length_field{std::make_shared<CurveLengthParameterFieldInput>()};
  Field<int> index_on_spline_field{std::make_shared<IndexOnSplineFieldInput>()};
  params.set_output("Factor", std::move(parameter_field));
  params.set_output("Length", std::move(length_field));
  params.set_output("Index", std::move(index_on_spline_field));
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_CURVE_SPLINE_PARAMETER, "Spline Parameter", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_spline_parameter_cc

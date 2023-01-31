/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_spline_parameter_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Factor"))
      .field_source()
      .description(
          N_("For points, the portion of the spline's total length at the control point. For "
             "Splines, the factor of that spline within the entire curve"));
  b.add_output<decl::Float>(N_("Length"))
      .field_source()
      .description(
          N_("For points, the distance along the control point's spline, For splines, the "
             "distance along the entire curve"));
  b.add_output<decl::Int>(N_("Index"))
      .field_source()
      .description(N_("Each control point's index on its spline"));
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
static Array<float> curve_length_point_domain(const bke::CurvesGeometry &curves)
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
      const Span<float> evaluated_lengths = curves.evaluated_lengths_for_curve(i_curve,
                                                                               cyclic[i_curve]);
      MutableSpan<float> lengths = result.as_mutable_span().slice(points);
      lengths.first() = 0.0f;
      switch (types[i_curve]) {
        case CURVE_TYPE_CATMULL_ROM: {
          const int resolution = resolutions[i_curve];
          for (const int i : IndexRange(points.size()).drop_back(1)) {
            lengths[i + 1] = evaluated_lengths[resolution * (i + 1) - 1];
          }
          break;
        }
        case CURVE_TYPE_POLY:
          lengths.drop_front(1).copy_from(evaluated_lengths.take_front(lengths.size() - 1));
          break;
        case CURVE_TYPE_BEZIER: {
          const Span<int> offsets = curves.bezier_evaluated_offsets_for_curve(i_curve);
          for (const int i : IndexRange(points.size()).drop_back(1)) {
            lengths[i + 1] = evaluated_lengths[offsets[i + 1] - 1];
          }
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
          break;
        }
      }
    }
  });
  return result;
}

static VArray<float> construct_curve_parameter_varray(const bke::CurvesGeometry &curves,
                                                      const IndexMask /*mask*/,
                                                      const eAttrDomain domain)
{
  const VArray<bool> cyclic = curves.cyclic();

  if (domain == ATTR_DOMAIN_POINT) {
    Array<float> result = curve_length_point_domain(curves);
    MutableSpan<float> lengths = result.as_mutable_span();
    const OffsetIndices points_by_curve = curves.points_by_curve();

    threading::parallel_for(curves.curves_range(), 1024, [&](IndexRange range) {
      for (const int i_curve : range) {
        MutableSpan<float> curve_lengths = lengths.slice(points_by_curve[i_curve]);
        const float total_length = curve_lengths.last();
        if (total_length > 0.0f) {
          const float factor = 1.0f / total_length;
          for (float &value : curve_lengths) {
            value *= factor;
          }
        }
        else if (curve_lengths.size() == 1) {
          /* The curve is a single point. */
          curve_lengths[0] = 0.0f;
        }
        else {
          /* It is arbitrary what to do in those rare cases when all the points are
           * in the same position. In this case we are just arbitrarily giving a valid
           * value in the range based on the point index. */
          for (const int i : curve_lengths.index_range()) {
            curve_lengths[i] = i / (curve_lengths.size() - 1.0f);
          }
        }
      }
    });
    return VArray<float>::ForContainer(std::move(result));
  }

  if (domain == ATTR_DOMAIN_CURVE) {
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
    return VArray<float>::ForContainer(std::move(lengths));
  }
  return {};
}

static VArray<float> construct_curve_length_parameter_varray(const bke::CurvesGeometry &curves,
                                                             const IndexMask /*mask*/,
                                                             const eAttrDomain domain)
{
  curves.ensure_evaluated_lengths();

  if (domain == ATTR_DOMAIN_POINT) {
    Array<float> lengths = curve_length_point_domain(curves);
    return VArray<float>::ForContainer(std::move(lengths));
  }

  if (domain == ATTR_DOMAIN_CURVE) {
    Array<float> lengths = accumulated_lengths_curve_domain(curves);
    return VArray<float>::ForContainer(std::move(lengths));
  }

  return {};
}

static VArray<int> construct_index_on_spline_varray(const bke::CurvesGeometry &curves,
                                                    const IndexMask /*mask*/,
                                                    const eAttrDomain domain)
{
  if (domain == ATTR_DOMAIN_POINT) {
    Array<int> result(curves.points_num());
    MutableSpan<int> span = result.as_mutable_span();
    const OffsetIndices points_by_curve = curves.points_by_curve();
    threading::parallel_for(curves.curves_range(), 1024, [&](IndexRange range) {
      for (const int i_curve : range) {
        MutableSpan<int> indices = span.slice(points_by_curve[i_curve]);
        for (const int i : indices.index_range()) {
          indices[i] = i;
        }
      }
    });
    return VArray<int>::ForContainer(std::move(result));
  }
  return {};
}

class CurveParameterFieldInput final : public bke::CurvesFieldInput {
 public:
  CurveParameterFieldInput() : bke::CurvesFieldInput(CPPType::get<float>(), "Curve Parameter node")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const bke::CurvesGeometry &curves,
                                 const eAttrDomain domain,
                                 IndexMask mask) const final
  {
    return construct_curve_parameter_varray(curves, mask, domain);
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
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
                                 IndexMask mask) const final
  {
    return construct_curve_length_parameter_varray(curves, mask, domain);
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
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
                                 IndexMask mask) const final
  {
    return construct_index_on_spline_varray(curves, mask, domain);
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
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

}  // namespace blender::nodes::node_geo_curve_spline_parameter_cc

void register_node_type_geo_curve_spline_parameter()
{
  namespace file_ns = blender::nodes::node_geo_curve_spline_parameter_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_CURVE_SPLINE_PARAMETER, "Spline Parameter", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}

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

#include "BLI_task.hh"

#include "BKE_spline.hh"

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
 * A basic interpolation from the point domain to the spline domain would be useless, since the
 * average parameter for each spline would just be 0.5, or close to it. Instead, the parameter for
 * each spline is the portion of the total length at the start of the spline.
 */
static Array<float> curve_length_spline_domain(const CurveEval &curve,
                                               const IndexMask UNUSED(mask))
{
  Span<SplinePtr> splines = curve.splines();
  float length = 0.0f;
  Array<float> lengths(splines.size());
  for (const int i : splines.index_range()) {
    lengths[i] = length;
    length += splines[i]->length();
  }
  return lengths;
}

/**
 * The parameter at each control point is the factor at the corresponding evaluated point.
 */
static void calculate_bezier_lengths(const BezierSpline &spline, MutableSpan<float> lengths)
{
  Span<int> offsets = spline.control_point_offsets();
  Span<float> lengths_eval = spline.evaluated_lengths();
  for (const int i : IndexRange(1, spline.size() - 1)) {
    lengths[i] = lengths_eval[offsets[i] - 1];
  }
}

/**
 * The parameter for poly splines is simply the evaluated lengths divided by the total length.
 */
static void calculate_poly_length(const PolySpline &spline, MutableSpan<float> lengths)
{
  Span<float> lengths_eval = spline.evaluated_lengths();
  if (spline.is_cyclic()) {
    lengths.drop_front(1).copy_from(lengths_eval.drop_back(1));
  }
  else {
    lengths.drop_front(1).copy_from(lengths_eval);
  }
}

/**
 * Since NURBS control points do not necessarily coincide with the evaluated curve's path, and
 * each control point doesn't correspond well to a specific evaluated point, the parameter at
 * each point is not well defined. So instead, treat the control points as if they were a poly
 * spline.
 */
static void calculate_nurbs_lengths(const NURBSpline &spline, MutableSpan<float> lengths)
{
  Span<float3> positions = spline.positions();
  Array<float> control_point_lengths(spline.size());
  float length = 0.0f;
  for (const int i : IndexRange(positions.size() - 1)) {
    lengths[i] = length;
    length += math::distance(positions[i], positions[i + 1]);
  }
  lengths.last() = length;
}

static Array<float> curve_length_point_domain(const CurveEval &curve)
{
  Span<SplinePtr> splines = curve.splines();
  Array<int> offsets = curve.control_point_offsets();
  const int total_size = offsets.last();
  Array<float> lengths(total_size);

  threading::parallel_for(splines.index_range(), 128, [&](IndexRange range) {
    for (const int i : range) {
      const Spline &spline = *splines[i];
      MutableSpan spline_factors{lengths.as_mutable_span().slice(offsets[i], spline.size())};
      spline_factors.first() = 0.0f;
      switch (splines[i]->type()) {
        case Spline::Type::Bezier: {
          calculate_bezier_lengths(static_cast<const BezierSpline &>(spline), spline_factors);
          break;
        }
        case Spline::Type::Poly: {
          calculate_poly_length(static_cast<const PolySpline &>(spline), spline_factors);
          break;
        }
        case Spline::Type::NURBS: {
          calculate_nurbs_lengths(static_cast<const NURBSpline &>(spline), spline_factors);
          break;
        }
      }
    }
  });
  return lengths;
}

static VArray<float> construct_curve_parameter_varray(const CurveEval &curve,
                                                      const IndexMask mask,
                                                      const AttributeDomain domain)
{
  if (domain == ATTR_DOMAIN_POINT) {
    Span<SplinePtr> splines = curve.splines();
    Array<float> values = curve_length_point_domain(curve);

    const Array<int> offsets = curve.control_point_offsets();
    for (const int i_spline : curve.splines().index_range()) {
      const Spline &spline = *splines[i_spline];
      const float spline_length = spline.length();
      const float spline_length_inv = spline_length == 0.0f ? 0.0f : 1.0f / spline_length;
      for (const int i : IndexRange(spline.size())) {
        values[offsets[i_spline] + i] *= spline_length_inv;
      }
    }
    return VArray<float>::ForContainer(std::move(values));
  }

  if (domain == ATTR_DOMAIN_CURVE) {
    Array<float> values = curve.accumulated_spline_lengths();
    const float total_length_inv = values.last() == 0.0f ? 0.0f : 1.0f / values.last();
    for (const int i : mask) {
      values[i] *= total_length_inv;
    }
    return VArray<float>::ForContainer(std::move(values));
  }
  return {};
}

static VArray<float> construct_curve_length_varray(const CurveEval &curve,
                                                   const IndexMask mask,
                                                   const AttributeDomain domain)
{
  if (domain == ATTR_DOMAIN_POINT) {
    Array<float> lengths = curve_length_point_domain(curve);
    return VArray<float>::ForContainer(std::move(lengths));
  }

  if (domain == ATTR_DOMAIN_CURVE) {
    if (curve.splines().size() == 1) {
      Array<float> lengths(1, 0.0f);
      return VArray<float>::ForContainer(std::move(lengths));
    }

    Array<float> lengths = curve_length_spline_domain(curve, mask);
    return VArray<float>::ForContainer(std::move(lengths));
  }

  return {};
}

static VArray<int> construct_index_on_spline_varray(const CurveEval &curve,
                                                    const IndexMask UNUSED(mask),
                                                    const AttributeDomain domain)
{
  if (domain == ATTR_DOMAIN_POINT) {
    Array<int> output(curve.total_control_point_size());
    int output_index = 0;
    for (int spline_index : curve.splines().index_range()) {
      for (int point_index : IndexRange(curve.splines()[spline_index]->size())) {
        output[output_index++] = point_index;
      }
    }
    return VArray<int>::ForContainer(std::move(output));
  }
  return {};
}

class CurveParameterFieldInput final : public GeometryFieldInput {
 public:
  CurveParameterFieldInput() : GeometryFieldInput(CPPType::get<float>(), "Curve Parameter node")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask mask) const final
  {
    if (component.type() == GEO_COMPONENT_TYPE_CURVE) {
      const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
      const CurveEval *curve = curve_component.get_for_read();
      if (curve) {
        return construct_curve_parameter_varray(*curve, mask, domain);
      }
    }
    return {};
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

class CurveLengthFieldInput final : public GeometryFieldInput {
 public:
  CurveLengthFieldInput() : GeometryFieldInput(CPPType::get<float>(), "Curve Length node")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask mask) const final
  {
    if (component.type() == GEO_COMPONENT_TYPE_CURVE) {
      const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
      const CurveEval *curve = curve_component.get_for_read();
      if (curve) {
        return construct_curve_length_varray(*curve, mask, domain);
      }
    }
    return {};
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 345634563454;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const CurveLengthFieldInput *>(&other) != nullptr;
  }
};

class IndexOnSplineFieldInput final : public GeometryFieldInput {
 public:
  IndexOnSplineFieldInput() : GeometryFieldInput(CPPType::get<int>(), "Spline Index")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask mask) const final
  {
    if (component.type() == GEO_COMPONENT_TYPE_CURVE) {
      const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
      const CurveEval *curve = curve_component.get_for_read();
      if (curve) {
        return construct_index_on_spline_varray(*curve, mask, domain);
      }
    }
    return {};
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
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float> parameter_field{std::make_shared<CurveParameterFieldInput>()};
  Field<float> length_field{std::make_shared<CurveLengthFieldInput>()};
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

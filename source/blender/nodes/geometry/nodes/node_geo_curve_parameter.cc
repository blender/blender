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

namespace blender::nodes {

static void geo_node_curve_parameter_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Factor").field_source();
}

/**
 * A basic interpolation from the point domain to the spline domain would be useless, since the
 * average parameter for each spline would just be 0.5, or close to it. Instead, the parameter for
 * each spline is the portion of the total length at the start of the spline.
 */
static Array<float> curve_parameter_spline_domain(const CurveEval &curve, const IndexMask mask)
{
  Span<SplinePtr> splines = curve.splines();
  float length = 0.0f;
  Array<float> parameters(splines.size());
  for (const int i : splines.index_range()) {
    parameters[i] = length;
    length += splines[i]->length();
  }
  const float total_length_inverse = length == 0.0f ? 0.0f : 1.0f / length;
  mask.foreach_index([&](const int64_t i) { parameters[i] *= total_length_inverse; });

  return parameters;
}

/**
 * The parameter at each control point is the factor at the corresponding evaluated point.
 */
static void calculate_bezier_parameters(const BezierSpline &spline, MutableSpan<float> parameters)
{
  Span<int> offsets = spline.control_point_offsets();
  Span<float> lengths = spline.evaluated_lengths();
  const float total_length = spline.length();
  const float total_length_inverse = total_length == 0.0f ? 0.0f : 1.0f / total_length;

  for (const int i : IndexRange(1, spline.size() - 1)) {
    parameters[i] = lengths[offsets[i] - 1] * total_length_inverse;
  }
}

/**
 * The parameter for poly splines is simply the evaluated lengths divided by the total length.
 */
static void calculate_poly_parameters(const PolySpline &spline, MutableSpan<float> parameters)
{
  Span<float> lengths = spline.evaluated_lengths();
  const float total_length = spline.length();
  const float total_length_inverse = total_length == 0.0f ? 0.0f : 1.0f / total_length;

  for (const int i : IndexRange(1, spline.size() - 1)) {
    parameters[i] = lengths[i - 1] * total_length_inverse;
  }
}

/**
 * Since NURBS control points do not necessarily coincide with the evaluated curve's path, and
 * each control point doesn't correspond well to a specific evaluated point, the parameter at
 * each point is not well defined. So instead, treat the control points as if they were a poly
 * spline.
 */
static void calculate_nurbs_parameters(const NURBSpline &spline, MutableSpan<float> parameters)
{
  Span<float3> positions = spline.positions();
  Array<float> control_point_lengths(spline.size());

  float length = 0.0f;
  for (const int i : IndexRange(positions.size() - 1)) {
    parameters[i] = length;
    length += float3::distance(positions[i], positions[i + 1]);
  }

  const float total_length_inverse = length == 0.0f ? 0.0f : 1.0f / length;
  for (float &parameter : parameters) {
    parameter *= total_length_inverse;
  }
}

static Array<float> curve_parameter_point_domain(const CurveEval &curve)
{
  Span<SplinePtr> splines = curve.splines();
  Array<int> offsets = curve.control_point_offsets();
  const int total_size = offsets.last();
  Array<float> parameters(total_size);

  threading::parallel_for(splines.index_range(), 128, [&](IndexRange range) {
    for (const int i : range) {
      const Spline &spline = *splines[i];
      MutableSpan spline_factors{parameters.as_mutable_span().slice(offsets[i], spline.size())};
      spline_factors.first() = 0.0f;
      switch (splines[i]->type()) {
        case Spline::Type::Bezier: {
          calculate_bezier_parameters(static_cast<const BezierSpline &>(spline), spline_factors);
          break;
        }
        case Spline::Type::Poly: {
          calculate_poly_parameters(static_cast<const PolySpline &>(spline), spline_factors);
          break;
        }
        case Spline::Type::NURBS: {
          calculate_nurbs_parameters(static_cast<const NURBSpline &>(spline), spline_factors);
          break;
        }
      }
    }
  });
  return parameters;
}

static const GVArray *construct_curve_parameter_gvarray(const CurveEval &curve,
                                                        const IndexMask mask,
                                                        const AttributeDomain domain,
                                                        ResourceScope &scope)
{
  if (domain == ATTR_DOMAIN_POINT) {
    Array<float> parameters = curve_parameter_point_domain(curve);
    return &scope.construct<fn::GVArray_For_ArrayContainer<Array<float>>>(std::move(parameters));
  }

  if (domain == ATTR_DOMAIN_CURVE) {
    Array<float> parameters = curve_parameter_spline_domain(curve, mask);
    return &scope.construct<fn::GVArray_For_ArrayContainer<Array<float>>>(std::move(parameters));
  }

  return nullptr;
}

class CurveParameterFieldInput final : public fn::FieldInput {
 public:
  CurveParameterFieldInput() : fn::FieldInput(CPPType::get<float>(), "Curve Parameter node")
  {
    category_ = Category::Generated;
  }

  const GVArray *get_varray_for_context(const fn::FieldContext &context,
                                        IndexMask mask,
                                        ResourceScope &scope) const final
  {
    if (const GeometryComponentFieldContext *geometry_context =
            dynamic_cast<const GeometryComponentFieldContext *>(&context)) {

      const GeometryComponent &component = geometry_context->geometry_component();
      const AttributeDomain domain = geometry_context->domain();

      if (component.type() == GEO_COMPONENT_TYPE_CURVE) {
        const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
        const CurveEval *curve = curve_component.get_for_read();
        if (curve) {
          return construct_curve_parameter_gvarray(*curve, mask, domain, scope);
        }
      }
    }
    return nullptr;
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

static void geo_node_curve_parameter_exec(GeoNodeExecParams params)
{
  Field<float> parameter_field{std::make_shared<CurveParameterFieldInput>()};
  params.set_output("Factor", std::move(parameter_field));
}

}  // namespace blender::nodes

void register_node_type_geo_curve_parameter()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_CURVE_PARAMETER, "Curve Parameter", NODE_CLASS_INPUT, 0);
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_parameter_exec;
  ntype.declare = blender::nodes::geo_node_curve_parameter_declare;
  nodeRegisterType(&ntype);
}

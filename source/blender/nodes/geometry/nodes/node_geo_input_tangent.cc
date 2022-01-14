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

namespace blender::nodes::node_geo_input_tangent_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>(N_("Tangent")).field_source();
}

static void calculate_bezier_tangents(const BezierSpline &spline, MutableSpan<float3> tangents)
{
  Span<int> offsets = spline.control_point_offsets();
  Span<float3> evaluated_tangents = spline.evaluated_tangents();
  for (const int i : IndexRange(spline.size())) {
    tangents[i] = evaluated_tangents[offsets[i]];
  }
}

static void calculate_poly_tangents(const PolySpline &spline, MutableSpan<float3> tangents)
{
  tangents.copy_from(spline.evaluated_tangents());
}

/**
 * Because NURBS control points are not necessarily on the path, the tangent at the control points
 * is not well defined, so create a temporary poly spline to find the tangents. This requires extra
 * copying currently, but may be more efficient in the future if attributes have some form of CoW.
 */
static void calculate_nurbs_tangents(const NURBSpline &spline, MutableSpan<float3> tangents)
{
  PolySpline poly_spline;
  poly_spline.resize(spline.size());
  poly_spline.positions().copy_from(spline.positions());
  tangents.copy_from(poly_spline.evaluated_tangents());
}

static Array<float3> curve_tangent_point_domain(const CurveEval &curve)
{
  Span<SplinePtr> splines = curve.splines();
  Array<int> offsets = curve.control_point_offsets();
  const int total_size = offsets.last();
  Array<float3> tangents(total_size);

  threading::parallel_for(splines.index_range(), 128, [&](IndexRange range) {
    for (const int i : range) {
      const Spline &spline = *splines[i];
      MutableSpan spline_tangents{tangents.as_mutable_span().slice(offsets[i], spline.size())};
      switch (splines[i]->type()) {
        case Spline::Type::Bezier: {
          calculate_bezier_tangents(static_cast<const BezierSpline &>(spline), spline_tangents);
          break;
        }
        case Spline::Type::Poly: {
          calculate_poly_tangents(static_cast<const PolySpline &>(spline), spline_tangents);
          break;
        }
        case Spline::Type::NURBS: {
          calculate_nurbs_tangents(static_cast<const NURBSpline &>(spline), spline_tangents);
          break;
        }
      }
    }
  });
  return tangents;
}

static VArray<float3> construct_curve_tangent_gvarray(const CurveComponent &component,
                                                      const AttributeDomain domain)
{
  const CurveEval *curve = component.get_for_read();
  if (curve == nullptr) {
    return nullptr;
  }

  if (domain == ATTR_DOMAIN_POINT) {
    const Span<SplinePtr> splines = curve->splines();

    /* Use a reference to evaluated tangents if possible to avoid an allocation and a copy.
     * This is only possible when there is only one poly spline. */
    if (splines.size() == 1 && splines.first()->type() == Spline::Type::Poly) {
      const PolySpline &spline = static_cast<PolySpline &>(*splines.first());
      return VArray<float3>::ForSpan(spline.evaluated_tangents());
    }

    Array<float3> tangents = curve_tangent_point_domain(*curve);
    return VArray<float3>::ForContainer(std::move(tangents));
  }

  if (domain == ATTR_DOMAIN_CURVE) {
    Array<float3> point_tangents = curve_tangent_point_domain(*curve);
    return component.attribute_try_adapt_domain<float3>(
        VArray<float3>::ForContainer(std::move(point_tangents)),
        ATTR_DOMAIN_POINT,
        ATTR_DOMAIN_CURVE);
  }

  return nullptr;
}

class TangentFieldInput final : public GeometryFieldInput {
 public:
  TangentFieldInput() : GeometryFieldInput(CPPType::get<float3>(), "Tangent node")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    if (component.type() == GEO_COMPONENT_TYPE_CURVE) {
      const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
      return construct_curve_tangent_gvarray(curve_component, domain);
    }
    return {};
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 91827364589;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const TangentFieldInput *>(&other) != nullptr;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float3> tangent_field{std::make_shared<TangentFieldInput>()};
  params.set_output("Tangent", std::move(tangent_field));
}

}  // namespace blender::nodes::node_geo_input_tangent_cc

void register_node_type_geo_input_tangent()
{
  namespace file_ns = blender::nodes::node_geo_input_tangent_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_TANGENT, "Curve Tangent", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}

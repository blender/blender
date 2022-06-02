/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_tangent_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>(N_("Tangent")).field_source();
}

static Array<float3> curve_tangent_point_domain(const bke::CurvesGeometry &curves)
{
  const VArray<int8_t> types = curves.curve_types();
  const VArray<int> resolutions = curves.resolution();
  const VArray<bool> cyclic = curves.cyclic();
  const Span<float3> positions = curves.positions();

  const Span<float3> evaluated_tangents = curves.evaluated_tangents();

  Array<float3> results(curves.points_num());

  threading::parallel_for(curves.curves_range(), 128, [&](IndexRange range) {
    for (const int i_curve : range) {
      const IndexRange points = curves.points_for_curve(i_curve);
      const IndexRange evaluated_points = curves.evaluated_points_for_curve(i_curve);

      MutableSpan<float3> curve_tangents = results.as_mutable_span().slice(points);

      switch (types[i_curve]) {
        case CURVE_TYPE_CATMULL_ROM: {
          Span<float3> tangents = evaluated_tangents.slice(evaluated_points);
          const int resolution = resolutions[i_curve];
          for (const int i : IndexRange(points.size())) {
            curve_tangents[i] = tangents[resolution * i];
          }
          break;
        }
        case CURVE_TYPE_POLY:
          curve_tangents.copy_from(evaluated_tangents.slice(evaluated_points));
          break;
        case CURVE_TYPE_BEZIER: {
          Span<float3> tangents = evaluated_tangents.slice(evaluated_points);
          curve_tangents.first() = tangents.first();
          const Span<int> offsets = curves.bezier_evaluated_offsets_for_curve(i_curve);
          for (const int i : IndexRange(points.size()).drop_front(1)) {
            curve_tangents[i] = tangents[offsets[i - 1]];
          }
          break;
        }
        case CURVE_TYPE_NURBS: {
          const Span<float3> curve_positions = positions.slice(points);
          bke::curves::poly::calculate_tangents(curve_positions, cyclic[i_curve], curve_tangents);
          break;
        }
      }
    }
  });
  return results;
}

static VArray<float3> construct_curve_tangent_gvarray(const CurveComponent &component,
                                                      const eAttrDomain domain)
{
  if (!component.has_curves()) {
    return {};
  }

  const Curves &curves_id = *component.get_for_read();
  const bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id.geometry);

  const VArray<int8_t> types = curves.curve_types();
  if (curves.is_single_type(CURVE_TYPE_POLY)) {
    return component.attribute_try_adapt_domain<float3>(
        VArray<float3>::ForSpan(curves.evaluated_tangents()), ATTR_DOMAIN_POINT, domain);
  }

  Array<float3> tangents = curve_tangent_point_domain(curves);

  if (domain == ATTR_DOMAIN_POINT) {
    return VArray<float3>::ForContainer(std::move(tangents));
  }

  if (domain == ATTR_DOMAIN_CURVE) {
    return component.attribute_try_adapt_domain<float3>(
        VArray<float3>::ForContainer(std::move(tangents)), ATTR_DOMAIN_POINT, ATTR_DOMAIN_CURVE);
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
                                 const eAttrDomain domain,
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

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

#include "node_geometry_util.hh"

#include "BKE_spline.hh"

namespace blender::nodes::node_geo_input_spline_length_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>(N_("Length")).field_source();
  b.add_output<decl::Int>(N_("Point Count")).field_source();
}

/* --------------------------------------------------------------------
 * Spline Length
 */

static VArray<float> construct_spline_length_gvarray(const CurveComponent &component,
                                                     const AttributeDomain domain)
{
  const CurveEval *curve = component.get_for_read();
  if (curve == nullptr) {
    return {};
  }

  Span<SplinePtr> splines = curve->splines();
  auto length_fn = [splines](int i) { return splines[i]->length(); };

  if (domain == ATTR_DOMAIN_CURVE) {
    return VArray<float>::ForFunc(splines.size(), length_fn);
  }
  if (domain == ATTR_DOMAIN_POINT) {
    VArray<float> length = VArray<float>::ForFunc(splines.size(), length_fn);
    return component.attribute_try_adapt_domain<float>(
        std::move(length), ATTR_DOMAIN_CURVE, ATTR_DOMAIN_POINT);
  }

  return {};
}

class SplineLengthFieldInput final : public GeometryFieldInput {
 public:
  SplineLengthFieldInput() : GeometryFieldInput(CPPType::get<float>(), "Spline Length node")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    if (component.type() == GEO_COMPONENT_TYPE_CURVE) {
      const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
      return construct_spline_length_gvarray(curve_component, domain);
    }
    return {};
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 3549623580;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const SplineLengthFieldInput *>(&other) != nullptr;
  }
};

/* --------------------------------------------------------------------
 * Spline Count
 */

static VArray<int> construct_spline_count_gvarray(const CurveComponent &component,
                                                  const AttributeDomain domain)
{
  const CurveEval *curve = component.get_for_read();
  if (curve == nullptr) {
    return {};
  }

  Span<SplinePtr> splines = curve->splines();
  auto count_fn = [splines](int i) { return splines[i]->size(); };

  if (domain == ATTR_DOMAIN_CURVE) {
    return VArray<int>::ForFunc(splines.size(), count_fn);
  }
  if (domain == ATTR_DOMAIN_POINT) {
    VArray<int> count = VArray<int>::ForFunc(splines.size(), count_fn);
    return component.attribute_try_adapt_domain<int>(
        std::move(count), ATTR_DOMAIN_CURVE, ATTR_DOMAIN_POINT);
  }

  return {};
}

class SplineCountFieldInput final : public GeometryFieldInput {
 public:
  SplineCountFieldInput() : GeometryFieldInput(CPPType::get<int>(), "Spline Point Count")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const AttributeDomain domain,
                                 IndexMask UNUSED(mask)) const final
  {
    if (component.type() == GEO_COMPONENT_TYPE_CURVE) {
      const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
      return construct_spline_count_gvarray(curve_component, domain);
    }
    return {};
  }

  uint64_t hash() const override
  {
    /* Some random constant hash. */
    return 456364322625;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const SplineCountFieldInput *>(&other) != nullptr;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float> spline_length_field{std::make_shared<SplineLengthFieldInput>()};
  Field<int> spline_count_field{std::make_shared<SplineCountFieldInput>()};

  params.set_output("Length", std::move(spline_length_field));
  params.set_output("Point Count", std::move(spline_count_field));
}

}  // namespace blender::nodes::node_geo_input_spline_length_cc

void register_node_type_geo_input_spline_length()
{
  namespace file_ns = blender::nodes::node_geo_input_spline_length_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_INPUT_SPLINE_LENGTH, "Spline Length", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}

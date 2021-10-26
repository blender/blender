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

namespace blender::nodes {

static void geo_node_input_spline_length_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Length").field_source();
}

static const GVArray *construct_spline_length_gvarray(const CurveComponent &component,
                                                      const AttributeDomain domain,
                                                      ResourceScope &scope)
{
  const CurveEval *curve = component.get_for_read();
  if (curve == nullptr) {
    return nullptr;
  }

  Span<SplinePtr> splines = curve->splines();
  auto length_fn = [splines](int i) { return splines[i]->length(); };

  if (domain == ATTR_DOMAIN_CURVE) {
    return &scope.construct<
        fn::GVArray_For_EmbeddedVArray<float, VArray_For_Func<float, decltype(length_fn)>>>(
        splines.size(), splines.size(), length_fn);
  }
  if (domain == ATTR_DOMAIN_POINT) {
    GVArrayPtr length = std::make_unique<
        fn::GVArray_For_EmbeddedVArray<float, VArray_For_Func<float, decltype(length_fn)>>>(
        splines.size(), splines.size(), length_fn);
    return scope
        .add_value(component.attribute_try_adapt_domain(
            std::move(length), ATTR_DOMAIN_CURVE, ATTR_DOMAIN_POINT))
        .get();
  }

  return nullptr;
}

class SplineLengthFieldInput final : public fn::FieldInput {
 public:
  SplineLengthFieldInput() : fn::FieldInput(CPPType::get<float>(), "Spline Length node")
  {
    category_ = Category::Generated;
  }

  const GVArray *get_varray_for_context(const fn::FieldContext &context,
                                        IndexMask UNUSED(mask),
                                        ResourceScope &scope) const final
  {
    if (const GeometryComponentFieldContext *geometry_context =
            dynamic_cast<const GeometryComponentFieldContext *>(&context)) {

      const GeometryComponent &component = geometry_context->geometry_component();
      const AttributeDomain domain = geometry_context->domain();
      if (component.type() == GEO_COMPONENT_TYPE_CURVE) {
        const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
        return construct_spline_length_gvarray(curve_component, domain, scope);
      }
    }
    return nullptr;
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

static void geo_node_input_spline_length_exec(GeoNodeExecParams params)
{
  Field<float> length_field{std::make_shared<SplineLengthFieldInput>()};
  params.set_output("Length", std::move(length_field));
}

}  // namespace blender::nodes

void register_node_type_geo_input_spline_length()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_SPLINE_LENGTH, "Spline Length", NODE_CLASS_INPUT, 0);
  ntype.geometry_node_execute = blender::nodes::geo_node_input_spline_length_exec;
  ntype.declare = blender::nodes::geo_node_input_spline_length_declare;
  nodeRegisterType(&ntype);
}

/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BKE_curves.hh"

namespace blender::nodes::node_geo_input_spline_length_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Length").field_source();
  b.add_output<decl::Int>("Point Count").field_source();
}

/* --------------------------------------------------------------------
 * Spline Count
 */

static VArray<int> construct_curve_point_count_gvarray(const bke::CurvesGeometry &curves,
                                                       const eAttrDomain domain)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  auto count_fn = [points_by_curve](int64_t i) { return points_by_curve[i].size(); };

  if (domain == ATTR_DOMAIN_CURVE) {
    return VArray<int>::ForFunc(curves.curves_num(), count_fn);
  }
  if (domain == ATTR_DOMAIN_POINT) {
    VArray<int> count = VArray<int>::ForFunc(curves.curves_num(), count_fn);
    return curves.adapt_domain<int>(std::move(count), ATTR_DOMAIN_CURVE, ATTR_DOMAIN_POINT);
  }

  return {};
}

class SplineCountFieldInput final : public bke::CurvesFieldInput {
 public:
  SplineCountFieldInput() : bke::CurvesFieldInput(CPPType::get<int>(), "Spline Point Count")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const bke::CurvesGeometry &curves,
                                 const eAttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    return construct_curve_point_count_gvarray(curves, domain);
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

  std::optional<eAttrDomain> preferred_domain(const bke::CurvesGeometry & /*curves*/) const final
  {
    return ATTR_DOMAIN_CURVE;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float> spline_length_field{std::make_shared<bke::CurveLengthFieldInput>()};
  Field<int> spline_count_field{std::make_shared<SplineCountFieldInput>()};

  params.set_output("Length", std::move(spline_length_field));
  params.set_output("Point Count", std::move(spline_count_field));
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_INPUT_SPLINE_LENGTH, "Spline Length", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_spline_length_cc

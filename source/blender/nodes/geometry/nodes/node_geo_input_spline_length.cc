/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BKE_curves.hh"

namespace blender::nodes::node_geo_input_spline_length_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Float>("Length"_ustr).field_source();
  b.add_output<decl::Int>("Point Count"_ustr).field_source();
}

/* --------------------------------------------------------------------
 * Spline Count
 */

static VArray<int> construct_curve_point_count_gvarray(const bke::CurvesGeometry &curves,
                                                       const AttrDomain domain)
{
  const OffsetIndices points_by_curve = curves.points_by_curve();
  auto count_fn = [points_by_curve](int64_t i) { return points_by_curve[i].size(); };

  if (domain == AttrDomain::Curve) {
    return VArray<int>::from_func(curves.curves_num(), count_fn);
  }
  if (domain == AttrDomain::Point) {
    VArray<int> count = VArray<int>::from_func(curves.curves_num(), count_fn);
    return curves.adapt_domain<int>(std::move(count), AttrDomain::Curve, AttrDomain::Point);
  }

  return {};
}

class SplineCountFieldInput final : public bke::CurvesFieldInput {
 public:
  SplineCountFieldInput() : bke::CurvesFieldInput(CPPType::get<int>(), "Spline Point Count") {}

  GVArray get_varray_for_context(const bke::CurvesGeometry &curves,
                                 const AttrDomain domain,
                                 const IndexMask & /*mask*/) const final
  {
    return construct_curve_point_count_gvarray(curves, domain);
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep & /*deep_hash_cache*/) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
  }

  std::optional<AttrDomain> preferred_domain(const bke::CurvesGeometry & /*curves*/) const final
  {
    return AttrDomain::Curve;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  params.set_output("Length"_ustr, Field<float>::from_input<bke::CurveLengthFieldInput>());
  params.set_output("Point Count"_ustr, Field<int>::from_input<SplineCountFieldInput>());
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeSplineLength"_ustr, GEO_NODE_INPUT_SPLINE_LENGTH);
  ntype.ui_name = "Spline Length";
  ntype.ui_description =
      "Retrieve the total length of each spline, as a distance or as a number of points";
  ntype.enum_name_legacy = "SPLINE_LENGTH";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_spline_length_cc

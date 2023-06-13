/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_topology_curve_of_point_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Point Index")
      .implicit_field(implicit_field_inputs::index)
      .description("The control point to retrieve data from");
  b.add_output<decl::Int>("Curve Index")
      .field_source_reference_all()
      .description("The curve the control point is part of");
  b.add_output<decl::Int>("Index in Curve")
      .field_source_reference_all()
      .description("How far along the control point is along its curve");
}

class CurveOfPointInput final : public bke::CurvesFieldInput {
 public:
  CurveOfPointInput() : bke::CurvesFieldInput(CPPType::get<int>(), "Point Curve Index")
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
    return VArray<int>::ForContainer(curves.point_to_curve_map());
  }

  uint64_t hash() const override
  {
    return 413209687345908697;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (dynamic_cast<const CurveOfPointInput *>(&other)) {
      return true;
    }
    return false;
  }

  std::optional<eAttrDomain> preferred_domain(const bke::CurvesGeometry & /*curves*/) const final
  {
    return ATTR_DOMAIN_POINT;
  }
};

class PointIndexInCurveInput final : public bke::CurvesFieldInput {
 public:
  PointIndexInCurveInput() : bke::CurvesFieldInput(CPPType::get<int>(), "Point Index in Curve")
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
    const Span<int> offsets = curves.offsets();
    Array<int> point_to_curve_map = curves.point_to_curve_map();
    return VArray<int>::ForFunc(
        curves.points_num(),
        [offsets, point_to_curve_map = std::move(point_to_curve_map)](const int point_i) {
          const int curve_i = point_to_curve_map[point_i];
          return point_i - offsets[curve_i];
        });
  }

  uint64_t hash() const final
  {
    return 9834765987345677;
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    if (dynamic_cast<const PointIndexInCurveInput *>(&other)) {
      return true;
    }
    return false;
  }

  std::optional<eAttrDomain> preferred_domain(const bke::CurvesGeometry & /*curves*/)
  {
    return ATTR_DOMAIN_POINT;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<int> point_index = params.extract_input<Field<int>>("Point Index");
  if (params.output_is_required("Curve Index")) {
    params.set_output(
        "Curve Index",
        Field<int>(std::make_shared<EvaluateAtIndexInput>(
            point_index, Field<int>(std::make_shared<CurveOfPointInput>()), ATTR_DOMAIN_POINT)));
  }
  if (params.output_is_required("Index in Curve")) {
    params.set_output("Index in Curve",
                      Field<int>(std::make_shared<EvaluateAtIndexInput>(
                          point_index,
                          Field<int>(std::make_shared<PointIndexInCurveInput>()),
                          ATTR_DOMAIN_POINT)));
  }
}

}  // namespace blender::nodes::node_geo_curve_topology_curve_of_point_cc

void register_node_type_geo_curve_topology_curve_of_point()
{
  namespace file_ns = blender::nodes::node_geo_curve_topology_curve_of_point_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_CURVE_TOPOLOGY_CURVE_OF_POINT, "Curve of Point", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}

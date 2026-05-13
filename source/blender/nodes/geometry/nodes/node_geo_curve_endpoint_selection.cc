/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_endpoint_selection_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Start Size"_ustr)
      .min(0)
      .default_value(1)
      .supports_field()
      .description("The amount of points to select from the start of each spline");
  b.add_input<decl::Int>("End Size"_ustr)
      .min(0)
      .default_value(1)
      .supports_field()
      .description("The amount of points to select from the end of each spline");
  b.add_output<decl::Bool>("Selection"_ustr)
      .field_source_reference_all()
      .description("The selection from the start and end of the splines based on the input sizes");
}

class EndpointFieldInput final : public bke::GeometryFieldInput {
  Field<int> start_size_;
  Field<int> end_size_;

 public:
  EndpointFieldInput(Field<int> start_size, Field<int> end_size)
      : bke::GeometryFieldInput(CPPType::get<bool>(), "Endpoint Selection node"),
        start_size_(start_size),
        end_size_(end_size)
  {
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask & /*mask*/) const final
  {
    if (context.domain() != AttrDomain::Point) {
      return {};
    }
    const bke::CurvesGeometry *curves_ptr = context.curves_or_strokes();
    if (!curves_ptr) {
      return {};
    }
    const bke::CurvesGeometry &curves = *curves_ptr;
    if (curves.is_empty()) {
      return {};
    }

    const bke::GeometryFieldContext sub_context{context, AttrDomain::Curve};
    fn::FieldEvaluator evaluator{sub_context, curves.curves_num()};
    evaluator.add(start_size_);
    evaluator.add(end_size_);
    evaluator.evaluate();
    const VArray<int> start_size = evaluator.get_evaluated<int>(0);
    const VArray<int> end_size = evaluator.get_evaluated<int>(1);

    Array<bool> selection(curves.points_num(), false);
    MutableSpan<bool> selection_span = selection.as_mutable_span();
    const OffsetIndices points_by_curve = curves.points_by_curve();
    devirtualize_varray2(start_size, end_size, [&](const auto start_size, const auto end_size) {
      threading::parallel_for(curves.curves_range(), 1024, [&](IndexRange curves_range) {
        for (const int i : curves_range) {
          const IndexRange points = points_by_curve[i];
          const int start = std::max(start_size[i], 0);
          const int end = std::max(end_size[i], 0);

          selection_span.slice(points.take_front(start)).fill(true);
          selection_span.slice(points.take_back(end)).fill(true);
        }
      });
    });

    return VArray<bool>::from_container(std::move(selection));
  };

  void foreach_recursive_field(FunctionRef<void(const GField &)> fn) const final
  {
    fn(start_size_);
    fn(end_size_);
  }

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const final
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
    hash.add(deep_hash_cache.ensure(start_size_));
    hash.add(deep_hash_cache.ensure(end_size_));
  }

  std::optional<AttrDomain> preferred_domain(const GeometryComponent & /*component*/) const final
  {
    return AttrDomain::Point;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<int> start_size = params.extract_input<Field<int>>("Start Size"_ustr);
  Field<int> end_size = params.extract_input<Field<int>>("End Size"_ustr);
  Field<bool> selection_field = Field<bool>::from_input<EndpointFieldInput>(start_size, end_size);
  params.set_output("Selection"_ustr, std::move(selection_field));
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, "GeometryNodeCurveEndpointSelection"_ustr, GEO_NODE_CURVE_ENDPOINT_SELECTION);
  ntype.ui_name = "Endpoint Selection";
  ntype.ui_description = "Provide a selection for an arbitrary number of endpoints in each spline";
  ntype.enum_name_legacy = "CURVE_ENDPOINT_SELECTION";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_endpoint_selection_cc

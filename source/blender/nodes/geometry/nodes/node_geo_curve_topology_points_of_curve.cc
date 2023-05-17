/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "BLI_task.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_topology_points_of_curve_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Curve Index")
      .implicit_field(implicit_field_inputs::index)
      .description("The curve to retrieve data from. Defaults to the curve from the context");
  b.add_input<decl::Float>("Weights").supports_field().hide_value().description(
      "Values used to sort the curve's points. Uses indices by default");
  b.add_input<decl::Int>("Sort Index")
      .min(0)
      .supports_field()
      .description("Which of the sorted points to output");
  b.add_output<decl::Int>("Point Index")
      .field_source_reference_all()
      .description("A point of the curve, chosen by the sort index");
  b.add_output<decl::Int>("Total").field_source().reference_pass({0}).description(
      "The number of points in the curve");
}

class PointsOfCurveInput final : public bke::CurvesFieldInput {
  const Field<int> curve_index_;
  const Field<int> sort_index_;
  const Field<float> sort_weight_;

 public:
  PointsOfCurveInput(Field<int> curve_index, Field<int> sort_index, Field<float> sort_weight)
      : bke::CurvesFieldInput(CPPType::get<int>(), "Point of Curve"),
        curve_index_(std::move(curve_index)),
        sort_index_(std::move(sort_index)),
        sort_weight_(std::move(sort_weight))
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const bke::CurvesGeometry &curves,
                                 const eAttrDomain domain,
                                 const IndexMask mask) const final
  {
    const OffsetIndices points_by_curve = curves.points_by_curve();

    const bke::CurvesFieldContext context{curves, domain};
    fn::FieldEvaluator evaluator{context, &mask};
    evaluator.add(curve_index_);
    evaluator.add(sort_index_);
    evaluator.evaluate();
    const VArray<int> curve_indices = evaluator.get_evaluated<int>(0);
    const VArray<int> indices_in_sort = evaluator.get_evaluated<int>(1);

    const bke::CurvesFieldContext point_context{curves, ATTR_DOMAIN_POINT};
    fn::FieldEvaluator point_evaluator{point_context, curves.points_num()};
    point_evaluator.add(sort_weight_);
    point_evaluator.evaluate();
    const VArray<float> all_sort_weights = point_evaluator.get_evaluated<float>(0);
    const bool use_sorting = !all_sort_weights.is_single();

    Array<int> point_of_curve(mask.min_array_size());
    threading::parallel_for(mask.index_range(), 256, [&](const IndexRange range) {
      /* Reuse arrays to avoid allocation. */
      Array<float> sort_weights;
      Array<int> sort_indices;

      for (const int selection_i : mask.slice(range)) {
        const int curve_i = curve_indices[selection_i];
        const int index_in_sort = indices_in_sort[selection_i];
        if (!curves.curves_range().contains(curve_i)) {
          point_of_curve[selection_i] = 0;
          continue;
        }
        const IndexRange points = points_by_curve[curve_i];

        const int index_in_sort_wrapped = mod_i(index_in_sort, points.size());
        if (use_sorting) {
          /* Retrieve the weights for each point. */
          sort_weights.reinitialize(points.size());
          all_sort_weights.materialize_compressed(IndexMask(points),
                                                  sort_weights.as_mutable_span());

          /* Sort a separate array of compressed indices corresponding to the compressed weights.
           * This allows using `materialize_compressed` to avoid virtual function call overhead
           * when accessing values in the sort weights. However, it means a separate array of
           * indices within the compressed array is necessary for sorting. */
          sort_indices.reinitialize(points.size());
          std::iota(sort_indices.begin(), sort_indices.end(), 0);
          std::stable_sort(sort_indices.begin(), sort_indices.end(), [&](int a, int b) {
            return sort_weights[a] < sort_weights[b];
          });
          point_of_curve[selection_i] = points[sort_indices[index_in_sort_wrapped]];
        }
        else {
          point_of_curve[selection_i] = points[index_in_sort_wrapped];
        }
      }
    });

    return VArray<int>::ForContainer(std::move(point_of_curve));
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    curve_index_.node().for_each_field_input_recursive(fn);
    sort_index_.node().for_each_field_input_recursive(fn);
    sort_weight_.node().for_each_field_input_recursive(fn);
  }

  uint64_t hash() const override
  {
    return 26978695677882;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const auto *typed = dynamic_cast<const PointsOfCurveInput *>(&other)) {
      return typed->curve_index_ == curve_index_ && typed->sort_index_ == sort_index_ &&
             typed->sort_weight_ == sort_weight_;
    }
    return false;
  }

  std::optional<eAttrDomain> preferred_domain(const bke::CurvesGeometry & /*curves*/) const final
  {
    return ATTR_DOMAIN_CURVE;
  }
};

class CurvePointCountInput final : public bke::CurvesFieldInput {
 public:
  CurvePointCountInput() : bke::CurvesFieldInput(CPPType::get<int>(), "Curve Point Count")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const bke::CurvesGeometry &curves,
                                 const eAttrDomain domain,
                                 const IndexMask /*mask*/) const final
  {
    if (domain != ATTR_DOMAIN_CURVE) {
      return {};
    }
    const OffsetIndices points_by_curve = curves.points_by_curve();
    return VArray<int>::ForFunc(curves.curves_num(), [points_by_curve](const int64_t curve_i) {
      return points_by_curve[curve_i].size();
    });
  }

  uint64_t hash() const final
  {
    return 903847569873762;
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    if (dynamic_cast<const CurvePointCountInput *>(&other)) {
      return true;
    }
    return false;
  }

  std::optional<eAttrDomain> preferred_domain(const bke::CurvesGeometry & /*curves*/) const final
  {
    return ATTR_DOMAIN_CURVE;
  }
};

/**
 * The node is often used to retrieve the root point of the curve. If the curve indices are in
 * order, the sort weights have no effect, and the sort index is the first point, then we can just
 * return the curve offsets as a span directly.
 */
static bool use_start_point_special_case(const Field<int> &curve_index,
                                         const Field<int> &sort_index,
                                         const Field<float> &sort_weights)
{
  if (!dynamic_cast<const fn::IndexFieldInput *>(&curve_index.node())) {
    return false;
  }
  if (sort_index.node().depends_on_input() || sort_weights.node().depends_on_input()) {
    return false;
  }
  return fn::evaluate_constant_field(sort_index) == 0;
}

class CurveStartPointInput final : public bke::CurvesFieldInput {
 public:
  CurveStartPointInput() : bke::CurvesFieldInput(CPPType::get<int>(), "Point of Curve")
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const bke::CurvesGeometry &curves,
                                 const eAttrDomain /*domain*/,
                                 const IndexMask /*mask*/) const final
  {
    return VArray<int>::ForSpan(curves.offsets());
  }

  uint64_t hash() const final
  {
    return 2938459815345;
  }

  bool is_equal_to(const fn::FieldNode &other) const final
  {
    if (dynamic_cast<const CurveStartPointInput *>(&other)) {
      return true;
    }
    return false;
  }

  std::optional<eAttrDomain> preferred_domain(const bke::CurvesGeometry & /*curves*/) const final
  {
    return ATTR_DOMAIN_CURVE;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<int> curve_index = params.extract_input<Field<int>>("Curve Index");
  if (params.output_is_required("Total")) {
    params.set_output("Total",
                      Field<int>(std::make_shared<EvaluateAtIndexInput>(
                          curve_index,
                          Field<int>(std::make_shared<CurvePointCountInput>()),
                          ATTR_DOMAIN_CURVE)));
  }
  if (params.output_is_required("Point Index")) {
    Field<int> sort_index = params.extract_input<Field<int>>("Sort Index");
    Field<float> sort_weight = params.extract_input<Field<float>>("Weights");
    if (use_start_point_special_case(curve_index, sort_index, sort_weight)) {
      params.set_output("Point Index", Field<int>(std::make_shared<CurveStartPointInput>()));
    }
    else {
      params.set_output("Point Index",
                        Field<int>(std::make_shared<PointsOfCurveInput>(
                            curve_index, std::move(sort_index), std::move(sort_weight))));
    }
  }
}

}  // namespace blender::nodes::node_geo_curve_topology_points_of_curve_cc

void register_node_type_geo_curve_topology_points_of_curve()
{
  namespace file_ns = blender::nodes::node_geo_curve_topology_points_of_curve_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_CURVE_TOPOLOGY_POINTS_OF_CURVE, "Points of Curve", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}

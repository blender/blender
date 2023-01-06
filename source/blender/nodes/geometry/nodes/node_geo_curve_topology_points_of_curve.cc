/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "BLI_task.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_topology_points_of_curve_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Curve Index"))
      .implicit_field(implicit_field_inputs::index)
      .description(N_("The curve to retrieve data from. Defaults to the curve from the context"));
  b.add_input<decl::Float>(N_("Weights"))
      .supports_field()
      .hide_value()
      .description(N_("Values used to sort the curve's points. Uses indices by default"));
  b.add_input<decl::Int>(N_("Sort Index"))
      .min(0)
      .supports_field()
      .description(N_("Which of the sorted points to output"));
  b.add_output<decl::Int>(N_("Point Index"))
      .field_source_reference_all()
      .description(N_("A point of the curve, chosen by the sort index"));
  b.add_output<decl::Int>(N_("Total"))
      .field_source()
      .reference_pass({0})
      .description(N_("The number of points in the curve"));
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

        const IndexRange points = curves.points_for_curve(curve_i);

        /* Retrieve the weights for each point. */
        sort_weights.reinitialize(points.size());
        all_sort_weights.materialize_compressed(IndexMask(points), sort_weights.as_mutable_span());

        /* Sort a separate array of compressed indices corresponding to the compressed weights.
         * This allows using `materialize_compressed` to avoid virtual function call overhead
         * when accessing values in the sort weights. However, it means a separate array of
         * indices within the compressed array is necessary for sorting. */
        sort_indices.reinitialize(points.size());
        std::iota(sort_indices.begin(), sort_indices.end(), 0);
        std::stable_sort(sort_indices.begin(), sort_indices.end(), [&](int a, int b) {
          return sort_weights[a] < sort_weights[b];
        });

        const int index_in_sort_wrapped = mod_i(index_in_sort, points.size());
        point_of_curve[selection_i] = points[sort_indices[index_in_sort_wrapped]];
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
    return VArray<int>::ForFunc(curves.curves_num(), [&, curves](const int64_t curve_i) {
      return curves.points_num_for_curve(curve_i);
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

static void node_geo_exec(GeoNodeExecParams params)
{
  const Field<int> curve_index = params.extract_input<Field<int>>("Curve Index");
  if (params.output_is_required("Total")) {
    params.set_output("Total",
                      Field<int>(std::make_shared<FieldAtIndexInput>(
                          curve_index,
                          Field<int>(std::make_shared<CurvePointCountInput>()),
                          ATTR_DOMAIN_CURVE)));
  }
  if (params.output_is_required("Point Index")) {
    params.set_output("Point Index",
                      Field<int>(std::make_shared<PointsOfCurveInput>(
                          curve_index,
                          params.extract_input<Field<int>>("Sort Index"),
                          params.extract_input<Field<float>>("Weights"))));
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

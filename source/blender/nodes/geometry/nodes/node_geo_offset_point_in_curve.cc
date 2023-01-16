/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes {

int apply_offset_in_cyclic_range(const IndexRange range, const int start_index, const int offset)
{
  BLI_assert(range.contains(start_index));
  const int start_in_range = start_index - range.first();
  const int offset_in_range = start_in_range + offset;
  const int mod_offset = offset_in_range % range.size();
  if (mod_offset >= 0) {
    return range[mod_offset];
  }
  return range.last(-(mod_offset + 1));
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_offset_point_in_curve_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Point Index"))
      .implicit_field(implicit_field_inputs::index)
      .description(
          N_("The index of the control point to evaluate. Defaults to the current index"));
  b.add_input<decl::Int>(N_("Offset"))
      .supports_field()
      .description(N_("The number of control points along the curve to traverse"));
  b.add_output<decl::Bool>(N_("Is Valid Offset"))
      .field_source_reference_all()
      .description(N_("Whether the input control point plus the offset is a valid index of the "
                      "original curve"));
  b.add_output<decl::Int>(N_("Point Index"))
      .field_source_reference_all()
      .description(N_("The index of the control point plus the offset within the entire "
                      "curves data-block"));
}

class ControlPointNeighborFieldInput final : public bke::CurvesFieldInput {
 private:
  const Field<int> index_;
  const Field<int> offset_;

 public:
  ControlPointNeighborFieldInput(Field<int> index, Field<int> offset)
      : CurvesFieldInput(CPPType::get<int>(), "Offset Point in Curve"),
        index_(std::move(index)),
        offset_(std::move(offset))
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const bke::CurvesGeometry &curves,
                                 const eAttrDomain domain,
                                 const IndexMask mask) const final
  {
    const VArray<bool> cyclic = curves.cyclic();
    const Array<int> parent_curves = curves.point_to_curve_map();

    const bke::CurvesFieldContext context{curves, domain};
    fn::FieldEvaluator evaluator{context, &mask};
    evaluator.add(index_);
    evaluator.add(offset_);
    evaluator.evaluate();
    const VArray<int> indices = evaluator.get_evaluated<int>(0);
    const VArray<int> offsets = evaluator.get_evaluated<int>(1);

    Array<int> output(mask.min_array_size());
    for (const int i_selection : mask) {
      const int i_point = std::clamp(indices[i_selection], 0, curves.points_num() - 1);
      const int i_curve = parent_curves[i_point];
      const IndexRange curve_points = curves.points_for_curve(i_curve);
      const int offset_point = i_point + offsets[i_point];

      if (cyclic[i_curve]) {
        output[i_selection] = apply_offset_in_cyclic_range(
            curve_points, i_point, offsets[i_selection]);
        continue;
      }
      output[i_selection] = std::clamp(offset_point, 0, curves.points_num() - 1);
    }

    return VArray<int>::ForContainer(std::move(output));
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    index_.node().for_each_field_input_recursive(fn);
    offset_.node().for_each_field_input_recursive(fn);
  }
};

class OffsetValidFieldInput final : public bke::CurvesFieldInput {
 private:
  const Field<int> index_;
  const Field<int> offset_;

 public:
  OffsetValidFieldInput(Field<int> index, Field<int> offset)
      : CurvesFieldInput(CPPType::get<bool>(), "Offset Valid"),
        index_(std::move(index)),
        offset_(std::move(offset))
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const bke::CurvesGeometry &curves,
                                 const eAttrDomain domain,
                                 const IndexMask mask) const final
  {
    const VArray<bool> cyclic = curves.cyclic();
    const Array<int> parent_curves = curves.point_to_curve_map();

    const bke::CurvesFieldContext context{curves, domain};
    fn::FieldEvaluator evaluator{context, &mask};
    evaluator.add(index_);
    evaluator.add(offset_);
    evaluator.evaluate();
    const VArray<int> indices = evaluator.get_evaluated<int>(0);
    const VArray<int> offsets = evaluator.get_evaluated<int>(1);

    Array<bool> output(mask.min_array_size());
    for (const int i_selection : mask) {
      const int i_point = indices[i_selection];
      if (!curves.points_range().contains(i_point)) {
        output[i_selection] = false;
        continue;
      }

      const int i_curve = parent_curves[i_point];
      const IndexRange curve_points = curves.points_for_curve(i_curve);
      if (cyclic[i_curve]) {
        output[i_selection] = true;
        continue;
      }
      output[i_selection] = curve_points.contains(i_point + offsets[i_selection]);
    };
    return VArray<bool>::ForContainer(std::move(output));
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    index_.node().for_each_field_input_recursive(fn);
    offset_.node().for_each_field_input_recursive(fn);
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<int> index = params.extract_input<Field<int>>("Point Index");
  Field<int> offset = params.extract_input<Field<int>>("Offset");

  if (params.output_is_required("Point Index")) {
    Field<int> curve_point_field{std::make_shared<ControlPointNeighborFieldInput>(index, offset)};
    params.set_output("Point Index", std::move(curve_point_field));
  }
  if (params.output_is_required("Is Valid Offset")) {
    Field<bool> valid_field{std::make_shared<OffsetValidFieldInput>(index, offset)};
    params.set_output("Is Valid Offset", std::move(valid_field));
  }
}

}  // namespace blender::nodes::node_geo_offset_point_in_curve_cc

void register_node_type_geo_offset_point_in_curve()
{
  namespace file_ns = blender::nodes::node_geo_offset_point_in_curve_cc;
  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_OFFSET_POINT_IN_CURVE, "Offset Point in Curve", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}

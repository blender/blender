/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_offset_point_in_curve_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Point Index")
      .implicit_field(NODE_DEFAULT_INPUT_INDEX_FIELD)
      .description("The index of the control point to evaluate. Defaults to the current index");
  b.add_input<decl::Int>("Offset").supports_field().description(
      "The number of control points along the curve to traverse");
  b.add_output<decl::Bool>("Is Valid Offset")
      .field_source_reference_all()
      .description(
          "Whether the input control point plus the offset is a valid index of the "
          "original curve");
  b.add_output<decl::Int>("Point Index")
      .field_source_reference_all()
      .description(
          "The index of the control point plus the offset within the entire "
          "curves data-block");
}

class ControlPointNeighborFieldInput final : public bke::GeometryFieldInput {
 private:
  const Field<int> index_;
  const Field<int> offset_;

 public:
  ControlPointNeighborFieldInput(Field<int> index, Field<int> offset)
      : GeometryFieldInput(CPPType::get<int>(), "Offset Point in Curve"),
        index_(std::move(index)),
        offset_(std::move(offset))
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask &mask) const final
  {
    const bke::CurvesGeometry *curves_ptr = context.curves_or_strokes();
    if (!curves_ptr) {
      return {};
    }
    const bke::CurvesGeometry &curves = *curves_ptr;

    const OffsetIndices points_by_curve = curves.points_by_curve();
    const VArray<bool> cyclic = curves.cyclic();
    const Array<int> parent_curves = curves.point_to_curve_map();

    fn::FieldEvaluator evaluator{context, &mask};
    evaluator.add(index_);
    evaluator.add(offset_);
    evaluator.evaluate();
    const VArray<int> indices = evaluator.get_evaluated<int>(0);
    const VArray<int> offsets = evaluator.get_evaluated<int>(1);

    Array<int> output(mask.min_array_size());
    mask.foreach_index([&](const int i_selection) {
      const int point = std::clamp(indices[i_selection], 0, curves.points_num() - 1);
      const int curve = parent_curves[point];
      const IndexRange curve_points = points_by_curve[curve];
      const int shifted_point = point + offsets[i_selection];

      if (cyclic[curve]) {
        const int point_index_in_curve = shifted_point - curve_points.start();
        output[i_selection] = curve_points.start() +
                              math::mod_periodic<int>(point_index_in_curve, curve_points.size());
        return;
      }
      output[i_selection] = std::clamp(shifted_point, 0, curves.points_num() - 1);
    });

    return VArray<int>::from_container(std::move(output));
  }

  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override
  {
    index_.node().for_each_field_input_recursive(fn);
    offset_.node().for_each_field_input_recursive(fn);
  }
};

class OffsetValidFieldInput final : public bke::GeometryFieldInput {
 private:
  const Field<int> index_;
  const Field<int> offset_;

 public:
  OffsetValidFieldInput(Field<int> index, Field<int> offset)
      : GeometryFieldInput(CPPType::get<bool>(), "Offset Valid"),
        index_(std::move(index)),
        offset_(std::move(offset))
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask &mask) const final
  {
    const bke::CurvesGeometry *curves_ptr = context.curves_or_strokes();
    if (!curves_ptr) {
      return {};
    }
    const bke::CurvesGeometry &curves = *curves_ptr;

    const VArray<bool> cyclic = curves.cyclic();
    const OffsetIndices points_by_curve = curves.points_by_curve();
    const Array<int> parent_curves = curves.point_to_curve_map();

    fn::FieldEvaluator evaluator{context, &mask};
    evaluator.add(index_);
    evaluator.add(offset_);
    evaluator.evaluate();
    const VArray<int> indices = evaluator.get_evaluated<int>(0);
    const VArray<int> offsets = evaluator.get_evaluated<int>(1);

    Array<bool> output(mask.min_array_size());
    mask.foreach_index([&](const int i_selection) {
      const int i_point = indices[i_selection];
      if (!curves.points_range().contains(i_point)) {
        output[i_selection] = false;
        return;
      }

      const int i_curve = parent_curves[i_point];
      const IndexRange curve_points = points_by_curve[i_curve];
      if (cyclic[i_curve]) {
        output[i_selection] = true;
        return;
      }
      output[i_selection] = curve_points.contains(i_point + offsets[i_selection]);
    });
    return VArray<bool>::from_container(std::move(output));
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

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeOffsetPointInCurve", GEO_NODE_OFFSET_POINT_IN_CURVE);
  ntype.ui_name = "Offset Point in Curve";
  ntype.ui_description = "Offset a control point index within its curve";
  ntype.enum_name_legacy = "OFFSET_POINT_IN_CURVE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_offset_point_in_curve_cc

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_reverse_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(
      {GeometryComponent::Type::Curve, GeometryComponent::Type::GreasePencil});
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();
}

static void reverse_curve(bke::CurvesGeometry &curves,
                          const fn::FieldContext &field_context,
                          const Field<bool> &selection_field)
{
  fn::FieldEvaluator selection_evaluator{field_context, curves.curves_num()};
  selection_evaluator.add(selection_field);
  selection_evaluator.evaluate();
  const IndexMask selection = selection_evaluator.get_evaluated_as_mask(0);
  if (selection.is_empty()) {
    return;
  }
  curves.reverse_curves(selection);
}

static void reverse_grease_pencil(GreasePencil &grease_pencil, const Field<bool> &selection_field)
{
  using namespace blender::bke::greasepencil;
  for (const int layer_index : grease_pencil.layers().index_range()) {
    Drawing *drawing = get_eval_grease_pencil_layer_drawing_for_write(grease_pencil, layer_index);
    if (drawing == nullptr) {
      continue;
    }
    bke::CurvesGeometry &curves = drawing->strokes_for_write();
    const bke::GreasePencilLayerFieldContext field_context(
        grease_pencil, AttrDomain::Curve, layer_index);
    reverse_curve(curves, field_context, selection_field);
    drawing->tag_topology_changed();
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  GeometryComponentEditData::remember_deformed_positions_if_necessary(geometry_set);

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Curves *curves_id = geometry_set.get_curves_for_write()) {
      bke::CurvesGeometry &curves = curves_id->geometry.wrap();
      const bke::CurvesFieldContext field_context{curves, AttrDomain::Curve};
      reverse_curve(curves, field_context, selection_field);
    }
    if (GreasePencil *grease_pencil = geometry_set.get_grease_pencil_for_write()) {
      reverse_grease_pencil(*grease_pencil, selection_field);
    }
  });

  params.set_output("Curve", std::move(geometry_set));
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_REVERSE_CURVE, "Reverse Curve", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_reverse_cc

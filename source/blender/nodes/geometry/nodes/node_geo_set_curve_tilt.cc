/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_curve_tilt_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(
      {GeometryComponent::Type::Curve, GeometryComponent::Type::GreasePencil});
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Float>("Tilt").subtype(PROP_ANGLE).field_on_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();
}

static void set_curve_tilt(bke::CurvesGeometry &curves,
                           const fn::FieldContext &field_context,
                           const Field<bool> &selection_field,
                           const Field<float> &tilt_field)
{
  if (curves.points_num() == 0) {
    return;
  }
  MutableAttributeAccessor attributes = curves.attributes_for_write();
  AttributeWriter<float> tilts = attributes.lookup_or_add_for_write<float>("tilt",
                                                                           AttrDomain::Point);
  fn::FieldEvaluator evaluator{field_context, curves.points_num()};
  evaluator.set_selection(selection_field);
  evaluator.add_with_destination(tilt_field, tilts.varray);
  evaluator.evaluate();

  tilts.finish();
}

static void set_grease_pencil_tilt(GreasePencil &grease_pencil,
                                   const Field<bool> &selection_field,
                                   const Field<float> &tilt_field)
{
  using namespace blender::bke::greasepencil;
  for (const int layer_index : grease_pencil.layers().index_range()) {
    Drawing *drawing = get_eval_grease_pencil_layer_drawing_for_write(grease_pencil, layer_index);
    if (drawing == nullptr) {
      continue;
    }
    bke::CurvesGeometry &curves = drawing->strokes_for_write();
    const bke::GreasePencilLayerFieldContext field_context(
        grease_pencil, AttrDomain::Point, layer_index);
    set_curve_tilt(curves, field_context, selection_field, tilt_field);
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<float> tilt_field = params.extract_input<Field<float>>("Tilt");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Curves *curves_id = geometry_set.get_curves_for_write()) {
      bke::CurvesGeometry &curves = curves_id->geometry.wrap();
      const bke::CurvesFieldContext field_context{curves, AttrDomain::Point};
      set_curve_tilt(curves, field_context, selection_field, tilt_field);
    }
    if (geometry_set.has_grease_pencil()) {
      set_grease_pencil_tilt(
          *geometry_set.get_grease_pencil_for_write(), selection_field, tilt_field);
    }
  });

  params.set_output("Curve", std::move(geometry_set));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SET_CURVE_TILT, "Set Curve Tilt", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_curve_tilt_cc

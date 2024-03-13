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
                           const Field<bool> &selection,
                           const Field<float> &tilt)
{
  bke::try_capture_field_on_geometry(curves.attributes_for_write(),
                                     field_context,
                                     "tilt",
                                     bke::AttrDomain::Point,
                                     selection,
                                     tilt);
}

static void set_grease_pencil_tilt(GreasePencil &grease_pencil,
                                   const Field<bool> &selection,
                                   const Field<float> &tilt)
{
  using namespace blender::bke::greasepencil;
  for (const int layer_index : grease_pencil.layers().index_range()) {
    Drawing *drawing = get_eval_grease_pencil_layer_drawing_for_write(grease_pencil, layer_index);
    if (drawing == nullptr) {
      continue;
    }
    set_curve_tilt(
        drawing->strokes_for_write(),
        bke::GreasePencilLayerFieldContext(grease_pencil, AttrDomain::Point, layer_index),
        selection,
        tilt);
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");
  const Field<float> tilt = params.extract_input<Field<float>>("Tilt");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Curves *curves_id = geometry_set.get_curves_for_write()) {
      bke::CurvesGeometry &curves = curves_id->geometry.wrap();
      const bke::CurvesFieldContext field_context(curves, AttrDomain::Point);
      set_curve_tilt(curves, field_context, selection, tilt);
    }
    if (GreasePencil *grease_pencil = geometry_set.get_grease_pencil_for_write()) {
      set_grease_pencil_tilt(*grease_pencil, selection, tilt);
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

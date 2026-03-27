/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "GEO_foreach_geometry.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_nurbs_weight_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Curves")
      .supported_type({GeometryComponent::Type::Curve, GeometryComponent::Type::GreasePencil})
      .description("Curves to set the weight on");
  b.add_output<decl::Geometry>("Curves").propagate_all().align_with_previous();
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Float>("Weight").min(0.0f).default_value(1.0f).field_on_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curves"_ustr);
  Field<bool> selection = params.extract_input<Field<bool>>("Selection"_ustr);

  static auto clamp_negative = mf::build::SI1_SO<float, float>(
      "Clamp Negative", [](float value) { return std::max(value, 0.0f); });
  Field<float> weight(
      FieldOperation::from(clamp_negative, {params.extract_input<Field<float>>("Weight"_ustr)}));

  std::atomic<bool> has_curves = false;
  std::atomic<bool> has_nurbs = false;

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
    if (Curves *curves_id = geometry_set.get_curves_for_write()) {
      bke::CurvesGeometry &curves = curves_id->geometry.wrap();
      has_curves = true;
      if (curves.has_curve_with_type(CURVE_TYPE_NURBS)) {
        const bke::CurvesFieldContext field_context(*curves_id, AttrDomain::Point);
        bke::try_capture_field_on_geometry(curves.attributes_for_write(),
                                           field_context,
                                           "nurbs_weight",
                                           bke::AttrDomain::Point,
                                           selection,
                                           weight);
        has_nurbs = true;
      }
    }
    if (GreasePencil *grease_pencil = geometry_set.get_grease_pencil_for_write()) {
      using namespace blender::bke::greasepencil;
      for (const int layer_index : grease_pencil->layers().index_range()) {
        Drawing *drawing = grease_pencil->get_eval_drawing(grease_pencil->layer(layer_index));
        if (drawing == nullptr) {
          continue;
        }
        has_curves = true;
        bke::CurvesGeometry &curves = drawing->strokes_for_write();
        if (curves.has_curve_with_type(CURVE_TYPE_NURBS)) {
          bke::try_capture_field_on_geometry(
              curves.attributes_for_write(),
              bke::GreasePencilLayerFieldContext(*grease_pencil, AttrDomain::Point, layer_index),
              "nurbs_weight",
              bke::AttrDomain::Point,
              selection,
              weight);
          has_nurbs = true;
        }
      }
    };
  });

  if (has_curves && !has_nurbs) {
    params.error_message_add(NodeWarningType::Info, TIP_("Input curves do not have NURBS type"));
  }

  params.set_output("Curves"_ustr, std::move(geometry_set));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSetNURBSWeight");
  ntype.ui_name = "Set NURBS Weight";
  ntype.ui_description =
      "Control the influence of each NURBS control point on the curve by changing the "
      "\"nurbs_weight\" attribute";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_nurbs_weight_cc

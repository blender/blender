/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_grease_pencil_types.h"

#include "BKE_grease_pencil.hh"

#include "GEO_fillet_curves.hh"
#include "GEO_foreach_geometry.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_fillet_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveFillet)

static EnumPropertyItem mode_items[] = {
    {GEO_NODE_CURVE_FILLET_BEZIER,
     "BEZIER",
     0,
     N_("Bézier"),
     N_("Align Bézier handles to create circular arcs at each control point")},
    {GEO_NODE_CURVE_FILLET_POLY,
     "POLY",
     0,
     N_("Poly"),
     N_("Add control points along a circular arc (handle type is vector if Bézier Spline)")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Curve")
      .supported_type({GeometryComponent::Type::Curve, GeometryComponent::Type::GreasePencil})
      .description("Curves to generate rounded corners on");
  b.add_output<decl::Geometry>("Curve").propagate_all().align_with_previous();
  b.add_input<decl::Float>("Radius")
      .min(0.0f)
      .max(FLT_MAX)
      .subtype(PropertySubType::PROP_DISTANCE)
      .default_value(0.25f)
      .field_on_all();
  b.add_input<decl::Bool>("Limit Radius")
      .description("Limit the maximum value of the radius in order to avoid overlapping fillets");
  b.add_input<decl::Menu>("Mode")
      .static_items(mode_items)
      .optional_label()
      .description("How to choose number of vertices on fillet");
  b.add_input<decl::Int>("Count")
      .default_value(1)
      .min(1)
      .max(1000)
      .field_on_all()
      .usage_by_single_menu(GEO_NODE_CURVE_FILLET_POLY);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  /* Still used for forward compatibility. */
  node->storage = MEM_callocN<NodeGeometryCurveFillet>(__func__);
}

static bke::CurvesGeometry fillet_curve(const bke::CurvesGeometry &src_curves,
                                        const GeometryNodeCurveFilletMode mode,
                                        const fn::FieldContext &field_context,
                                        const std::optional<Field<int>> &count_field,
                                        const Field<float> &radius_field,
                                        const bool limit_radius,
                                        const AttributeFilter &attribute_filter)
{
  fn::FieldEvaluator evaluator{field_context, src_curves.points_num()};
  evaluator.add(radius_field);

  switch (mode) {
    case GEO_NODE_CURVE_FILLET_BEZIER: {
      evaluator.evaluate();
      return geometry::fillet_curves_bezier(src_curves,
                                            src_curves.curves_range(),
                                            evaluator.get_evaluated<float>(0),
                                            limit_radius,
                                            attribute_filter);
    }
    case GEO_NODE_CURVE_FILLET_POLY: {
      evaluator.add(*count_field);
      evaluator.evaluate();
      return geometry::fillet_curves_poly(src_curves,
                                          src_curves.curves_range(),
                                          evaluator.get_evaluated<float>(0),
                                          evaluator.get_evaluated<int>(1),
                                          limit_radius,
                                          attribute_filter);
    }
  }
  return bke::CurvesGeometry();
}

static void fillet_grease_pencil(GreasePencil &grease_pencil,
                                 const GeometryNodeCurveFilletMode mode,
                                 const std::optional<Field<int>> &count_field,
                                 const Field<float> &radius_field,
                                 const bool limit_radius,
                                 const AttributeFilter &attribute_filter)
{
  using namespace blender::bke::greasepencil;
  for (const int layer_index : grease_pencil.layers().index_range()) {
    Drawing *drawing = grease_pencil.get_eval_drawing(grease_pencil.layer(layer_index));
    if (drawing == nullptr) {
      continue;
    }
    const bke::CurvesGeometry &src_curves = drawing->strokes();
    if (src_curves.is_empty()) {
      continue;
    }
    const bke::GreasePencilLayerFieldContext field_context(
        grease_pencil, AttrDomain::Curve, layer_index);
    bke::CurvesGeometry dst_curves = fillet_curve(src_curves,
                                                  mode,
                                                  field_context,
                                                  count_field,
                                                  radius_field,
                                                  limit_radius,
                                                  attribute_filter);
    drawing->strokes_for_write() = std::move(dst_curves);
    drawing->tag_topology_changed();
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  const GeometryNodeCurveFilletMode mode = params.extract_input<GeometryNodeCurveFilletMode>(
      "Mode");

  Field<float> radius_field = params.extract_input<Field<float>>("Radius");
  const bool limit_radius = params.extract_input<bool>("Limit Radius");

  std::optional<Field<int>> count_field;
  if (mode == GEO_NODE_CURVE_FILLET_POLY) {
    count_field.emplace(params.extract_input<Field<int>>("Count"));
  }

  const NodeAttributeFilter &attribute_filter = params.get_attribute_filter("Curve");

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
    if (geometry_set.has_curves()) {
      const Curves &curves_id = *geometry_set.get_curves();
      const bke::CurvesGeometry &src_curves = curves_id.geometry.wrap();
      const bke::CurvesFieldContext field_context{curves_id, AttrDomain::Point};
      bke::CurvesGeometry dst_curves = fillet_curve(src_curves,
                                                    mode,
                                                    field_context,
                                                    count_field,
                                                    radius_field,
                                                    limit_radius,
                                                    attribute_filter);
      Curves *dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
      bke::curves_copy_parameters(curves_id, *dst_curves_id);
      geometry_set.replace_curves(dst_curves_id);
    }
    if (geometry_set.has_grease_pencil()) {
      GreasePencil &grease_pencil = *geometry_set.get_grease_pencil_for_write();
      fillet_grease_pencil(
          grease_pencil, mode, count_field, radius_field, limit_radius, attribute_filter);
    }
  });

  params.set_output("Curve", std::move(geometry_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeFilletCurve", GEO_NODE_FILLET_CURVE);
  ntype.ui_name = "Fillet Curve";
  ntype.ui_description = "Round corners by generating circular arcs on each control point";
  ntype.enum_name_legacy = "FILLET_CURVE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryCurveFillet", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_fillet_cc

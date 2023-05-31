/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface.h"
#include "UI_resources.h"

#include "GEO_fillet_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_fillet_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveFillet)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Int>("Count").default_value(1).min(1).max(1000).field_on_all().make_available(
      [](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_FILLET_POLY; });
  b.add_input<decl::Float>("Radius")
      .min(0.0f)
      .max(FLT_MAX)
      .subtype(PropertySubType::PROP_DISTANCE)
      .default_value(0.25f)
      .field_on_all();
  b.add_input<decl::Bool>("Limit Radius")
      .description("Limit the maximum value of the radius in order to avoid overlapping fillets");
  b.add_output<decl::Geometry>("Curve").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryCurveFillet *data = MEM_cnew<NodeGeometryCurveFillet>(__func__);
  data->mode = GEO_NODE_CURVE_FILLET_BEZIER;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryCurveFillet &storage = node_storage(*node);
  const GeometryNodeCurveFilletMode mode = (GeometryNodeCurveFilletMode)storage.mode;
  bNodeSocket *poly_socket = static_cast<bNodeSocket *>(node->inputs.first)->next;
  bke::nodeSetSocketAvailability(ntree, poly_socket, mode == GEO_NODE_CURVE_FILLET_POLY);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");

  const NodeGeometryCurveFillet &storage = node_storage(params.node());
  const GeometryNodeCurveFilletMode mode = (GeometryNodeCurveFilletMode)storage.mode;

  Field<float> radius_field = params.extract_input<Field<float>>("Radius");
  const bool limit_radius = params.extract_input<bool>("Limit Radius");

  std::optional<Field<int>> count_field;
  if (mode == GEO_NODE_CURVE_FILLET_POLY) {
    count_field.emplace(params.extract_input<Field<int>>("Count"));
  }

  const AnonymousAttributePropagationInfo &propagation_info = params.get_output_propagation_info(
      "Curve");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_curves()) {
      return;
    }

    const Curves &curves_id = *geometry_set.get_curves_for_read();
    const bke::CurvesGeometry &curves = curves_id.geometry.wrap();
    const bke::CurvesFieldContext context{curves, ATTR_DOMAIN_POINT};
    fn::FieldEvaluator evaluator{context, curves.points_num()};
    evaluator.add(radius_field);

    switch (mode) {
      case GEO_NODE_CURVE_FILLET_BEZIER: {
        evaluator.evaluate();
        bke::CurvesGeometry dst_curves = geometry::fillet_curves_bezier(
            curves,
            curves.curves_range(),
            evaluator.get_evaluated<float>(0),
            limit_radius,
            propagation_info);
        Curves *dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
        bke::curves_copy_parameters(curves_id, *dst_curves_id);
        geometry_set.replace_curves(dst_curves_id);
        break;
      }
      case GEO_NODE_CURVE_FILLET_POLY: {
        evaluator.add(*count_field);
        evaluator.evaluate();
        bke::CurvesGeometry dst_curves = geometry::fillet_curves_poly(
            curves,
            curves.curves_range(),
            evaluator.get_evaluated<float>(0),
            evaluator.get_evaluated<int>(1),
            limit_radius,
            propagation_info);
        Curves *dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
        bke::curves_copy_parameters(curves_id, *dst_curves_id);
        geometry_set.replace_curves(dst_curves_id);
        break;
      }
    }
  });

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_curve_fillet_cc

void register_node_type_geo_curve_fillet()
{
  namespace file_ns = blender::nodes::node_geo_curve_fillet_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_FILLET_CURVE, "Fillet Curve", NODE_CLASS_GEOMETRY);
  ntype.draw_buttons = file_ns::node_layout;
  node_type_storage(
      &ntype, "NodeGeometryCurveFillet", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}

/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_spline.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_set_handle_type_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveSetHandles)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "handle_type", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveSetHandles *data = MEM_cnew<NodeGeometryCurveSetHandles>(__func__);

  data->handle_type = GEO_NODE_CURVE_HANDLE_AUTO;
  data->mode = GEO_NODE_CURVE_HANDLE_LEFT | GEO_NODE_CURVE_HANDLE_RIGHT;
  node->storage = data;
}

static HandleType handle_type_from_input_type(GeometryNodeCurveHandleType type)
{
  switch (type) {
    case GEO_NODE_CURVE_HANDLE_AUTO:
      return BEZIER_HANDLE_AUTO;
    case GEO_NODE_CURVE_HANDLE_ALIGN:
      return BEZIER_HANDLE_ALIGN;
    case GEO_NODE_CURVE_HANDLE_FREE:
      return BEZIER_HANDLE_FREE;
    case GEO_NODE_CURVE_HANDLE_VECTOR:
      return BEZIER_HANDLE_VECTOR;
  }
  BLI_assert_unreachable();
  return BEZIER_HANDLE_AUTO;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurveSetHandles &storage = node_storage(params.node());
  const GeometryNodeCurveHandleType type = (GeometryNodeCurveHandleType)storage.handle_type;
  const GeometryNodeCurveHandleMode mode = (GeometryNodeCurveHandleMode)storage.mode;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  bool has_bezier_spline = false;
  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_curves()) {
      return;
    }

    /* Retrieve data for write access so we can avoid new allocations for the handles data. */
    CurveComponent &curve_component = geometry_set.get_component_for_write<CurveComponent>();
    std::unique_ptr<CurveEval> curve = curves_to_curve_eval(*curve_component.get_for_read());
    MutableSpan<SplinePtr> splines = curve->splines();

    GeometryComponentFieldContext field_context{curve_component, ATTR_DOMAIN_POINT};
    const int domain_size = curve_component.attribute_domain_size(ATTR_DOMAIN_POINT);

    fn::FieldEvaluator selection_evaluator{field_context, domain_size};
    selection_evaluator.add(selection_field);
    selection_evaluator.evaluate();
    const VArray<bool> &selection = selection_evaluator.get_evaluated<bool>(0);

    const HandleType new_handle_type = handle_type_from_input_type(type);
    int point_index = 0;

    for (SplinePtr &spline : splines) {
      if (spline->type() != CURVE_TYPE_BEZIER) {
        point_index += spline->positions().size();
        continue;
      }

      has_bezier_spline = true;
      BezierSpline &bezier_spline = static_cast<BezierSpline &>(*spline);
      if (ELEM(new_handle_type, BEZIER_HANDLE_FREE, BEZIER_HANDLE_ALIGN)) {
        /* In this case the automatically calculated handle types need to be "baked", because
         * they're possibly changing from a type that is calculated automatically to a type that
         * is positioned manually. */
        bezier_spline.ensure_auto_handles();
      }

      for (int i_point : IndexRange(bezier_spline.size())) {
        if (selection[point_index]) {
          if (mode & GEO_NODE_CURVE_HANDLE_LEFT) {
            bezier_spline.handle_types_left()[i_point] = new_handle_type;
          }
          if (mode & GEO_NODE_CURVE_HANDLE_RIGHT) {
            bezier_spline.handle_types_right()[i_point] = new_handle_type;
          }
        }
        point_index++;
      }
      bezier_spline.mark_cache_invalid();
    }

    curve_component.replace(curve_eval_to_curves(*curve));
  });
  if (!has_bezier_spline) {
    params.error_message_add(NodeWarningType::Info, TIP_("No Bezier splines in input curve"));
  }
  params.set_output("Curve", std::move(geometry_set));
}
}  // namespace blender::nodes::node_geo_curve_set_handle_type_cc

void register_node_type_geo_curve_set_handle_type()
{
  namespace file_ns = blender::nodes::node_geo_curve_set_handle_type_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_CURVE_SET_HANDLE_TYPE, "Set Handle Type", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  node_type_init(&ntype, file_ns::node_init);
  node_type_storage(&ntype,
                    "NodeGeometryCurveSetHandles",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.draw_buttons = file_ns::node_layout;

  nodeRegisterType(&ntype);
}

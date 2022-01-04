/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BKE_spline.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_set_handles_cc {

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
  data->mode = GEO_NODE_CURVE_HANDLE_LEFT;
  node->storage = data;
}

static BezierSpline::HandleType handle_type_from_input_type(GeometryNodeCurveHandleType type)
{
  switch (type) {
    case GEO_NODE_CURVE_HANDLE_AUTO:
      return BezierSpline::HandleType::Auto;
    case GEO_NODE_CURVE_HANDLE_ALIGN:
      return BezierSpline::HandleType::Align;
    case GEO_NODE_CURVE_HANDLE_FREE:
      return BezierSpline::HandleType::Free;
    case GEO_NODE_CURVE_HANDLE_VECTOR:
      return BezierSpline::HandleType::Vector;
  }
  BLI_assert_unreachable();
  return BezierSpline::HandleType::Auto;
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
    if (!geometry_set.has_curve()) {
      return;
    }

    /* Retrieve data for write access so we can avoid new allocations for the handles data. */
    CurveComponent &curve_component = geometry_set.get_component_for_write<CurveComponent>();
    CurveEval &curve = *curve_component.get_for_write();
    MutableSpan<SplinePtr> splines = curve.splines();

    GeometryComponentFieldContext field_context{curve_component, ATTR_DOMAIN_POINT};
    const int domain_size = curve_component.attribute_domain_size(ATTR_DOMAIN_POINT);

    fn::FieldEvaluator selection_evaluator{field_context, domain_size};
    selection_evaluator.add(selection_field);
    selection_evaluator.evaluate();
    const VArray<bool> &selection = selection_evaluator.get_evaluated<bool>(0);

    const BezierSpline::HandleType new_handle_type = handle_type_from_input_type(type);
    int point_index = 0;

    for (SplinePtr &spline : splines) {
      if (spline->type() != Spline::Type::Bezier) {
        point_index += spline->positions().size();
        continue;
      }

      has_bezier_spline = true;
      BezierSpline &bezier_spline = static_cast<BezierSpline &>(*spline);
      if (ELEM(new_handle_type, BezierSpline::HandleType::Free, BezierSpline::HandleType::Align)) {
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
  });
  if (!has_bezier_spline) {
    params.error_message_add(NodeWarningType::Info, TIP_("No Bezier splines in input curve"));
  }
  params.set_output("Curve", std::move(geometry_set));
}
}  // namespace blender::nodes::node_geo_curve_set_handles_cc

void register_node_type_geo_curve_set_handles()
{
  namespace file_ns = blender::nodes::node_geo_curve_set_handles_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_CURVE_SET_HANDLES, "Set Handle Type", NODE_CLASS_GEOMETRY);
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

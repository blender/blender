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

namespace blender::nodes::node_geo_set_curve_handles_cc {

NODE_STORAGE_FUNCS(NodeGeometrySetCurveHandlePositions)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_input<decl::Vector>(N_("Position")).implicit_field();
  b.add_input<decl::Vector>(N_("Offset")).default_value(float3(0.0f, 0.0f, 0.0f)).supports_field();
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometrySetCurveHandlePositions *data = MEM_cnew<NodeGeometrySetCurveHandlePositions>(
      __func__);

  data->mode = GEO_NODE_CURVE_HANDLE_LEFT;
  node->storage = data;
}

static void set_position_in_component(const GeometryNodeCurveHandleMode mode,
                                      GeometryComponent &component,
                                      const Field<bool> &selection_field,
                                      const Field<float3> &position_field,
                                      const Field<float3> &offset_field)
{
  GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_POINT};
  const int domain_size = component.attribute_domain_size(ATTR_DOMAIN_POINT);
  if (domain_size == 0) {
    return;
  }

  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);
  evaluator.add(position_field);
  evaluator.add(offset_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  CurveComponent *curve_component = static_cast<CurveComponent *>(&component);
  CurveEval *curve = curve_component->get_for_write();

  StringRef side = mode & GEO_NODE_CURVE_HANDLE_LEFT ? "handle_left" : "handle_right";

  int current_point = 0;
  int current_mask = 0;

  for (const SplinePtr &spline : curve->splines()) {
    if (spline->type() == Spline::Type::Bezier) {
      BezierSpline &bezier = static_cast<BezierSpline &>(*spline);
      for (int i : bezier.positions().index_range()) {
        if (current_mask < selection.size() && selection[current_mask] == current_point) {
          if (mode & GEO_NODE_CURVE_HANDLE_LEFT) {
            if (bezier.handle_types_left()[i] == BezierSpline::HandleType::Vector) {
              bezier.ensure_auto_handles();
              bezier.handle_types_left()[i] = BezierSpline::HandleType::Free;
            }
            else if (bezier.handle_types_left()[i] == BezierSpline::HandleType::Auto) {
              bezier.ensure_auto_handles();
              bezier.handle_types_left()[i] = BezierSpline::HandleType::Align;
            }
          }
          else {
            if (bezier.handle_types_right()[i] == BezierSpline::HandleType::Vector) {
              bezier.ensure_auto_handles();
              bezier.handle_types_right()[i] = BezierSpline::HandleType::Free;
            }
            else if (bezier.handle_types_right()[i] == BezierSpline::HandleType::Auto) {
              bezier.ensure_auto_handles();
              bezier.handle_types_right()[i] = BezierSpline::HandleType::Align;
            }
          }
          current_mask++;
        }
        current_point++;
      }
    }
    else {
      for (int UNUSED(i) : spline->positions().index_range()) {
        if (current_mask < selection.size() && selection[current_mask] == current_point) {
          current_mask++;
        }
        current_point++;
      }
    }
  }

  const VArray<float3> &positions_input = evaluator.get_evaluated<float3>(0);
  const VArray<float3> &offsets_input = evaluator.get_evaluated<float3>(1);

  OutputAttribute_Typed<float3> positions = component.attribute_try_get_for_output<float3>(
      side, ATTR_DOMAIN_POINT, {0, 0, 0});
  MutableSpan<float3> position_mutable = positions.as_span();

  for (int i : selection) {
    position_mutable[i] = positions_input[i] + offsets_input[i];
  }

  positions.save();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometrySetCurveHandlePositions &storage = node_storage(params.node());
  const GeometryNodeCurveHandleMode mode = (GeometryNodeCurveHandleMode)storage.mode;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<float3> position_field = params.extract_input<Field<float3>>("Position");
  Field<float3> offset_field = params.extract_input<Field<float3>>("Offset");

  bool has_bezier = false;
  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_curve() &&
        geometry_set.get_curve_for_read()->has_spline_with_type(Spline::Type::Bezier)) {
      has_bezier = true;
      set_position_in_component(mode,
                                geometry_set.get_component_for_write<CurveComponent>(),
                                selection_field,
                                position_field,
                                offset_field);
    }
  });
  if (!has_bezier) {
    params.error_message_add(NodeWarningType::Info,
                             TIP_("The input geometry does not contain a Bezier spline"));
  }
  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_set_curve_handles_cc

void register_node_type_geo_set_curve_handles()
{
  namespace file_ns = blender::nodes::node_geo_set_curve_handles_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SET_CURVE_HANDLES, "Set Handle Positions", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  ntype.minwidth = 100.0f;
  node_type_init(&ntype, file_ns::node_init);
  node_type_storage(&ntype,
                    "NodeGeometrySetCurveHandlePositions",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}

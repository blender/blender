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

namespace blender::nodes {

static void geo_node_set_curve_handles_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().supports_field();
  b.add_input<decl::Vector>("Position").implicit_field();
  b.add_output<decl::Geometry>("Geometry");
}

static void geo_node_set_curve_handles_layout(uiLayout *layout,
                                              bContext *UNUSED(C),
                                              PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void geo_node_set_curve_handles_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometrySetCurveHandlePositions *data = (NodeGeometrySetCurveHandlePositions *)MEM_callocN(
      sizeof(NodeGeometrySetCurveHandlePositions), __func__);

  data->mode = GEO_NODE_CURVE_HANDLE_LEFT;
  node->storage = data;
}

static void set_position_in_component(const GeometryNodeCurveHandleMode mode,
                                      GeometryComponent &component,
                                      const Field<bool> &selection_field,
                                      const Field<float3> &position_field)
{
  GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_POINT};
  const int domain_size = component.attribute_domain_size(ATTR_DOMAIN_POINT);
  if (domain_size == 0) {
    return;
  }

  fn::FieldEvaluator selection_evaluator{field_context, domain_size};
  selection_evaluator.add(selection_field);
  selection_evaluator.evaluate();
  const IndexMask selection = selection_evaluator.get_evaluated_as_mask(0);

  CurveComponent *curve_component = static_cast<CurveComponent *>(&component);
  CurveEval *curve = curve_component->get_for_write();

  StringRef side = mode & GEO_NODE_CURVE_HANDLE_LEFT ? "handle_left" : "handle_right";

  int current_point = 0;
  int current_mask = 0;

  for (const SplinePtr &spline : curve->splines()) {
    if (spline->type() == Spline::Type::Bezier) {
      BezierSpline &bezier = static_cast<BezierSpline &>(*spline);
      for (int i : bezier.positions().index_range()) {
        if (selection[current_mask] == current_point) {
          if (mode & GEO_NODE_CURVE_HANDLE_LEFT) {
            if (bezier.handle_types_left()[i] == BezierSpline::HandleType::Vector) {
              bezier.handle_types_left()[i] = BezierSpline::HandleType::Free;
            }
            else if (bezier.handle_types_left()[i] == BezierSpline::HandleType::Auto) {
              bezier.handle_types_left()[i] = BezierSpline::HandleType::Align;
            }
          }
          else {
            if (bezier.handle_types_right()[i] == BezierSpline::HandleType::Vector) {
              bezier.handle_types_right()[i] = BezierSpline::HandleType::Free;
            }
            else if (bezier.handle_types_right()[i] == BezierSpline::HandleType::Auto) {
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
        if (selection[current_mask] == current_point) {
          current_mask++;
        }
        current_point++;
      }
    }
  }

  OutputAttribute_Typed<float3> positions = component.attribute_try_get_for_output_only<float3>(
      side, ATTR_DOMAIN_POINT);
  fn::FieldEvaluator position_evaluator{field_context, &selection};
  position_evaluator.add_with_destination(position_field, positions.varray());
  position_evaluator.evaluate();
  positions.save();
}

static void geo_node_set_curve_handles_exec(GeoNodeExecParams params)
{
  const NodeGeometrySetCurveHandlePositions *node_storage =
      (NodeGeometrySetCurveHandlePositions *)params.node().storage;
  const GeometryNodeCurveHandleMode mode = (GeometryNodeCurveHandleMode)node_storage->mode;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<float3> position_field = params.extract_input<Field<float3>>("Position");

  bool has_bezier = false;
  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_curve() &&
        geometry_set.get_curve_for_read()->has_spline_with_type(Spline::Type::Bezier)) {
      has_bezier = true;
      set_position_in_component(mode,
                                geometry_set.get_component_for_write<CurveComponent>(),
                                selection_field,
                                position_field);
    }
  });
  if (!has_bezier) {
    params.error_message_add(NodeWarningType::Info,
                             TIP_("The input geometry does not contain a Bezier spline"));
  }
  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_set_curve_handles()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SET_CURVE_HANDLES, "Set Handle Positions", NODE_CLASS_GEOMETRY, 0);
  ntype.geometry_node_execute = blender::nodes::geo_node_set_curve_handles_exec;
  ntype.declare = blender::nodes::geo_node_set_curve_handles_declare;
  ntype.minwidth = 100.0f;
  node_type_init(&ntype, blender::nodes::geo_node_set_curve_handles_init);
  node_type_storage(&ntype,
                    "NodeGeometrySetCurveHandlePositions",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.draw_buttons = blender::nodes::geo_node_set_curve_handles_layout;
  nodeRegisterType(&ntype);
}

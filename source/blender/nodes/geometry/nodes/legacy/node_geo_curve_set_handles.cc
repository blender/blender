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

static void geo_node_curve_set_handles_decalre(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve"));
  b.add_input<decl::String>(N_("Selection"));
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void geo_node_curve_set_handles_layout(uiLayout *layout,
                                              bContext *UNUSED(C),
                                              PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "handle_type", 0, "", ICON_NONE);
}

static void geo_node_curve_set_handles_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveSetHandles *data = (NodeGeometryCurveSetHandles *)MEM_callocN(
      sizeof(NodeGeometryCurveSetHandles), __func__);

  data->handle_type = GEO_NODE_CURVE_HANDLE_AUTO;
  data->mode = GEO_NODE_CURVE_HANDLE_LEFT | GEO_NODE_CURVE_HANDLE_RIGHT;
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

static void geo_node_curve_set_handles_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurveSetHandles *node_storage =
      (NodeGeometryCurveSetHandles *)params.node().storage;
  const GeometryNodeCurveHandleType type = (GeometryNodeCurveHandleType)node_storage->handle_type;
  const GeometryNodeCurveHandleMode mode = (GeometryNodeCurveHandleMode)node_storage->mode;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  geometry_set = bke::geometry_set_realize_instances(geometry_set);
  if (!geometry_set.has_curve()) {
    params.set_output("Curve", geometry_set);
    return;
  }

  /* Retrieve data for write access so we can avoid new allocations for the handles data. */
  CurveComponent &curve_component = geometry_set.get_component_for_write<CurveComponent>();
  CurveEval &curve = *curve_component.get_for_write();
  MutableSpan<SplinePtr> splines = curve.splines();

  const std::string selection_name = params.extract_input<std::string>("Selection");
  GVArray_Typed<bool> selection = curve_component.attribute_get_for_read(
      selection_name, ATTR_DOMAIN_POINT, true);

  const BezierSpline::HandleType new_handle_type = handle_type_from_input_type(type);
  int point_index = 0;
  bool has_bezier_spline = false;
  for (SplinePtr &spline : splines) {
    if (spline->type() != Spline::Type::Bezier) {
      point_index += spline->positions().size();
      continue;
    }

    BezierSpline &bezier_spline = static_cast<BezierSpline &>(*spline);
    if (ELEM(new_handle_type, BezierSpline::HandleType::Free, BezierSpline::HandleType::Align)) {
      /* In this case the automatically calculated handle types need to be "baked", because
       * they're possibly changing from a type that is calculated automatically to a type that
       * is positioned manually. */
      bezier_spline.ensure_auto_handles();
    }
    has_bezier_spline = true;
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

  if (!has_bezier_spline) {
    params.error_message_add(NodeWarningType::Info, TIP_("No Bezier splines in input curve"));
  }

  params.set_output("Curve", geometry_set);
}
}  // namespace blender::nodes

void register_node_type_geo_legacy_curve_set_handles()
{
  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_CURVE_SET_HANDLES, "Set Handle Type", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_curve_set_handles_decalre;
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_set_handles_exec;
  node_type_init(&ntype, blender::nodes::geo_node_curve_set_handles_init);
  node_type_storage(&ntype,
                    "NodeGeometryCurveSetHandles",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.draw_buttons = blender::nodes::geo_node_curve_set_handles_layout;

  nodeRegisterType(&ntype);
}

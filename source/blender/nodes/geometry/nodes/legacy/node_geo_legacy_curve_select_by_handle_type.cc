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

#include "BLI_task.hh"

#include "BKE_spline.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_select_by_handle_type_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("Selection"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void geo_node_curve_select_by_handle_type_layout(uiLayout *layout,
                                                        bContext *UNUSED(C),
                                                        PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "handle_type", 0, "", ICON_NONE);
}

static void geo_node_curve_select_by_handle_type_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveSelectHandles *data = (NodeGeometryCurveSelectHandles *)MEM_callocN(
      sizeof(NodeGeometryCurveSelectHandles), __func__);

  data->handle_type = GEO_NODE_CURVE_HANDLE_AUTO;
  data->mode = GEO_NODE_CURVE_HANDLE_LEFT | GEO_NODE_CURVE_HANDLE_RIGHT;
  node->storage = data;
}

static BezierSpline::HandleType handle_type_from_input_type(const GeometryNodeCurveHandleType type)
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

static void select_curve_by_handle_type(const CurveEval &curve,
                                        const BezierSpline::HandleType type,
                                        const GeometryNodeCurveHandleMode mode,
                                        const MutableSpan<bool> r_selection)
{
  const Array<int> offsets = curve.control_point_offsets();
  Span<SplinePtr> splines = curve.splines();
  threading::parallel_for(splines.index_range(), 128, [&](IndexRange range) {
    for (const int i_spline : range) {
      const Spline &spline = *splines[i_spline];
      if (spline.type() == Spline::Type::Bezier) {
        const BezierSpline &bezier_spline = static_cast<const BezierSpline &>(spline);
        Span<BezierSpline::HandleType> types_left = bezier_spline.handle_types_left();
        Span<BezierSpline::HandleType> types_right = bezier_spline.handle_types_right();
        for (const int i_point : IndexRange(bezier_spline.size())) {
          r_selection[offsets[i_spline] + i_point] = (mode & GEO_NODE_CURVE_HANDLE_LEFT &&
                                                      types_left[i_point] == type) ||
                                                     (mode & GEO_NODE_CURVE_HANDLE_RIGHT &&
                                                      types_right[i_point] == type);
        }
      }
      else {
        r_selection.slice(offsets[i_spline], offsets[i_spline + 1]).fill(false);
      }
    }
  });
}

static void geo_node_select_by_handle_type_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurveSelectHandles *storage =
      (const NodeGeometryCurveSelectHandles *)params.node().storage;
  const BezierSpline::HandleType handle_type = handle_type_from_input_type(
      (GeometryNodeCurveHandleType)storage->handle_type);
  const GeometryNodeCurveHandleMode mode = (GeometryNodeCurveHandleMode)storage->mode;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  geometry_set = bke::geometry_set_realize_instances(geometry_set);

  CurveComponent &curve_component = geometry_set.get_component_for_write<CurveComponent>();
  const CurveEval *curve = curve_component.get_for_read();

  if (curve != nullptr) {
    const std::string selection_name = params.extract_input<std::string>("Selection");
    OutputAttribute_Typed<bool> selection =
        curve_component.attribute_try_get_for_output_only<bool>(selection_name, ATTR_DOMAIN_POINT);
    if (selection) {
      select_curve_by_handle_type(*curve, handle_type, mode, selection.as_span());
      selection.save();
    }
  }

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_legacy_select_by_handle_type()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype,
                     GEO_NODE_LEGACY_CURVE_SELECT_HANDLES,
                     "Select by Handle Type",
                     NODE_CLASS_GEOMETRY,
                     0);
  ntype.declare = blender::nodes::geo_node_select_by_handle_type_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_select_by_handle_type_exec;
  node_type_init(&ntype, blender::nodes::geo_node_curve_select_by_handle_type_init);
  node_type_storage(&ntype,
                    "NodeGeometryCurveSelectHandles",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.draw_buttons = blender::nodes::geo_node_curve_select_by_handle_type_layout;

  nodeRegisterType(&ntype);
}

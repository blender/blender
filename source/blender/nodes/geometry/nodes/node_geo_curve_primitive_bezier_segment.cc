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

static void geo_node_curve_primitive_bezier_segment_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>(N_("Resolution"))
      .default_value(16)
      .min(1)
      .max(256)
      .subtype(PROP_UNSIGNED);
  b.add_input<decl::Vector>(N_("Start"))
      .default_value({-1.0f, 0.0f, 0.0f})
      .subtype(PROP_TRANSLATION);
  b.add_input<decl::Vector>(N_("Start Handle"))
      .default_value({-0.5f, 0.5f, 0.0f})
      .subtype(PROP_TRANSLATION);
  b.add_input<decl::Vector>(N_("End Handle")).subtype(PROP_TRANSLATION);
  b.add_input<decl::Vector>(N_("End")).default_value({1.0f, 0.0f, 0.0f}).subtype(PROP_TRANSLATION);
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void geo_node_curve_primitive_bezier_segment_layout(uiLayout *layout,
                                                           bContext *UNUSED(C),
                                                           PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void geo_node_curve_primitive_bezier_segment_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurvePrimitiveBezierSegment *data = (NodeGeometryCurvePrimitiveBezierSegment *)
      MEM_callocN(sizeof(NodeGeometryCurvePrimitiveBezierSegment), __func__);

  data->mode = GEO_NODE_CURVE_PRIMITIVE_BEZIER_SEGMENT_POSITION;
  node->storage = data;
}

static std::unique_ptr<CurveEval> create_bezier_segment_curve(
    const float3 start,
    const float3 start_handle_right,
    const float3 end,
    const float3 end_handle_left,
    const int resolution,
    const GeometryNodeCurvePrimitiveBezierSegmentMode mode)
{
  std::unique_ptr<CurveEval> curve = std::make_unique<CurveEval>();
  std::unique_ptr<BezierSpline> spline = std::make_unique<BezierSpline>();

  if (mode == GEO_NODE_CURVE_PRIMITIVE_BEZIER_SEGMENT_POSITION) {
    spline->add_point(start,
                      BezierSpline::HandleType::Align,
                      2.0f * start - start_handle_right,
                      BezierSpline::HandleType::Align,
                      start_handle_right,
                      1.0f,
                      0.0f);
    spline->add_point(end,
                      BezierSpline::HandleType::Align,
                      end_handle_left,
                      BezierSpline::HandleType::Align,
                      2.0f * end - end_handle_left,
                      1.0f,
                      0.0f);
  }
  else {
    spline->add_point(start,
                      BezierSpline::HandleType::Align,
                      start - start_handle_right,
                      BezierSpline::HandleType::Align,
                      start + start_handle_right,
                      1.0f,
                      0.0f);
    spline->add_point(end,
                      BezierSpline::HandleType::Align,
                      end + end_handle_left,
                      BezierSpline::HandleType::Align,
                      end - end_handle_left,
                      1.0f,
                      0.0f);
  }

  spline->set_resolution(resolution);
  spline->attributes.reallocate(spline->size());
  curve->add_spline(std::move(spline));
  curve->attributes.reallocate(curve->splines().size());
  return curve;
}

static void geo_node_curve_primitive_bezier_segment_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurvePrimitiveBezierSegment *node_storage =
      (NodeGeometryCurvePrimitiveBezierSegment *)params.node().storage;
  const GeometryNodeCurvePrimitiveBezierSegmentMode mode =
      (const GeometryNodeCurvePrimitiveBezierSegmentMode)node_storage->mode;

  std::unique_ptr<CurveEval> curve = create_bezier_segment_curve(
      params.extract_input<float3>("Start"),
      params.extract_input<float3>("Start Handle"),
      params.extract_input<float3>("End"),
      params.extract_input<float3>("End Handle"),
      std::max(params.extract_input<int>("Resolution"), 1),
      mode);
  params.set_output("Curve", GeometrySet::create_with_curve(curve.release()));
}

}  // namespace blender::nodes

void register_node_type_geo_curve_primitive_bezier_segment()
{
  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_CURVE_PRIMITIVE_BEZIER_SEGMENT, "Bezier Segment", NODE_CLASS_GEOMETRY, 0);
  node_type_init(&ntype, blender::nodes::geo_node_curve_primitive_bezier_segment_init);
  node_type_storage(&ntype,
                    "NodeGeometryCurvePrimitiveBezierSegment",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.declare = blender::nodes::geo_node_curve_primitive_bezier_segment_declare;
  ntype.draw_buttons = blender::nodes::geo_node_curve_primitive_bezier_segment_layout;
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_primitive_bezier_segment_exec;
  nodeRegisterType(&ntype);
}

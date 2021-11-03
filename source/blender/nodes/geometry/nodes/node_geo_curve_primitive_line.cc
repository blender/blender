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

static void geo_node_curve_primitive_line_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>(N_("Start"))
      .subtype(PROP_TRANSLATION)
      .description(N_("Position of the first control point"));
  b.add_input<decl::Vector>(N_("End"))
      .default_value({0.0f, 0.0f, 1.0f})
      .subtype(PROP_TRANSLATION)
      .description(N_("Position of the second control point"));
  b.add_input<decl::Vector>(N_("Direction"))
      .default_value({0.0f, 0.0f, 1.0f})
      .description(
          N_("Direction the line is going in. The length of this vector does not matter"));
  b.add_input<decl::Float>(N_("Length"))
      .default_value(1.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("Distance between the two points"));
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void geo_node_curve_primitive_line_layout(uiLayout *layout,
                                                 bContext *UNUSED(C),
                                                 PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void geo_node_curve_primitive_line_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurvePrimitiveLine *data = (NodeGeometryCurvePrimitiveLine *)MEM_callocN(
      sizeof(NodeGeometryCurvePrimitiveLine), __func__);

  data->mode = GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_POINTS;
  node->storage = data;
}

static void geo_node_curve_primitive_line_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  const NodeGeometryCurvePrimitiveLine *node_storage = (NodeGeometryCurvePrimitiveLine *)
                                                           node->storage;
  const GeometryNodeCurvePrimitiveLineMode mode = (const GeometryNodeCurvePrimitiveLineMode)
                                                      node_storage->mode;

  bNodeSocket *p2_socket = ((bNodeSocket *)node->inputs.first)->next;
  bNodeSocket *direction_socket = p2_socket->next;
  bNodeSocket *length_socket = direction_socket->next;

  nodeSetSocketAvailability(p2_socket, mode == GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_POINTS);
  nodeSetSocketAvailability(direction_socket,
                            mode == GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_DIRECTION);
  nodeSetSocketAvailability(length_socket, mode == GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_DIRECTION);
}

static std::unique_ptr<CurveEval> create_point_line_curve(const float3 start, const float3 end)
{
  std::unique_ptr<CurveEval> curve = std::make_unique<CurveEval>();
  std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();

  spline->resize(2);
  MutableSpan<float3> positions = spline->positions();
  positions[0] = start;
  positions[1] = end;
  spline->radii().fill(1.0f);
  spline->tilts().fill(0.0f);
  curve->add_spline(std::move(spline));
  curve->attributes.reallocate(curve->splines().size());
  return curve;
}

static std::unique_ptr<CurveEval> create_direction_line_curve(const float3 start,
                                                              const float3 direction,
                                                              const float length)
{
  std::unique_ptr<CurveEval> curve = std::make_unique<CurveEval>();
  std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();

  spline->resize(2);
  MutableSpan<float3> positions = spline->positions();
  positions[0] = start;
  positions[1] = direction.normalized() * length + start;

  spline->radii().fill(1.0f);
  spline->tilts().fill(0.0f);
  curve->add_spline(std::move(spline));
  curve->attributes.reallocate(curve->splines().size());
  return curve;
}

static void geo_node_curve_primitive_line_exec(GeoNodeExecParams params)
{

  const NodeGeometryCurvePrimitiveLine *node_storage =
      (NodeGeometryCurvePrimitiveLine *)params.node().storage;

  GeometryNodeCurvePrimitiveLineMode mode = (GeometryNodeCurvePrimitiveLineMode)node_storage->mode;

  std::unique_ptr<CurveEval> curve;
  if (mode == GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_POINTS) {
    curve = create_point_line_curve(params.extract_input<float3>("Start"),
                                    params.extract_input<float3>("End"));
  }
  else if (mode == GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_DIRECTION) {
    curve = create_direction_line_curve(params.extract_input<float3>("Start"),
                                        params.extract_input<float3>("Direction"),
                                        params.extract_input<float>("Length"));
  }

  params.set_output("Curve", GeometrySet::create_with_curve(curve.release()));
}

}  // namespace blender::nodes

void register_node_type_geo_curve_primitive_line()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_CURVE_PRIMITIVE_LINE, "Curve Line", NODE_CLASS_GEOMETRY, 0);
  node_type_init(&ntype, blender::nodes::geo_node_curve_primitive_line_init);
  node_type_update(&ntype, blender::nodes::geo_node_curve_primitive_line_update);
  node_type_storage(&ntype,
                    "NodeGeometryCurvePrimitiveLine",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.declare = blender::nodes::geo_node_curve_primitive_line_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_primitive_line_exec;
  ntype.draw_buttons = blender::nodes::geo_node_curve_primitive_line_layout;
  nodeRegisterType(&ntype);
}

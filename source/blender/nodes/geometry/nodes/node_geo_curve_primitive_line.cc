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

namespace blender::nodes::node_geo_curve_primitive_line_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurvePrimitiveLine)

static void node_declare(NodeDeclarationBuilder &b)
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

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurvePrimitiveLine *data = MEM_cnew<NodeGeometryCurvePrimitiveLine>(__func__);

  data->mode = GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_POINTS;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryCurvePrimitiveLine &storage = node_storage(*node);
  const GeometryNodeCurvePrimitiveLineMode mode = (GeometryNodeCurvePrimitiveLineMode)storage.mode;

  bNodeSocket *p2_socket = ((bNodeSocket *)node->inputs.first)->next;
  bNodeSocket *direction_socket = p2_socket->next;
  bNodeSocket *length_socket = direction_socket->next;

  nodeSetSocketAvailability(ntree, p2_socket, mode == GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_POINTS);
  nodeSetSocketAvailability(
      ntree, direction_socket, mode == GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_DIRECTION);
  nodeSetSocketAvailability(
      ntree, length_socket, mode == GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_DIRECTION);
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
  positions[1] = math::normalize(direction) * length + start;

  spline->radii().fill(1.0f);
  spline->tilts().fill(0.0f);
  curve->add_spline(std::move(spline));
  curve->attributes.reallocate(curve->splines().size());
  return curve;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurvePrimitiveLine &storage = node_storage(params.node());
  const GeometryNodeCurvePrimitiveLineMode mode = (GeometryNodeCurvePrimitiveLineMode)storage.mode;

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

}  // namespace blender::nodes::node_geo_curve_primitive_line_cc

void register_node_type_geo_curve_primitive_line()
{
  namespace file_ns = blender::nodes::node_geo_curve_primitive_line_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_CURVE_PRIMITIVE_LINE, "Curve Line", NODE_CLASS_GEOMETRY);
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  node_type_storage(&ntype,
                    "NodeGeometryCurvePrimitiveLine",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}

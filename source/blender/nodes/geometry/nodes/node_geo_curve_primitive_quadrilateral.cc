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

static void geo_node_curve_primitive_quadrilateral_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Width")).default_value(2.0f).min(0.0f).subtype(PROP_DISTANCE);
  b.add_input<decl::Float>(N_("Height")).default_value(2.0f).min(0.0f).subtype(PROP_DISTANCE);
  b.add_input<decl::Float>(N_("Bottom Width"))
      .default_value(4.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE);
  b.add_input<decl::Float>(N_("Top Width")).default_value(2.0f).min(0.0f).subtype(PROP_DISTANCE);
  b.add_input<decl::Float>(N_("Offset")).default_value(1.0f).subtype(PROP_DISTANCE);
  b.add_input<decl::Float>(N_("Bottom Height"))
      .default_value(3.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE);
  b.add_input<decl::Float>(N_("Top Height")).default_value(1.0f).subtype(PROP_DISTANCE);
  b.add_input<decl::Vector>(N_("Point 1"))
      .default_value({-1.0f, -1.0f, 0.0f})
      .subtype(PROP_DISTANCE);
  b.add_input<decl::Vector>(N_("Point 2"))
      .default_value({1.0f, -1.0f, 0.0f})
      .subtype(PROP_DISTANCE);
  b.add_input<decl::Vector>(N_("Point 3"))
      .default_value({1.0f, 1.0f, 0.0f})
      .subtype(PROP_DISTANCE);
  b.add_input<decl::Vector>(N_("Point 4"))
      .default_value({-1.0f, 1.0f, 0.0f})
      .subtype(PROP_DISTANCE);
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void geo_node_curve_primitive_quadrilateral_layout(uiLayout *layout,
                                                          bContext *UNUSED(C),
                                                          PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void geo_node_curve_primitive_quadrilateral_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurvePrimitiveQuad *data = (NodeGeometryCurvePrimitiveQuad *)MEM_callocN(
      sizeof(NodeGeometryCurvePrimitiveQuad), __func__);
  data->mode = GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_RECTANGLE;
  node->storage = data;
}

static void geo_node_curve_primitive_quadrilateral_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryCurvePrimitiveQuad &node_storage = *(NodeGeometryCurvePrimitiveQuad *)node->storage;
  GeometryNodeCurvePrimitiveQuadMode mode = static_cast<GeometryNodeCurvePrimitiveQuadMode>(
      node_storage.mode);

  bNodeSocket *width = ((bNodeSocket *)node->inputs.first);
  bNodeSocket *height = width->next;
  bNodeSocket *bottom = height->next;
  bNodeSocket *top = bottom->next;
  bNodeSocket *offset = top->next;
  bNodeSocket *bottom_height = offset->next;
  bNodeSocket *top_height = bottom_height->next;
  bNodeSocket *p1 = top_height->next;
  bNodeSocket *p2 = p1->next;
  bNodeSocket *p3 = p2->next;
  bNodeSocket *p4 = p3->next;

  LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
    nodeSetSocketAvailability(sock, false);
  }

  if (mode == GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_RECTANGLE) {
    nodeSetSocketAvailability(width, true);
    nodeSetSocketAvailability(height, true);
  }
  else if (mode == GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_PARALLELOGRAM) {
    nodeSetSocketAvailability(width, true);
    nodeSetSocketAvailability(height, true);
    nodeSetSocketAvailability(offset, true);
  }
  else if (mode == GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_TRAPEZOID) {
    nodeSetSocketAvailability(bottom, true);
    nodeSetSocketAvailability(top, true);
    nodeSetSocketAvailability(offset, true);
    nodeSetSocketAvailability(height, true);
  }
  else if (mode == GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_KITE) {
    nodeSetSocketAvailability(width, true);
    nodeSetSocketAvailability(bottom_height, true);
    nodeSetSocketAvailability(top_height, true);
  }
  else if (mode == GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_POINTS) {
    nodeSetSocketAvailability(p1, true);
    nodeSetSocketAvailability(p2, true);
    nodeSetSocketAvailability(p3, true);
    nodeSetSocketAvailability(p4, true);
  }
}

static void create_rectangle_curve(MutableSpan<float3> positions,
                                   const float height,
                                   const float width)
{
  positions[0] = float3(width / 2.0f, height / 2.0f, 0.0f);
  positions[1] = float3(-width / 2.0f, height / 2.0f, 0.0f);
  positions[2] = float3(-width / 2.0f, -height / 2.0f, 0.0f);
  positions[3] = float3(width / 2.0f, -height / 2.0f, 0.0f);
}

static void create_points_curve(MutableSpan<float3> positions,
                                const float3 &p1,
                                const float3 &p2,
                                const float3 &p3,
                                const float3 &p4)
{
  positions[0] = p1;
  positions[1] = p2;
  positions[2] = p3;
  positions[3] = p4;
}

static void create_parallelogram_curve(MutableSpan<float3> positions,
                                       const float height,
                                       const float width,
                                       const float offset)
{
  positions[0] = float3(width / 2.0f + offset / 2.0f, height / 2.0f, 0.0f);
  positions[1] = float3(-width / 2.0f + offset / 2.0f, height / 2.0f, 0.0f);
  positions[2] = float3(-width / 2.0f - offset / 2.0f, -height / 2.0f, 0.0f);
  positions[3] = float3(width / 2.0f - offset / 2.0f, -height / 2.0f, 0.0f);
}
static void create_trapezoid_curve(MutableSpan<float3> positions,
                                   const float bottom,
                                   const float top,
                                   const float offset,
                                   const float height)
{
  positions[0] = float3(top / 2.0f + offset, height / 2.0f, 0.0f);
  positions[1] = float3(-top / 2.0f + offset, height / 2.0f, 0.0f);
  positions[2] = float3(-bottom / 2.0f, -height / 2.0f, 0.0f);
  positions[3] = float3(bottom / 2.0f, -height / 2.0f, 0.0f);
}

static void create_kite_curve(MutableSpan<float3> positions,
                              const float width,
                              const float bottom_height,
                              const float top_height)
{
  positions[0] = float3(0, -bottom_height, 0);
  positions[1] = float3(width / 2, 0, 0);
  positions[2] = float3(0, top_height, 0);
  positions[3] = float3(-width / 2.0f, 0, 0);
}

static void geo_node_curve_primitive_quadrilateral_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurvePrimitiveQuad &node_storage =
      *(NodeGeometryCurvePrimitiveQuad *)(params.node()).storage;
  const GeometryNodeCurvePrimitiveQuadMode mode = static_cast<GeometryNodeCurvePrimitiveQuadMode>(
      node_storage.mode);

  std::unique_ptr<CurveEval> curve = std::make_unique<CurveEval>();
  std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();
  spline->resize(4);
  spline->radii().fill(1.0f);
  spline->tilts().fill(0.0f);
  spline->set_cyclic(true);
  MutableSpan<float3> positions = spline->positions();

  switch (mode) {
    case GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_RECTANGLE:
      create_rectangle_curve(positions,
                             std::max(params.extract_input<float>("Height"), 0.0f),
                             std::max(params.extract_input<float>("Width"), 0.0f));
      break;

    case GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_PARALLELOGRAM:
      create_parallelogram_curve(positions,
                                 std::max(params.extract_input<float>("Height"), 0.0f),
                                 std::max(params.extract_input<float>("Width"), 0.0f),
                                 params.extract_input<float>("Offset"));
      break;
    case GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_TRAPEZOID:
      create_trapezoid_curve(positions,
                             std::max(params.extract_input<float>("Bottom Width"), 0.0f),
                             std::max(params.extract_input<float>("Top Width"), 0.0f),
                             params.extract_input<float>("Offset"),
                             std::max(params.extract_input<float>("Height"), 0.0f));
      break;
    case GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_KITE:
      create_kite_curve(positions,
                        std::max(params.extract_input<float>("Width"), 0.0f),
                        std::max(params.extract_input<float>("Bottom Height"), 0.0f),
                        params.extract_input<float>("Top Height"));
      break;
    case GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_POINTS:
      create_points_curve(positions,
                          params.extract_input<float3>("Point 1"),
                          params.extract_input<float3>("Point 2"),
                          params.extract_input<float3>("Point 3"),
                          params.extract_input<float3>("Point 4"));
      break;
    default:
      params.set_output("Curve", GeometrySet());
      return;
  }

  curve->add_spline(std::move(spline));
  curve->attributes.reallocate(curve->splines().size());
  params.set_output("Curve", GeometrySet::create_with_curve(curve.release()));
}

}  // namespace blender::nodes

void register_node_type_geo_curve_primitive_quadrilateral()
{
  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_CURVE_PRIMITIVE_QUADRILATERAL, "Quadrilateral", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_curve_primitive_quadrilateral_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_primitive_quadrilateral_exec;
  ntype.draw_buttons = blender::nodes::geo_node_curve_primitive_quadrilateral_layout;
  node_type_update(&ntype, blender::nodes::geo_node_curve_primitive_quadrilateral_update);
  node_type_init(&ntype, blender::nodes::geo_node_curve_primitive_quadrilateral_init);
  node_type_storage(&ntype,
                    "NodeGeometryCurvePrimitiveQuad",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  nodeRegisterType(&ntype);
}

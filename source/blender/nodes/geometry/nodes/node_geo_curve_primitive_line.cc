/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_primitive_line_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurvePrimitiveLine)

static void node_declare(NodeDeclarationBuilder &b)
{
  auto enable_direction = [](bNode &node) {
    node_storage(node).mode = GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_DIRECTION;
  };

  b.add_input<decl::Vector>(N_("Start"))
      .subtype(PROP_TRANSLATION)
      .description(N_("Position of the first control point"));
  b.add_input<decl::Vector>(N_("End"))
      .default_value({0.0f, 0.0f, 1.0f})
      .subtype(PROP_TRANSLATION)
      .description(N_("Position of the second control point"))
      .make_available([](bNode &node) {
        node_storage(node).mode = GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_POINTS;
      });
  b.add_input<decl::Vector>(N_("Direction"))
      .default_value({0.0f, 0.0f, 1.0f})
      .description(N_("Direction the line is going in. The length of this vector does not matter"))
      .make_available(enable_direction);
  b.add_input<decl::Float>(N_("Length"))
      .default_value(1.0f)
      .subtype(PROP_DISTANCE)
      .description(N_("Distance between the two points"))
      .make_available(enable_direction);
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryCurvePrimitiveLine *data = MEM_cnew<NodeGeometryCurvePrimitiveLine>(__func__);

  data->mode = GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_POINTS;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryCurvePrimitiveLine &storage = node_storage(*node);
  const GeometryNodeCurvePrimitiveLineMode mode = (GeometryNodeCurvePrimitiveLineMode)storage.mode;

  bNodeSocket *p2_socket = static_cast<bNodeSocket *>(node->inputs.first)->next;
  bNodeSocket *direction_socket = p2_socket->next;
  bNodeSocket *length_socket = direction_socket->next;

  bke::nodeSetSocketAvailability(ntree, p2_socket, mode == GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_POINTS);
  bke::nodeSetSocketAvailability(
      ntree, direction_socket, mode == GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_DIRECTION);
  bke::nodeSetSocketAvailability(
      ntree, length_socket, mode == GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_DIRECTION);
}

static Curves *create_point_line_curve(const float3 start, const float3 end)
{
  Curves *curves_id = bke::curves_new_nomain_single(2, CURVE_TYPE_POLY);
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();

  curves.positions_for_write().first() = start;
  curves.positions_for_write().last() = end;

  return curves_id;
}

static Curves *create_direction_line_curve(const float3 start,
                                           const float3 direction,
                                           const float length)
{
  Curves *curves_id = bke::curves_new_nomain_single(2, CURVE_TYPE_POLY);
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();

  curves.positions_for_write().first() = start;
  curves.positions_for_write().last() = math::normalize(direction) * length + start;

  return curves_id;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurvePrimitiveLine &storage = node_storage(params.node());
  const GeometryNodeCurvePrimitiveLineMode mode = (GeometryNodeCurvePrimitiveLineMode)storage.mode;

  Curves *curves = nullptr;
  if (mode == GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_POINTS) {
    curves = create_point_line_curve(params.extract_input<float3>("Start"),
                                     params.extract_input<float3>("End"));
  }
  else if (mode == GEO_NODE_CURVE_PRIMITIVE_LINE_MODE_DIRECTION) {
    curves = create_direction_line_curve(params.extract_input<float3>("Start"),
                                         params.extract_input<float3>("Direction"),
                                         params.extract_input<float>("Length"));
  }

  params.set_output("Curve", GeometrySet::create_with_curves(curves));
}

}  // namespace blender::nodes::node_geo_curve_primitive_line_cc

void register_node_type_geo_curve_primitive_line()
{
  namespace file_ns = blender::nodes::node_geo_curve_primitive_line_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_CURVE_PRIMITIVE_LINE, "Curve Line", NODE_CLASS_GEOMETRY);
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  node_type_storage(&ntype,
                    "NodeGeometryCurvePrimitiveLine",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}

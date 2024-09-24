/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_primitive_quadrilateral_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurvePrimitiveQuad)

static void node_declare(NodeDeclarationBuilder &b)
{
  auto &width = b.add_input<decl::Float>("Width")
                    .default_value(2.0f)
                    .min(0.0f)
                    .subtype(PROP_DISTANCE)
                    .description("The X axis size of the shape");
  auto &height = b.add_input<decl::Float>("Height")
                     .default_value(2.0f)
                     .min(0.0f)
                     .subtype(PROP_DISTANCE)
                     .description("The Y axis size of the shape")
                     .available(false);
  auto &bottom = b.add_input<decl::Float>("Bottom Width")
                     .default_value(4.0f)
                     .min(0.0f)
                     .subtype(PROP_DISTANCE)
                     .description("The X axis size of the shape")
                     .available(false);
  auto &top = b.add_input<decl::Float>("Top Width")
                  .default_value(2.0f)
                  .min(0.0f)
                  .subtype(PROP_DISTANCE)
                  .description("The X axis size of the shape")
                  .available(false);
  auto &offset =
      b.add_input<decl::Float>("Offset")
          .default_value(1.0f)
          .subtype(PROP_DISTANCE)
          .description(
              "For Parallelogram, the relative X difference between the top and bottom edges. For "
              "Trapezoid, the amount to move the top edge in the positive X axis")
          .available(false);
  auto &bottom_height = b.add_input<decl::Float>("Bottom Height")
                            .default_value(3.0f)
                            .min(0.0f)
                            .subtype(PROP_DISTANCE)
                            .description("The distance between the bottom point and the X axis")
                            .available(false);
  auto &top_height = b.add_input<decl::Float>("Top Height")
                         .default_value(1.0f)
                         .subtype(PROP_DISTANCE)
                         .description("The distance between the top point and the X axis")
                         .available(false);
  auto &p1 = b.add_input<decl::Vector>("Point 1")
                 .default_value({-1.0f, -1.0f, 0.0f})
                 .subtype(PROP_TRANSLATION)
                 .description("The exact location of the point to use")
                 .available(false);
  auto &p2 = b.add_input<decl::Vector>("Point 2")
                 .default_value({1.0f, -1.0f, 0.0f})
                 .subtype(PROP_TRANSLATION)
                 .description("The exact location of the point to use")
                 .available(false);
  auto &p3 = b.add_input<decl::Vector>("Point 3")
                 .default_value({1.0f, 1.0f, 0.0f})
                 .subtype(PROP_TRANSLATION)
                 .description("The exact location of the point to use")
                 .available(false);
  auto &p4 = b.add_input<decl::Vector>("Point 4")
                 .default_value({-1.0f, 1.0f, 0.0f})
                 .subtype(PROP_TRANSLATION)
                 .description("The exact location of the point to use")
                 .available(false);
  b.add_output<decl::Geometry>("Curve");

  const bNode *node = b.node_or_null();
  if (node != nullptr) {
    const NodeGeometryCurvePrimitiveQuad &storage = node_storage(*node);
    switch (GeometryNodeCurvePrimitiveQuadMode(storage.mode)) {
      case GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_RECTANGLE:
        width.available(true);
        height.available(true);
        break;
      case GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_PARALLELOGRAM:
        width.available(true);
        height.available(true);
        offset.available(true);
        break;
      case GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_TRAPEZOID:
        bottom.available(true);
        top.available(true);
        offset.available(true);
        height.available(true);
        break;
      case GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_KITE:
        width.available(true);
        bottom_height.available(true);
        top_height.available(true);
        break;
      case GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_POINTS:
        p1.available(true);
        p2.available(true);
        p3.available(true);
        p4.available(true);
        break;
    }
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryCurvePrimitiveQuad *data = MEM_cnew<NodeGeometryCurvePrimitiveQuad>(__func__);
  data->mode = GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_RECTANGLE;
  node->storage = data;
}

class SocketSearchOp {
 public:
  std::string socket_name;
  GeometryNodeCurvePrimitiveQuadMode quad_mode;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("GeometryNodeCurvePrimitiveQuadrilateral");
    node_storage(node).mode = quad_mode;
    params.update_and_connect_available_socket(node, socket_name);
  }
};

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().static_declaration;
  if (params.in_out() == SOCK_OUT) {
    search_link_ops_for_declarations(params, declaration.outputs);
  }
  else if (params.node_tree().typeinfo->validate_link(
               eNodeSocketDatatype(params.other_socket().type), SOCK_FLOAT))
  {
    params.add_item(IFACE_("Width"),
                    SocketSearchOp{"Width", GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_RECTANGLE});
    params.add_item(IFACE_("Height"),
                    SocketSearchOp{"Height", GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_RECTANGLE});
    params.add_item(IFACE_("Bottom Width"),
                    SocketSearchOp{"Bottom Width", GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_TRAPEZOID});
    params.add_item(IFACE_("Top Width"),
                    SocketSearchOp{"Top Width", GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_TRAPEZOID});
    params.add_item(IFACE_("Offset"),
                    SocketSearchOp{"Offset", GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_PARALLELOGRAM});
    params.add_item(IFACE_("Point 1"),
                    SocketSearchOp{"Point 1", GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_POINTS});
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

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurvePrimitiveQuad &storage = node_storage(params.node());
  const GeometryNodeCurvePrimitiveQuadMode mode = (GeometryNodeCurvePrimitiveQuadMode)storage.mode;

  Curves *curves_id = bke::curves_new_nomain_single(4, CURVE_TYPE_POLY);
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  curves.cyclic_for_write().first() = true;

  MutableSpan<float3> positions = curves.positions_for_write();

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
      params.set_default_remaining_outputs();
      return;
  }

  params.set_output("Curve", GeometrySet::from_curves(curves_id));
}

static void node_rna(StructRNA *srna)
{
  static EnumPropertyItem mode_items[] = {
      {GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_RECTANGLE,
       "RECTANGLE",
       0,
       "Rectangle",
       "Create a rectangle"},
      {GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_PARALLELOGRAM,
       "PARALLELOGRAM",
       0,
       "Parallelogram",
       "Create a parallelogram"},
      {GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_TRAPEZOID,
       "TRAPEZOID",
       0,
       "Trapezoid",
       "Create a trapezoid"},
      {GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_KITE, "KITE", 0, "Kite", "Create a Kite / Dart"},
      {GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_POINTS,
       "POINTS",
       0,
       "Points",
       "Create a quadrilateral from four points"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "mode",
                    "Mode",
                    "",
                    mode_items,
                    NOD_storage_enum_accessors(mode),
                    GEO_NODE_CURVE_PRIMITIVE_QUAD_MODE_RECTANGLE);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_CURVE_PRIMITIVE_QUADRILATERAL, "Quadrilateral", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(&ntype,
                                  "NodeGeometryCurvePrimitiveQuad",
                                  node_free_standard_storage,
                                  node_copy_standard_storage);
  ntype.gather_link_search_ops = node_gather_link_searches;
  blender::bke::node_register_type(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_primitive_quadrilateral_cc

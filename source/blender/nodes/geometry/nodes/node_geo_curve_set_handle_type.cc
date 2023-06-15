/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <atomic>

#include "BKE_curves.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_set_handle_type_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveSetHandles)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(GeometryComponent::Type::Curve);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "handle_type", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryCurveSetHandles *data = MEM_cnew<NodeGeometryCurveSetHandles>(__func__);

  data->handle_type = GEO_NODE_CURVE_HANDLE_AUTO;
  data->mode = GEO_NODE_CURVE_HANDLE_LEFT | GEO_NODE_CURVE_HANDLE_RIGHT;
  node->storage = data;
}

static HandleType handle_type_from_input_type(GeometryNodeCurveHandleType type)
{
  switch (type) {
    case GEO_NODE_CURVE_HANDLE_AUTO:
      return BEZIER_HANDLE_AUTO;
    case GEO_NODE_CURVE_HANDLE_ALIGN:
      return BEZIER_HANDLE_ALIGN;
    case GEO_NODE_CURVE_HANDLE_FREE:
      return BEZIER_HANDLE_FREE;
    case GEO_NODE_CURVE_HANDLE_VECTOR:
      return BEZIER_HANDLE_VECTOR;
  }
  BLI_assert_unreachable();
  return BEZIER_HANDLE_AUTO;
}

static void set_handle_type(bke::CurvesGeometry &curves,
                            const GeometryNodeCurveHandleMode mode,
                            const HandleType new_handle_type,
                            const Field<bool> &selection_field)
{
  const bke::CurvesFieldContext field_context{curves, ATTR_DOMAIN_POINT};
  fn::FieldEvaluator evaluator{field_context, curves.points_num()};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  if (mode & GEO_NODE_CURVE_HANDLE_LEFT) {
    index_mask::masked_fill<int8_t>(
        curves.handle_types_left_for_write(), new_handle_type, selection);
  }
  if (mode & GEO_NODE_CURVE_HANDLE_RIGHT) {
    index_mask::masked_fill<int8_t>(
        curves.handle_types_right_for_write(), new_handle_type, selection);
  }

  /* Eagerly calculate automatically derived handle positions if necessary. */
  if (ELEM(new_handle_type, BEZIER_HANDLE_AUTO, BEZIER_HANDLE_VECTOR, BEZIER_HANDLE_ALIGN)) {
    curves.calculate_bezier_auto_handles();
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurveSetHandles &storage = node_storage(params.node());
  const GeometryNodeCurveHandleType type = (GeometryNodeCurveHandleType)storage.handle_type;
  const GeometryNodeCurveHandleMode mode = (GeometryNodeCurveHandleMode)storage.mode;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  const HandleType new_handle_type = handle_type_from_input_type(type);

  std::atomic<bool> has_curves = false;
  std::atomic<bool> has_bezier = false;

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Curves *curves_id = geometry_set.get_curves_for_write()) {
      bke::CurvesGeometry &curves = curves_id->geometry.wrap();
      has_curves = true;
      const AttributeAccessor attributes = curves.attributes();
      if (!attributes.contains("handle_type_left") || !attributes.contains("handle_type_right")) {
        return;
      }
      has_bezier = true;

      set_handle_type(curves, mode, new_handle_type, selection_field);
    }
  });

  if (has_curves && !has_bezier) {
    params.error_message_add(NodeWarningType::Info, TIP_("Input curves do not have Bezier type"));
  }

  params.set_output("Curve", std::move(geometry_set));
}
}  // namespace blender::nodes::node_geo_curve_set_handle_type_cc

void register_node_type_geo_curve_set_handle_type()
{
  namespace file_ns = blender::nodes::node_geo_curve_set_handle_type_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_CURVE_SET_HANDLE_TYPE, "Set Handle Type", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.initfunc = file_ns::node_init;
  node_type_storage(&ntype,
                    "NodeGeometryCurveSetHandles",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.draw_buttons = file_ns::node_layout;

  nodeRegisterType(&ntype);
}

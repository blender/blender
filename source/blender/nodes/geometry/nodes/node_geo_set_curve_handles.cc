/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <atomic>

#include "BKE_curves.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_curve_handles_cc {

NODE_STORAGE_FUNCS(NodeGeometrySetCurveHandlePositions)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_input<decl::Vector>(N_("Position")).implicit_field();
  b.add_input<decl::Vector>(N_("Offset")).default_value(float3(0.0f, 0.0f, 0.0f)).supports_field();
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometrySetCurveHandlePositions *data = MEM_cnew<NodeGeometrySetCurveHandlePositions>(
      __func__);

  data->mode = GEO_NODE_CURVE_HANDLE_LEFT;
  node->storage = data;
}

static void update_handle_types_for_movement(int8_t &type, int8_t &other)
{
  switch (type) {
    case BEZIER_HANDLE_FREE:
      break;
    case BEZIER_HANDLE_AUTO:
      /* Converting auto handles to aligned handled instead of free handles is
       * arbitrary, but expected and "standard" based on behavior in edit mode. */
      if (other == BEZIER_HANDLE_AUTO) {
        /* Convert pairs of auto handles to aligned handles when moving one side. */
        type = BEZIER_HANDLE_ALIGN;
        other = BEZIER_HANDLE_ALIGN;
      }
      else {
        /* If the other handle isn't automatic, just make the handle free. */
        type = BEZIER_HANDLE_FREE;
      }
      break;
    case BEZIER_HANDLE_VECTOR:
      type = BEZIER_HANDLE_FREE;
      break;
    case BEZIER_HANDLE_ALIGN:
      /* The handle can stay aligned if the other handle is also aligned (in which case the other
       * handle should be updated to be consistent). But otherwise the handle must be made free to
       * avoid conflicting with its "aligned" type. */
      if (other != BEZIER_HANDLE_ALIGN) {
        type = BEZIER_HANDLE_FREE;
      }
      break;
  }
}

static void set_position_in_component(CurveComponent &component,
                                      const GeometryNodeCurveHandleMode mode,
                                      const Field<bool> &selection_field,
                                      const Field<float3> &position_field,
                                      const Field<float3> &offset_field)
{
  GeometryComponentFieldContext field_context{component, ATTR_DOMAIN_POINT};
  const int domain_size = component.attribute_domain_size(ATTR_DOMAIN_POINT);
  if (domain_size == 0) {
    return;
  }

  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);
  evaluator.add(position_field);
  evaluator.add(offset_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const VArray<float3> new_positions = evaluator.get_evaluated<float3>(0);
  const VArray<float3> new_offsets = evaluator.get_evaluated<float3>(1);

  Curves &curves_id = *component.get_for_write();
  bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id.geometry);

  Span<float3> positions = curves.positions();

  const bool use_left = mode == GEO_NODE_CURVE_HANDLE_LEFT;
  MutableSpan<int8_t> handle_types = use_left ? curves.handle_types_left_for_write() :
                                                curves.handle_types_right_for_write();
  MutableSpan<int8_t> handle_types_other = use_left ? curves.handle_types_right_for_write() :
                                                      curves.handle_types_left_for_write();
  MutableSpan<float3> handle_positions = use_left ? curves.handle_positions_left_for_write() :
                                                    curves.handle_positions_right_for_write();
  MutableSpan<float3> handle_positions_other = use_left ?
                                                   curves.handle_positions_right_for_write() :
                                                   curves.handle_positions_left_for_write();

  threading::parallel_for(selection.index_range(), 2048, [&](IndexRange range) {
    for (const int i : selection.slice(range)) {
      update_handle_types_for_movement(handle_types[i], handle_types_other[i]);
    }
  });

  threading::parallel_for(selection.index_range(), 2048, [&](IndexRange range) {
    for (const int i : selection.slice(range)) {
      bke::curves::bezier::set_handle_position(positions[i],
                                               HandleType(handle_types[i]),
                                               HandleType(handle_types_other[i]),
                                               new_positions[i] + new_offsets[i],
                                               handle_positions[i],
                                               handle_positions_other[i]);
    }
  });

  curves.calculate_bezier_auto_handles();

  curves.tag_positions_changed();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometrySetCurveHandlePositions &storage = node_storage(params.node());
  const GeometryNodeCurveHandleMode mode = (GeometryNodeCurveHandleMode)storage.mode;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<float3> position_field = params.extract_input<Field<float3>>("Position");
  Field<float3> offset_field = params.extract_input<Field<float3>>("Offset");

  std::atomic<bool> has_curves = false;
  std::atomic<bool> has_bezier = false;

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_curves()) {
      return;
    }
    has_curves = true;
    const CurveComponent &component = *geometry_set.get_component_for_read<CurveComponent>();
    const AttributeAccessor attributes = *component.attributes();
    if (!attributes.contains("handle_left") || !attributes.contains("handle_right")) {
      return;
    }
    has_bezier = true;

    set_position_in_component(geometry_set.get_component_for_write<CurveComponent>(),
                              mode,
                              selection_field,
                              position_field,
                              offset_field);
  });

  if (has_curves && !has_bezier) {
    params.error_message_add(NodeWarningType::Info, TIP_("Input curves do not have Bezier type"));
  }

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_set_curve_handles_cc

void register_node_type_geo_set_curve_handles()
{
  namespace file_ns = blender::nodes::node_geo_set_curve_handles_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SET_CURVE_HANDLES, "Set Handle Positions", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  ntype.minwidth = 100.0f;
  node_type_init(&ntype, file_ns::node_init);
  node_type_storage(&ntype,
                    "NodeGeometrySetCurveHandlePositions",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}

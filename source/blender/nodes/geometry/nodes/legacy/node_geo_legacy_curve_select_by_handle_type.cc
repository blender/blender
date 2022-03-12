/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "BKE_spline.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_legacy_curve_select_by_handle_type_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("Selection"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "handle_type", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveSelectHandles *data = MEM_cnew<NodeGeometryCurveSelectHandles>(__func__);

  data->handle_type = GEO_NODE_CURVE_HANDLE_AUTO;
  data->mode = GEO_NODE_CURVE_HANDLE_LEFT | GEO_NODE_CURVE_HANDLE_RIGHT;
  node->storage = data;
}

static HandleType handle_type_from_input_type(const GeometryNodeCurveHandleType type)
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

static void select_curve_by_handle_type(const CurveEval &curve,
                                        const HandleType type,
                                        const GeometryNodeCurveHandleMode mode,
                                        const MutableSpan<bool> r_selection)
{
  const Array<int> offsets = curve.control_point_offsets();
  Span<SplinePtr> splines = curve.splines();
  threading::parallel_for(splines.index_range(), 128, [&](IndexRange range) {
    for (const int i_spline : range) {
      const Spline &spline = *splines[i_spline];
      if (spline.type() == CURVE_TYPE_BEZIER) {
        const BezierSpline &bezier_spline = static_cast<const BezierSpline &>(spline);
        Span<int8_t> types_left = bezier_spline.handle_types_left();
        Span<int8_t> types_right = bezier_spline.handle_types_right();
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

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurveSelectHandles *storage =
      (const NodeGeometryCurveSelectHandles *)params.node().storage;
  const HandleType handle_type = handle_type_from_input_type(
      (GeometryNodeCurveHandleType)storage->handle_type);
  const GeometryNodeCurveHandleMode mode = (GeometryNodeCurveHandleMode)storage->mode;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  geometry_set = geometry::realize_instances_legacy(geometry_set);

  CurveComponent &curve_component = geometry_set.get_component_for_write<CurveComponent>();
  if (curve_component.has_curves()) {
    const std::unique_ptr<CurveEval> curve = curves_to_curve_eval(*curve_component.get_for_read());
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

}  // namespace blender::nodes::node_geo_legacy_curve_select_by_handle_type_cc

void register_node_type_geo_legacy_select_by_handle_type()
{
  namespace file_ns = blender::nodes::node_geo_legacy_curve_select_by_handle_type_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_CURVE_SELECT_HANDLES, "Select by Handle Type", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  node_type_init(&ntype, file_ns::node_init);
  node_type_storage(&ntype,
                    "NodeGeometryCurveSelectHandles",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.draw_buttons = file_ns::node_layout;

  nodeRegisterType(&ntype);
}

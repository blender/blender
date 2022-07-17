/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_handle_type_selection_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveSelectHandles)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Bool>(N_("Selection")).field_source();
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

static void select_by_handle_type(const bke::CurvesGeometry &curves,
                                  const HandleType type,
                                  const GeometryNodeCurveHandleMode mode,
                                  const MutableSpan<bool> r_selection)
{
  VArray<int8_t> curve_types = curves.curve_types();
  VArray<int8_t> left = curves.handle_types_left();
  VArray<int8_t> right = curves.handle_types_right();

  for (const int i_curve : curves.curves_range()) {
    const IndexRange points = curves.points_for_curve(i_curve);
    if (curve_types[i_curve] != CURVE_TYPE_BEZIER) {
      r_selection.slice(points).fill(false);
    }
    else {
      for (const int i_point : points) {
        r_selection[i_point] = (mode & GEO_NODE_CURVE_HANDLE_LEFT && left[i_point] == type) ||
                               (mode & GEO_NODE_CURVE_HANDLE_RIGHT && right[i_point] == type);
      }
    }
  }
}

class HandleTypeFieldInput final : public GeometryFieldInput {
  HandleType type_;
  GeometryNodeCurveHandleMode mode_;

 public:
  HandleTypeFieldInput(HandleType type, GeometryNodeCurveHandleMode mode)
      : GeometryFieldInput(CPPType::get<bool>(), "Handle Type Selection node"),
        type_(type),
        mode_(mode)
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const eAttrDomain domain,
                                 IndexMask mask) const final
  {
    if (component.type() != GEO_COMPONENT_TYPE_CURVE || domain != ATTR_DOMAIN_POINT) {
      return {};
    }

    const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
    const Curves *curves_id = curve_component.get_for_read();
    if (curves_id == nullptr) {
      return {};
    }

    Array<bool> selection(mask.min_array_size());
    select_by_handle_type(bke::CurvesGeometry::wrap(curves_id->geometry), type_, mode_, selection);
    return VArray<bool>::ForContainer(std::move(selection));
  }

  uint64_t hash() const override
  {
    return get_default_hash_2((int)mode_, (int)type_);
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    if (const HandleTypeFieldInput *other_handle_selection =
            dynamic_cast<const HandleTypeFieldInput *>(&other)) {
      return mode_ == other_handle_selection->mode_ && type_ == other_handle_selection->type_;
    }
    return false;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurveSelectHandles &storage = node_storage(params.node());
  const HandleType handle_type = handle_type_from_input_type(
      (GeometryNodeCurveHandleType)storage.handle_type);
  const GeometryNodeCurveHandleMode mode = (GeometryNodeCurveHandleMode)storage.mode;

  Field<bool> selection_field{std::make_shared<HandleTypeFieldInput>(handle_type, mode)};
  params.set_output("Selection", std::move(selection_field));
}

}  // namespace blender::nodes::node_geo_curve_handle_type_selection_cc

void register_node_type_geo_curve_handle_type_selection()
{
  namespace file_ns = blender::nodes::node_geo_curve_handle_type_selection_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_CURVE_HANDLE_TYPE_SELECTION, "Handle Type Selection", NODE_CLASS_INPUT);
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

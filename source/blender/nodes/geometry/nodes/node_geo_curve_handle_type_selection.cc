/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_spline.hh"

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

static void select_by_handle_type(const CurveEval &curve,
                                  const HandleType type,
                                  const GeometryNodeCurveHandleMode mode,
                                  const MutableSpan<bool> r_selection)
{
  int offset = 0;
  for (const SplinePtr &spline : curve.splines()) {
    if (spline->type() != CURVE_TYPE_BEZIER) {
      r_selection.slice(offset, spline->size()).fill(false);
      offset += spline->size();
    }
    else {
      BezierSpline *b = static_cast<BezierSpline *>(spline.get());
      for (int i : IndexRange(b->size())) {
        r_selection[offset++] = (mode & GEO_NODE_CURVE_HANDLE_LEFT &&
                                 b->handle_types_left()[i] == type) ||
                                (mode & GEO_NODE_CURVE_HANDLE_RIGHT &&
                                 b->handle_types_right()[i] == type);
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
                                 const AttributeDomain domain,
                                 IndexMask mask) const final
  {
    if (component.type() != GEO_COMPONENT_TYPE_CURVE) {
      return {};
    }

    const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
    const Curves *curve = curve_component.get_for_read();
    if (curve == nullptr) {
      return {};
    }

    if (domain == ATTR_DOMAIN_POINT) {
      Array<bool> selection(mask.min_array_size());
      select_by_handle_type(*curves_to_curve_eval(*curve), type_, mode_, selection);
      return VArray<bool>::ForContainer(std::move(selection));
    }
    return {};
  }

  uint64_t hash() const override
  {
    return get_default_hash_2((int)mode_, (int)type_);
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const HandleTypeFieldInput *>(&other) != nullptr;
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

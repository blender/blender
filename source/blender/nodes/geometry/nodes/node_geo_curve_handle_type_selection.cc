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

static void geo_node_curve_handle_type_selection_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Bool>("Selection").field_source();
}

static void geo_node_curve_handle_type_selection_layout(uiLayout *layout,
                                                        bContext *UNUSED(C),
                                                        PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "handle_type", 0, "", ICON_NONE);
}

static void geo_node_curve_handle_type_selection_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveSelectHandles *data = (NodeGeometryCurveSelectHandles *)MEM_callocN(
      sizeof(NodeGeometryCurveSelectHandles), __func__);

  data->handle_type = GEO_NODE_CURVE_HANDLE_AUTO;
  data->mode = GEO_NODE_CURVE_HANDLE_LEFT | GEO_NODE_CURVE_HANDLE_RIGHT;
  node->storage = data;
}

static BezierSpline::HandleType handle_type_from_input_type(const GeometryNodeCurveHandleType type)
{
  switch (type) {
    case GEO_NODE_CURVE_HANDLE_AUTO:
      return BezierSpline::HandleType::Auto;
    case GEO_NODE_CURVE_HANDLE_ALIGN:
      return BezierSpline::HandleType::Align;
    case GEO_NODE_CURVE_HANDLE_FREE:
      return BezierSpline::HandleType::Free;
    case GEO_NODE_CURVE_HANDLE_VECTOR:
      return BezierSpline::HandleType::Vector;
  }
  BLI_assert_unreachable();
  return BezierSpline::HandleType::Auto;
}

static void select_by_handle_type(const CurveEval &curve,
                                  const BezierSpline::HandleType type,
                                  const GeometryNodeCurveHandleMode mode,
                                  const MutableSpan<bool> r_selection)
{
  int offset = 0;
  for (const SplinePtr &spline : curve.splines()) {
    if (spline->type() != Spline::Type::Bezier) {
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

class HandleTypeFieldInput final : public fn::FieldInput {
  BezierSpline::HandleType type_;
  GeometryNodeCurveHandleMode mode_;

 public:
  HandleTypeFieldInput(BezierSpline::HandleType type, GeometryNodeCurveHandleMode mode)
      : FieldInput(CPPType::get<bool>(), "Handle Type Selection node"), type_(type), mode_(mode)
  {
    category_ = Category::Generated;
  }

  const GVArray *get_varray_for_context(const fn::FieldContext &context,
                                        IndexMask mask,
                                        ResourceScope &scope) const final
  {
    if (const GeometryComponentFieldContext *geometry_context =
            dynamic_cast<const GeometryComponentFieldContext *>(&context)) {

      const GeometryComponent &component = geometry_context->geometry_component();
      const AttributeDomain domain = geometry_context->domain();
      if (component.type() != GEO_COMPONENT_TYPE_CURVE) {
        return nullptr;
      }

      const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
      const CurveEval *curve = curve_component.get_for_read();
      if (curve == nullptr) {
        return nullptr;
      }

      if (domain == ATTR_DOMAIN_POINT) {
        Array<bool> selection(mask.min_array_size());
        select_by_handle_type(*curve, type_, mode_, selection);
        return &scope.construct<fn::GVArray_For_ArrayContainer<Array<bool>>>(std::move(selection));
      }
    }
    return nullptr;
  };

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

static void geo_node_curve_handle_type_selection_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurveSelectHandles *storage =
      (const NodeGeometryCurveSelectHandles *)params.node().storage;
  const BezierSpline::HandleType handle_type = handle_type_from_input_type(
      (GeometryNodeCurveHandleType)storage->handle_type);
  const GeometryNodeCurveHandleMode mode = (GeometryNodeCurveHandleMode)storage->mode;

  Field<bool> selection_field{std::make_shared<HandleTypeFieldInput>(handle_type, mode)};
  params.set_output("Selection", std::move(selection_field));
}

}  // namespace blender::nodes

void register_node_type_geo_curve_handle_type_selection()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype,
                     GEO_NODE_CURVE_HANDLE_TYPE_SELECTION,
                     "Handle Type Selection",
                     NODE_CLASS_GEOMETRY,
                     0);
  ntype.declare = blender::nodes::geo_node_curve_handle_type_selection_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_curve_handle_type_selection_exec;
  node_type_init(&ntype, blender::nodes::geo_node_curve_handle_type_selection_init);
  node_type_storage(&ntype,
                    "NodeGeometryCurveSelectHandles",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.draw_buttons = blender::nodes::geo_node_curve_handle_type_selection_layout;

  nodeRegisterType(&ntype);
}

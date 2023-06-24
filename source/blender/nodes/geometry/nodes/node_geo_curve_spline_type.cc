/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "GEO_set_curve_type.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_spline_type_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveSplineType)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(GeometryComponent::Type::Curve);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "spline_type", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryCurveSplineType *data = MEM_cnew<NodeGeometryCurveSplineType>(__func__);

  data->spline_type = CURVE_TYPE_POLY;
  node->storage = data;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurveSplineType &storage = node_storage(params.node());
  const CurveType dst_type = CurveType(storage.spline_type);

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_curves()) {
      return;
    }
    const Curves &src_curves_id = *geometry_set.get_curves_for_read();
    const bke::CurvesGeometry &src_curves = src_curves_id.geometry.wrap();
    if (src_curves.is_single_type(dst_type)) {
      return;
    }

    const bke::CurvesFieldContext field_context{src_curves, ATTR_DOMAIN_CURVE};
    fn::FieldEvaluator evaluator{field_context, src_curves.curves_num()};
    evaluator.set_selection(selection_field);
    evaluator.evaluate();
    const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
    if (selection.is_empty()) {
      return;
    }

    if (geometry::try_curves_conversion_in_place(
            selection, dst_type, [&]() -> bke::CurvesGeometry & {
              return geometry_set.get_curves_for_write()->geometry.wrap();
            }))
    {
      return;
    }

    bke::CurvesGeometry dst_curves = geometry::convert_curves(
        src_curves, selection, dst_type, params.get_output_propagation_info("Curve"));

    Curves *dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
    bke::curves_copy_parameters(src_curves_id, *dst_curves_id);
    geometry_set.replace_curves(dst_curves_id);
  });

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_curve_spline_type_cc

void register_node_type_geo_curve_spline_type()
{
  namespace file_ns = blender::nodes::node_geo_curve_spline_type_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_CURVE_SPLINE_TYPE, "Set Spline Type", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.initfunc = file_ns::node_init;
  node_type_storage(&ntype,
                    "NodeGeometryCurveSplineType",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.draw_buttons = file_ns::node_layout;

  nodeRegisterType(&ntype);
}

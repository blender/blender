/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_curve_normal_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(GeometryComponent::Type::Curve);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = NORMAL_MODE_MINIMUM_TWIST;
}

static void set_normal_mode(bke::CurvesGeometry &curves,
                            const NormalMode mode,
                            const Field<bool> &selection_field)
{
  const bke::CurvesFieldContext field_context{curves, ATTR_DOMAIN_CURVE};
  fn::FieldEvaluator evaluator{field_context, curves.curves_num()};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  index_mask::masked_fill<int8_t>(curves.normal_mode_for_write(), mode, selection);
  curves.tag_normals_changed();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NormalMode mode = static_cast<NormalMode>(params.node().custom1);

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Curves *curves_id = geometry_set.get_curves_for_write()) {
      bke::CurvesGeometry &curves = curves_id->geometry.wrap();
      set_normal_mode(curves, mode, selection_field);
    }
  });

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_set_curve_normal_cc

void register_node_type_geo_set_curve_normal()
{
  namespace file_ns = blender::nodes::node_geo_set_curve_normal_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_SET_CURVE_NORMAL, "Set Curve Normal", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.initfunc = file_ns::node_init;
  ntype.draw_buttons = file_ns::node_layout;

  nodeRegisterType(&ntype);
}

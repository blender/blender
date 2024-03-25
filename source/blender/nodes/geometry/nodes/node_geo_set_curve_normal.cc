/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_curve_normal_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(
      {GeometryComponent::Type::Curve, GeometryComponent::Type::GreasePencil});
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Vector>("Normal")
      .default_value({0.0f, 0.0f, 1.0f})
      .subtype(PROP_XYZ)
      .field_on_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = NORMAL_MODE_MINIMUM_TWIST;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NormalMode mode = NormalMode(node->custom1);
  bNodeSocket *normal_socket = static_cast<bNodeSocket *>(node->inputs.last);
  bke::nodeSetSocketAvailability(ntree, normal_socket, mode == NORMAL_MODE_FREE);
}

static void set_curve_normal(bke::CurvesGeometry &curves,
                             const NormalMode mode,
                             const fn::FieldContext &curve_context,
                             const fn::FieldContext &point_context,
                             const Field<bool> &selection_field,
                             const Field<float3> &custom_normal)
{
  bke::try_capture_field_on_geometry(curves.attributes_for_write(),
                                     curve_context,
                                     "normal_mode",
                                     AttrDomain::Curve,
                                     selection_field,
                                     fn::make_constant_field<int8_t>(mode));

  if (mode == NORMAL_MODE_FREE) {
    bke::try_capture_field_on_geometry(curves.attributes_for_write(),
                                       point_context,
                                       "custom_normal",
                                       AttrDomain::Point,
                                       Field<bool>(std::make_shared<bke::EvaluateOnDomainInput>(
                                           selection_field, AttrDomain::Curve)),
                                       custom_normal);
  }

  curves.tag_normals_changed();
}

static void set_grease_pencil_normal(GreasePencil &grease_pencil,
                                     const NormalMode mode,
                                     const Field<bool> &selection_field,
                                     const Field<float3> &custom_normal)
{
  using namespace blender::bke::greasepencil;
  for (const int layer_index : grease_pencil.layers().index_range()) {
    Drawing *drawing = get_eval_grease_pencil_layer_drawing_for_write(grease_pencil, layer_index);
    if (drawing == nullptr) {
      continue;
    }
    set_curve_normal(
        drawing->strokes_for_write(),
        mode,
        bke::GreasePencilLayerFieldContext(grease_pencil, AttrDomain::Curve, layer_index),
        bke::GreasePencilLayerFieldContext(grease_pencil, AttrDomain::Point, layer_index),
        selection_field,
        custom_normal);
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NormalMode mode = static_cast<NormalMode>(params.node().custom1);

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<float3> custom_normal;
  if (mode == NORMAL_MODE_FREE) {
    custom_normal = params.extract_input<Field<float3>>("Normal");
  }

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (Curves *curves_id = geometry_set.get_curves_for_write()) {
      bke::CurvesGeometry &curves = curves_id->geometry.wrap();
      set_curve_normal(curves,
                       mode,
                       bke::CurvesFieldContext(curves, AttrDomain::Curve),
                       bke::CurvesFieldContext(curves, AttrDomain::Point),
                       selection_field,
                       custom_normal);
    }
    if (GreasePencil *grease_pencil = geometry_set.get_grease_pencil_for_write()) {
      set_grease_pencil_normal(*grease_pencil, mode, selection_field, custom_normal);
    }
  });

  params.set_output("Curve", std::move(geometry_set));
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "mode",
                    "Mode",
                    "Mode for curve normal evaluation",
                    rna_enum_curve_normal_mode_items,
                    NOD_inline_enum_accessors(custom1));
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_SET_CURVE_NORMAL, "Set Curve Normal", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.updatefunc = node_update;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;

  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_curve_normal_cc

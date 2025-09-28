/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "RNA_enum_types.hh"

#include "GEO_foreach_geometry.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_curve_normal_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Curve")
      .supported_type({GeometryComponent::Type::Curve, GeometryComponent::Type::GreasePencil})
      .description("Curves to change the normals on");
  b.add_output<decl::Geometry>("Curve").propagate_all().align_with_previous();
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Menu>("Mode")
      .static_items(rna_enum_curve_normal_mode_items)
      .optional_label()
      .description("Mode for curve normal evaluation");
  b.add_input<decl::Vector>("Normal")
      .default_value({0.0f, 0.0f, 1.0f})
      .subtype(PROP_XYZ)
      .field_on_all()
      .usage_by_single_menu(NORMAL_MODE_FREE);
}

static void set_curve_normal(bke::CurvesGeometry &curves,
                             const NormalMode mode,
                             const fn::FieldContext &curve_context,
                             const fn::FieldContext &point_context,
                             const Field<bool> &selection_field,
                             const Field<float3> &custom_normal)
{
  /* First evaluate the normal modes without changing the geometry, since that will influence the
   * result of the "Normal" node if used in the input to the custom normal field evaluation. */
  fn::FieldEvaluator evaluator(curve_context, curves.curves_num());
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask curve_mask = evaluator.get_evaluated_selection_as_mask();

  if (mode == NORMAL_MODE_FREE) {
    bke::try_capture_field_on_geometry(curves.attributes_for_write(),
                                       point_context,
                                       "custom_normal",
                                       AttrDomain::Point,
                                       Field<bool>(std::make_shared<bke::EvaluateOnDomainInput>(
                                           selection_field, AttrDomain::Curve)),
                                       custom_normal);
  }

  index_mask::masked_fill(curves.normal_mode_for_write(), int8_t(mode), curve_mask);

  curves.tag_normals_changed();
}

static void set_grease_pencil_normal(GreasePencil &grease_pencil,
                                     const NormalMode mode,
                                     const Field<bool> &selection_field,
                                     const Field<float3> &custom_normal)
{
  using namespace blender::bke::greasepencil;
  for (const int layer_index : grease_pencil.layers().index_range()) {
    Drawing *drawing = grease_pencil.get_eval_drawing(grease_pencil.layer(layer_index));
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
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  const NormalMode mode = params.get_input<NormalMode>("Mode");
  Field<float3> custom_normal;
  if (mode == NORMAL_MODE_FREE) {
    custom_normal = params.extract_input<Field<float3>>("Normal");
  }

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
    if (Curves *curves_id = geometry_set.get_curves_for_write()) {
      bke::CurvesGeometry &curves = curves_id->geometry.wrap();
      set_curve_normal(curves,
                       mode,
                       bke::CurvesFieldContext(*curves_id, AttrDomain::Curve),
                       bke::CurvesFieldContext(*curves_id, AttrDomain::Point),
                       selection_field,
                       custom_normal);
    }
    if (GreasePencil *grease_pencil = geometry_set.get_grease_pencil_for_write()) {
      set_grease_pencil_normal(*grease_pencil, mode, selection_field, custom_normal);
    }
  });

  params.set_output("Curve", std::move(geometry_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeSetCurveNormal", GEO_NODE_SET_CURVE_NORMAL);
  ntype.ui_name = "Set Curve Normal";
  ntype.ui_description = "Set the evaluation mode for curve normals";
  ntype.enum_name_legacy = "SET_CURVE_NORMAL";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_curve_normal_cc

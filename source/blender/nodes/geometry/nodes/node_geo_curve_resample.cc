/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_foreach_geometry.hh"
#include "GEO_resample_curves.hh"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_resample_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveResample)

static EnumPropertyItem mode_items[] = {
    {GEO_NODE_CURVE_RESAMPLE_EVALUATED,
     "EVALUATED",
     0,
     N_("Evaluated"),
     N_("Output the input spline's evaluated points, based on the resolution attribute for NURBS "
        "and Bézier splines. Poly splines are unchanged")},
    {GEO_NODE_CURVE_RESAMPLE_COUNT,
     "COUNT",
     0,
     N_("Count"),
     N_("Sample the specified number of points along each spline")},
    {GEO_NODE_CURVE_RESAMPLE_LENGTH,
     "LENGTH",
     0,
     N_("Length"),
     N_("Calculate the number of samples by splitting each spline into segments with the "
        "specified "
        "length")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Curve")
      .supported_type({GeometryComponent::Type::Curve, GeometryComponent::Type::GreasePencil})
      .description("Curves to resample");
  b.add_output<decl::Geometry>("Curve").propagate_all().align_with_previous();
  b.add_input<decl::Bool>("Selection").default_value(true).field_on_all().hide_value();
  b.add_input<decl::Menu>("Mode")
      .static_items(mode_items)
      .optional_label()
      .description("How to specify the amount of samples");
  b.add_input<decl::Int>("Count")
      .default_value(10)
      .min(1)
      .max(100000)
      .field_on_all()
      .usage_by_single_menu(GEO_NODE_CURVE_RESAMPLE_COUNT);
  b.add_input<decl::Float>("Length")
      .default_value(0.1f)
      .min(0.01f)
      .subtype(PROP_DISTANCE)
      .field_on_all()
      .usage_by_single_menu(GEO_NODE_CURVE_RESAMPLE_LENGTH);
}

static void node_layout_ex(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "keep_last_segment", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryCurveResample *data = MEM_callocN<NodeGeometryCurveResample>(__func__);
  data->keep_last_segment = true;
  node->storage = data;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  const auto mode = params.extract_input<GeometryNodeCurveResampleMode>("Mode");

  const NodeGeometryCurveResample &storage = node_storage(params.node());

  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");

  GeometryComponentEditData::remember_deformed_positions_if_necessary(geometry_set);

  switch (mode) {
    case GEO_NODE_CURVE_RESAMPLE_COUNT: {
      Field<int> count = params.extract_input<Field<int>>("Count");
      geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry) {
        if (const Curves *src_curves_id = geometry.get_curves()) {
          const bke::CurvesGeometry &src_curves = src_curves_id->geometry.wrap();
          const bke::CurvesFieldContext field_context{*src_curves_id, AttrDomain::Curve};
          bke::CurvesGeometry dst_curves = geometry::resample_to_count(
              src_curves, field_context, selection, count);
          Curves *dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
          bke::curves_copy_parameters(*src_curves_id, *dst_curves_id);
          geometry.replace_curves(dst_curves_id);
        }
        if (GreasePencil *grease_pencil = geometry.get_grease_pencil_for_write()) {
          using namespace blender::bke::greasepencil;
          for (const int layer_index : grease_pencil->layers().index_range()) {
            Drawing *drawing = grease_pencil->get_eval_drawing(grease_pencil->layer(layer_index));

            if (drawing == nullptr) {
              continue;
            }
            const bke::CurvesGeometry &src_curves = drawing->strokes();
            const bke::GreasePencilLayerFieldContext field_context(
                *grease_pencil, AttrDomain::Curve, layer_index);
            bke::CurvesGeometry dst_curves = geometry::resample_to_count(
                src_curves, field_context, selection, count);
            drawing->strokes_for_write() = std::move(dst_curves);
            drawing->tag_topology_changed();
          }
        }
      });
      break;
    }
    case GEO_NODE_CURVE_RESAMPLE_LENGTH: {
      Field<float> length = params.extract_input<Field<float>>("Length");
      geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry) {
        if (const Curves *src_curves_id = geometry.get_curves()) {
          const bke::CurvesGeometry &src_curves = src_curves_id->geometry.wrap();
          const bke::CurvesFieldContext field_context{*src_curves_id, AttrDomain::Curve};
          bke::CurvesGeometry dst_curves = geometry::resample_to_length(
              src_curves, field_context, selection, length, {}, storage.keep_last_segment);
          Curves *dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
          bke::curves_copy_parameters(*src_curves_id, *dst_curves_id);
          geometry.replace_curves(dst_curves_id);
        }
        if (GreasePencil *grease_pencil = geometry.get_grease_pencil_for_write()) {
          using namespace blender::bke::greasepencil;
          for (const int layer_index : grease_pencil->layers().index_range()) {
            Drawing *drawing = grease_pencil->get_eval_drawing(grease_pencil->layer(layer_index));
            if (drawing == nullptr) {
              continue;
            }
            const bke::CurvesGeometry &src_curves = drawing->strokes();
            const bke::GreasePencilLayerFieldContext field_context(
                *grease_pencil, AttrDomain::Curve, layer_index);
            bke::CurvesGeometry dst_curves = geometry::resample_to_length(
                src_curves, field_context, selection, length, {}, storage.keep_last_segment);
            drawing->strokes_for_write() = std::move(dst_curves);
            drawing->tag_topology_changed();
          }
        }
      });
      break;
    }
    case GEO_NODE_CURVE_RESAMPLE_EVALUATED:
      geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry) {
        if (const Curves *src_curves_id = geometry.get_curves()) {
          const bke::CurvesGeometry &src_curves = src_curves_id->geometry.wrap();
          const bke::CurvesFieldContext field_context{*src_curves_id, AttrDomain::Curve};
          bke::CurvesGeometry dst_curves = geometry::resample_to_evaluated(
              src_curves, field_context, selection);
          Curves *dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
          bke::curves_copy_parameters(*src_curves_id, *dst_curves_id);
          geometry.replace_curves(dst_curves_id);
        }
        if (GreasePencil *grease_pencil = geometry.get_grease_pencil_for_write()) {
          using namespace blender::bke::greasepencil;
          for (const int layer_index : grease_pencil->layers().index_range()) {
            Drawing *drawing = grease_pencil->get_eval_drawing(grease_pencil->layer(layer_index));
            if (drawing == nullptr) {
              continue;
            }
            const bke::CurvesGeometry &src_curves = drawing->strokes();
            const bke::GreasePencilLayerFieldContext field_context(
                *grease_pencil, AttrDomain::Curve, layer_index);
            bke::CurvesGeometry dst_curves = geometry::resample_to_evaluated(
                src_curves, field_context, selection);
            drawing->strokes_for_write() = std::move(dst_curves);
            drawing->tag_topology_changed();
          }
        }
      });
      break;
  }

  params.set_output("Curve", std::move(geometry_set));
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_boolean(srna,
                       "keep_last_segment",
                       "Keep Last Segment",
                       "Do not collapse curves to single points if they are shorter than the "
                       "given length. The collapsing behavior exists for compatibility reasons.",
                       NOD_storage_boolean_accessors(keep_last_segment, 1));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeResampleCurve", GEO_NODE_RESAMPLE_CURVE);
  ntype.ui_name = "Resample Curve";
  ntype.ui_description = "Generate a poly spline for each input spline";
  ntype.enum_name_legacy = "RESAMPLE_CURVE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.draw_buttons_ex = node_layout_ex;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryCurveResample", node_free_standard_storage, node_copy_standard_storage);
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_resample_cc

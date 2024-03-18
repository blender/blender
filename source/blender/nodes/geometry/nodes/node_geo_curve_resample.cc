/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_resample_curves.hh"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_resample_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveResample)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(
      {GeometryComponent::Type::Curve, GeometryComponent::Type::GreasePencil});
  b.add_input<decl::Bool>("Selection").default_value(true).field_on_all().hide_value();
  b.add_input<decl::Int>("Count").default_value(10).min(1).max(100000).field_on_all();
  b.add_input<decl::Float>("Length")
      .default_value(0.1f)
      .min(0.01f)
      .subtype(PROP_DISTANCE)
      .field_on_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryCurveResample *data = MEM_cnew<NodeGeometryCurveResample>(__func__);

  data->mode = GEO_NODE_CURVE_RESAMPLE_COUNT;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryCurveResample &storage = node_storage(*node);
  const GeometryNodeCurveResampleMode mode = (GeometryNodeCurveResampleMode)storage.mode;

  bNodeSocket *count_socket = static_cast<bNodeSocket *>(node->inputs.first)->next->next;
  bNodeSocket *length_socket = count_socket->next;

  bke::nodeSetSocketAvailability(ntree, count_socket, mode == GEO_NODE_CURVE_RESAMPLE_COUNT);
  bke::nodeSetSocketAvailability(ntree, length_socket, mode == GEO_NODE_CURVE_RESAMPLE_LENGTH);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");

  const NodeGeometryCurveResample &storage = node_storage(params.node());
  const GeometryNodeCurveResampleMode mode = (GeometryNodeCurveResampleMode)storage.mode;

  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");

  GeometryComponentEditData::remember_deformed_positions_if_necessary(geometry_set);

  switch (mode) {
    case GEO_NODE_CURVE_RESAMPLE_COUNT: {
      Field<int> count = params.extract_input<Field<int>>("Count");
      geometry_set.modify_geometry_sets([&](GeometrySet &geometry) {
        if (const Curves *src_curves_id = geometry.get_curves()) {
          const bke::CurvesGeometry &src_curves = src_curves_id->geometry.wrap();
          const bke::CurvesFieldContext field_context{src_curves, AttrDomain::Curve};
          bke::CurvesGeometry dst_curves = geometry::resample_to_count(
              src_curves, field_context, selection, count);
          Curves *dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
          bke::curves_copy_parameters(*src_curves_id, *dst_curves_id);
          geometry.replace_curves(dst_curves_id);
        }
        if (geometry_set.has_grease_pencil()) {
          using namespace blender::bke::greasepencil;
          GreasePencil &grease_pencil = *geometry_set.get_grease_pencil_for_write();
          for (const int layer_index : grease_pencil.layers().index_range()) {
            Drawing *drawing = get_eval_grease_pencil_layer_drawing_for_write(grease_pencil,
                                                                              layer_index);
            if (drawing == nullptr) {
              continue;
            }
            const bke::CurvesGeometry &src_curves = drawing->strokes();
            const bke::GreasePencilLayerFieldContext field_context(
                grease_pencil, AttrDomain::Curve, layer_index);
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
      geometry_set.modify_geometry_sets([&](GeometrySet &geometry) {
        if (const Curves *src_curves_id = geometry.get_curves()) {
          const bke::CurvesGeometry &src_curves = src_curves_id->geometry.wrap();
          const bke::CurvesFieldContext field_context{src_curves, AttrDomain::Curve};
          bke::CurvesGeometry dst_curves = geometry::resample_to_length(
              src_curves, field_context, selection, length);
          Curves *dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
          bke::curves_copy_parameters(*src_curves_id, *dst_curves_id);
          geometry.replace_curves(dst_curves_id);
        }
        if (geometry_set.has_grease_pencil()) {
          using namespace blender::bke::greasepencil;
          GreasePencil &grease_pencil = *geometry_set.get_grease_pencil_for_write();
          for (const int layer_index : grease_pencil.layers().index_range()) {
            Drawing *drawing = get_eval_grease_pencil_layer_drawing_for_write(grease_pencil,
                                                                              layer_index);
            if (drawing == nullptr) {
              continue;
            }
            const bke::CurvesGeometry &src_curves = drawing->strokes();
            const bke::GreasePencilLayerFieldContext field_context(
                grease_pencil, AttrDomain::Curve, layer_index);
            bke::CurvesGeometry dst_curves = geometry::resample_to_length(
                src_curves, field_context, selection, length);
            drawing->strokes_for_write() = std::move(dst_curves);
            drawing->tag_topology_changed();
          }
        }
      });
      break;
    }
    case GEO_NODE_CURVE_RESAMPLE_EVALUATED:
      geometry_set.modify_geometry_sets([&](GeometrySet &geometry) {
        if (const Curves *src_curves_id = geometry.get_curves()) {
          const bke::CurvesGeometry &src_curves = src_curves_id->geometry.wrap();
          const bke::CurvesFieldContext field_context{src_curves, AttrDomain::Curve};
          bke::CurvesGeometry dst_curves = geometry::resample_to_evaluated(
              src_curves, field_context, selection);
          Curves *dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
          bke::curves_copy_parameters(*src_curves_id, *dst_curves_id);
          geometry.replace_curves(dst_curves_id);
        }
        if (geometry_set.has_grease_pencil()) {
          using namespace blender::bke::greasepencil;
          GreasePencil &grease_pencil = *geometry_set.get_grease_pencil_for_write();
          for (const int layer_index : grease_pencil.layers().index_range()) {
            Drawing *drawing = get_eval_grease_pencil_layer_drawing_for_write(grease_pencil,
                                                                              layer_index);
            if (drawing == nullptr) {
              continue;
            }
            const bke::CurvesGeometry &src_curves = drawing->strokes();
            const bke::GreasePencilLayerFieldContext field_context(
                grease_pencil, AttrDomain::Curve, layer_index);
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
  static EnumPropertyItem mode_items[] = {
      {GEO_NODE_CURVE_RESAMPLE_EVALUATED,
       "EVALUATED",
       0,
       "Evaluated",
       "Output the input spline's evaluated points, based on the resolution attribute for NURBS "
       "and Bézier splines. Poly splines are unchanged"},
      {GEO_NODE_CURVE_RESAMPLE_COUNT,
       "COUNT",
       0,
       "Count",
       "Sample the specified number of points along each spline"},
      {GEO_NODE_CURVE_RESAMPLE_LENGTH,
       "LENGTH",
       0,
       "Length",
       "Calculate the number of samples by splitting each spline into segments with the specified "
       "length"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "mode",
                    "Mode",
                    "How to specify the amount of samples",
                    mode_items,
                    NOD_storage_enum_accessors(mode));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_RESAMPLE_CURVE, "Resample Curve", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  node_type_storage(
      &ntype, "NodeGeometryCurveResample", node_free_standard_storage, node_copy_standard_storage);
  ntype.initfunc = node_init;
  ntype.updatefunc = node_update;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_resample_cc

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "NOD_socket_search_link.hh"

#include "GEO_trim_curves.hh"

#include "NOD_rna_define.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_trim_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveTrim)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(
      {GeometryComponent::Type::Curve, GeometryComponent::Type::GreasePencil});
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().supports_field();
  auto &start_fac = b.add_input<decl::Float>("Start")
                        .min(0.0f)
                        .max(1.0f)
                        .subtype(PROP_FACTOR)
                        .make_available([](bNode &node) {
                          node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_FACTOR;
                        })
                        .field_on_all();
  auto &end_fac = b.add_input<decl::Float>("End")
                      .min(0.0f)
                      .max(1.0f)
                      .default_value(1.0f)
                      .subtype(PROP_FACTOR)
                      .make_available([](bNode &node) {
                        node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_FACTOR;
                      })
                      .field_on_all();
  auto &start_len = b.add_input<decl::Float>("Start", "Start_001")
                        .min(0.0f)
                        .subtype(PROP_DISTANCE)
                        .make_available([](bNode &node) {
                          node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_LENGTH;
                        })
                        .field_on_all();
  auto &end_len = b.add_input<decl::Float>("End", "End_001")
                      .min(0.0f)
                      .default_value(1.0f)
                      .subtype(PROP_DISTANCE)
                      .make_available([](bNode &node) {
                        node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_LENGTH;
                      })
                      .field_on_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();

  const bNode *node = b.node_or_null();
  if (node != nullptr) {
    const NodeGeometryCurveTrim &storage = node_storage(*node);
    const GeometryNodeCurveSampleMode mode = GeometryNodeCurveSampleMode(storage.mode);

    start_fac.available(mode == GEO_NODE_CURVE_SAMPLE_FACTOR);
    end_fac.available(mode == GEO_NODE_CURVE_SAMPLE_FACTOR);
    start_len.available(mode == GEO_NODE_CURVE_SAMPLE_LENGTH);
    end_len.available(mode == GEO_NODE_CURVE_SAMPLE_LENGTH);
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryCurveTrim *data = MEM_cnew<NodeGeometryCurveTrim>(__func__);

  data->mode = GEO_NODE_CURVE_SAMPLE_FACTOR;
  node->storage = data;
}

class SocketSearchOp {
 public:
  StringRef socket_name;
  GeometryNodeCurveSampleMode mode;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("GeometryNodeTrimCurve");
    node_storage(node).mode = mode;
    params.update_and_connect_available_socket(node, socket_name);
  }
};

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().static_declaration;

  search_link_ops_for_declarations(params, declaration.outputs);
  search_link_ops_for_declarations(params, declaration.inputs.as_span().take_front(1));

  if (params.in_out() == SOCK_IN) {
    if (params.node_tree().typeinfo->validate_link(eNodeSocketDatatype(params.other_socket().type),
                                                   SOCK_FLOAT))
    {
      params.add_item(IFACE_("Start (Factor)"),
                      SocketSearchOp{"Start", GEO_NODE_CURVE_SAMPLE_FACTOR});
      params.add_item(IFACE_("End (Factor)"), SocketSearchOp{"End", GEO_NODE_CURVE_SAMPLE_FACTOR});
      params.add_item(IFACE_("Start (Length)"),
                      SocketSearchOp{"Start", GEO_NODE_CURVE_SAMPLE_LENGTH});
      params.add_item(IFACE_("End (Length)"), SocketSearchOp{"End", GEO_NODE_CURVE_SAMPLE_LENGTH});
    }
  }
}

static bool trim_curves(const bke::CurvesGeometry &src_curves,
                        const GeometryNodeCurveSampleMode mode,
                        const fn::FieldContext &field_context,
                        Field<bool> &selection_field,
                        Field<float> &start_field,
                        Field<float> &end_field,
                        const AttributeFilter &attribute_filter,
                        bke::CurvesGeometry &dst_curves)
{
  if (src_curves.is_empty()) {
    return false;
  }
  fn::FieldEvaluator evaluator{field_context, src_curves.curves_num()};
  evaluator.add(selection_field);
  evaluator.add(start_field);
  evaluator.add(end_field);
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_as_mask(0);
  const VArray<float> starts = evaluator.get_evaluated<float>(1);
  const VArray<float> ends = evaluator.get_evaluated<float>(2);

  if (selection.is_empty()) {
    return false;
  }

  dst_curves = geometry::trim_curves(src_curves, selection, starts, ends, mode, attribute_filter);
  return true;
}

static void geometry_set_curve_trim(GeometrySet &geometry_set,
                                    const GeometryNodeCurveSampleMode mode,
                                    Field<bool> &selection_field,
                                    Field<float> &start_field,
                                    Field<float> &end_field,
                                    const AttributeFilter &attribute_filter)
{
  if (geometry_set.has_curves()) {
    const Curves &src_curves_id = *geometry_set.get_curves();
    const bke::CurvesGeometry &src_curves = src_curves_id.geometry.wrap();
    const bke::CurvesFieldContext field_context{src_curves_id, AttrDomain::Curve};
    bke::CurvesGeometry dst_curves;
    if (trim_curves(src_curves,
                    mode,
                    field_context,
                    selection_field,
                    start_field,
                    end_field,
                    attribute_filter,
                    dst_curves))
    {
      Curves *dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
      bke::curves_copy_parameters(src_curves_id, *dst_curves_id);
      geometry_set.replace_curves(dst_curves_id);
    }
  }
  if (geometry_set.has_grease_pencil()) {
    using namespace bke::greasepencil;
    GreasePencil &grease_pencil = *geometry_set.get_grease_pencil_for_write();
    for (const int layer_index : grease_pencil.layers().index_range()) {
      Drawing *drawing = grease_pencil.get_eval_drawing(grease_pencil.layer(layer_index));
      if (drawing == nullptr) {
        continue;
      }
      const bke::CurvesGeometry &src_curves = drawing->strokes();
      const bke::GreasePencilLayerFieldContext field_context{
          grease_pencil, AttrDomain::Curve, layer_index};
      bke::CurvesGeometry dst_curves;
      if (trim_curves(src_curves,
                      mode,
                      field_context,
                      selection_field,
                      start_field,
                      end_field,
                      attribute_filter,
                      dst_curves))
      {
        drawing->strokes_for_write() = std::move(dst_curves);
        drawing->tag_topology_changed();
      }
    }
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurveTrim &storage = node_storage(params.node());
  const GeometryNodeCurveSampleMode mode = (GeometryNodeCurveSampleMode)storage.mode;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  GeometryComponentEditData::remember_deformed_positions_if_necessary(geometry_set);

  const NodeAttributeFilter &attribute_filter = params.get_attribute_filter("Curve");

  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  if (mode == GEO_NODE_CURVE_SAMPLE_FACTOR) {
    Field<float> start_field = params.extract_input<Field<float>>("Start");
    Field<float> end_field = params.extract_input<Field<float>>("End");
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      geometry_set_curve_trim(
          geometry_set, mode, selection_field, start_field, end_field, attribute_filter);
    });
  }
  else if (mode == GEO_NODE_CURVE_SAMPLE_LENGTH) {
    Field<float> start_field = params.extract_input<Field<float>>("Start_001");
    Field<float> end_field = params.extract_input<Field<float>>("End_001");
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      geometry_set_curve_trim(
          geometry_set, mode, selection_field, start_field, end_field, attribute_filter);
    });
  }

  params.set_output("Curve", std::move(geometry_set));
}

static void node_rna(StructRNA *srna)
{
  static EnumPropertyItem mode_items[] = {
      {GEO_NODE_CURVE_SAMPLE_FACTOR,
       "FACTOR",
       0,
       "Factor",
       "Find the endpoint positions using a factor of each spline's length"},
      {GEO_NODE_CURVE_RESAMPLE_LENGTH,
       "LENGTH",
       0,
       "Length",
       "Find the endpoint positions using a length from the start of each spline"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "mode",
                    "Mode",
                    "How to find endpoint positions for the trimmed spline",
                    mode_items,
                    NOD_storage_enum_accessors(mode));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeTrimCurve", GEO_NODE_TRIM_CURVE);
  ntype.ui_name = "Trim Curve";
  ntype.ui_description = "Shorten curves by removing portions at the start or end";
  ntype.enum_name_legacy = "TRIM_CURVE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.declare = node_declare;
  blender::bke::node_type_storage(
      &ntype, "NodeGeometryCurveTrim", node_free_standard_storage, node_copy_standard_storage);
  ntype.initfunc = node_init;
  ntype.gather_link_search_ops = node_gather_link_searches;
  blender::bke::node_register_type(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_trim_cc

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BLI_task.hh"

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
  b.add_input<decl::Geometry>("Curve").supported_type(GeometryComponent::Type::Curve);
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().supports_field();
  b.add_input<decl::Float>("Start")
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_FACTOR; })
      .field_on_all();
  b.add_input<decl::Float>("End")
      .min(0.0f)
      .max(1.0f)
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_FACTOR; })
      .field_on_all();
  b.add_input<decl::Float>("Start", "Start_001")
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_LENGTH; })
      .field_on_all();
  b.add_input<decl::Float>("End", "End_001")
      .min(0.0f)
      .default_value(1.0f)
      .subtype(PROP_DISTANCE)
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_LENGTH; })
      .field_on_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryCurveTrim *data = MEM_cnew<NodeGeometryCurveTrim>(__func__);

  data->mode = GEO_NODE_CURVE_SAMPLE_FACTOR;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryCurveTrim &storage = node_storage(*node);
  const GeometryNodeCurveSampleMode mode = (GeometryNodeCurveSampleMode)storage.mode;

  bNodeSocket *start_fac = static_cast<bNodeSocket *>(node->inputs.first)->next->next;
  bNodeSocket *end_fac = start_fac->next;
  bNodeSocket *start_len = end_fac->next;
  bNodeSocket *end_len = start_len->next;

  bke::nodeSetSocketAvailability(ntree, start_fac, mode == GEO_NODE_CURVE_SAMPLE_FACTOR);
  bke::nodeSetSocketAvailability(ntree, end_fac, mode == GEO_NODE_CURVE_SAMPLE_FACTOR);
  bke::nodeSetSocketAvailability(ntree, start_len, mode == GEO_NODE_CURVE_SAMPLE_LENGTH);
  bke::nodeSetSocketAvailability(ntree, end_len, mode == GEO_NODE_CURVE_SAMPLE_LENGTH);
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
  const NodeDeclaration &declaration = *params.node_type().fixed_declaration;

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

static void geometry_set_curve_trim(GeometrySet &geometry_set,
                                    const GeometryNodeCurveSampleMode mode,
                                    Field<bool> &selection_field,
                                    Field<float> &start_field,
                                    Field<float> &end_field,
                                    const AnonymousAttributePropagationInfo &propagation_info)
{
  if (!geometry_set.has_curves()) {
    return;
  }
  const Curves &src_curves_id = *geometry_set.get_curves();
  const bke::CurvesGeometry &src_curves = src_curves_id.geometry.wrap();
  if (src_curves.curves_num() == 0) {
    return;
  }

  const bke::CurvesFieldContext field_context{src_curves, ATTR_DOMAIN_CURVE};
  fn::FieldEvaluator evaluator{field_context, src_curves.curves_num()};
  evaluator.add(selection_field);
  evaluator.add(start_field);
  evaluator.add(end_field);
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_as_mask(0);
  const VArray<float> starts = evaluator.get_evaluated<float>(1);
  const VArray<float> ends = evaluator.get_evaluated<float>(2);

  if (selection.is_empty()) {
    return;
  }

  bke::CurvesGeometry dst_curves = geometry::trim_curves(
      src_curves, selection, starts, ends, mode, propagation_info);
  Curves *dst_curves_id = bke::curves_new_nomain(std::move(dst_curves));
  bke::curves_copy_parameters(src_curves_id, *dst_curves_id);
  geometry_set.replace_curves(dst_curves_id);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const NodeGeometryCurveTrim &storage = node_storage(params.node());
  const GeometryNodeCurveSampleMode mode = (GeometryNodeCurveSampleMode)storage.mode;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Curve");
  GeometryComponentEditData::remember_deformed_curve_positions_if_necessary(geometry_set);

  const AnonymousAttributePropagationInfo &propagation_info = params.get_output_propagation_info(
      "Curve");

  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  if (mode == GEO_NODE_CURVE_SAMPLE_FACTOR) {
    Field<float> start_field = params.extract_input<Field<float>>("Start");
    Field<float> end_field = params.extract_input<Field<float>>("End");
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      geometry_set_curve_trim(
          geometry_set, mode, selection_field, start_field, end_field, propagation_info);
    });
  }
  else if (mode == GEO_NODE_CURVE_SAMPLE_LENGTH) {
    Field<float> start_field = params.extract_input<Field<float>>("Start_001");
    Field<float> end_field = params.extract_input<Field<float>>("End_001");
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      geometry_set_curve_trim(
          geometry_set, mode, selection_field, start_field, end_field, propagation_info);
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
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_TRIM_CURVE, "Trim Curve", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.declare = node_declare;
  node_type_storage(
      &ntype, "NodeGeometryCurveTrim", node_free_standard_storage, node_copy_standard_storage);
  ntype.initfunc = node_init;
  ntype.updatefunc = node_update;
  ntype.gather_link_search_ops = node_gather_link_searches;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_curve_trim_cc

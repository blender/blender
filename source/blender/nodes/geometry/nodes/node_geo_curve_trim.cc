/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BLI_task.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_socket_search_link.hh"

#include "GEO_trim_curves.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_trim_cc {

NODE_STORAGE_FUNCS(NodeGeometryCurveTrim)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Curve")).supported_type(GEO_COMPONENT_TYPE_CURVE);
  b.add_input<decl::Float>(N_("Start"))
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_FACTOR; })
      .supports_field();
  b.add_input<decl::Float>(N_("End"))
      .min(0.0f)
      .max(1.0f)
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_FACTOR; })
      .supports_field();
  b.add_input<decl::Float>(N_("Start"), "Start_001")
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_LENGTH; })
      .supports_field();
  b.add_input<decl::Float>(N_("End"), "End_001")
      .min(0.0f)
      .default_value(1.0f)
      .subtype(PROP_DISTANCE)
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_CURVE_SAMPLE_LENGTH; })
      .supports_field();
  b.add_output<decl::Geometry>(N_("Curve"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryCurveTrim *data = MEM_cnew<NodeGeometryCurveTrim>(__func__);

  data->mode = GEO_NODE_CURVE_SAMPLE_FACTOR;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryCurveTrim &storage = node_storage(*node);
  const GeometryNodeCurveSampleMode mode = (GeometryNodeCurveSampleMode)storage.mode;

  bNodeSocket *start_fac = ((bNodeSocket *)node->inputs.first)->next;
  bNodeSocket *end_fac = start_fac->next;
  bNodeSocket *start_len = end_fac->next;
  bNodeSocket *end_len = start_len->next;

  nodeSetSocketAvailability(ntree, start_fac, mode == GEO_NODE_CURVE_SAMPLE_FACTOR);
  nodeSetSocketAvailability(ntree, end_fac, mode == GEO_NODE_CURVE_SAMPLE_FACTOR);
  nodeSetSocketAvailability(ntree, start_len, mode == GEO_NODE_CURVE_SAMPLE_LENGTH);
  nodeSetSocketAvailability(ntree, end_len, mode == GEO_NODE_CURVE_SAMPLE_LENGTH);
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

  search_link_ops_for_declarations(params, declaration.outputs());
  search_link_ops_for_declarations(params, declaration.inputs().take_front(1));

  if (params.in_out() == SOCK_IN) {
    if (params.node_tree().typeinfo->validate_link(
            static_cast<eNodeSocketDatatype>(params.other_socket().type), SOCK_FLOAT)) {
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
                                    Field<float> &start_field,
                                    Field<float> &end_field)
{
  if (!geometry_set.has_curves()) {
    return;
  }
  const Curves &src_curves_id = *geometry_set.get_curves_for_read();
  const bke::CurvesGeometry &src_curves = bke::CurvesGeometry::wrap(src_curves_id.geometry);
  if (src_curves.curves_num() == 0) {
    return;
  }

  bke::CurvesFieldContext field_context{src_curves, ATTR_DOMAIN_CURVE};
  fn::FieldEvaluator evaluator{field_context, src_curves.curves_num()};
  evaluator.add(start_field);
  evaluator.add(end_field);
  evaluator.evaluate();
  const VArray<float> starts = evaluator.get_evaluated<float>(0);
  const VArray<float> ends = evaluator.get_evaluated<float>(1);

  const VArray<bool> cyclic = src_curves.cyclic();

  /* If node length input is on form [0, 1] instead of [0, length]*/
  const bool normalized_length_lookup = mode == GEO_NODE_CURVE_SAMPLE_FACTOR;

  /* Stack start + end field. */
  Vector<float> length_factors(src_curves.curves_num() * 2);
  Vector<int64_t> lookup_indices(src_curves.curves_num() * 2);
  threading::parallel_for(src_curves.curves_range(), 512, [&](IndexRange curve_range) {
    for (const int64_t curve_i : curve_range) {
      const bool negative_trim = !cyclic[curve_i] && starts[curve_i] > ends[curve_i];
      length_factors[curve_i] = starts[curve_i];
      length_factors[curve_i + src_curves.curves_num()] = negative_trim ? starts[curve_i] :
                                                                          ends[curve_i];
      lookup_indices[curve_i] = curve_i;
      lookup_indices[curve_i + src_curves.curves_num()] = curve_i;
    }
  });

  /* Create curve trim lookup table. */
  Array<bke::curves::CurvePoint, 12> point_lookups = geometry::lookup_curve_points(
      src_curves, length_factors, lookup_indices, normalized_length_lookup);

  bke::CurvesGeometry dst_curves = geometry::trim_curves(
      src_curves,
      src_curves.curves_range().as_span(),
      point_lookups.as_span().slice(0, src_curves.curves_num()),
      point_lookups.as_span().slice(src_curves.curves_num(), src_curves.curves_num()));

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

  if (mode == GEO_NODE_CURVE_SAMPLE_FACTOR) {
    Field<float> start_field = params.extract_input<Field<float>>("Start");
    Field<float> end_field = params.extract_input<Field<float>>("End");
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      geometry_set_curve_trim(geometry_set, mode, start_field, end_field);
    });
  }
  else if (mode == GEO_NODE_CURVE_SAMPLE_LENGTH) {
    Field<float> start_field = params.extract_input<Field<float>>("Start_001");
    Field<float> end_field = params.extract_input<Field<float>>("End_001");
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      geometry_set_curve_trim(geometry_set, mode, start_field, end_field);
    });
  }

  params.set_output("Curve", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_curve_trim_cc

void register_node_type_geo_curve_trim()
{
  namespace file_ns = blender::nodes::node_geo_curve_trim_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_TRIM_CURVE, "Trim Curve", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.declare = file_ns::node_declare;
  node_type_storage(
      &ntype, "NodeGeometryCurveTrim", node_free_standard_storage, node_copy_standard_storage);
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  nodeRegisterType(&ntype);
}

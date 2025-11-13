/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BKE_node_tree_reference_lifetimes.hh"

#include "NOD_node_extra_info.hh"
#include "NOD_rna_define.hh"
#include "NOD_socket.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

#include "COM_node_operation.hh"
#include "COM_result.hh"

namespace blender::nodes::node_geo_enable_output_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_default_layout();
  b.add_input<decl::Bool>("Enable").default_value(false).structure_type(StructureType::Single);

  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);

  auto &input_value = b.add_input(data_type, "Value").hide_value();
  auto &output_value = b.add_output(data_type, "Value").align_with_previous();

  if (nodes::socket_type_supports_fields(data_type)) {
    input_value.supports_field();
  }

  if (bke::node_tree_reference_lifetimes::can_contain_referenced_data(data_type)) {
    output_value.propagate_all();
  }

  if (bke::node_tree_reference_lifetimes::can_contain_reference(data_type)) {
    output_value.reference_pass_all();
  }

  input_value.structure_type(StructureType::Dynamic);
  output_value.structure_type(StructureType::Dynamic);
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

class LazyFunctionForEnableOutputNode : public LazyFunction {
  const bNode &node_;

 public:
  LazyFunctionForEnableOutputNode(const bNode &node, MutableSpan<int> r_lf_index_by_bsocket)
      : node_(node)
  {
    r_lf_index_by_bsocket[node.input_socket(0).index_in_tree()] = inputs_.append_and_get_index_as(
        "Enable", CPPType::get<SocketValueVariant>());
    r_lf_index_by_bsocket[node.input_socket(1).index_in_tree()] = inputs_.append_and_get_index_as(
        "Value", CPPType::get<SocketValueVariant>(), lf::ValueUsage::Maybe);
    r_lf_index_by_bsocket[node.output_socket(0).index_in_tree()] =
        outputs_.append_and_get_index_as("Value", CPPType::get<SocketValueVariant>());
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    const bke::SocketValueVariant enable_variant = params.get_input<bke::SocketValueVariant>(0);
    if (!enable_variant.is_single()) {
      set_default_remaining_node_outputs(params, node_);
      return;
    }
    const bool keep = enable_variant.get<bool>();
    if (!keep) {
      set_default_remaining_node_outputs(params, node_);
      return;
    }
    const bke::SocketValueVariant *value_variant =
        params.try_get_input_data_ptr_or_request<bke::SocketValueVariant>(1);
    if (!value_variant) {
      /* Wait until the value is available. */
      return;
    }
    params.set_output(0, std::move(*value_variant));
  }
};

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = SOCK_FLOAT;
}

using namespace blender::compositor;

class EnableOutputOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const bool keep = this->get_input("Enable").get_single_value_default<bool>(true);
    Result &output = this->get_result("Value");
    if (keep) {
      const Result &input = this->get_input("Value");
      output.share_data(input);
    }
    else {
      output.allocate_invalid();
    }
  }
};

static NodeOperation *node_get_compositor_operation(Context &context, DNode node)
{
  return new EnableOutputOperation(context, node);
}

static void node_extra_info(NodeExtraInfoParams &params)
{
  params.tree.ensure_topology_cache();
  const bNodeSocket &output_socket = params.node.output_socket(0);
  if (!output_socket.is_directly_linked()) {
    return;
  }
  for (const bNodeSocket *target_socket : output_socket.logically_linked_sockets()) {
    const bNode &target_node = target_socket->owner_node();
    if (!target_node.is_group_output() && !target_node.is_reroute()) {
      NodeExtraInfoRow row;
      row.text = RPT_("Invalid Output Link");
      row.tooltip = TIP_("This node should be linked to the group output node");
      row.icon = ICON_ERROR;
      params.rows.append(std::move(row));
      return;
    }
  }
}

static const EnumPropertyItem *data_type_items_callback(bContext * /*C*/,
                                                        PointerRNA *ptr,
                                                        PropertyRNA * /*prop*/,
                                                        bool *r_free)
{
  *r_free = true;
  const bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  blender::bke::bNodeTreeType *ntree_type = ntree.typeinfo;
  return enum_items_filter(
      rna_enum_node_socket_data_type_items, [&](const EnumPropertyItem &item) -> bool {
        bke::bNodeSocketType *socket_type = bke::node_socket_type_find_static(item.value);
        return ntree_type->valid_socket_type(ntree_type, socket_type);
      });
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "",
                    rna_enum_node_socket_data_type_items,
                    NOD_inline_enum_accessors(custom1),
                    SOCK_FLOAT,
                    data_type_items_callback);
}

static const bNodeSocket *node_internally_linked_input(const bNodeTree & /*tree*/,
                                                       const bNode &node,
                                                       const bNodeSocket &output_socket)
{
  /* Internal links should always map corresponding input and output sockets. */
  return node.input_by_identifier(output_socket.identifier);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_cmp_node_type_base(&ntype, "NodeEnableOutput");
  ntype.ui_name = "Enable Output";
  ntype.ui_description = "Either pass through the input value or output the fallback value";
  ntype.nclass = NODE_CLASS_INTERFACE;
  ntype.ignore_inferred_input_socket_visibility = true;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;
  ntype.declare = node_declare;
  ntype.get_compositor_operation = node_get_compositor_operation;
  ntype.get_extra_info = node_extra_info;
  ntype.internally_linked_input = node_internally_linked_input;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_enable_output_cc

namespace blender::nodes {

std::unique_ptr<LazyFunction> get_enable_output_node_lazy_function(
    const bNode &node, GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info)
{
  using namespace node_geo_enable_output_cc;
  return std::make_unique<LazyFunctionForEnableOutputNode>(
      node, own_lf_graph_info.mapping.lf_index_by_bsocket);
}

}  // namespace blender::nodes

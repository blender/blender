/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket.hh"
#include "NOD_socket_search_link.hh"
#include "NOD_zone_socket_items.hh"

#include "RNA_enum_types.hh"

#include "BKE_node_socket_value.hh"

namespace blender::nodes::node_geo_index_switch_cc {

NODE_STORAGE_FUNCS(NodeIndexSwitch)

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }
  const NodeIndexSwitch &storage = node_storage(*node);
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(storage.data_type);
  const bool supports_fields = socket_type_supports_fields(data_type);

  const Span<IndexSwitchItem> items = storage.items_span();
  auto &index = b.add_input<decl::Int>("Index").min(0).max(std::max<int>(0, items.size() - 1));
  if (supports_fields) {
    index.supports_field();
  }

  for (const int i : items.index_range()) {
    const std::string identifier = IndexSwitchItemsAccessor::socket_identifier_for_item(items[i]);
    auto &input = b.add_input(data_type, std::to_string(i), std::move(identifier));
    if (supports_fields) {
      input.supports_field();
    }
    /* Labels are ugly in combination with data-block pickers and are usually disabled. */
    input.hide_label(ELEM(data_type, SOCK_OBJECT, SOCK_IMAGE, SOCK_COLLECTION, SOCK_MATERIAL));
  }

  auto &output = b.add_output(data_type, "Output");
  if (supports_fields) {
    output.dependent_field().reference_pass_all();
  }
  else if (data_type == SOCK_GEOMETRY) {
    output.propagate_all();
  }

  b.add_input<decl::Extend>("", "__extend__");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeIndexSwitch *data = MEM_cnew<NodeIndexSwitch>(__func__);
  data->data_type = SOCK_GEOMETRY;
  data->next_identifier = 0;

  BLI_assert(data->items == nullptr);
  const int default_items_num = 2;
  data->items = MEM_cnew_array<IndexSwitchItem>(default_items_num, __func__);
  for (const int i : IndexRange(default_items_num)) {
    data->items[i].identifier = data->next_identifier++;
  }
  data->items_num = default_items_num;

  node->storage = data;
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  if (params.in_out() == SOCK_OUT) {
    params.add_item(IFACE_("Output"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeIndexSwitch");
      node_storage(node).data_type = params.socket.type;
      params.update_and_connect_available_socket(node, "Output");
    });
  }
  else {
    const eNodeSocketDatatype other_type = eNodeSocketDatatype(params.other_socket().type);
    if (params.node_tree().typeinfo->validate_link(other_type, SOCK_INT)) {
      params.add_item(IFACE_("Index"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeIndexSwitch");
        params.update_and_connect_available_socket(node, "Index");
      });
    }
  }
}

constexpr int value_inputs_start = 1;

class IndexSwitchFunction : public mf::MultiFunction {
  mf::Signature signature_;
  Array<std::string> debug_names_;

 public:
  IndexSwitchFunction(const CPPType &type, const int items_num)
  {
    mf::SignatureBuilder builder{"Index Switch", signature_};
    builder.single_input<int>("Index");
    debug_names_.reinitialize(items_num);
    for (const int i : IndexRange(items_num)) {
      debug_names_[i] = std::to_string(i);
      builder.single_input(debug_names_[i].c_str(), type);
    }
    builder.single_output("Output", type);
    this->set_signature(&signature_);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const int inputs_num = signature_.params.size() - 2;
    const VArray<int> indices = params.readonly_single_input<int>(0, "Index");

    GMutableSpan output = params.uninitialized_single_output(
        signature_.params.index_range().last(), "Output");
    const CPPType &type = output.type();

    if (const std::optional<int> i = indices.get_if_single()) {
      if (IndexRange(inputs_num).contains(*i)) {
        const GVArray inputs = params.readonly_single_input(value_inputs_start + *i);
        inputs.materialize_to_uninitialized(mask, output.data());
      }
      else {
        type.fill_construct_indices(type.default_value(), output.data(), mask);
      }
      return;
    }

    /* Use one extra mask at the end for invalid indices. */
    const int invalid_index = inputs_num;
    IndexMaskMemory memory;
    Array<IndexMask> masks(inputs_num + 1);
    IndexMask::from_groups<int64_t>(
        mask,
        memory,
        [&](const int64_t i) {
          const int index = indices[i];
          return IndexRange(inputs_num).contains(index) ? index : invalid_index;
        },
        masks);

    for (const int i : IndexRange(inputs_num)) {
      if (!masks[i].is_empty()) {
        const GVArray inputs = params.readonly_single_input(value_inputs_start + i);
        inputs.materialize_to_uninitialized(masks[i], output.data());
      }
    }

    type.fill_construct_indices(type.default_value(), output.data(), masks[invalid_index]);
  }

  ExecutionHints get_execution_hints() const override
  {
    ExecutionHints hints;
    hints.allocates_array = true;
    return hints;
  }
};

class LazyFunctionForIndexSwitchNode : public LazyFunction {
 private:
  const bNode &node_;
  bool can_be_field_ = false;
  const CPPType *field_base_type_;

 public:
  LazyFunctionForIndexSwitchNode(const bNode &node,
                                 GeometryNodesLazyFunctionGraphInfo &lf_graph_info)
      : node_(node)
  {
    const NodeIndexSwitch &storage = node_storage(node);
    const eNodeSocketDatatype data_type = eNodeSocketDatatype(storage.data_type);
    const bNodeSocket &index_socket = node.input_socket(0);
    const bNodeSocket &output_socket = node.output_socket(0);
    const CPPType &cpp_type = *output_socket.typeinfo->geometry_nodes_cpp_type;

    debug_name_ = node.name;
    can_be_field_ = socket_type_supports_fields(data_type);
    field_base_type_ = output_socket.typeinfo->base_cpp_type;

    MutableSpan<int> lf_index_by_bsocket = lf_graph_info.mapping.lf_index_by_bsocket;

    lf_index_by_bsocket[index_socket.index_in_tree()] = inputs_.append_and_get_index_as(
        "Index", CPPType::get<SocketValueVariant>(), lf::ValueUsage::Used);
    lf_index_by_bsocket[output_socket.index_in_tree()] = outputs_.append_and_get_index_as(
        "Value", cpp_type);

    for (const int i : storage.items_span().index_range()) {
      const bNodeSocket &input = node.input_socket(value_inputs_start + i);
      lf_index_by_bsocket[input.index_in_tree()] = inputs_.append_and_get_index_as(
          input.identifier, cpp_type, lf::ValueUsage::Maybe);
    }
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    SocketValueVariant index_variant = params.get_input<SocketValueVariant>(0);
    if (index_variant.is_context_dependent_field() && can_be_field_) {
      this->execute_field(index_variant.get<Field<int>>(), params);
    }
    else {
      this->execute_single(index_variant.get<int>(), params);
    }
  }

  int values_num() const
  {
    return inputs_.size() - value_inputs_start;
  }

  void execute_single(const int index, lf::Params &params) const
  {
    const int values_num = this->values_num();
    for (const int i : IndexRange(values_num)) {
      if (i != index) {
        params.set_input_unused(value_inputs_start + i);
      }
    }

    /* Check for an invalid index. */
    if (!IndexRange(values_num).contains(index)) {
      set_default_remaining_node_outputs(params, node_);
      return;
    }

    /* Request input and try again if unavailable. */
    void *value_to_forward = params.try_get_input_data_ptr_or_request(index + value_inputs_start);
    if (value_to_forward == nullptr) {
      return;
    }

    const CPPType &type = *outputs_[0].type;
    void *output_ptr = params.get_output_data_ptr(0);
    type.move_construct(value_to_forward, output_ptr);
    params.output_set(0);
  }

  void execute_field(Field<int> index, lf::Params &params) const
  {
    const int values_num = this->values_num();
    Array<SocketValueVariant *, 8> input_values(values_num);
    for (const int i : IndexRange(values_num)) {
      input_values[i] = params.try_get_input_data_ptr_or_request<SocketValueVariant>(
          value_inputs_start + i);
    }
    if (input_values.as_span().contains(nullptr)) {
      /* Try again when inputs are available. */
      return;
    }

    Vector<GField> input_fields({std::move(index)});
    for (const int i : IndexRange(values_num)) {
      input_fields.append(input_values[i]->extract<GField>());
    }

    std::unique_ptr<mf::MultiFunction> switch_fn = std::make_unique<IndexSwitchFunction>(
        *field_base_type_, values_num);
    GField output_field(FieldOperation::Create(std::move(switch_fn), std::move(input_fields)));

    void *output_ptr = params.get_output_data_ptr(0);
    new (output_ptr) SocketValueVariant(std::move(output_field));
    params.output_set(0);
  }
};

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(
      srna,
      "data_type",
      "Data Type",
      "",
      rna_enum_node_socket_data_type_items,
      NOD_storage_enum_accessors(data_type),
      SOCK_GEOMETRY,
      [](bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free) {
        *r_free = true;
        return enum_items_filter(rna_enum_node_socket_data_type_items,
                                 [](const EnumPropertyItem &item) -> bool {
                                   return ELEM(item.value,
                                               SOCK_FLOAT,
                                               SOCK_INT,
                                               SOCK_BOOLEAN,
                                               SOCK_ROTATION,
                                               SOCK_VECTOR,
                                               SOCK_STRING,
                                               SOCK_RGBA,
                                               SOCK_GEOMETRY,
                                               SOCK_OBJECT,
                                               SOCK_COLLECTION,
                                               SOCK_MATERIAL,
                                               SOCK_IMAGE,
                                               SOCK_MENU);
                                 });
      });
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<IndexSwitchItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeIndexSwitch &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_new<NodeIndexSwitch>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<IndexSwitchItemsAccessor>(*src_node, *dst_node);
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  return socket_items::try_add_item_via_any_extend_socket<IndexSwitchItemsAccessor>(
      *ntree, *node, *node, *link);
}

static void register_node()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INDEX_SWITCH, "Index Switch", NODE_CLASS_CONVERTER);
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.insert_link = node_insert_link;
  node_type_storage(&ntype, "NodeIndexSwitch", node_free_storage, node_copy_storage);
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.draw_buttons = node_layout;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(register_node)

}  // namespace blender::nodes::node_geo_index_switch_cc

namespace blender::nodes {

std::unique_ptr<LazyFunction> get_index_switch_node_lazy_function(
    const bNode &node, GeometryNodesLazyFunctionGraphInfo &lf_graph_info)
{
  using namespace node_geo_index_switch_cc;
  BLI_assert(node.type == GEO_NODE_INDEX_SWITCH);
  return std::make_unique<LazyFunctionForIndexSwitchNode>(node, lf_graph_info);
}

}  // namespace blender::nodes

blender::Span<IndexSwitchItem> NodeIndexSwitch::items_span() const
{
  return blender::Span<IndexSwitchItem>(items, items_num);
}

blender::MutableSpan<IndexSwitchItem> NodeIndexSwitch::items_span()
{
  return blender::MutableSpan<IndexSwitchItem>(items, items_num);
}

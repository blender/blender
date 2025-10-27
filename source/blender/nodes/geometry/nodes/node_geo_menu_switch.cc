/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "node_geometry_util.hh"
#include "shader/node_shader_util.hh"

#include "DNA_node_types.h"

#include "FN_multi_function.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_geo_menu_switch.hh"
#include "NOD_rna_define.hh"
#include "NOD_socket.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"

#include "BLO_read_write.hh"

#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "BKE_node_tree_reference_lifetimes.hh"
#include "BKE_screen.hh"

#include "WM_api.hh"

#include "COM_node_operation.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::nodes::node_geo_menu_switch_cc {

NODE_STORAGE_FUNCS(NodeMenuSwitch)

static void node_declare(blender::nodes::NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  const bNodeTree *ntree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  if (node == nullptr) {
    return;
  }
  const NodeMenuSwitch &storage = node_storage(*node);
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(storage.data_type);
  const bool supports_fields = socket_type_supports_fields(data_type) &&
                               ntree->type == NTREE_GEOMETRY;

  StructureType value_structure_type = socket_type_always_single(data_type) ?
                                           StructureType::Single :
                                           StructureType::Dynamic;
  StructureType menu_structure_type = value_structure_type;

  if (ntree->type == NTREE_COMPOSIT) {
    const bool is_single_compositor_type = compositor::Result::is_single_value_only_type(
        compositor::socket_data_type_to_result_type(data_type));
    if (is_single_compositor_type) {
      value_structure_type = StructureType::Single;
    }
    menu_structure_type = StructureType::Single;
  }

  auto &output = b.add_output(data_type, "Output");
  if (supports_fields) {
    output.dependent_field().reference_pass_all();
  }
  if (bke::node_tree_reference_lifetimes::can_contain_referenced_data(data_type)) {
    output.propagate_all();
  }
  if (bke::node_tree_reference_lifetimes::can_contain_reference(data_type)) {
    output.reference_pass_all();
  }
  output.structure_type(value_structure_type);

  b.add_default_layout();

  auto &menu = b.add_input<decl::Menu>("Menu");
  if (supports_fields) {
    menu.supports_field();
  }
  menu.structure_type(menu_structure_type);
  menu.optional_label();

  for (const NodeEnumItem &enum_item : storage.enum_definition.items()) {
    const std::string identifier = MenuSwitchItemsAccessor::socket_identifier_for_item(enum_item);
    auto &input = b.add_input(data_type, enum_item.name, identifier)
                      .socket_name_ptr(
                          &ntree->id, MenuSwitchItemsAccessor::item_srna, &enum_item, "name")
                      .compositor_realization_mode(CompositorInputRealizationMode::None)
                      .description("Becomes the output value if it is chosen by the menu input");
    if (supports_fields) {
      input.supports_field();
    }
    /* Labels are ugly in combination with data-block pickers and are usually disabled. */
    input.optional_label(ELEM(data_type, SOCK_OBJECT, SOCK_IMAGE, SOCK_COLLECTION, SOCK_MATERIAL));
    input.structure_type(value_structure_type);
    auto &item_output = b.add_output<decl::Bool>(enum_item.name, std::move(identifier))
                            .align_with_previous()
                            .description("True if this item is chosen by the menu input");
    if (supports_fields) {
      item_output.dependent_field({menu.index()});
      item_output.structure_type(menu_structure_type);
    }
  }

  b.add_input<decl::Extend>("", "__extend__")
      .structure_type(StructureType::Dynamic)
      .custom_draw([](CustomSocketDrawParams &params) {
        uiLayout &layout = params.layout;
        layout.emboss_set(ui::EmbossType::None);
        PointerRNA op_ptr = layout.op("node.enum_definition_item_add", "", ICON_ADD);
        RNA_int_set(&op_ptr, "node_identifier", params.node.identifier);
      });
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree *tree, bNode *node)
{
  NodeMenuSwitch *data = MEM_callocN<NodeMenuSwitch>(__func__);
  data->data_type = tree->type == NTREE_GEOMETRY ? SOCK_GEOMETRY : SOCK_RGBA;
  data->enum_definition.next_identifier = 0;
  data->enum_definition.items_array = nullptr;
  data->enum_definition.items_num = 0;
  node->storage = data;

  socket_items::add_item_with_name<MenuSwitchItemsAccessor>(*node, "A");
  socket_items::add_item_with_name<MenuSwitchItemsAccessor>(*node, "B");
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<MenuSwitchItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeMenuSwitch &src_storage = node_storage(*src_node);
  NodeMenuSwitch *dst_storage = MEM_dupallocN<NodeMenuSwitch>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<MenuSwitchItemsAccessor>(*src_node, *dst_node);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(params.other_socket().type);
  if (params.in_out() == SOCK_IN) {
    if (data_type == SOCK_MENU) {
      params.add_item(IFACE_("Menu"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeMenuSwitch");
        params.update_and_connect_available_socket(node, "Menu");
      });
    }
  }
  else {
    if (data_type != SOCK_MENU) {
      params.add_item(IFACE_("Output"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeMenuSwitch");
        node_storage(node).data_type = params.socket.type;
        params.update_and_connect_available_socket(node, "Output");
      });
    }
  }
}

/**
 * Multi-function which evaluates the switch input for each enum item and partially fills the
 * output array with values from the input array where the identifier matches.
 */
class MenuSwitchFn : public mf::MultiFunction {
  const NodeEnumDefinition &enum_def_;
  const CPPType &type_;
  mf::Signature signature_;

 public:
  MenuSwitchFn(const NodeEnumDefinition &enum_def, const CPPType &type)
      : enum_def_(enum_def), type_(type)
  {
    mf::SignatureBuilder builder{"Menu Switch", signature_};
    builder.single_input<MenuValue>("Menu");
    for (const NodeEnumItem &enum_item : enum_def.items()) {
      builder.single_input(enum_item.name, type);
    }
    builder.single_output("Output", type, mf::ParamFlag::SupportsUnusedOutput);
    for (const NodeEnumItem &item : enum_def.items()) {
      builder.single_output<bool>(item.name, mf::ParamFlag::SupportsUnusedOutput);
    }

    this->set_signature(&signature_);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const int value_inputs_start = 1;
    const int inputs_num = enum_def_.items_num;
    const VArray<MenuValue> values = params.readonly_single_input<MenuValue>(0, "Menu");
    /* Use one extra mask at the end for invalid indices. */
    const int invalid_index = inputs_num;

    GMutableSpan value_output = params.uninitialized_single_output_if_required(1 + inputs_num,
                                                                               "Output");

    Array<MutableSpan<bool>> item_mask_outputs(inputs_num);
    for (const int item_i : IndexRange(inputs_num)) {
      const int param_index = 2 + inputs_num + item_i;
      item_mask_outputs[item_i] = params.uninitialized_single_output_if_required<bool>(
          param_index);
    }

    auto find_item_index = [&](const MenuValue value) -> int {
      for (const int i : enum_def_.items().index_range()) {
        const NodeEnumItem &item = enum_def_.items()[i];
        if (item.identifier == value.value) {
          return i;
        }
      }
      return invalid_index;
    };

    if (const std::optional<MenuValue> value = values.get_if_single()) {
      const int index = find_item_index(*value);
      if (index < inputs_num) {
        if (!value_output.is_empty()) {
          const GVArray inputs = params.readonly_single_input(value_inputs_start + index);
          inputs.materialize_to_uninitialized(mask, value_output.data());
        }
        for (const int item_i : IndexRange(inputs_num)) {
          MutableSpan<bool> item_mask_output = item_mask_outputs[item_i];
          if (!item_mask_output.is_empty()) {
            index_mask::masked_fill(item_mask_output, item_i == index, mask);
          }
        }
      }
      else {
        if (!value_output.is_empty()) {
          type_.fill_construct_indices(type_.default_value(), value_output.data(), mask);
        }
        for (const int item_i : IndexRange(inputs_num)) {
          MutableSpan<bool> item_mask_output = item_mask_outputs[item_i];
          if (!item_mask_output.is_empty()) {
            index_mask::masked_fill(item_mask_output, false, mask);
          }
        }
      }
      return;
    }

    IndexMaskMemory memory;
    Array<IndexMask> masks(inputs_num + 1);
    IndexMask::from_groups<int64_t>(
        mask, memory, [&](const int64_t i) { return find_item_index(values[i]); }, masks);

    for (const int item_i : IndexRange(inputs_num)) {
      const IndexMask &mask_for_index = masks[item_i];
      if (!mask_for_index.is_empty() && !value_output.is_empty()) {
        const GVArray inputs = params.readonly_single_input(value_inputs_start + item_i);
        inputs.materialize_to_uninitialized(mask_for_index, value_output.data());
      }
      MutableSpan<bool> item_mask_output = item_mask_outputs[item_i];
      if (!item_mask_output.is_empty()) {
        if (mask.size() != mask_for_index.size()) {
          /* First set output to false before setting selected items to true. */
          index_mask::masked_fill(item_mask_output, false, mask);
        }
        index_mask::masked_fill(item_mask_output, true, mask_for_index);
      }
    }

    type_.fill_construct_indices(type_.default_value(), value_output.data(), masks[invalid_index]);
  }
};

class LazyFunctionForMenuSwitchNode : public LazyFunction {
 private:
  const bNode &node_;
  bool can_be_field_ = false;
  const NodeEnumDefinition &enum_def_;
  const CPPType *field_base_type_;

 public:
  LazyFunctionForMenuSwitchNode(const bNode &node,
                                GeometryNodesLazyFunctionGraphInfo &lf_graph_info)
      : node_(node), enum_def_(node_storage(node).enum_definition)
  {
    const NodeMenuSwitch &storage = node_storage(node);
    const eNodeSocketDatatype data_type = eNodeSocketDatatype(storage.data_type);
    can_be_field_ = socket_type_supports_fields(data_type);
    const bke::bNodeSocketType *socket_type = bke::node_socket_type_find_static(data_type);
    BLI_assert(socket_type != nullptr);
    field_base_type_ = socket_type->base_cpp_type;

    MutableSpan<int> lf_index_by_bsocket = lf_graph_info.mapping.lf_index_by_bsocket;
    debug_name_ = node.name;
    lf_index_by_bsocket[node.input_socket(0).index_in_tree()] = inputs_.append_and_get_index_as(
        "Switch", CPPType::get<SocketValueVariant>(), lf::ValueUsage::Used);
    for (const int i : enum_def_.items().index_range()) {
      const NodeEnumItem &enum_item = enum_def_.items()[i];
      lf_index_by_bsocket[node.input_socket(i + 1).index_in_tree()] =
          inputs_.append_and_get_index_as(
              enum_item.name, CPPType::get<bke::SocketValueVariant>(), lf::ValueUsage::Maybe);
    }
    lf_index_by_bsocket[node.output_socket(0).index_in_tree()] = outputs_.append_and_get_index_as(
        "Value", CPPType::get<bke::SocketValueVariant>());
    for (const int i : enum_def_.items().index_range()) {
      const NodeEnumItem &enum_item = enum_def_.items()[i];
      lf_index_by_bsocket[node.output_socket(i + 1).index_in_tree()] =
          outputs_.append_and_get_index_as(enum_item.name, CPPType::get<SocketValueVariant>());
    }
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    SocketValueVariant condition_variant = params.get_input<SocketValueVariant>(0);
    if (condition_variant.is_context_dependent_field() && can_be_field_) {
      this->execute_field(condition_variant.get<Field<MenuValue>>(), params);
    }
    else {
      this->execute_single(condition_variant.get<MenuValue>(), params);
    }
  }

  void execute_single(const MenuValue condition, lf::Params &params) const
  {
    for (const int i : IndexRange(enum_def_.items_num)) {
      const NodeEnumItem &enum_item = enum_def_.items_array[i];
      const int input_index = i + 1;
      const bool is_selected = enum_item.identifier == condition.value;
      if (is_selected) {
        SocketValueVariant *value_to_forward =
            params.try_get_input_data_ptr_or_request<SocketValueVariant>(input_index);
        if (value_to_forward == nullptr) {
          /* Try again when the value is available. */
          return;
        }

        params.set_output(0, std::move(*value_to_forward));
      }
      else {
        params.set_input_unused(input_index);
      }
      if (!params.output_was_set(i + 1)) {
        params.set_output(i + 1, SocketValueVariant(is_selected));
      }
    }
    /* No guarantee that the switch input matches any enum,
     * set default outputs to ensure valid state. */
    set_default_remaining_node_outputs(params, node_);
  }

  void execute_field(Field<MenuValue> condition, lf::Params &params) const
  {
    /* When the condition is a non-constant field, we need all inputs. */
    const int values_num = this->enum_def_.items_num;
    Array<SocketValueVariant *, 8> input_values(values_num);
    for (const int i : IndexRange(values_num)) {
      const int input_index = i + 1;
      input_values[i] = params.try_get_input_data_ptr_or_request<SocketValueVariant>(input_index);
    }
    if (input_values.as_span().contains(nullptr)) {
      /* Try again when inputs are available. */
      return;
    }

    Vector<GField> item_fields(enum_def_.items_num + 1);
    item_fields[0] = std::move(condition);
    for (const int i : IndexRange(enum_def_.items_num)) {
      item_fields[i + 1] = input_values[i]->extract<GField>();
    }
    std::unique_ptr<MultiFunction> multi_function = std::make_unique<MenuSwitchFn>(
        enum_def_, *field_base_type_);
    std::shared_ptr<fn::FieldOperation> operation = FieldOperation::from(std::move(multi_function),
                                                                         std::move(item_fields));

    params.set_output(0, SocketValueVariant::From(GField(operation, 0)));
    for (const int item_i : IndexRange(enum_def_.items_num)) {
      params.set_output(item_i + 1, SocketValueVariant::From(GField(operation, item_i + 1)));
    }
  }
};

/**
 * Outputs booleans that indicate which inputs of a menu switch node
 * are used. Note that it's possible that multiple inputs are used
 * when the condition is a field.
 */
class LazyFunctionForMenuSwitchSocketUsage : public lf::LazyFunction {
  const NodeEnumDefinition &enum_def_;

 public:
  LazyFunctionForMenuSwitchSocketUsage(const bNode &node)
      : enum_def_(node_storage(node).enum_definition)
  {
    debug_name_ = "Menu Switch Socket Usage";
    inputs_.append_as("Condition", CPPType::get<SocketValueVariant>());
    for (const int i : IndexRange(enum_def_.items_num)) {
      const NodeEnumItem &enum_item = enum_def_.items()[i];
      outputs_.append_as(enum_item.name, CPPType::get<bool>());
    }
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    const SocketValueVariant &condition_variant = params.get_input<SocketValueVariant>(0);
    if (condition_variant.is_context_dependent_field()) {
      for (const int i : IndexRange(enum_def_.items_num)) {
        params.set_output(i, true);
      }
    }
    else {
      const MenuValue value = condition_variant.get<MenuValue>();
      for (const int i : IndexRange(enum_def_.items_num)) {
        const NodeEnumItem &enum_item = enum_def_.items()[i];
        params.set_output(i, value.value == enum_item.identifier);
      }
    }
  }
};

using namespace blender::compositor;

class MenuSwitchOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &value_output = this->get_result("Output");
    const MenuValue menu_identifier = this->get_input("Menu").get_single_value<MenuValue>();
    const NodeEnumDefinition &enum_definition = node_storage(bnode()).enum_definition;
    bool found_item = false;

    for (const int i : IndexRange(enum_definition.items_num)) {
      const NodeEnumItem &enum_item = enum_definition.items()[i];
      const std::string identifier = MenuSwitchItemsAccessor::socket_identifier_for_item(
          enum_item);
      const bool is_selected = enum_item.identifier == menu_identifier.value;
      Result &item_output = this->get_result(identifier);
      if (item_output.should_compute()) {
        item_output.allocate_single_value();
        item_output.set_single_value(is_selected);
      }
      if (!is_selected) {
        continue;
      }
      const Result &input = this->get_input(identifier);
      value_output.share_data(input);
      found_item = true;
    }

    if (!found_item) {
      /* The menu identifier didn't match any item, so allocate an invalid output. */
      value_output.allocate_invalid();
    }
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new MenuSwitchOperation(context, node);
}

static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNodeTree &tree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode &node = *static_cast<bNode *>(ptr->data);

  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);

  if (uiLayout *panel = layout->panel(C, "menu_switch_items", false, IFACE_("Menu Items"))) {
    socket_items::ui::draw_items_list_with_operators<MenuSwitchItemsAccessor>(
        C, panel, tree, node);
    socket_items::ui::draw_active_item_props<MenuSwitchItemsAccessor>(
        tree, node, [&](PointerRNA *item_ptr) {
          panel->use_property_split_set(true);
          panel->use_property_decorate_set(false);
          panel->prop(item_ptr, "description", UI_ITEM_NONE, std::nullopt, ICON_NONE);
        });
  }
}

static void node_operators()
{
  socket_items::ops::make_common_operators<MenuSwitchItemsAccessor>();
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  return socket_items::try_add_item_via_any_extend_socket<MenuSwitchItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void node_blend_write(const bNodeTree & /*ntree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<MenuSwitchItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*ntree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<MenuSwitchItemsAccessor>(&reader, node);
}

static const bNodeSocket *node_internally_linked_input(const bNodeTree & /*tree*/,
                                                       const bNode &node,
                                                       const bNodeSocket &output_socket)
{
  const NodeMenuSwitch &storage = node_storage(node);
  if (storage.enum_definition.items_num == 0) {
    return nullptr;
  }
  if (&output_socket == node.outputs.first) {
    /* Default to the first enum item input. */
    return &node.input_socket(1);
  }
  return nullptr;
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
                    NOD_storage_enum_accessors(data_type),
                    SOCK_GEOMETRY,
                    data_type_items_callback);
}

static void register_node()
{
  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "GeometryNodeMenuSwitch", GEO_NODE_MENU_SWITCH);
  ntype.ui_name = "Menu Switch";
  ntype.ui_description = "Select from multiple inputs by name";
  ntype.enum_name_legacy = "MENU_SWITCH";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(ntype, "NodeMenuSwitch", node_free_storage, node_copy_storage);
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.draw_buttons = node_layout;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.register_operators = node_operators;
  ntype.insert_link = node_insert_link;
  ntype.ignore_inferred_input_socket_visibility = true;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  ntype.internally_linked_input = node_internally_linked_input;
  ntype.get_compositor_operation = get_compositor_operation;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(register_node)

}  // namespace blender::nodes::node_geo_menu_switch_cc

namespace blender::nodes {

std::unique_ptr<LazyFunction> get_menu_switch_node_lazy_function(
    const bNode &node, GeometryNodesLazyFunctionGraphInfo &lf_graph_info)
{
  using namespace node_geo_menu_switch_cc;
  BLI_assert(node.type_legacy == GEO_NODE_MENU_SWITCH);
  return std::make_unique<LazyFunctionForMenuSwitchNode>(node, lf_graph_info);
}

std::unique_ptr<LazyFunction> get_menu_switch_node_socket_usage_lazy_function(const bNode &node)
{
  using namespace node_geo_menu_switch_cc;
  BLI_assert(node.type_legacy == GEO_NODE_MENU_SWITCH);
  return std::make_unique<LazyFunctionForMenuSwitchSocketUsage>(node);
}

StructRNA *MenuSwitchItemsAccessor::item_srna = &RNA_NodeEnumItem;

void MenuSwitchItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
  BLO_write_string(writer, item.description);
}

void MenuSwitchItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
  BLO_read_string(reader, &item.description);
}

}  // namespace blender::nodes

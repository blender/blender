/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BKE_node_tree_reference_lifetimes.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

#include "FN_multi_function_builder.hh"

namespace blender::nodes::node_geo_switch_cc {

NODE_STORAGE_FUNCS(NodeSwitch)

static void node_declare(NodeDeclarationBuilder &b)
{
  auto &switch_decl = b.add_input<decl::Bool>("Switch");
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }
  const NodeSwitch &storage = node_storage(*node);
  const eNodeSocketDatatype socket_type = eNodeSocketDatatype(storage.input_type);

  auto &false_decl = b.add_input(socket_type, "False");
  auto &true_decl = b.add_input(socket_type, "True");
  auto &output_decl = b.add_output(socket_type, "Output");

  if (socket_type_supports_fields(socket_type)) {
    switch_decl.supports_field();
    false_decl.supports_field();
    true_decl.supports_field();
    output_decl.dependent_field().reference_pass_all();
  }
  if (bke::node_tree_reference_lifetimes::can_contain_referenced_data(socket_type)) {
    output_decl.propagate_all();
  }
  if (bke::node_tree_reference_lifetimes::can_contain_reference(socket_type)) {
    output_decl.reference_pass_all();
  }

  const StructureType structure_type = socket_type_always_single(socket_type) ?
                                           StructureType::Single :
                                           StructureType::Dynamic;

  switch_decl.structure_type(structure_type);
  false_decl.structure_type(structure_type);
  true_decl.structure_type(structure_type);
  output_decl.structure_type(structure_type);
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "input_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeSwitch *data = MEM_callocN<NodeSwitch>(__func__);
  data->input_type = SOCK_GEOMETRY;
  node->storage = data;
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  if (params.in_out() == SOCK_OUT) {
    params.add_item(IFACE_("Output"), [](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeSwitch");
      node_storage(node).input_type = params.socket.type;
      params.update_and_connect_available_socket(node, "Output");
    });
  }
  else {
    /* Make sure the switch input comes first in the search for boolean sockets. */
    int true_false_weights = 0;
    if (params.other_socket().type == SOCK_BOOLEAN) {
      params.add_item(IFACE_("Switch"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeSwitch");
        params.update_and_connect_available_socket(node, "Switch");
      });
      true_false_weights--;
    }

    params.add_item(
        IFACE_("False"),
        [](LinkSearchOpParams &params) {
          bNode &node = params.add_node("GeometryNodeSwitch");
          node_storage(node).input_type = params.socket.type;
          params.update_and_connect_available_socket(node, "False");
        },
        true_false_weights);
    params.add_item(
        IFACE_("True"),
        [](LinkSearchOpParams &params) {
          bNode &node = params.add_node("GeometryNodeSwitch");
          node_storage(node).input_type = params.socket.type;
          params.update_and_connect_available_socket(node, "True");
        },
        true_false_weights);
  }
}

class LazyFunctionForSwitchNode : public LazyFunction {
 private:
  bool can_be_field_ = false;
  const CPPType *base_type_;

 public:
  LazyFunctionForSwitchNode(const bNode &node)
  {
    const NodeSwitch &storage = node_storage(node);
    const eNodeSocketDatatype data_type = eNodeSocketDatatype(storage.input_type);
    can_be_field_ = socket_type_supports_fields(data_type);

    const bke::bNodeSocketType *socket_type = nullptr;
    for (const bNodeSocket *socket : node.output_sockets()) {
      if (socket->type == data_type) {
        socket_type = socket->typeinfo;
        break;
      }
    }
    BLI_assert(socket_type != nullptr);
    const CPPType &cpp_type = CPPType::get<SocketValueVariant>();
    base_type_ = socket_type->base_cpp_type;

    debug_name_ = node.name;
    inputs_.append_as("Condition", CPPType::get<SocketValueVariant>());
    inputs_.append_as("False", cpp_type, lf::ValueUsage::Maybe);
    inputs_.append_as("True", cpp_type, lf::ValueUsage::Maybe);
    outputs_.append_as("Value", cpp_type);
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    SocketValueVariant condition_variant = params.get_input<SocketValueVariant>(0);
    if (condition_variant.is_context_dependent_field() && can_be_field_) {
      this->execute_field(condition_variant.get<Field<bool>>(), params);
    }
    else {
      this->execute_single(condition_variant.get<bool>(), params);
    }
  }

  static constexpr int false_input_index = 1;
  static constexpr int true_input_index = 2;

  void execute_single(const bool condition, lf::Params &params) const
  {
    const int input_to_forward = condition ? true_input_index : false_input_index;
    const int input_to_ignore = condition ? false_input_index : true_input_index;

    params.set_input_unused(input_to_ignore);
    void *value_to_forward = params.try_get_input_data_ptr_or_request(input_to_forward);
    if (value_to_forward == nullptr) {
      /* Try again when the value is available. */
      return;
    }

    const CPPType &type = *outputs_[0].type;
    void *output_ptr = params.get_output_data_ptr(0);
    type.move_construct(value_to_forward, output_ptr);
    params.output_set(0);
  }

  void execute_field(Field<bool> condition, lf::Params &params) const
  {
    /* When the condition is a non-constant field, we need both inputs. */
    auto *false_value_variant = params.try_get_input_data_ptr_or_request<SocketValueVariant>(
        false_input_index);
    auto *true_value_variant = params.try_get_input_data_ptr_or_request<SocketValueVariant>(
        true_input_index);
    if (ELEM(nullptr, false_value_variant, true_value_variant)) {
      /* Try again when inputs are available. */
      return;
    }

    const MultiFunction &switch_multi_function = this->get_switch_multi_function();

    GField false_field = false_value_variant->extract<GField>();
    GField true_field = true_value_variant->extract<GField>();

    GField output_field{FieldOperation::from(
        switch_multi_function,
        {std::move(condition), std::move(false_field), std::move(true_field)})};

    void *output_ptr = params.get_output_data_ptr(0);
    SocketValueVariant::ConstructIn(output_ptr, std::move(output_field));
    params.output_set(0);
  }

  const MultiFunction &get_switch_multi_function() const
  {
    const MultiFunction *switch_multi_function = nullptr;
    base_type_->to_static_type_tag<float,
                                   int,
                                   bool,
                                   float3,
                                   ColorGeometry4f,
                                   std::string,
                                   math::Quaternion,
                                   float4x4,
                                   MenuValue>([&](auto type_tag) {
      using T = typename decltype(type_tag)::type;
      if constexpr (std::is_void_v<T>) {
        BLI_assert_unreachable();
      }
      else {
        static auto switch_fn = mf::build::SI3_SO<bool, T, T, T>(
            "Switch", [](const bool condition, const T &false_value, const T &true_value) {
              return condition ? true_value : false_value;
            });
        switch_multi_function = &switch_fn;
      }
    });
    BLI_assert(switch_multi_function != nullptr);
    return *switch_multi_function;
  }
};

static const bNodeSocket *node_internally_linked_input(const bNodeTree & /*tree*/,
                                                       const bNode &node,
                                                       const bNodeSocket & /*output_socket*/)
{
  /* Default to the False input. */
  return &node.input_socket(1);
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(
      srna,
      "input_type",
      "Input Type",
      "",
      rna_enum_node_socket_data_type_items,
      NOD_storage_enum_accessors(input_type),
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
                                               SOCK_MATRIX,
                                               SOCK_VECTOR,
                                               SOCK_STRING,
                                               SOCK_RGBA,
                                               SOCK_GEOMETRY,
                                               SOCK_OBJECT,
                                               SOCK_COLLECTION,
                                               SOCK_MATERIAL,
                                               SOCK_IMAGE,
                                               SOCK_MENU,
                                               SOCK_BUNDLE,
                                               SOCK_CLOSURE);
                                 });
      });
}

static void register_node()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSwitch", GEO_NODE_SWITCH);
  ntype.ui_name = "Switch";
  ntype.ui_description = "Switch between two inputs";
  ntype.enum_name_legacy = "SWITCH";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(
      ntype, "NodeSwitch", node_free_standard_storage, node_copy_standard_storage);
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.draw_buttons = node_layout;
  ntype.ignore_inferred_input_socket_visibility = true;
  ntype.internally_linked_input = node_internally_linked_input;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(register_node)

}  // namespace blender::nodes::node_geo_switch_cc

namespace blender::nodes {

std::unique_ptr<LazyFunction> get_switch_node_lazy_function(const bNode &node)
{
  using namespace node_geo_switch_cc;
  BLI_assert(node.type_legacy == GEO_NODE_SWITCH);
  return std::make_unique<LazyFunctionForSwitchNode>(node);
}

}  // namespace blender::nodes

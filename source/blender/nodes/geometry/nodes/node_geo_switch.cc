/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_socket_search_link.hh"

#include "FN_field_cpp_type.hh"

namespace blender::nodes::node_geo_switch_cc {

NODE_STORAGE_FUNCS(NodeSwitch)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>(N_("Switch")).default_value(false).supports_field();
  b.add_input<decl::Bool>(N_("Switch"), "Switch_001").default_value(false);

  b.add_input<decl::Float>(N_("False")).supports_field();
  b.add_input<decl::Float>(N_("True")).supports_field();
  b.add_input<decl::Int>(N_("False"), "False_001").min(-100000).max(100000).supports_field();
  b.add_input<decl::Int>(N_("True"), "True_001").min(-100000).max(100000).supports_field();
  b.add_input<decl::Bool>(N_("False"), "False_002")
      .default_value(false)
      .hide_value()
      .supports_field();
  b.add_input<decl::Bool>(N_("True"), "True_002")
      .default_value(true)
      .hide_value()
      .supports_field();
  b.add_input<decl::Vector>(N_("False"), "False_003").supports_field();
  b.add_input<decl::Vector>(N_("True"), "True_003").supports_field();
  b.add_input<decl::Color>(N_("False"), "False_004")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .supports_field();
  b.add_input<decl::Color>(N_("True"), "True_004")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .supports_field();
  b.add_input<decl::String>(N_("False"), "False_005").supports_field();
  b.add_input<decl::String>(N_("True"), "True_005").supports_field();

  b.add_input<decl::Geometry>(N_("False"), "False_006");
  b.add_input<decl::Geometry>(N_("True"), "True_006");
  b.add_input<decl::Object>(N_("False"), "False_007");
  b.add_input<decl::Object>(N_("True"), "True_007");
  b.add_input<decl::Collection>(N_("False"), "False_008");
  b.add_input<decl::Collection>(N_("True"), "True_008");
  b.add_input<decl::Texture>(N_("False"), "False_009");
  b.add_input<decl::Texture>(N_("True"), "True_009");
  b.add_input<decl::Material>(N_("False"), "False_010");
  b.add_input<decl::Material>(N_("True"), "True_010");
  b.add_input<decl::Image>(N_("False"), "False_011");
  b.add_input<decl::Image>(N_("True"), "True_011");

  b.add_output<decl::Float>(N_("Output")).dependent_field().reference_pass_all();
  b.add_output<decl::Int>(N_("Output"), "Output_001").dependent_field().reference_pass_all();
  b.add_output<decl::Bool>(N_("Output"), "Output_002").dependent_field().reference_pass_all();
  b.add_output<decl::Vector>(N_("Output"), "Output_003").dependent_field().reference_pass_all();
  b.add_output<decl::Color>(N_("Output"), "Output_004").dependent_field().reference_pass_all();
  b.add_output<decl::String>(N_("Output"), "Output_005").dependent_field().reference_pass_all();
  b.add_output<decl::Geometry>(N_("Output"), "Output_006").propagate_all();
  b.add_output<decl::Object>(N_("Output"), "Output_007");
  b.add_output<decl::Collection>(N_("Output"), "Output_008");
  b.add_output<decl::Texture>(N_("Output"), "Output_009");
  b.add_output<decl::Material>(N_("Output"), "Output_010");
  b.add_output<decl::Image>(N_("Output"), "Output_011");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "input_type", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeSwitch *data = MEM_cnew<NodeSwitch>(__func__);
  data->input_type = SOCK_GEOMETRY;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeSwitch &storage = node_storage(*node);
  int index = 0;
  bNodeSocket *field_switch = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *non_field_switch = static_cast<bNodeSocket *>(field_switch->next);

  const bool fields_type = ELEM(
      storage.input_type, SOCK_FLOAT, SOCK_INT, SOCK_BOOLEAN, SOCK_VECTOR, SOCK_RGBA, SOCK_STRING);

  nodeSetSocketAvailability(ntree, field_switch, fields_type);
  nodeSetSocketAvailability(ntree, non_field_switch, !fields_type);

  LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, &node->inputs, index) {
    if (index <= 1) {
      continue;
    }
    nodeSetSocketAvailability(ntree, socket, socket->type == storage.input_type);
  }

  LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
    nodeSetSocketAvailability(ntree, socket, socket->type == storage.input_type);
  }
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

 public:
  LazyFunctionForSwitchNode(const bNode &node)
  {
    const NodeSwitch &storage = node_storage(node);
    const eNodeSocketDatatype data_type = eNodeSocketDatatype(storage.input_type);
    can_be_field_ = ELEM(data_type, SOCK_FLOAT, SOCK_INT, SOCK_BOOLEAN, SOCK_VECTOR, SOCK_RGBA);

    const bNodeSocketType *socket_type = nullptr;
    for (const bNodeSocket *socket : node.output_sockets()) {
      if (socket->type == data_type) {
        socket_type = socket->typeinfo;
        break;
      }
    }
    BLI_assert(socket_type != nullptr);
    const CPPType &cpp_type = *socket_type->geometry_nodes_cpp_type;

    inputs_.append_as("Condition", CPPType::get<ValueOrField<bool>>());
    inputs_.append_as("False", cpp_type, lf::ValueUsage::Maybe);
    inputs_.append_as("True", cpp_type, lf::ValueUsage::Maybe);
    outputs_.append_as("Value", cpp_type);
  }

  void execute_impl(lf::Params &params, const lf::Context & /*context*/) const override
  {
    const ValueOrField<bool> condition = params.get_input<ValueOrField<bool>>(0);
    if (condition.is_field() && can_be_field_) {
      Field<bool> condition_field = condition.as_field();
      if (condition_field.node().depends_on_input()) {
        this->execute_field(condition.as_field(), params);
        return;
      }
      const bool condition_bool = fn::evaluate_constant_field(condition_field);
      this->execute_single(condition_bool, params);
      return;
    }
    this->execute_single(condition.as_value(), params);
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
    void *false_value_or_field = params.try_get_input_data_ptr_or_request(false_input_index);
    void *true_value_or_field = params.try_get_input_data_ptr_or_request(true_input_index);
    if (ELEM(nullptr, false_value_or_field, true_value_or_field)) {
      /* Try again when inputs are available. */
      return;
    }

    const CPPType &type = *outputs_[0].type;
    const fn::ValueOrFieldCPPType &value_or_field_type = *fn::ValueOrFieldCPPType::get_from_self(
        type);
    const CPPType &value_type = value_or_field_type.value;
    const MultiFunction &switch_multi_function = this->get_switch_multi_function(value_type);

    GField false_field = value_or_field_type.as_field(false_value_or_field);
    GField true_field = value_or_field_type.as_field(true_value_or_field);

    GField output_field{FieldOperation::Create(
        switch_multi_function,
        {std::move(condition), std::move(false_field), std::move(true_field)})};

    void *output_ptr = params.get_output_data_ptr(0);
    value_or_field_type.construct_from_field(output_ptr, std::move(output_field));
    params.output_set(0);
  }

  const MultiFunction &get_switch_multi_function(const CPPType &type) const
  {
    const MultiFunction *switch_multi_function = nullptr;
    type.to_static_type_tag<float, int, bool, float3, ColorGeometry4f, std::string>(
        [&](auto type_tag) {
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

}  // namespace blender::nodes::node_geo_switch_cc

namespace blender::nodes {

std::unique_ptr<LazyFunction> get_switch_node_lazy_function(const bNode &node)
{
  using namespace node_geo_switch_cc;
  BLI_assert(node.type == GEO_NODE_SWITCH);
  return std::make_unique<LazyFunctionForSwitchNode>(node);
}

}  // namespace blender::nodes

void register_node_type_geo_switch()
{
  namespace file_ns = blender::nodes::node_geo_switch_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SWITCH, "Switch", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  node_type_storage(&ntype, "NodeSwitch", node_free_standard_storage, node_copy_standard_storage);
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}

/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <regex>

#include "NOD_menu_value.hh"
#include "NOD_multi_function.hh"
#include "NOD_node_declaration.hh"
#include "NOD_node_in_compute_context.hh"
#include "NOD_socket_usage_inference.hh"

#include "DNA_anim_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"

#include "BKE_compute_context_cache.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_type_conversions.hh"

#include "ANIM_action.hh"
#include "ANIM_action_iterators.hh"

#include "BLI_listbase.h"
#include "BLI_stack.hh"

namespace blender::nodes {

class SocketValueInferencerImpl {
 private:
  ResourceScope &scope_;
  bke::ComputeContextCache &compute_context_cache_;

  Stack<SocketInContext> value_tasks_;
  /**
   * Once a socket value has been determined, it is added to this map. Note that a socket value may
   * be determined to be unknown because it depends on values that are not known statically.
   */
  Map<SocketInContext, InferenceValue> all_socket_values_;

  FunctionRef<InferenceValue(int group_input_i)> group_input_value_fn_;

  /**
   * All sockets that have animation data and thus their value is not fixed statically. This can
   * contain sockets from multiple different trees.
   */
  Set<const bNodeSocket *> animated_sockets_;
  Set<const bNodeTree *> trees_with_handled_animation_data_;
  std::optional<Span<bool>> top_level_ignored_inputs_;

  const bNodeTree &root_tree_;

 public:
  SocketValueInferencerImpl(
      const bNodeTree &tree,
      ResourceScope &scope,
      bke::ComputeContextCache &compute_context_cache,
      const FunctionRef<InferenceValue(int group_input_i)> group_input_value_fn,
      const std::optional<Span<bool>> top_level_ignored_inputs)
      : scope_(scope),
        compute_context_cache_(compute_context_cache),
        group_input_value_fn_(group_input_value_fn),
        top_level_ignored_inputs_(top_level_ignored_inputs),
        root_tree_(tree)
  {
    root_tree_.ensure_topology_cache();
    root_tree_.ensure_interface_cache();
    this->ensure_animation_data_processed(root_tree_);
  }

  InferenceValue get_socket_value(const SocketInContext &socket)
  {
    const std::optional<InferenceValue> value = all_socket_values_.lookup_try(socket);
    if (value.has_value()) {
      return *value;
    }
    if (socket->owner_tree().has_available_link_cycle()) {
      return InferenceValue::Unknown();
    }

    BLI_assert(value_tasks_.is_empty());
    value_tasks_.push(socket);

    while (!value_tasks_.is_empty()) {
      const SocketInContext &socket = value_tasks_.peek();
      this->value_task(socket);
      if (&socket == &value_tasks_.peek()) {
        /* The task is finished if it hasn't added any new task it depends on. */
        value_tasks_.pop();
      }
    }

    return all_socket_values_.lookup(socket);
  }

 private:
  void value_task(const SocketInContext &socket)
  {
    if (all_socket_values_.contains(socket)) {
      /* Task is done already. */
      return;
    }
    const bNode &node = socket->owner_node();
    if (node.is_undefined() && !node.is_custom_group()) {
      all_socket_values_.add_new(socket, InferenceValue::Unknown());
      return;
    }
    const CPPType *base_type = socket->typeinfo->base_cpp_type;
    if (!base_type) {
      /* The socket type is unknown for some reason (maybe a socket type from the future?). */
      all_socket_values_.add_new(socket, InferenceValue::Unknown());
      return;
    }
    if (socket->is_input()) {
      this->value_task__input(socket);
    }
    else {
      this->value_task__output(socket);
    }
  }

  void value_task__output(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    if (node->is_muted()) {
      this->value_task__output__muted_node(socket);
      return;
    }
    switch (node->type_legacy) {
      case NODE_GROUP:
      case NODE_CUSTOM_GROUP: {
        this->value_task__output__group_node(socket);
        return;
      }
      case NODE_GROUP_INPUT: {
        this->value_task__output__group_input_node(socket);
        return;
      }
      case NODE_REROUTE: {
        this->value_task__output__reroute_node(socket);
        return;
      }
      case GEO_NODE_SWITCH: {
        this->value_task__output__generic_switch(
            socket, switch_node_inference_utils::is_socket_selected__switch);
        return;
      }
      case GEO_NODE_INDEX_SWITCH: {
        this->value_task__output__generic_switch(
            socket, switch_node_inference_utils::is_socket_selected__index_switch);
        return;
      }
      case GEO_NODE_MENU_SWITCH: {
        if (socket->index() == 0) {
          this->value_task__output__generic_switch(
              socket, switch_node_inference_utils::is_socket_selected__menu_switch);
        }
        else {
          this->value_task__output__menu_switch_selection(socket);
        }
        return;
      }
      case SH_NODE_MIX: {
        this->value_task__output__generic_switch(
            socket, switch_node_inference_utils::is_socket_selected__mix_node);
        return;
      }
      case SH_NODE_MIX_SHADER: {
        this->value_task__output__generic_switch(
            socket, switch_node_inference_utils::is_socket_selected__shader_mix_node);
        return;
      }
      case SH_NODE_MATH: {
        this->value_task__output__float_math(socket);
        return;
      }
      case SH_NODE_VECTOR_MATH: {
        this->value_task__output__vector_math(socket);
        return;
      }
      case FN_NODE_INTEGER_MATH: {
        this->value_task__output__integer_math(socket);
        return;
      }
      case FN_NODE_BOOLEAN_MATH: {
        this->value_task__output__boolean_math(socket);
        return;
      }
      case GEO_NODE_WARNING: {
        this->value_task__output__warning(socket);
        return;
      }
      default: {
        if (node->is_type("NodeEnableOutput")) {
          this->value_task__output__enable_output(socket);
          return;
        }
        if (node->typeinfo->build_multi_function) {
          this->value_task__output__multi_function_node(socket);
          return;
        }
        break;
      }
    }
    /* If none of the above cases work, the socket value is set to null which means that it is
     * unknown/dynamic. */
    all_socket_values_.add_new(socket, InferenceValue::Unknown());
  }

  void value_task__output__group_node(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node->id);
    if (!group || ID_MISSING(&group->id)) {
      all_socket_values_.add_new(socket, InferenceValue::Unknown());
      return;
    }
    group->ensure_topology_cache();
    if (group->has_available_link_cycle()) {
      all_socket_values_.add_new(socket, InferenceValue::Unknown());
      return;
    }
    this->ensure_animation_data_processed(*group);
    const bNode *group_output_node = group->group_output_node();
    if (!group_output_node) {
      /* Can't compute the value if the group does not have an output node. */
      all_socket_values_.add_new(socket, InferenceValue::Unknown());
      return;
    }
    const ComputeContext &group_context = compute_context_cache_.for_group_node(
        socket.context, node->identifier, &node->owner_tree());
    const SocketInContext socket_in_group{&group_context,
                                          &group_output_node->input_socket(socket->index())};
    const std::optional<InferenceValue> value = all_socket_values_.lookup_try(socket_in_group);
    if (!value.has_value()) {
      this->push_value_task(socket_in_group);
      return;
    }
    all_socket_values_.add_new(socket, *value);
  }

  void value_task__output__group_input_node(const SocketInContext &socket)
  {
    const bool is_root_context = socket.context == nullptr;
    if (is_root_context) {
      InferenceValue value = InferenceValue::Unknown();
      if (group_input_value_fn_) {
        value = group_input_value_fn_(socket->index());
      }
      all_socket_values_.add_new(socket, value);
      return;
    }

    const bke::GroupNodeComputeContext &group_context =
        *static_cast<const bke::GroupNodeComputeContext *>(socket.context);
    const SocketInContext group_node_input{group_context.parent(),
                                           &group_context.node()->input_socket(socket->index())};
    const std::optional<InferenceValue> value = all_socket_values_.lookup_try(group_node_input);
    if (!value.has_value()) {
      this->push_value_task(group_node_input);
      return;
    }
    all_socket_values_.add_new(socket, *value);
  }

  void value_task__output__reroute_node(const SocketInContext &socket)
  {
    const SocketInContext input_socket = socket.owner_node().input_socket(0);
    const std::optional<InferenceValue> value = all_socket_values_.lookup_try(input_socket);
    if (!value.has_value()) {
      this->push_value_task(input_socket);
      return;
    }
    all_socket_values_.add_new(socket, *value);
  }

  void value_task__output__menu_switch_selection(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const SocketInContext input_socket = node.input_socket(0);
    const std::optional<InferenceValue> value = all_socket_values_.lookup_try(input_socket);
    if (!value.has_value()) {
      this->push_value_task(input_socket);
      return;
    }
    const std::optional<MenuValue> menu_value = value->get_if_primitive<MenuValue>();
    if (!menu_value.has_value()) {
      all_socket_values_.add_new(socket, InferenceValue::Unknown());
      return;
    }
    const NodeMenuSwitch &storage = *static_cast<const NodeMenuSwitch *>(node->storage);
    const NodeEnumItem &item = storage.enum_definition.items_array[socket->index() - 1];
    const bool is_selected = item.identifier == menu_value->value;
    all_socket_values_.add_new(socket, this->make_primitive_inference_value(is_selected));
  }

  void value_task__output__float_math(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const NodeMathOperation operation = NodeMathOperation(node->custom1);
    switch (operation) {
      case NODE_MATH_MULTIPLY: {
        this->value_task__output__generic_eval(
            socket, [&](const Span<InferenceValue> inputs) -> std::optional<InferenceValue> {
              const std::optional<float> a = inputs[0].get_if_primitive<float>();
              const std::optional<float> b = inputs[1].get_if_primitive<float>();
              if (a == 0.0f || b == 0.0f) {
                return this->make_primitive_inference_value(0.0f);
              }
              if (a.has_value() && b.has_value()) {
                return this->make_primitive_inference_value(*a * *b);
              }
              return std::nullopt;
            });
        break;
      }
      default: {
        this->value_task__output__multi_function_node(socket);
        break;
      }
    }
  }

  void value_task__output__vector_math(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const NodeVectorMathOperation operation = NodeVectorMathOperation(node->custom1);
    switch (operation) {
      case NODE_VECTOR_MATH_MULTIPLY: {
        this->value_task__output__generic_eval(
            socket, [&](const Span<InferenceValue> inputs) -> std::optional<InferenceValue> {
              const std::optional<float3> a = inputs[0].get_if_primitive<float3>();
              const std::optional<float3> b = inputs[1].get_if_primitive<float3>();
              if (a == float3(0.0f) || b == float3(0.0f)) {
                return this->make_primitive_inference_value(float3(0.0f));
              }
              if (a.has_value() && b.has_value()) {
                return this->make_primitive_inference_value(float3(*a * *b));
              }
              return std::nullopt;
            });
        break;
      }
      case NODE_VECTOR_MATH_SCALE: {
        this->value_task__output__generic_eval(
            socket, [&](const Span<InferenceValue> inputs) -> std::optional<InferenceValue> {
              const std::optional<float3> a = inputs[0].get_if_primitive<float3>();
              const std::optional<float> scale = inputs[3].get_if_primitive<float>();
              if (a == float3(0.0f) || scale == 0.0f) {
                return this->make_primitive_inference_value(float3(0.0f));
              }
              if (a.has_value() && scale.has_value()) {
                return this->make_primitive_inference_value(float3(*a * *scale));
              }
              return std::nullopt;
            });
        break;
      }
      default: {
        this->value_task__output__multi_function_node(socket);
        break;
      }
    }
  }

  void value_task__output__integer_math(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const NodeIntegerMathOperation operation = NodeIntegerMathOperation(node->custom1);
    switch (operation) {
      case NODE_INTEGER_MATH_MULTIPLY: {
        this->value_task__output__generic_eval(
            socket, [&](const Span<InferenceValue> inputs) -> std::optional<InferenceValue> {
              const std::optional<int> a = inputs[0].get_if_primitive<int>();
              const std::optional<int> b = inputs[1].get_if_primitive<int>();
              if (a == 0 || b == 0) {
                return this->make_primitive_inference_value(0);
              }
              if (a.has_value() && b.has_value()) {
                return this->make_primitive_inference_value(*a * *b);
              }
              return std::nullopt;
            });
        break;
      }
      default: {
        this->value_task__output__multi_function_node(socket);
        break;
      }
    }
  }

  void value_task__output__boolean_math(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const NodeBooleanMathOperation operation = NodeBooleanMathOperation(node->custom1);

    const auto handle_binary_op =
        [&](FunctionRef<std::optional<bool>(std::optional<bool>, std::optional<bool>)> fn) {
          this->value_task__output__generic_eval(
              socket, [&](const Span<InferenceValue> inputs) -> std::optional<InferenceValue> {
                const std::optional<bool> a = inputs[0].get_if_primitive<bool>();
                const std::optional<bool> b = inputs[1].get_if_primitive<bool>();
                const std::optional<bool> result = fn(a, b);
                if (result.has_value()) {
                  return this->make_primitive_inference_value(*result);
                }
                return std::nullopt;
              });
        };
    switch (operation) {
      case NODE_BOOLEAN_MATH_AND: {
        handle_binary_op(
            [](const std::optional<bool> &a, const std::optional<bool> &b) -> std::optional<bool> {
              if (a == false || b == false) {
                return false;
              }
              if (a.has_value() && b.has_value()) {
                return *a && *b;
              }
              return std::nullopt;
            });
        break;
      }
      case NODE_BOOLEAN_MATH_OR: {
        handle_binary_op(
            [](const std::optional<bool> &a, const std::optional<bool> &b) -> std::optional<bool> {
              if (a == true || b == true) {
                return true;
              }
              if (a.has_value() && b.has_value()) {
                return *a || *b;
              }
              return std::nullopt;
            });
        break;
      }
      case NODE_BOOLEAN_MATH_NAND: {
        handle_binary_op(
            [](const std::optional<bool> &a, const std::optional<bool> &b) -> std::optional<bool> {
              if (a == false || b == false) {
                return true;
              }
              if (a.has_value() && b.has_value()) {
                return !(*a && *b);
              }
              return std::nullopt;
            });
        break;
      }
      case NODE_BOOLEAN_MATH_NOR: {
        handle_binary_op(
            [](const std::optional<bool> &a, const std::optional<bool> &b) -> std::optional<bool> {
              if (a == true || b == true) {
                return false;
              }
              if (a.has_value() && b.has_value()) {
                return !(*a || *b);
              }
              return std::nullopt;
            });
        break;
      }
      case NODE_BOOLEAN_MATH_IMPLY: {
        handle_binary_op(
            [](const std::optional<bool> &a, const std::optional<bool> &b) -> std::optional<bool> {
              if (a == false || b == true) {
                return true;
              }
              if (a.has_value() && b.has_value()) {
                return !*a || *b;
              }
              return std::nullopt;
            });
        break;
      }
      case NODE_BOOLEAN_MATH_NIMPLY: {
        handle_binary_op(
            [](const std::optional<bool> &a, const std::optional<bool> &b) -> std::optional<bool> {
              if (a == false || b == true) {
                return false;
              }
              if (a.has_value() && b.has_value()) {
                return *a && !*b;
              }
              return std::nullopt;
            });
        break;
      }
      default: {
        this->value_task__output__multi_function_node(socket);
        break;
      }
    }
  }

  void value_task__output__warning(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const SocketInContext show_input_socket = node.input_socket(0);
    const std::optional<InferenceValue> value = all_socket_values_.lookup_try(show_input_socket);
    if (!value.has_value()) {
      this->push_value_task(show_input_socket);
      return;
    }
    all_socket_values_.add_new(socket, *value);
  }

  void value_task__output__enable_output(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const SocketInContext enable_input_socket = node.input_socket(0);
    const SocketInContext value_input_socket = node.input_socket(1);

    const std::optional<InferenceValue> keep_value = all_socket_values_.lookup_try(
        enable_input_socket);
    if (!keep_value.has_value()) {
      this->push_value_task(enable_input_socket);
      return;
    }
    if (!keep_value->is_primitive_value()) {
      all_socket_values_.add_new(socket, InferenceValue::Unknown());
      return;
    }
    const bool keep = keep_value->get_primitive<bool>();
    if (!keep) {
      const CPPType &type = *socket->typeinfo->base_cpp_type;
      all_socket_values_.add_new(socket, InferenceValue::from_primitive(type.default_value()));
      return;
    }
    const std::optional<InferenceValue> value = all_socket_values_.lookup_try(value_input_socket);
    if (!value.has_value()) {
      this->push_value_task(value_input_socket);
      return;
    }
    all_socket_values_.add_new(socket, *value);
  }

  /**
   * Assumes that the first available input is a condition that selects one of the remaining inputs
   * which is then output.
   */
  void value_task__output__generic_switch(
      const SocketInContext &socket,
      const FunctionRef<bool(const SocketInContext &socket, InferenceValue condition)>
          is_selected_socket)
  {
    const NodeInContext node = socket.owner_node();
    BLI_assert(node->input_sockets().size() >= 1);
    BLI_assert(node->output_sockets().size() >= 1);

    const SocketInContext condition_socket{socket.context,
                                           get_first_available_bsocket(node->input_sockets())};
    const std::optional<InferenceValue> condition_value = all_socket_values_.lookup_try(
        condition_socket);
    if (!condition_value.has_value()) {
      this->push_value_task(condition_socket);
      return;
    }
    if (condition_value->is_unknown()) {
      /* The condition value is not a simple static value, so the output is unknown. */
      all_socket_values_.add_new(socket, InferenceValue::Unknown());
      return;
    }
    Vector<const bNodeSocket *> selected_inputs;
    for (const int input_i :
         node->input_sockets().index_range().drop_front(condition_socket->index() + 1))
    {
      const SocketInContext input_socket = node.input_socket(input_i);
      if (!input_socket->is_available()) {
        continue;
      }
      if (input_socket->type == SOCK_CUSTOM && STREQ(input_socket->idname, "NodeSocketVirtual")) {
        continue;
      }
      const bool is_selected = is_selected_socket(input_socket, *condition_value);
      if (is_selected) {
        selected_inputs.append(input_socket.socket);
      }
    }
    if (selected_inputs.is_empty()) {
      all_socket_values_.add_new(socket, InferenceValue::Unknown());
      return;
    }
    if (selected_inputs.size() == 1) {
      /* A single input is selected, so just pass through this value without regarding others. */
      const SocketInContext selected_input{socket.context, selected_inputs[0]};
      const std::optional<InferenceValue> input_value = all_socket_values_.lookup_try(
          selected_input);
      if (!input_value.has_value()) {
        this->push_value_task(selected_input);
        return;
      }
      all_socket_values_.add_new(socket, *input_value);
      return;
    }

    /* Multiple inputs are selected. */
    if (node->typeinfo->build_multi_function) {
      /* Try to compute the output value from the multiple selected inputs. */
      this->value_task__output__multi_function_node(socket);
      return;
    }
    /* Can't compute the output value, so set it to be unknown. */
    all_socket_values_.add_new(socket, InferenceValue::Unknown());
  }

  void value_task__output__generic_eval(
      const SocketInContext &socket,
      const FunctionRef<std::optional<InferenceValue>(Span<InferenceValue> inputs)> eval_fn)
  {
    const NodeInContext node = socket.owner_node();
    const int inputs_num = node->input_sockets().size();

    Array<InferenceValue, 16> input_values(inputs_num, InferenceValue::Unknown());
    std::optional<int> next_unknown_input_index;
    for (const int input_i : IndexRange(inputs_num)) {
      const SocketInContext input_socket = node.input_socket(input_i);
      if (!input_socket->is_available()) {
        continue;
      }
      const std::optional<InferenceValue> input_value = all_socket_values_.lookup_try(
          input_socket);
      if (!input_value.has_value()) {
        next_unknown_input_index = input_i;
        break;
      }
      input_values[input_i] = *input_value;
    }
    const std::optional<InferenceValue> output_value = eval_fn(input_values);
    if (output_value.has_value()) {
      /* Was able to compute the output value. */
      all_socket_values_.add_new(socket, *output_value);
      return;
    }
    if (!next_unknown_input_index.has_value()) {
      /* The output is still unknown even though we know as much about the inputs as possible
       * already. */
      all_socket_values_.add_new(socket, InferenceValue::Unknown());
      return;
    }
    /* Request the next input socket. */
    const SocketInContext next_input = node.input_socket(*next_unknown_input_index);
    this->push_value_task(next_input);
  }

  void value_task__output__multi_function_node(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const int inputs_num = node->input_sockets().size();

    /* Gather all input values are return early if any of them is not known. */
    Vector<const void *> input_values(inputs_num);
    for (const int input_i : IndexRange(inputs_num)) {
      const SocketInContext input_socket = node.input_socket(input_i);
      const std::optional<InferenceValue> input_value = all_socket_values_.lookup_try(
          input_socket);
      if (!input_value.has_value()) {
        this->push_value_task(input_socket);
        return;
      }
      if (!input_value->is_primitive_value()) {
        all_socket_values_.add_new(socket, InferenceValue::Unknown());
        return;
      }
      input_values[input_i] = input_value->get_primitive_ptr();
    }

    /* Get the multi-function for the node. */
    NodeMultiFunctionBuilder builder{*node.node, node->owner_tree()};
    node->typeinfo->build_multi_function(builder);
    const mf::MultiFunction &fn = builder.function();

    /* We only evaluate the node for a single value here. */
    const IndexMask mask(1);

    /* Prepare parameters for the multi-function evaluation. */
    mf::ParamsBuilder params{fn, &mask};
    for (const int input_i : IndexRange(inputs_num)) {
      const SocketInContext input_socket = node.input_socket(input_i);
      if (!input_socket->is_available()) {
        continue;
      }
      params.add_readonly_single_input(
          GPointer(input_socket->typeinfo->base_cpp_type, input_values[input_i]));
    }
    for (const int output_i : node->output_sockets().index_range()) {
      const SocketInContext output_socket = node.output_socket(output_i);
      if (!output_socket->is_available()) {
        continue;
      }
      /* Allocate memory for the output value. */
      const CPPType &base_type = *output_socket->typeinfo->base_cpp_type;
      void *value = scope_.allocate_owned(base_type);
      params.add_uninitialized_single_output(GMutableSpan(base_type, value, 1));
      all_socket_values_.add_new(output_socket, InferenceValue::from_primitive(value));
    }
    mf::ContextBuilder context;
    /* Actually evaluate the multi-function. The outputs will be written into the memory allocated
     * earlier, which has been added to #all_socket_values_ already. */
    fn.call(mask, params, context);
  }

  void value_task__output__muted_node(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();

    SocketInContext input_socket;
    for (const bNodeLink &internal_link : node->internal_links()) {
      if (internal_link.tosock == socket.socket) {
        input_socket = SocketInContext{socket.context, internal_link.fromsock};
        break;
      }
    }
    if (!input_socket) {
      /* The output does not have an internal link to an input. */
      all_socket_values_.add_new(socket, InferenceValue::Unknown());
      return;
    }
    const std::optional<InferenceValue> input_value = all_socket_values_.lookup_try(input_socket);
    if (!input_value.has_value()) {
      this->push_value_task(input_socket);
      return;
    }
    const InferenceValue converted_value = this->convert_type_if_necessary(
        *input_value, *input_socket.socket, *socket.socket);
    all_socket_values_.add_new(socket, converted_value);
  }

  void value_task__input(const SocketInContext &socket)
  {
    if (socket->is_multi_input()) {
      /* Can't know the single value of a multi-input. */
      all_socket_values_.add_new(socket, InferenceValue::Unknown());
      return;
    }
    const bNodeLink *source_link = nullptr;
    const Span<const bNodeLink *> connected_links = socket->directly_linked_links();
    for (const bNodeLink *link : connected_links) {
      if (!link->is_used()) {
        continue;
      }
      if (link->fromnode->is_dangling_reroute()) {
        continue;
      }
      source_link = link;
      break;
    }
    if (!source_link) {
      this->value_task__input__unlinked(socket);
      return;
    }
    this->value_task__input__linked({socket.context, source_link->fromsock}, socket);
  }

  void value_task__input__unlinked(const SocketInContext &socket)
  {
    if (this->treat_socket_as_unknown(socket)) {
      all_socket_values_.add_new(socket, InferenceValue::Unknown());
      return;
    }
    if (animated_sockets_.contains(socket.socket)) {
      /* The value of animated sockets is not known statically. */
      all_socket_values_.add_new(socket, InferenceValue::Unknown());
      return;
    }
    if (const SocketDeclaration *socket_decl = socket.socket->runtime->declaration) {
      if (socket_decl->input_field_type == InputSocketFieldType::Implicit) {
        /* Implicit fields inputs don't have a single static value. */
        all_socket_values_.add_new(socket, InferenceValue::Unknown());
        return;
      }
    }

    void *value_buffer = scope_.allocate_owned(*socket->typeinfo->base_cpp_type);
    socket->typeinfo->get_base_cpp_value(socket->default_value, value_buffer);
    all_socket_values_.add_new(socket, InferenceValue::from_primitive(value_buffer));
  }

  void value_task__input__linked(const SocketInContext &from_socket,
                                 const SocketInContext &to_socket)
  {
    const std::optional<InferenceValue> from_value = all_socket_values_.lookup_try(from_socket);
    if (!from_value.has_value()) {
      this->push_value_task(from_socket);
      return;
    }
    const InferenceValue converted_value = this->convert_type_if_necessary(
        *from_value, *from_socket.socket, *to_socket.socket);
    all_socket_values_.add_new(to_socket, converted_value);
  }

  InferenceValue convert_type_if_necessary(const InferenceValue &src,
                                           const bNodeSocket &from_socket,
                                           const bNodeSocket &to_socket)
  {
    if (!src.is_primitive_value()) {
      return InferenceValue::Unknown();
    }
    const CPPType *from_type = from_socket.typeinfo->base_cpp_type;
    const CPPType *to_type = to_socket.typeinfo->base_cpp_type;
    if (from_type == to_type) {
      return src;
    }
    if (!to_type) {
      return InferenceValue::Unknown();
    }
    const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
    if (!conversions.is_convertible(*from_type, *to_type)) {
      return InferenceValue::Unknown();
    }
    void *dst = scope_.allocate_owned(*to_type);
    conversions.convert_to_uninitialized(*from_type, *to_type, src.get_primitive_ptr(), dst);
    return InferenceValue::from_primitive(dst);
  }

  bool treat_socket_as_unknown(const SocketInContext &socket) const
  {
    if (!top_level_ignored_inputs_.has_value()) {
      return false;
    }
    if (socket.context) {
      return false;
    }
    if (socket->is_output()) {
      return false;
    }
    return (*top_level_ignored_inputs_)[socket->index_in_all_inputs()];
  }

  void ensure_animation_data_processed(const bNodeTree &tree)
  {
    if (!trees_with_handled_animation_data_.add(&tree)) {
      return;
    }
    if (!tree.adt) {
      return;
    }

    static std::regex pattern(R"#(nodes\["(.*)"\].inputs\[(\d+)\].default_value)#");
    MultiValueMap<StringRef, int> animated_inputs_by_node_name;
    auto handle_rna_path = [&](const char *rna_path) {
      std::cmatch match;
      if (!std::regex_match(rna_path, match, pattern)) {
        return;
      }
      const StringRef node_name{match[1].first, match[1].second - match[1].first};
      const int socket_index = std::stoi(match[2]);
      animated_inputs_by_node_name.add(node_name, socket_index);
    };

    /* Gather all inputs controlled by fcurves. */
    if (tree.adt->action) {
      animrig::foreach_fcurve_in_action_slot(
          tree.adt->action->wrap(), tree.adt->slot_handle, [&](const FCurve &fcurve) {
            handle_rna_path(fcurve.rna_path);
          });
    }
    /* Gather all inputs controlled by drivers. */
    LISTBASE_FOREACH (const FCurve *, driver, &tree.adt->drivers) {
      handle_rna_path(driver->rna_path);
    }

    /* Actually find the #bNodeSocket for each controlled input. */
    if (!animated_inputs_by_node_name.is_empty()) {
      for (const bNode *node : tree.all_nodes()) {
        const Span<int> animated_inputs = animated_inputs_by_node_name.lookup(node->name);
        const Span<const bNodeSocket *> input_sockets = node->input_sockets();
        for (const int socket_index : animated_inputs) {
          if (socket_index < 0 || socket_index >= input_sockets.size()) {
            /* This can happen when the animation data is not immediately updated after a socket is
             * removed. */
            continue;
          }
          const bNodeSocket &socket = *input_sockets[socket_index];
          animated_sockets_.add(&socket);
        }
      }
    }
  }

  void push_value_task(const SocketInContext &socket)
  {
    value_tasks_.push(socket);
  }

  template<typename T> InferenceValue make_primitive_inference_value(const T &value)
  {
    static_assert(is_same_any_v<std::decay_t<T>, bool, float, int, float3>);
    return InferenceValue::from_primitive(&scope_.construct<T>(value));
  }

  static const bNodeSocket *get_first_available_bsocket(const Span<const bNodeSocket *> sockets)
  {
    for (const bNodeSocket *socket : sockets) {
      if (socket->is_available()) {
        return socket;
      }
    }
    return nullptr;
  }
};

SocketValueInferencer::SocketValueInferencer(
    const bNodeTree &tree,
    ResourceScope &scope,
    bke::ComputeContextCache &compute_context_cache,
    const FunctionRef<InferenceValue(int group_input_i)> group_input_value_fn,
    const std::optional<Span<bool>> top_level_ignored_inputs)
    : impl_(scope.construct<SocketValueInferencerImpl>(
          tree, scope, compute_context_cache, group_input_value_fn, top_level_ignored_inputs))
{
}

InferenceValue SocketValueInferencer::get_socket_value(const SocketInContext &socket)
{
  return impl_.get_socket_value(socket);
}

namespace switch_node_inference_utils {

bool is_socket_selected__switch(const SocketInContext &socket, const InferenceValue &condition)
{
  if (!condition.is_primitive_value()) {
    return true;
  }
  const bool is_true = condition.get_primitive<bool>();
  const int selected_index = is_true ? 2 : 1;
  return socket->index() == selected_index;
}

bool is_socket_selected__index_switch(const SocketInContext &socket,
                                      const InferenceValue &condition)
{
  if (!condition.is_primitive_value()) {
    return true;
  }
  const int index = condition.get_primitive<int>();
  return socket->index() == index + 1;
}

bool is_socket_selected__menu_switch(const SocketInContext &socket,
                                     const InferenceValue &condition)
{
  if (!condition.is_primitive_value()) {
    return true;
  }
  const NodeMenuSwitch &storage = *static_cast<const NodeMenuSwitch *>(
      socket->owner_node().storage);
  const int menu_value = condition.get_primitive<int>();
  const NodeEnumItem &item = storage.enum_definition.items_array[socket->index() - 1];
  return menu_value == item.identifier;
}

bool is_socket_selected__mix_node(const SocketInContext &socket, const InferenceValue &condition)
{
  if (!condition.is_primitive_value()) {
    return true;
  }
  const NodeShaderMix &storage = *static_cast<const NodeShaderMix *>(socket.owner_node()->storage);
  if (storage.data_type == SOCK_RGBA && storage.blend_type != MA_RAMP_BLEND) {
    return true;
  }

  const bool clamp_factor = storage.clamp_factor != 0;
  bool only_a = false;
  bool only_b = false;
  if (storage.data_type == SOCK_VECTOR && storage.factor_mode == NODE_MIX_MODE_NON_UNIFORM) {
    const float3 mix_factor = condition.get_primitive<float3>();
    if (clamp_factor) {
      only_a = mix_factor.x <= 0.0f && mix_factor.y <= 0.0f && mix_factor.z <= 0.0f;
      only_b = mix_factor.x >= 1.0f && mix_factor.y >= 1.0f && mix_factor.z >= 1.0f;
    }
    else {
      only_a = float3{0.0f, 0.0f, 0.0f} == mix_factor;
      only_b = float3{1.0f, 1.0f, 1.0f} == mix_factor;
    }
  }
  else {
    const float mix_factor = condition.get_primitive<float>();
    if (clamp_factor) {
      only_a = mix_factor <= 0.0f;
      only_b = mix_factor >= 1.0f;
    }
    else {
      only_a = mix_factor == 0.0f;
      only_b = mix_factor == 1.0f;
    }
  }
  if (only_a) {
    if (STREQ(socket->name, "B")) {
      return false;
    }
  }
  if (only_b) {
    if (STREQ(socket->name, "A")) {
      return false;
    }
  }
  return true;
}

bool is_socket_selected__shader_mix_node(const SocketInContext &socket,
                                         const InferenceValue &condition)
{
  if (!condition.is_primitive_value()) {
    return true;
  }
  const float mix_factor = condition.get_primitive<float>();
  if (mix_factor == 0.0f) {
    if (STREQ(socket->identifier, "Shader_001")) {
      return false;
    }
  }
  else if (mix_factor == 1.0f) {
    if (STREQ(socket->identifier, "Shader")) {
      return false;
    }
  }
  return true;
}

}  // namespace switch_node_inference_utils

}  // namespace blender::nodes

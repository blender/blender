/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <regex>

#include "NOD_geometry_nodes_execute.hh"
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

namespace blender::nodes::socket_usage_inference {

/** Utility class to simplify passing global state into all the functions during inferencing. */
struct SocketUsageInferencer {
 private:
  friend InputSocketUsageParams;

  /** Owns e.g. intermediate evaluated values. */
  ResourceScope scope_;
  bke::ComputeContextCache compute_context_cache_;

  /** Root node tree. */
  const bNodeTree &root_tree_;

  /**
   * Stack of tasks that allows depth-first (partial) evaluation of the tree.
   */
  Stack<SocketInContext> usage_tasks_;
  Stack<SocketInContext> value_tasks_;

  /**
   * If the usage of a socket is known, it is added to this map. Sockets not in this map are not
   * known yet.
   */
  Map<SocketInContext, bool> all_socket_usages_;

  /**
   * Once a socket value has been determined, it is added to this map. Note that a socket value may
   * be determined to be unknown because it depends on values that are not known statically.
   */
  Map<SocketInContext, InferenceValue> all_socket_values_;

  /**
   * All sockets that have animation data and thus their value is not fixed statically. This can
   * contain sockets from multiple different trees.
   */
  Set<const bNodeSocket *> animated_sockets_;
  Set<const bNodeTree *> trees_with_handled_animation_data_;

  /** Some inline storage to reduce the number of allocations. */
  AlignedBuffer<1024, 8> scope_buffer_;

  std::optional<Span<bool>> top_level_ignored_inputs_;

 public:
  SocketUsageInferencer(const bNodeTree &tree,
                        const std::optional<Span<GPointer>> tree_input_values,
                        const std::optional<Span<bool>> top_level_ignored_inputs = std::nullopt)
      : root_tree_(tree), top_level_ignored_inputs_(top_level_ignored_inputs)
  {
    scope_.allocator().provide_buffer(scope_buffer_);
    root_tree_.ensure_topology_cache();
    root_tree_.ensure_interface_cache();
    this->ensure_animation_data_processed(root_tree_);

    for (const bNode *node : root_tree_.group_input_nodes()) {
      for (const int i : root_tree_.interface_inputs().index_range()) {
        const bNodeSocket &socket = node->output_socket(i);
        const SocketInContext socket_in_context{nullptr, &socket};
        const void *input_value = nullptr;
        if (!this->treat_socket_as_unknown(socket_in_context)) {
          if (tree_input_values.has_value()) {
            input_value = (*tree_input_values)[i].get();
          }
        }
        all_socket_values_.add_new(socket_in_context, InferenceValue(input_value));
      }
    }
  }

  void mark_top_level_node_outputs_as_used()
  {
    for (const bNodeSocket *socket : root_tree_.all_output_sockets()) {
      all_socket_usages_.add_new({nullptr, socket}, true);
    }
  }

  bool is_group_input_used(const int input_i)
  {
    for (const bNode *node : root_tree_.group_input_nodes()) {
      const SocketInContext socket{nullptr, &node->output_socket(input_i)};
      if (this->is_socket_used(socket)) {
        return true;
      }
    }
    return false;
  }

  bool is_socket_used(const SocketInContext &socket)
  {
    const std::optional<bool> is_used = all_socket_usages_.lookup_try(socket);
    if (is_used.has_value()) {
      return *is_used;
    }
    if (socket->owner_tree().has_available_link_cycle()) {
      return false;
    }

    BLI_assert(usage_tasks_.is_empty());
    usage_tasks_.push(socket);

    while (!usage_tasks_.is_empty()) {
      const SocketInContext &socket = usage_tasks_.peek();
      this->usage_task(socket);
      if (&socket == &usage_tasks_.peek()) {
        /* The task is finished if it hasn't added any new task it depends on. */
        usage_tasks_.pop();
      }
    }

    return all_socket_usages_.lookup(socket);
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
  void usage_task(const SocketInContext &socket)
  {
    if (all_socket_usages_.contains(socket)) {
      return;
    }
    const bNode &node = socket->owner_node();
    if (!socket->is_available()) {
      all_socket_usages_.add_new(socket, false);
      return;
    }
    if (node.is_undefined() && !node.is_custom_group()) {
      all_socket_usages_.add_new(socket, false);
      return;
    }
    if (socket->is_input()) {
      this->usage_task__input(socket);
    }
    else {
      this->usage_task__output(socket);
    }
  }

  void usage_task__input(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();

    if (node->is_muted()) {
      this->usage_task__input__muted_node(socket);
      return;
    }

    switch (node->type_legacy) {
      case NODE_GROUP:
      case NODE_CUSTOM_GROUP: {
        this->usage_task__input__group_node(socket);
        break;
      }
      case NODE_GROUP_OUTPUT: {
        this->usage_task__input__group_output_node(socket);
        break;
      }
      case GEO_NODE_SWITCH: {
        this->usage_task__input__generic_switch(socket, switch__is_socket_selected);
        break;
      }
      case GEO_NODE_INDEX_SWITCH: {
        this->usage_task__input__generic_switch(socket, index_switch__is_socket_selected);
        break;
      }
      case GEO_NODE_MENU_SWITCH: {
        this->usage_task__input__generic_switch(socket, menu_switch__is_socket_selected);
        break;
      }
      case SH_NODE_MIX: {
        this->usage_task__input__generic_switch(socket, mix_node__is_socket_selected);
        break;
      }
      case SH_NODE_MIX_SHADER: {
        this->usage_task__input__generic_switch(socket, shader_mix_node__is_socket_selected);
        break;
      }
      case GEO_NODE_SIMULATION_INPUT: {
        this->usage_task__input__simulation_input_node(socket);
        break;
      }
      case GEO_NODE_REPEAT_INPUT: {
        this->usage_task__input__repeat_input_node(socket);
        break;
      }
      case GEO_NODE_FOREACH_GEOMETRY_ELEMENT_INPUT: {
        this->usage_task__input__foreach_element_input_node(socket);
        break;
      }
      case GEO_NODE_FOREACH_GEOMETRY_ELEMENT_OUTPUT: {
        this->usage_task__input__foreach_element_output_node(socket);
        break;
      }
      case GEO_NODE_CAPTURE_ATTRIBUTE: {
        this->usage_task__input__capture_attribute_node(socket);
        break;
      }
      case SH_NODE_OUTPUT_AOV:
      case SH_NODE_OUTPUT_LIGHT:
      case SH_NODE_OUTPUT_WORLD:
      case SH_NODE_OUTPUT_LINESTYLE:
      case SH_NODE_OUTPUT_MATERIAL:
      case CMP_NODE_OUTPUT_FILE:
      case TEX_NODE_OUTPUT: {
        this->usage_task__input__output_node(socket);
        break;
      }
      default: {
        this->usage_task__input__fallback(socket);
        break;
      }
    }
  }

  void usage_task__input__output_node(const SocketInContext &socket)
  {
    all_socket_usages_.add_new(socket, true);
  }

  /**
   * Assumes that the first input is a condition that selects one of the remaining inputs which is
   * then output. If necessary, this can trigger a value task for the condition socket.
   */
  void usage_task__input__generic_switch(
      const SocketInContext &socket,
      const FunctionRef<bool(const SocketInContext &socket, const InferenceValue &condition)>
          is_selected_socket)
  {
    const NodeInContext node = socket.owner_node();
    BLI_assert(node->input_sockets().size() >= 1);
    BLI_assert(node->output_sockets().size() >= 1);

    if (socket->type == SOCK_CUSTOM && STREQ(socket->idname, "NodeSocketVirtual")) {
      all_socket_usages_.add_new(socket, false);
      return;
    }
    const SocketInContext output_socket{socket.context,
                                        this->get_first_available_bsocket(node->output_sockets())};
    const std::optional<bool> output_is_used = all_socket_usages_.lookup_try(output_socket);
    if (!output_is_used.has_value()) {
      this->push_usage_task(output_socket);
      return;
    }
    if (!*output_is_used) {
      all_socket_usages_.add_new(socket, false);
      return;
    }
    const SocketInContext condition_socket{
        socket.context, this->get_first_available_bsocket(node->input_sockets())};
    if (socket == condition_socket) {
      all_socket_usages_.add_new(socket, true);
      return;
    }
    const InferenceValue condition_value = this->get_socket_value(condition_socket);
    if (condition_value.is_unknown()) {
      /* The exact condition value is unknown, so any input may be used. */
      all_socket_usages_.add_new(socket, true);
      return;
    }
    const bool is_used = is_selected_socket(socket, condition_value);
    all_socket_usages_.add_new(socket, is_used);
  }

  const bNodeSocket *get_first_available_bsocket(const Span<const bNodeSocket *> sockets) const
  {
    for (const bNodeSocket *socket : sockets) {
      if (socket->is_available()) {
        return socket;
      }
    }
    return nullptr;
  }

  void usage_task__input__group_node(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node->id);
    if (!group || ID_MISSING(&group->id)) {
      all_socket_usages_.add_new(socket, false);
      return;
    }
    group->ensure_topology_cache();
    if (group->has_available_link_cycle()) {
      all_socket_usages_.add_new(socket, false);
      return;
    }
    this->ensure_animation_data_processed(*group);

    /* The group node input is used if any of the matching group inputs within the group is
     * used. */
    const ComputeContext &group_context = compute_context_cache_.for_group_node(
        socket.context, node->identifier, &node->owner_tree());
    Vector<const bNodeSocket *> dependent_sockets;
    for (const bNode *group_input_node : group->group_input_nodes()) {
      dependent_sockets.append(&group_input_node->output_socket(socket->index()));
    }
    this->usage_task__with_dependent_sockets(socket, dependent_sockets, {}, &group_context);
  }

  void usage_task__input__group_output_node(const SocketInContext &socket)
  {
    const int output_i = socket->index();
    if (socket.context == nullptr) {
      /* This is a final output which is always used. */
      all_socket_usages_.add_new(socket, true);
      return;
    }
    /* The group output node is used if the matching output of the parent group node is used. */
    const bke::GroupNodeComputeContext &group_context =
        *static_cast<const bke::GroupNodeComputeContext *>(socket.context);
    const bNodeSocket &group_node_output = group_context.node()->output_socket(output_i);
    this->usage_task__with_dependent_sockets(
        socket, {&group_node_output}, {}, group_context.parent());
  }

  void usage_task__output(const SocketInContext &socket)
  {
    /* An output socket is used if any of the sockets it is connected to is used. */
    Vector<const bNodeSocket *> dependent_sockets;
    for (const bNodeLink *link : socket->directly_linked_links()) {
      if (link->is_used()) {
        dependent_sockets.append(link->tosock);
      }
    }
    this->usage_task__with_dependent_sockets(socket, dependent_sockets, {}, socket.context);
  }

  void usage_task__input__simulation_input_node(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const bNodeTree &tree = socket->owner_tree();

    const NodeGeometrySimulationInput &storage = *static_cast<const NodeGeometrySimulationInput *>(
        node->storage);
    const bNode *sim_output_node = tree.node_by_id(storage.output_node_id);
    if (!sim_output_node) {
      all_socket_usages_.add_new(socket, false);
      return;
    }
    /* Simulation inputs are also used when any of the simulation outputs are used. */
    Vector<const bNodeSocket *, 16> dependent_sockets;
    dependent_sockets.extend(node->output_sockets());
    dependent_sockets.extend(sim_output_node->output_sockets());
    this->usage_task__with_dependent_sockets(socket, dependent_sockets, {}, socket.context);
  }

  void usage_task__input__repeat_input_node(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const bNodeTree &tree = socket->owner_tree();

    const NodeGeometryRepeatInput &storage = *static_cast<const NodeGeometryRepeatInput *>(
        node->storage);
    const bNode *repeat_output_node = tree.node_by_id(storage.output_node_id);
    if (!repeat_output_node) {
      all_socket_usages_.add_new(socket, false);
      return;
    }
    /* Assume that all repeat inputs are used when any of the outputs are used. This check could
     * become more precise in the future if necessary. */
    Vector<const bNodeSocket *, 16> dependent_sockets;
    dependent_sockets.extend(node->output_sockets());
    dependent_sockets.extend(repeat_output_node->output_sockets());
    this->usage_task__with_dependent_sockets(socket, dependent_sockets, {}, socket.context);
  }

  void usage_task__input__foreach_element_output_node(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    this->usage_task__with_dependent_sockets(
        socket, {&node->output_by_identifier(socket->identifier)}, {}, socket.context);
  }

  void usage_task__input__capture_attribute_node(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    this->usage_task__with_dependent_sockets(
        socket, {&node->output_socket(socket->index())}, {}, socket.context);
  }

  void usage_task__input__fallback(const SocketInContext &socket)
  {
    const SocketDeclaration *socket_decl = socket->runtime->declaration;
    if (!socket_decl) {
      all_socket_usages_.add_new(socket, true);
      return;
    }
    if (!socket_decl->usage_inference_fn) {
      this->usage_task__with_dependent_sockets(
          socket, socket->owner_node().output_sockets(), {}, socket.context);
      return;
    }
    InputSocketUsageParams params{
        *this, socket.context, socket->owner_tree(), socket->owner_node(), *socket};
    const std::optional<bool> is_used = (*socket_decl->usage_inference_fn)(params);
    if (!is_used.has_value()) {
      /* Some value was requested, come back later when that value is available. */
      return;
    }
    all_socket_usages_.add_new(socket, *is_used);
  }

  void usage_task__input__foreach_element_input_node(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const bNodeTree &tree = socket->owner_tree();

    const NodeGeometryForeachGeometryElementInput &storage =
        *static_cast<const NodeGeometryForeachGeometryElementInput *>(node->storage);
    const bNode *foreach_output_node = tree.node_by_id(storage.output_node_id);
    if (!foreach_output_node) {
      all_socket_usages_.add_new(socket, false);
      return;
    }
    Vector<const bNodeSocket *, 16> dependent_sockets;
    if (StringRef(socket->identifier).startswith("Input_")) {
      dependent_sockets.append(&node->output_by_identifier(socket->identifier));
    }
    else {
      /* The geometry and selection inputs are used whenever any of the zone outputs is used. */
      dependent_sockets.extend(node->output_sockets());
      dependent_sockets.extend(foreach_output_node->output_sockets());
    }
    this->usage_task__with_dependent_sockets(socket, dependent_sockets, {}, socket.context);
  }

  void usage_task__input__muted_node(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    Vector<const bNodeSocket *> dependent_sockets;
    for (const bNodeLink &internal_link : node->internal_links()) {
      if (internal_link.fromsock != socket.socket) {
        continue;
      }
      dependent_sockets.append(internal_link.tosock);
    }
    this->usage_task__with_dependent_sockets(socket, dependent_sockets, {}, socket.context);
  }

  /**
   * Utility that handles simple cases where a socket is used if any of its dependent sockets is
   * used.
   */
  void usage_task__with_dependent_sockets(const SocketInContext &socket,
                                          const Span<const bNodeSocket *> dependent_outputs,
                                          const Span<const bNodeSocket *> condition_inputs,
                                          const ComputeContext *dependent_socket_context)
  {
    /* Check if any of the dependent outputs are used. */
    SocketInContext next_unknown_output;
    bool any_output_used = false;
    for (const bNodeSocket *dependent_socket_ptr : dependent_outputs) {
      const SocketInContext dependent_socket{dependent_socket_context, dependent_socket_ptr};
      const std::optional<bool> is_used = all_socket_usages_.lookup_try(dependent_socket);
      if (!is_used.has_value() && !next_unknown_output) {
        next_unknown_output = dependent_socket;
        continue;
      }
      if (is_used.value_or(false)) {
        any_output_used = true;
        break;
      }
    }
    if (next_unknown_output) {
      /* Create a task that checks if the next dependent socket is used. Intentionally only create
       * a task for the very next one and not for all, because that could potentially trigger a lot
       * of unnecessary evaluations. */
      this->push_usage_task(next_unknown_output);
      return;
    }
    if (!any_output_used) {
      all_socket_usages_.add_new(socket, false);
      return;
    }
    bool all_condition_inputs_true = true;
    for (const bNodeSocket *condition_input_ptr : condition_inputs) {
      const SocketInContext condition_input{dependent_socket_context, condition_input_ptr};
      const InferenceValue condition_value = this->get_socket_value(condition_input);
      if (condition_value.is_unknown()) {
        /* The condition is not known, so it may be true. */
        continue;
      }
      BLI_assert(condition_input_ptr->type == SOCK_BOOLEAN);
      if (!condition_value.get_known<bool>()) {
        all_condition_inputs_true = false;
        break;
      }
    }
    all_socket_usages_.add_new(socket, all_condition_inputs_true);
  }

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
        this->value_task__output__generic_switch(socket, switch__is_socket_selected);
        return;
      }
      case GEO_NODE_INDEX_SWITCH: {
        this->value_task__output__generic_switch(socket, index_switch__is_socket_selected);
        return;
      }
      case GEO_NODE_MENU_SWITCH: {
        this->value_task__output__generic_switch(socket, menu_switch__is_socket_selected);
        return;
      }
      case SH_NODE_MIX: {
        this->value_task__output__generic_switch(socket, mix_node__is_socket_selected);
        return;
      }
      case SH_NODE_MIX_SHADER: {
        this->value_task__output__generic_switch(socket, shader_mix_node__is_socket_selected);
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
      default: {
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
    /* Group inputs for the root context should be initialized already. */
    BLI_assert(socket.context != nullptr);

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

  void value_task__output__float_math(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const NodeMathOperation operation = NodeMathOperation(node->custom1);
    switch (operation) {
      case NODE_MATH_MULTIPLY: {
        this->value_task__output__generic_eval(
            socket, [&](const Span<InferenceValue> inputs) -> std::optional<InferenceValue> {
              const std::optional<float> a = inputs[0].get<float>();
              const std::optional<float> b = inputs[1].get<float>();
              if (a == 0.0f || b == 0.0f) {
                return InferenceValue(&scope_.construct<float>(0.0f));
              }
              if (a.has_value() && b.has_value()) {
                return InferenceValue(&scope_.construct<float>(*a * *b));
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
              const std::optional<float3> a = inputs[0].get<float3>();
              const std::optional<float3> b = inputs[1].get<float3>();
              if (a == float3(0.0f) || b == float3(0.0f)) {
                return InferenceValue(&scope_.construct<float3>(0.0f));
              }
              if (a.has_value() && b.has_value()) {
                return InferenceValue(&scope_.construct<float3>(*a * *b));
              }
              return std::nullopt;
            });
        break;
      }
      case NODE_VECTOR_MATH_SCALE: {
        this->value_task__output__generic_eval(
            socket, [&](const Span<InferenceValue> inputs) -> std::optional<InferenceValue> {
              const std::optional<float3> a = inputs[0].get<float3>();
              const std::optional<float> scale = inputs[3].get<float>();
              if (a == float3(0.0f) || scale == 0.0f) {
                return InferenceValue(&scope_.construct<float3>(0.0f));
              }
              if (a.has_value() && scale.has_value()) {
                return InferenceValue(&scope_.construct<float3>(*a * *scale));
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
              const std::optional<int> a = inputs[0].get<int>();
              const std::optional<int> b = inputs[1].get<int>();
              if (a == 0 || b == 0) {
                return InferenceValue(&scope_.construct<int>(0));
              }
              if (a.has_value() && b.has_value()) {
                return InferenceValue(&scope_.construct<int>(*a * *b));
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
                const std::optional<bool> a = inputs[0].get<bool>();
                const std::optional<bool> b = inputs[1].get<bool>();
                const std::optional<bool> result = fn(a, b);
                if (result.has_value()) {
                  return InferenceValue(&scope_.construct<bool>(*result));
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

    const SocketInContext condition_socket{
        socket.context, this->get_first_available_bsocket(node->input_sockets())};
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
      if (input_value->is_unknown()) {
        all_socket_values_.add_new(socket, InferenceValue::Unknown());
        return;
      }
      input_values[input_i] = input_value->data();
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
      all_socket_values_.add_new(output_socket, InferenceValue(value));
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
    const void *converted_value = this->convert_type_if_necessary(
        input_value->data(), *input_socket.socket, *socket.socket);
    all_socket_values_.add_new(socket, InferenceValue(converted_value));
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
    all_socket_values_.add_new(socket, InferenceValue(value_buffer));
  }

  void value_task__input__linked(const SocketInContext &from_socket,
                                 const SocketInContext &to_socket)
  {
    const std::optional<InferenceValue> from_value = all_socket_values_.lookup_try(from_socket);
    if (!from_value.has_value()) {
      this->push_value_task(from_socket);
      return;
    }
    const void *converted_value = this->convert_type_if_necessary(
        from_value->data(), *from_socket.socket, *to_socket.socket);
    all_socket_values_.add_new(to_socket, InferenceValue(converted_value));
  }

  const void *convert_type_if_necessary(const void *src,
                                        const bNodeSocket &from_socket,
                                        const bNodeSocket &to_socket)
  {
    if (!src) {
      return nullptr;
    }
    const CPPType *from_type = from_socket.typeinfo->base_cpp_type;
    const CPPType *to_type = to_socket.typeinfo->base_cpp_type;
    if (from_type == to_type) {
      return src;
    }
    if (!to_type) {
      return nullptr;
    }
    const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
    if (!conversions.is_convertible(*from_type, *to_type)) {
      return nullptr;
    }
    void *dst = scope_.allocate_owned(*to_type);
    conversions.convert_to_uninitialized(*from_type, *to_type, src, dst);
    return dst;
  }

  static bool switch__is_socket_selected(const SocketInContext &socket,
                                         const InferenceValue &condition)
  {
    const bool is_true = condition.get_known<bool>();
    const int selected_index = is_true ? 2 : 1;
    return socket->index() == selected_index;
  }

  static bool index_switch__is_socket_selected(const SocketInContext &socket,
                                               const InferenceValue &condition)
  {
    const int index = condition.get_known<int>();
    return socket->index() == index + 1;
  }

  static bool menu_switch__is_socket_selected(const SocketInContext &socket,
                                              const InferenceValue &condition)
  {
    const NodeMenuSwitch &storage = *static_cast<const NodeMenuSwitch *>(
        socket->owner_node().storage);
    const int menu_value = condition.get_known<int>();
    const NodeEnumItem &item = storage.enum_definition.items_array[socket->index() - 1];
    return menu_value == item.identifier;
  }

  static bool mix_node__is_socket_selected(const SocketInContext &socket,
                                           const InferenceValue &condition)
  {
    const NodeShaderMix &storage = *static_cast<const NodeShaderMix *>(
        socket.owner_node()->storage);
    if (storage.data_type == SOCK_RGBA && storage.blend_type != MA_RAMP_BLEND) {
      return true;
    }

    const bool clamp_factor = storage.clamp_factor != 0;
    bool only_a = false;
    bool only_b = false;
    if (storage.data_type == SOCK_VECTOR && storage.factor_mode == NODE_MIX_MODE_NON_UNIFORM) {
      const float3 mix_factor = condition.get_known<float3>();
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
      const float mix_factor = condition.get_known<float>();
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

  static bool shader_mix_node__is_socket_selected(const SocketInContext &socket,
                                                  const InferenceValue &condition)
  {
    const float mix_factor = condition.get_known<float>();
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

  void push_usage_task(const SocketInContext &socket)
  {
    usage_tasks_.push(socket);
  }

  void push_value_task(const SocketInContext &socket)
  {
    value_tasks_.push(socket);
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
};

static bool input_may_affect_visibility(const bNodeTreeInterfaceSocket &socket)
{
  return socket.socket_type == StringRef("NodeSocketMenu");
}

static bool input_may_affect_visibility(const bNodeSocket &socket)
{
  return socket.type == SOCK_MENU;
}

Array<SocketUsage> infer_all_input_sockets_usage(const bNodeTree &tree)
{
  tree.ensure_topology_cache();
  const Span<const bNodeSocket *> all_input_sockets = tree.all_input_sockets();
  Array<SocketUsage> all_usages(all_input_sockets.size());

  {
    /* Find actual socket usages. */
    SocketUsageInferencer inferencer{tree, std::nullopt};
    inferencer.mark_top_level_node_outputs_as_used();
    for (const int i : all_input_sockets.index_range()) {
      const bNodeSocket &socket = *all_input_sockets[i];
      all_usages[i].is_used = inferencer.is_socket_used({nullptr, &socket});
    }
  }

  /* Find input sockets that should be hidden. */
  Array<bool> only_controllers_used(all_input_sockets.size(), NoInitialization{});
  Array<bool> all_ignored_inputs(all_input_sockets.size(), true);
  threading::parallel_for(all_input_sockets.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const bNodeSocket &socket = *all_input_sockets[i];
      only_controllers_used[i] = !input_may_affect_visibility(socket);
    }
  });
  SocketUsageInferencer inferencer_all_unknown{tree, std::nullopt, all_ignored_inputs};
  SocketUsageInferencer inferencer_only_controllers{tree, std::nullopt, only_controllers_used};
  inferencer_all_unknown.mark_top_level_node_outputs_as_used();
  inferencer_only_controllers.mark_top_level_node_outputs_as_used();
  for (const int i : all_input_sockets.index_range()) {
    if (all_usages[i].is_used) {
      /* Used inputs are always visible. */
      continue;
    }
    const SocketInContext socket{nullptr, all_input_sockets[i]};
    if (inferencer_only_controllers.is_socket_used((socket))) {
      /* The input should be visible if it's used if only visibility-controlling inputs are
       * considered. */
      continue;
    }
    if (!inferencer_all_unknown.is_socket_used(socket)) {
      /* The input should be visible if it's never used, regardless of any inputs. Its usage does
       * not depend on any visibility-controlling input. */
      continue;
    }
    all_usages[i].is_visible = false;
  }

  return all_usages;
}

void infer_group_interface_inputs_usage(const bNodeTree &group,
                                        const Span<GPointer> group_input_values,
                                        const MutableSpan<SocketUsage> r_input_usages)
{

  SocketUsage default_usage;
  default_usage.is_used = false;
  default_usage.is_visible = true;
  r_input_usages.fill(default_usage);

  {
    /* Detect actually used inputs. */
    SocketUsageInferencer inferencer{group, group_input_values};
    for (const bNode *node : group.group_input_nodes()) {
      for (const int i : group.interface_inputs().index_range()) {
        const bNodeSocket &socket = node->output_socket(i);
        r_input_usages[i].is_used |= inferencer.is_socket_used({nullptr, &socket});
      }
    }
  }
  if (std::all_of(r_input_usages.begin(), r_input_usages.end(), [](const SocketUsage &usage) {
        return usage.is_used;
      }))
  {
    /* If all inputs are used, there is no need to infer visibility because all inputs should be
     * visible. */
    return;
  }
  bool visibility_controlling_input_exists = false;
  Array<GPointer, 32> inputs_all_unknown(group_input_values.size());
  Array<GPointer, 32> inputs_only_controllers = group_input_values;
  for (const int i : group.interface_inputs().index_range()) {
    const bNodeTreeInterfaceSocket &io_socket = *group.interface_inputs()[i];
    if (input_may_affect_visibility(io_socket)) {
      visibility_controlling_input_exists = true;
    }
    else {
      inputs_only_controllers[i] = {};
    }
  }
  if (!visibility_controlling_input_exists) {
    /* If there is no visibility controller inputs, all inputs are always visible. */
    return;
  }
  SocketUsageInferencer inferencer_all_unknown{group, inputs_all_unknown};
  SocketUsageInferencer inferencer_only_controllers{group, inputs_only_controllers};
  for (const int i : group.interface_inputs().index_range()) {
    if (r_input_usages[i].is_used) {
      /* Used inputs are always visible. */
      continue;
    }
    if (inferencer_only_controllers.is_group_input_used(i)) {
      /* The input should be visible if it's used if only visibility-controlling inputs are
       * considered. */
      continue;
    }
    if (!inferencer_all_unknown.is_group_input_used(i)) {
      /* The input should be visible if it's never used, regardless of any inputs. Its usage does
       * not depend on any visibility-controlling input. */
      continue;
    }
    r_input_usages[i].is_visible = false;
  }
}

void infer_group_interface_inputs_usage(const bNodeTree &group,
                                        Span<const bNodeSocket *> input_sockets,
                                        MutableSpan<SocketUsage> r_input_usages)
{
  BLI_assert(group.interface_inputs().size() == input_sockets.size());

  AlignedBuffer<1024, 8> allocator_buffer;
  LinearAllocator<> allocator;
  allocator.provide_buffer(allocator_buffer);

  Array<GPointer> input_values(input_sockets.size());
  for (const int i : input_sockets.index_range()) {
    const bNodeSocket &socket = *input_sockets[i];
    if (socket.is_directly_linked()) {
      continue;
    }

    const bke::bNodeSocketType &stype = *socket.typeinfo;
    const CPPType *base_type = stype.base_cpp_type;
    if (base_type == nullptr) {
      continue;
    }
    void *value = allocator.allocate(*base_type);
    stype.get_base_cpp_value(socket.default_value, value);
    input_values[i] = GPointer(base_type, value);
  }

  infer_group_interface_inputs_usage(group, input_values, r_input_usages);

  for (GPointer &value : input_values) {
    if (const void *data = value.get()) {
      value.type()->destruct(const_cast<void *>(data));
    }
  }
}

void infer_group_interface_inputs_usage(const bNodeTree &group,
                                        const PropertiesVectorSet &properties,
                                        MutableSpan<SocketUsage> r_input_usages)
{
  const int inputs_num = group.interface_inputs().size();
  Array<GPointer> input_values(inputs_num);
  ResourceScope scope;
  nodes::get_geometry_nodes_input_base_values(group, properties, scope, input_values);
  nodes::socket_usage_inference::infer_group_interface_inputs_usage(
      group, input_values, r_input_usages);
}

InputSocketUsageParams::InputSocketUsageParams(SocketUsageInferencer &inferencer,
                                               const ComputeContext *compute_context,
                                               const bNodeTree &tree,
                                               const bNode &node,
                                               const bNodeSocket &socket)
    : inferencer_(inferencer),
      compute_context_(compute_context),
      tree(tree),
      node(node),
      socket(socket)
{
}

InferenceValue InputSocketUsageParams::get_input(const StringRef identifier) const
{
  const SocketInContext input_socket{compute_context_,
                                     &this->node.input_by_identifier(identifier)};
  return inferencer_.get_socket_value(input_socket);
}

bool InputSocketUsageParams::menu_input_may_be(const StringRef identifier,
                                               const int enum_value) const
{
  BLI_assert(this->node.input_by_identifier(identifier).type == SOCK_MENU);
  const InferenceValue value = this->get_input(identifier);
  if (value.is_unknown()) {
    /* The value is unknown, so it may be the requested enum value. */
    return true;
  }
  return value.get_known<int>() == enum_value;
}

}  // namespace blender::nodes::socket_usage_inference

/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>
#include <variant>

#include "BKE_compute_context_cache.hh"
#include "BKE_lib_id.hh"
#include "BKE_node_tree_zones.hh"
#include "BKE_type_conversions.hh"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_stack.hh"

#include "NOD_menu_value.hh"
#include "NOD_multi_function.hh"
#include "NOD_node_declaration.hh"
#include "NOD_node_in_compute_context.hh"
#include "NOD_shader_nodes_inline.hh"

namespace blender::nodes {
namespace {

struct BundleSocketValue;
using BundleSocketValuePtr = std::shared_ptr<BundleSocketValue>;

struct FallbackValue {};

/** This indicates that the value should be ignored when it is linked to an input socket. */
struct DanglingValue {};

struct NodeAndSocket {
  bNode *node = nullptr;
  bNodeSocket *socket = nullptr;
};

struct PrimitiveSocketValue {
  std::variant<int, float, bool, ColorGeometry4f, float3, MenuValue> value;

  const void *buffer() const
  {
    return std::visit([](auto &&value) -> const void * { return &value; }, value);
  }

  void *buffer()
  {
    return const_cast<void *>(const_cast<const PrimitiveSocketValue *>(this)->buffer());
  }

  static PrimitiveSocketValue from_value(const GPointer value)
  {
    const CPPType &type = *value.type();
    if (type.is<int>()) {
      return {*static_cast<const int *>(value.get())};
    }
    if (type.is<float>()) {
      return {*static_cast<const float *>(value.get())};
    }
    if (type.is<bool>()) {
      return {*static_cast<const bool *>(value.get())};
    }
    if (type.is<ColorGeometry4f>()) {
      return {*static_cast<const ColorGeometry4f *>(value.get())};
    }
    if (type.is<float3>()) {
      return {*static_cast<const float3 *>(value.get())};
    }
    if (type.is<MenuValue>()) {
      return {*static_cast<const MenuValue *>(value.get())};
    }
    BLI_assert_unreachable();
    return {};
  }
};

/** References an output socket in the generated node tree. */
struct LinkedSocketValue {
  bNode *node = nullptr;
  bNodeSocket *socket = nullptr;
};

/** References an input socket in the source node tree. */
struct InputSocketValue {
  const bNodeSocket *socket = nullptr;
};

struct ClosureZoneValue {
  const bke::bNodeTreeZone *zone = nullptr;
  const ComputeContext *closure_creation_context = nullptr;
};

struct SocketValue {
  /**
   * The value of an arbitrary socket value can have one of many different types. At a high level
   * it can either have a specific constant-folded value, or it references a socket that can't be
   * constant-folded.
   */
  std::variant<FallbackValue,
               DanglingValue,
               LinkedSocketValue,
               InputSocketValue,
               PrimitiveSocketValue,
               ClosureZoneValue,
               BundleSocketValuePtr>
      value;

  /** Try to get the value as a primitive value. */
  std::optional<PrimitiveSocketValue> to_primitive(const bke::bNodeSocketType &type) const
  {
    if (const auto *primitive_value = std::get_if<PrimitiveSocketValue>(&this->value)) {
      return *primitive_value;
    }
    if (const auto *input_socket_value = std::get_if<InputSocketValue>(&this->value)) {
      const bNodeSocket &socket = *input_socket_value->socket;
      BLI_assert(socket.type == type.type);
      if (!socket.runtime->declaration) {
        return std::nullopt;
      }
      if (socket.runtime->declaration->default_input_type != NODE_DEFAULT_INPUT_VALUE) {
        return std::nullopt;
      }
      switch (socket.typeinfo->type) {
        case SOCK_FLOAT:
          return PrimitiveSocketValue{socket.default_value_typed<bNodeSocketValueFloat>()->value};
        case SOCK_INT:
          return PrimitiveSocketValue{socket.default_value_typed<bNodeSocketValueInt>()->value};
        case SOCK_BOOLEAN:
          return PrimitiveSocketValue{
              socket.default_value_typed<bNodeSocketValueBoolean>()->value};
        case SOCK_VECTOR:
          return PrimitiveSocketValue{
              float3(socket.default_value_typed<bNodeSocketValueVector>()->value)};
        case SOCK_RGBA:
          return PrimitiveSocketValue{
              ColorGeometry4f(socket.default_value_typed<bNodeSocketValueRGBA>()->value)};
        case SOCK_MENU:
          return PrimitiveSocketValue{
              MenuValue(socket.default_value_typed<bNodeSocketValueMenu>()->value)};
        default:
          return std::nullopt;
      }
    }
    if (std::get_if<FallbackValue>(&this->value)) {
      switch (type.type) {
        case SOCK_INT:
        case SOCK_BOOLEAN:
        case SOCK_VECTOR:
        case SOCK_RGBA:
        case SOCK_FLOAT:
          return PrimitiveSocketValue::from_value(
              {type.base_cpp_type, type.base_cpp_type->default_value()});
        default:
          return std::nullopt;
      }
    }
    return std::nullopt;
  }
};

struct BundleSocketValue {
  struct Item {
    std::string key;
    SocketValue value;
    const bke::bNodeSocketType *socket_type = nullptr;
  };

  Vector<Item> items;
};

struct PreservedZone {
  bNode *input_node = nullptr;
  bNode *output_node = nullptr;
};

class ShaderNodesInliner {
 private:
  /** Cache for intermediate values used during the inline process. */
  ResourceScope scope_;
  /** The original tree the has to be inlined. */
  const bNodeTree &src_tree_;
  /** The tree where the inlined nodes will be added. */
  bNodeTree &dst_tree_;
  /** Parameters passed in by the caller. */
  InlineShaderNodeTreeParams &params_;
  /** Simplifies building the all the compute contexts for nodes in zones and groups. */
  bke::ComputeContextCache compute_context_cache_;
  /**
   * Stores compute context of the direct parent of each zone. In most cases, this is just the
   * parent compute context directly, except for closures.
   */
  Map<const ComputeContext *, const ComputeContext *> parent_zone_contexts_;
  /** Stores the computed value for each socket. The final value for each socket may be constant */
  Map<SocketInContext, SocketValue> value_by_socket_;
  /**
   * Remember zone nodes that have been copied to the destination so that they can be connected
   * again in the end.
   */
  Map<NodeInContext, PreservedZone> copied_zone_by_zone_output_node_;
  /** Sockets that still have to be evaluated. */
  Stack<SocketInContext> scheduled_sockets_stack_;
  /** Knows how to compute between different data types. */
  const bke::DataTypeConversions &data_type_conversions_;
  /** This is used to generate unique names and ids. */
  int dst_node_counter_ = 0;

 public:
  ShaderNodesInliner(const bNodeTree &src_tree,
                     bNodeTree &dst_tree,
                     InlineShaderNodeTreeParams &params)
      : src_tree_(src_tree),
        dst_tree_(dst_tree),
        params_(params),
        data_type_conversions_(bke::get_implicit_type_conversions())
  {
  }

  bool do_inline()
  {
    src_tree_.ensure_topology_cache();
    if (src_tree_.has_available_link_cycle()) {
      return false;
    }

    const Vector<SocketInContext> final_output_sockets = this->find_final_output_sockets();

    /* Evaluation starts at the final output sockets which will request the evaluation of whether
     * sockets are linked to them. */
    for (const SocketInContext &socket : final_output_sockets) {
      this->schedule_socket(socket);
    }

    /* Evaluate until all scheduled sockets have a value. While evaluating a single socket, it may
     * either end up having a value, or request more other sockets that need to be evaluated first.
     *
     * This uses an explicit stack instead of recursion to avoid stack overflows which can easily
     * happen when there are long chains of nodes (or e.g. repeat zones with many iterations). */
    while (!scheduled_sockets_stack_.is_empty()) {
      const SocketInContext socket = scheduled_sockets_stack_.peek();
      const int old_stack_size = scheduled_sockets_stack_.size();

      this->handle_socket(socket);

      if (scheduled_sockets_stack_.size() == old_stack_size) {
        /* No additional dependencies were pushed, so this socket is fully handled and can be
         * popped from the stack. */
        BLI_assert(socket == scheduled_sockets_stack_.peek());
        scheduled_sockets_stack_.pop();
      }
    }

    /* Create actual output nodes. */
    Map<NodeInContext, bNode *> final_output_nodes;
    for (const SocketInContext &socket : final_output_sockets) {
      const NodeInContext src_node = socket.owner_node();
      bNode *copied_node = final_output_nodes.lookup_or_add_cb(src_node, [&]() {
        Map<const bNodeSocket *, bNodeSocket *> socket_map;
        bNode *copied_node = bke::node_copy_with_mapping(&dst_tree_,
                                                         *src_node.node,
                                                         this->node_copy_flag(),
                                                         std::nullopt,
                                                         this->get_next_node_identifier(),
                                                         socket_map);
        copied_node->parent = nullptr;
        return copied_node;
      });
      bNodeSocket *copied_socket = static_cast<bNodeSocket *>(
          BLI_findlink(&copied_node->inputs, socket.socket->index()));
      this->set_input_socket_value(
          *src_node, *copied_node, *copied_socket, value_by_socket_.lookup(socket));
    }

    this->restore_zones_in_output_tree();
    this->position_nodes_in_output_tree();
    return true;
  }

  Vector<SocketInContext> find_final_output_sockets()
  {
    Vector<TreeInContext> trees;
    this->find_trees_potentially_containing_shader_outputs_recursive(nullptr, src_tree_, trees);

    Vector<SocketInContext> output_sockets;
    auto add_output_type = [&](const char *output_type) {
      for (const TreeInContext &tree : trees) {
        const bke::bNodeTreeZones &zones = *tree->zones();
        for (const bNode *node : tree->nodes_by_type(output_type)) {
          const bke::bNodeTreeZone *zone = zones.get_zone_by_node(node->identifier);
          if (zone) {
            params_.r_error_messages.append({node, TIP_("Output node must not be in zone")});
            continue;
          }
          for (const bNodeSocket *socket : node->input_sockets()) {
            output_sockets.append({tree.context, socket});
          }
        }
      }
    };

    /* owner_id can be null for DefaultSurfaceNodeTree. */
    ID_Type tree_type = src_tree_.owner_id ? GS(src_tree_.owner_id->name) : ID_MA;

    switch (tree_type) {
      case ID_MA:
        add_output_type("ShaderNodeOutputMaterial");
        add_output_type("ShaderNodeOutputAOV");
        add_output_type("ShaderNodeOutputLight");
        break;
      case ID_WO:
        add_output_type("ShaderNodeOutputWorld");
        add_output_type("ShaderNodeOutputAOV");
        break;
      case ID_LA:
        add_output_type("ShaderNodeOutputLight");
        break;
      default:
        BLI_assert_unreachable();
    }

    return output_sockets;
  }

  void find_trees_potentially_containing_shader_outputs_recursive(const ComputeContext *context,
                                                                  const bNodeTree &tree,
                                                                  Vector<TreeInContext> &r_trees)
  {
    const bke::bNodeTreeZones *zones = src_tree_.zones();
    if (!zones) {
      return;
    }
    if (tree.has_available_link_cycle()) {
      return;
    }
    r_trees.append({context, &tree});
    for (const bNode *group_node : tree.group_nodes()) {
      if (group_node->is_muted()) {
        continue;
      }
      const bNodeTree *group = id_cast<const bNodeTree *>(group_node->id);
      if (!group || ID_MISSING(&group->id)) {
        continue;
      }
      group->ensure_topology_cache();
      const bke::bNodeTreeZone *zone = zones->get_zone_by_node(group_node->identifier);
      if (zone) {
        /* Node groups in zones are ignored. */
        continue;
      }
      const ComputeContext &group_context = compute_context_cache_.for_group_node(
          context, group_node->identifier, &tree);
      this->find_trees_potentially_containing_shader_outputs_recursive(
          &group_context, *group, r_trees);
    }
  }

  void handle_socket(const SocketInContext &socket)
  {
    if (!socket->is_available()) {
      return;
    }
    if (value_by_socket_.contains(socket)) {
      /* The socket already has a value, so there is nothing to do. */
      return;
    }
    if (socket->is_input()) {
      this->handle_input_socket(socket);
    }
    else {
      this->handle_output_socket(socket);
    }
  }

  void handle_input_socket(const SocketInContext &socket)
  {
    /* Multi-inputs are not supported in shader nodes currently. */
    BLI_assert(!socket->is_multi_input());

    const bNodeLink *used_link = nullptr;
    for (const bNodeLink *link : socket->directly_linked_links()) {
      if (!link->is_used()) {
        continue;
      }
      used_link = link;
    }
    if (!used_link) {
      /* If there is no link on the input, use the value of the socket directly. */
      this->store_socket_value(socket, {InputSocketValue{socket.socket}});
      return;
    }

    const ComputeContext *from_context = this->get_link_source_context(*used_link, socket);
    const SocketInContext origin_socket = {from_context, used_link->fromsock};
    if (const auto *value = value_by_socket_.lookup_ptr(origin_socket)) {
      if (std::holds_alternative<DanglingValue>(value->value)) {
        if (this->input_socket_may_have_dangling_value(socket)) {
          this->store_socket_value(socket, {DanglingValue{}});
        }
        else {
          /* If the input value is dangling, use the value of the socket itself. */
          this->store_socket_value(socket, {InputSocketValue{socket.socket}});
        }
        return;
      }
      /* If the socket linked to the input has a value already, copy that value to the current
       * socket, potentially with an implicit conversion. */
      this->store_socket_value(socket,
                               this->handle_implicit_conversion(*value,
                                                                *used_link->fromsock->typeinfo,
                                                                *used_link->tosock->typeinfo));
      return;
    }
    /* If the origin socket does not have a value yet, only schedule it for evaluation for now.*/
    this->schedule_socket(origin_socket);
  }

  /**
   * Generally, input values of a node should never be dangling because otherwise the node can't be
   * evaluated. However, if a node is never evaluated anyway, then its inputs can be dangling. This
   * allows the dangling-state to be properly forwarded through the node.
   */
  bool input_socket_may_have_dangling_value(const SocketInContext &socket)
  {
    BLI_assert(socket->is_input());
    const NodeInContext node = socket.owner_node();
    return node->is_reroute() || node->is_muted();
  }

  const ComputeContext *get_link_source_context(const bNodeLink &link,
                                                const SocketInContext &to_socket)
  {
    const bNodeTree &tree = to_socket->owner_tree();
    const bke::bNodeTreeZones *zones = tree.zones();
    if (!zones) {
      return nullptr;
    }
    const bke::bNodeTreeZone *to_zone = zones->get_zone_by_socket(*to_socket);
    const bke::bNodeTreeZone *from_zone = zones->get_zone_by_socket(*link.fromsock);
    const ComputeContext *context = to_socket.context;
    for (const bke::bNodeTreeZone *zone = to_zone; zone != from_zone; zone = zone->parent_zone) {
      const bNode &zone_output_node = *zone->output_node();
      if (zone_output_node.is_type("GeometryNodeRepeatOutput")) {
        if (this->should_preserve_repeat_zone_node(zone_output_node)) {
          /* Preserved repeat zones are embedded into their outer compute context. */
          continue;
        }
      }
      context = parent_zone_contexts_.lookup(context);
    }
    return context;
  }

  void handle_output_socket(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    if (node->is_reroute()) {
      this->handle_output_socket__reroute(socket);
      return;
    }
    if (node->is_muted()) {
      if (!this->handle_output_socket__internal_links(socket)) {
        /* The output socket does not have a corresponding input, so the value is ignored. */
        this->store_socket_value_dangling(socket);
      }
      return;
    }
    if (node->is_group()) {
      this->handle_output_socket__group(socket);
      return;
    }
    if (node->is_group_input()) {
      this->handle_output_socket__group_input(socket);
      return;
    }
    if (node->is_type("GeometryNodeRepeatOutput")) {
      if (this->should_preserve_repeat_zone_node(*node)) {
        this->handle_output_socket__preserved_repeat_output(socket);
        return;
      }
      this->handle_output_socket__repeat_output(socket);
      return;
    }
    if (node->is_type("GeometryNodeRepeatInput")) {
      if (this->should_preserve_repeat_zone_node(*node)) {
        this->handle_output_socket__preserved_repeat_input(socket);
        return;
      }
      this->handle_output_socket__repeat_input(socket);
      return;
    }
    if (node->is_type("NodeClosureOutput")) {
      this->handle_output_socket__closure_output(socket);
      return;
    }
    if (node->is_type("NodeClosureInput")) {
      this->handle_output_socket__closure_input(socket);
      return;
    }
    if (node->is_type("NodeEvaluateClosure")) {
      this->handle_output_socket__evaluate_closure(socket);
      return;
    }
    if (node->is_type("NodeCombineBundle")) {
      this->handle_output_socket__combine_bundle(socket);
      return;
    }
    if (node->is_type("NodeSeparateBundle")) {
      this->handle_output_socket__separate_bundle(socket);
      return;
    }
    if (node->is_type("GeometryNodeMenuSwitch")) {
      this->handle_output_socket__menu_switch(socket);
      return;
    }
    this->handle_output_socket__eval(socket);
  }

  void handle_output_socket__reroute(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    if (node->is_dangling_reroute()) {
      this->store_socket_value_dangling(socket);
      return;
    }

    const SocketInContext input_socket = {socket.context, &node->input_socket(0)};
    this->forward_value_or_schedule(socket, input_socket);
  }

  /* Returns whether the socket was handled. */
  [[nodiscard]] bool handle_output_socket__internal_links(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    for (const bNodeLink &internal_link : node->internal_links()) {
      if (internal_link.tosock == socket.socket) {
        const SocketInContext src_socket = {socket.context, internal_link.fromsock};
        if (const SocketValue *value = value_by_socket_.lookup_ptr(src_socket)) {
          /* Pass the value of the internally linked input socket, with an implicit conversion if
           * necessary. */
          this->store_socket_value(
              socket,
              this->handle_implicit_conversion(
                  *value, *internal_link.fromsock->typeinfo, *internal_link.tosock->typeinfo));
          return true;
        }
        this->schedule_socket(src_socket);
        return true;
      }
    }
    return false;
  }

  void handle_output_socket__group(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node->id);
    if (!group || ID_MISSING(&group->id)) {
      this->store_socket_value_fallback(socket);
      return;
    }
    group->ensure_interface_cache();
    group->ensure_topology_cache();
    const bNode *group_output_node = group->group_output_node();
    if (!group_output_node) {
      this->store_socket_value_fallback(socket);
      return;
    }
    /* Get the value of an output of a group node by evaluating the corresponding output of the
     * node group. Since this socket is in a different tree, the compute context is different. */
    const ComputeContext &group_compute_context = compute_context_cache_.for_group_node(
        socket.context, node->identifier, &node->owner_tree());
    const SocketInContext group_output_socket_ctx = {
        &group_compute_context, &group_output_node->input_socket(socket->index())};
    this->forward_value_or_schedule(socket, group_output_socket_ctx);
  }

  void handle_output_socket__group_input(const SocketInContext &socket)
  {
    if (const auto *group_node_compute_context =
            dynamic_cast<const bke::GroupNodeComputeContext *>(socket.context))
    {
      /* Get the value of a group input from the corresponding input socket of the parent group
       * node. */
      const ComputeContext *parent_compute_context = group_node_compute_context->parent();
      const bNode *group_node = group_node_compute_context->node();
      BLI_assert(group_node);
      const bNodeSocket &group_node_input = group_node->input_socket(socket->index());
      const SocketInContext group_input_socket_ctx = {parent_compute_context, &group_node_input};
      this->forward_value_or_schedule(socket, group_input_socket_ctx);
      return;
    }
    this->store_socket_value_fallback(socket);
  }

  bool should_preserve_repeat_zone_node(const bNode &repeat_zone_node) const
  {
    BLI_assert(repeat_zone_node.is_type("GeometryNodeRepeatOutput") ||
               repeat_zone_node.is_type("GeometryNodeRepeatInput"));
    if (!params_.allow_preserving_repeat_zones) {
      return false;
    }
    const bNodeTree &tree = repeat_zone_node.owner_tree();
    const bke::bNodeTreeZones *zones = tree.zones();
    if (!zones) {
      return false;
    }
    const bke::bNodeTreeZone *zone = zones->get_zone_by_node(repeat_zone_node.identifier);
    if (!zone) {
      return false;
    }
    const bNode *repeat_zone_input_node = zone->input_node();
    const bNode *repeat_zone_output_node = zone->output_node();
    if (!repeat_zone_input_node || !repeat_zone_output_node) {
      return false;
    }
    const auto &storage = *static_cast<const NodeGeometryRepeatOutput *>(
        repeat_zone_output_node->storage);
    for (const int i : IndexRange(storage.items_num)) {
      const NodeRepeatItem &item = storage.items[i];
      if (!ELEM(item.socket_type, SOCK_INT, SOCK_FLOAT, SOCK_BOOLEAN, SOCK_RGBA, SOCK_VECTOR)) {
        /* Repeat zones with more special types have to be inlined. */
        return false;
      }
    }
    return true;
  }

  void handle_output_socket__repeat_output(const SocketInContext &socket)
  {
    const bNode &repeat_output_node = socket->owner_node();
    const bNodeTree &tree = socket->owner_tree();

    const bke::bNodeTreeZones *zones = tree.zones();
    if (!zones) {
      this->store_socket_value_fallback(socket);
      return;
    }
    const bke::bNodeTreeZone *zone = zones->get_zone_by_node(repeat_output_node.identifier);
    if (!zone) {
      this->store_socket_value_fallback(socket);
      return;
    }
    const NodeInContext repeat_input_node = {socket.context, zone->input_node()};
    const SocketInContext iterations_input = repeat_input_node.input_socket(0);
    const SocketValue *iterations_socket_value = value_by_socket_.lookup_ptr(iterations_input);
    if (!iterations_socket_value) {
      /* The number of iterations is not known yet, so only schedule that socket for now. */
      this->schedule_socket(iterations_input);
      return;
    }
    const std::optional<PrimitiveSocketValue> iterations_value_opt =
        iterations_socket_value->to_primitive(*iterations_input->typeinfo);
    if (!iterations_value_opt) {
      this->add_dynamic_repeat_zone_iterations_error(*repeat_input_node);
    }
    const int iterations = iterations_value_opt.has_value() ?
                               std::get<int>(iterations_value_opt->value) :
                               0;
    if (iterations <= 0) {
      /* If the number of iterations is zero, the values are copied directly from the repeat input
       * node. */
      const SocketInContext origin_socket = repeat_input_node.input_socket(1 + socket->index());
      this->forward_value_or_schedule(socket, origin_socket);
      return;
    }
    /* Otherwise, the value is copied from the output of the last iteration. */
    const ComputeContext &last_iteration_context = compute_context_cache_.for_repeat_zone(
        socket.context, repeat_output_node, iterations - 1);
    parent_zone_contexts_.add(&last_iteration_context, socket.context);
    const SocketInContext origin_socket = {&last_iteration_context,
                                           &repeat_output_node.input_socket(socket->index())};
    this->forward_value_or_schedule(socket, origin_socket);
  }

  void handle_output_socket__preserved_repeat_output(const SocketInContext &socket)
  {
    const bNodeTree &tree = socket->owner_tree();
    const NodeInContext repeat_output_node = socket.owner_node();
    const bke::bNodeTreeZones &zones = *tree.zones();
    const bke::bNodeTreeZone &zone = *zones.get_zone_by_node(repeat_output_node->identifier);
    const bNode &repeat_input_node = *zone.input_node();

    const EnsureInputsResult ensured_inputs = this->ensure_node_inputs(socket.owner_node());
    if (ensured_inputs.has_missing_inputs) {
      /* The node can only be evaluated if all inputs values are known. */
      return;
    }
    const NodeInContext node = socket.owner_node();
    bNode &copied_node = this->handle_output_socket__eval_copy_node(node);
    PreservedZone &preserved_zone = copied_zone_by_zone_output_node_.lookup_or_add_default(
        repeat_output_node);
    preserved_zone.output_node = &copied_node;
    /* Ensure that the repeat input node is created as well. */
    this->schedule_socket({node.context, &repeat_input_node.output_socket(0)});
  }

  void handle_output_socket__preserved_repeat_input(const SocketInContext &socket)
  {
    const EnsureInputsResult ensured_inputs = this->ensure_node_inputs(socket.owner_node());
    if (ensured_inputs.has_missing_inputs) {
      /* The node can only be evaluated if all inputs values are known. */
      return;
    }
    const bNodeTree &tree = socket->owner_tree();
    const NodeInContext node = socket.owner_node();
    bNode &copied_node = this->handle_output_socket__eval_copy_node(node);
    const auto &storage = *static_cast<const NodeGeometryRepeatInput *>(node->storage);
    const NodeInContext repeat_output_node{node.context, tree.node_by_id(storage.output_node_id)};
    PreservedZone &preserved_zone = copied_zone_by_zone_output_node_.lookup_or_add_default(
        repeat_output_node);
    preserved_zone.input_node = &copied_node;
  }

  void add_dynamic_repeat_zone_iterations_error(const bNode &repeat_input_node)
  {
    params_.r_error_messages.append(
        {&repeat_input_node, TIP_("Iterations input has to be a constant value")});
  }

  void handle_output_socket__repeat_input(const SocketInContext &socket)
  {
    const bNode &repeat_input_node = socket->owner_node();
    const auto *repeat_zone_context = dynamic_cast<const bke::RepeatZoneComputeContext *>(
        socket.context);
    if (!repeat_zone_context) {
      this->store_socket_value_fallback(socket);
      return;
    }
    /* The index of the current iteration comes from the context. */
    const int iteration = repeat_zone_context->iteration();

    if (socket->index() == 0) {
      /* The first output is the current iteration index. */
      this->store_socket_value(socket, {PrimitiveSocketValue{iteration}});
      return;
    }

    if (iteration == 0) {
      /* In the first iteration, the values are copied from the corresponding input socket. */
      const SocketInContext origin_socket = {repeat_zone_context->parent(),
                                             &repeat_input_node.input_socket(socket->index())};
      this->forward_value_or_schedule(socket, origin_socket);
      return;
    }
    /* For later iterations, the values are copied from the corresponding output of the previous
     * iteration. */
    const bNode &repeat_output_node = *repeat_input_node.owner_tree().node_by_id(
        repeat_zone_context->output_node_id());
    const int previous_iteration = iteration - 1;
    const ComputeContext &previous_iteration_context = compute_context_cache_.for_repeat_zone(
        repeat_zone_context->parent(), repeat_output_node, previous_iteration);
    parent_zone_contexts_.add(&previous_iteration_context, repeat_zone_context->parent());
    const SocketInContext origin_socket = {&previous_iteration_context,
                                           &repeat_output_node.input_socket(socket->index() - 1)};
    this->forward_value_or_schedule(socket, origin_socket);
  }

  void handle_output_socket__closure_output(const SocketInContext &socket)
  {
    const bNode &node = socket->owner_node();
    const bke::bNodeTreeZones *zones = node.owner_tree().zones();
    if (!zones) {
      this->store_socket_value_fallback(socket);
      return;
    }
    const bke::bNodeTreeZone *zone = zones->get_zone_by_node(node.identifier);
    if (!zone) {
      this->store_socket_value_fallback(socket);
      return;
    }
    /* Just store a reference to the closure. */
    this->store_socket_value(socket, {ClosureZoneValue{zone, socket.context}});
  }

  void handle_output_socket__evaluate_closure(const SocketInContext &socket)
  {
    const NodeInContext evaluate_closure_node = socket.owner_node();
    const SocketInContext closure_input_socket = evaluate_closure_node.input_socket(0);
    const SocketValue *closure_input_value = value_by_socket_.lookup_ptr(closure_input_socket);
    if (!closure_input_value) {
      /* The closure to evaluate is not known yet, so schedule the closure input before it can be
       * evaluated. */
      this->schedule_socket(closure_input_socket);
      return;
    }
    const ClosureZoneValue *closure_zone_value = std::get_if<ClosureZoneValue>(
        &closure_input_value->value);
    if (!closure_zone_value) {
      /* If the closure is null, the node behaves as if it is muted. */
      if (!this->handle_output_socket__internal_links(socket)) {
        this->store_socket_value_fallback(socket);
      }
      return;
    }
    const auto *evaluate_closure_storage = static_cast<const NodeEvaluateClosure *>(
        evaluate_closure_node->storage);
    const bNode &closure_output_node = *closure_zone_value->zone->output_node();
    const auto &closure_storage = *static_cast<const NodeClosureOutput *>(
        closure_output_node.storage);
    const StringRef key = evaluate_closure_storage->output_items.items[socket->index()].name;

    const ClosureSourceLocation closure_source_location{
        &closure_output_node.owner_tree(),
        closure_output_node.identifier,
        closure_zone_value->closure_creation_context ?
            closure_zone_value->closure_creation_context->hash() :
            ComputeContextHash{},
        closure_zone_value->closure_creation_context};
    const bke::EvaluateClosureComputeContext &closure_eval_context =
        compute_context_cache_.for_evaluate_closure(socket.context,
                                                    evaluate_closure_node->identifier,
                                                    &socket->owner_tree(),
                                                    closure_source_location);
    parent_zone_contexts_.add(&closure_eval_context, closure_zone_value->closure_creation_context);

    if (closure_eval_context.is_recursive()) {
      this->store_socket_value_fallback(socket);
      params_.r_error_messages.append(
          {&*evaluate_closure_node, TIP_("Recursive closures are not supported")});
      return;
    }

    for (const int i : IndexRange(closure_storage.output_items.items_num)) {
      const NodeClosureOutputItem &item = closure_storage.output_items.items[i];
      if (key != item.name) {
        continue;
      }
      /* Get the value of the output by evaluating the corresponding output in the closure zone. */
      const SocketInContext origin_socket = {&closure_eval_context,
                                             &closure_output_node.input_socket(i)};
      this->forward_value_or_schedule(socket, origin_socket);
      return;
    }
    this->store_socket_value_fallback(socket);
  }

  void handle_output_socket__closure_input(const SocketInContext &socket)
  {
    const bNode &closure_input_node = socket->owner_node();
    const auto *closure_eval_context = dynamic_cast<const bke::EvaluateClosureComputeContext *>(
        socket.context);
    if (!closure_eval_context) {
      this->store_socket_value_fallback(socket);
      return;
    }
    const bNode &closure_output_node = *closure_input_node.owner_tree().node_by_id(
        closure_eval_context->closure_source_location()->closure_output_node_id);
    const NodeInContext closure_eval_node = {closure_eval_context->parent(),
                                             closure_eval_context->node()};

    const auto &closure_storage = *static_cast<const NodeClosureOutput *>(
        closure_output_node.storage);
    const auto &eval_closure_storage = *static_cast<const NodeEvaluateClosure *>(
        closure_eval_node->storage);

    const StringRef key = closure_storage.input_items.items[socket->index()].name;
    for (const int i : IndexRange(eval_closure_storage.input_items.items_num)) {
      const NodeEvaluateClosureInputItem &item = eval_closure_storage.input_items.items[i];
      if (key != item.name) {
        continue;
      }
      /* The input of a closure zone gets its value from the corresponding input of the Evaluate
       * Closure node that evaluates it. */
      const SocketInContext origin_socket = closure_eval_node.input_socket(i + 1);
      this->forward_value_or_schedule(socket, origin_socket);
      return;
    }
    this->store_socket_value_fallback(socket);
  }

  void handle_output_socket__combine_bundle(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const auto &storage = *static_cast<const NodeCombineBundle *>(node->storage);

    bool all_inputs_available = true;
    for (const bNodeSocket *input_socket : node->input_sockets()) {
      const SocketInContext input_socket_ctx = {socket.context, input_socket};
      if (!value_by_socket_.lookup_ptr(input_socket_ctx)) {
        this->schedule_socket(input_socket_ctx);
        all_inputs_available = false;
      }
    }
    if (!all_inputs_available) {
      /* Can't create the bundle yet. Wait until all inputs are available. */
      return;
    }
    /* Build the actual bundle socket value from the input values. */
    auto bundle_value = std::make_shared<BundleSocketValue>();
    for (const int i : IndexRange(storage.items_num)) {
      const SocketInContext input_socket = node.input_socket(i);
      const NodeCombineBundleItem &item = storage.items[i];
      const StringRef key = item.name;
      const auto &socket_value = value_by_socket_.lookup(input_socket);
      bundle_value->items.append({key, socket_value, input_socket->typeinfo});
    }
    this->store_socket_value(socket, {bundle_value});
  }

  void handle_output_socket__separate_bundle(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const auto &storage = *static_cast<const NodeSeparateBundle *>(node->storage);

    const SocketInContext input_socket = node.input_socket(0);
    const SocketValue *socket_value = value_by_socket_.lookup_ptr(input_socket);
    if (!socket_value) {
      /* The input bundle is not known yet, so schedule it for now. */
      this->schedule_socket(input_socket);
      return;
    }
    const auto *bundle_value_ptr = std::get_if<BundleSocketValuePtr>(&socket_value->value);
    if (!bundle_value_ptr) {
      /* The bundle is empty. Use the fallback value. */
      this->store_socket_value_fallback(socket);
      return;
    }
    const BundleSocketValue &bundle_value = **bundle_value_ptr;

    const StringRef key = storage.items[socket->index()].name;
    for (const BundleSocketValue::Item &item : bundle_value.items) {
      if (key != item.key) {
        continue;
      }
      /* Extract the value from the bundle.*/
      const SocketValue converted_value = this->handle_implicit_conversion(
          item.value, *item.socket_type, *socket->typeinfo);
      this->store_socket_value(socket, converted_value);
      return;
    }
    /* The bundle does not contain the requested key, so use the fallback value. */
    this->store_socket_value_fallback(socket);
  }

  void handle_output_socket__menu_switch(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const auto &storage = *static_cast<const NodeMenuSwitch *>(node->storage);

    const SocketInContext menu_input = node.input_socket(0);
    const SocketValue *menu_socket_value = value_by_socket_.lookup_ptr(menu_input);
    if (!menu_socket_value) {
      /* The menu value is not known yet, so schedule it for now. */
      this->schedule_socket(menu_input);
      return;
    }

    const std::optional<PrimitiveSocketValue> menu_value_opt = menu_socket_value->to_primitive(
        *menu_input->typeinfo);
    if (!menu_value_opt) {
      /* This limitation may be lifted in the future. Menu Switch nodes could be supported natively
       * by render engines or we convert them to a bunch of mix nodes. */
      this->store_socket_value_fallback(socket);
      params_.r_error_messages.append({node.node, TIP_("Menu value has to be a constant value")});
      return;
    }
    const MenuValue menu_value = std::get<MenuValue>(menu_value_opt->value);
    /* Find the selected item index. */
    std::optional<int> selected_index;
    for (const int item_i : IndexRange(storage.enum_definition.items_num)) {
      const NodeEnumItem &item = storage.enum_definition.items_array[item_i];
      if (MenuValue(item.identifier) == menu_value) {
        selected_index = item_i;
        break;
      }
    }
    if (!selected_index.has_value()) {
      /* The input value does not exist in the menu. */
      this->store_socket_value_fallback(socket);
      return;
    }
    if (socket->index() == 0) {
      /* Handle forwarding the selected value. */
      this->forward_value_or_schedule(socket, node.input_socket(*selected_index + 1));
      return;
    }
    /* Set the value of the mask output. */
    const bool is_selected = selected_index == socket->index() - 1;
    this->store_socket_value(socket, {PrimitiveSocketValue{is_selected}});
  }

  /**
   * Evaluate a node to compute the value of the given output socket. This may also compute all the
   * other outputs of the node.
   */
  void handle_output_socket__eval(const SocketInContext &socket)
  {
    const NodeInContext node = socket.owner_node();
    const EnsureInputsResult ensured_inputs = this->ensure_node_inputs(node);
    if (ensured_inputs.has_missing_inputs) {
      /* The node can only be evaluated if all inputs values are known. */
      return;
    }
    const bke::bNodeType &node_type = *node->typeinfo;
    if (node_type.build_multi_function && ensured_inputs.all_inputs_primitive) {
      /* Do constant folding. */
      this->handle_output_socket__eval_multi_function(node);
      return;
    }
    /* The node can't be constant-folded. So copy it to the destination tree instead. */
    this->handle_output_socket__eval_copy_node(node);
  }

  struct EnsureInputsResult {
    bool has_missing_inputs = false;
    bool all_inputs_primitive = false;
  };

  EnsureInputsResult ensure_node_inputs(const NodeInContext &node)
  {
    EnsureInputsResult result;
    result.has_missing_inputs = false;
    result.all_inputs_primitive = true;
    for (const bNodeSocket *input_socket : node->input_sockets()) {
      if (!input_socket->is_available()) {
        continue;
      }
      const SocketInContext input_socket_ctx = {node.context, input_socket};
      const SocketValue *value = value_by_socket_.lookup_ptr(input_socket_ctx);
      if (!value) {
        this->schedule_socket(input_socket_ctx);
        result.has_missing_inputs = true;
        continue;
      }
      if (!value->to_primitive(*input_socket->typeinfo)) {
        result.all_inputs_primitive = false;
      }
    }
    return result;
  }

  void handle_output_socket__eval_multi_function(const NodeInContext &node)
  {
    NodeMultiFunctionBuilder builder{*node.node, node->owner_tree()};
    node->typeinfo->build_multi_function(builder);
    const mf::MultiFunction &fn = builder.function();
    mf::ContextBuilder context;
    IndexMask mask(1);
    mf::ParamsBuilder params{fn, &mask};

    /* Prepare inputs to the multi-function evaluation. */
    for (const bNodeSocket *input_socket : node->input_sockets()) {
      if (!input_socket->is_available()) {
        continue;
      }
      const SocketInContext input_socket_ctx = {node.context, input_socket};
      const PrimitiveSocketValue value =
          *value_by_socket_.lookup(input_socket_ctx).to_primitive(*input_socket->typeinfo);
      params.add_readonly_single_input(
          GVArray::from_single(*input_socket->typeinfo->base_cpp_type, 1, value.buffer()));
    }

    /* Prepare output buffers. */
    Vector<void *> output_values;
    for (const bNodeSocket *output_socket : node->output_sockets()) {
      if (!output_socket->is_available()) {
        continue;
      }
      void *value = scope_.allocate_owned(*output_socket->typeinfo->base_cpp_type);
      output_values.append(value);
      params.add_uninitialized_single_output(
          GMutableSpan(output_socket->typeinfo->base_cpp_type, value, 1));
    }

    fn.call(mask, params, context);

    /* Store constant-folded values for the output sockets. */
    int current_output_i = 0;
    for (const bNodeSocket *output_socket : node->output_sockets()) {
      if (!output_socket->is_available()) {
        continue;
      }
      const void *value = output_values[current_output_i++];
      this->store_socket_value(
          {node.context, output_socket},
          {PrimitiveSocketValue::from_value({output_socket->typeinfo->base_cpp_type, value})});
    }
  }

  bNode &handle_output_socket__eval_copy_node(const NodeInContext &node)
  {
    Map<const bNodeSocket *, bNodeSocket *> socket_map;
    /* We generate our own identifier and name here to get unique values without having to scan all
     * already existing nodes. */
    const int identifier = this->get_next_node_identifier();
    const std::string unique_name = fmt::format("{}_{}", identifier, node.node->name);
    bNode &copied_node = *bke::node_copy_with_mapping(
        &dst_tree_,
        *node.node,
        this->node_copy_flag(),
        unique_name.size() < sizeof(bNode::name) ? std::make_optional<StringRefNull>(unique_name) :
                                                   std::nullopt,
        identifier,
        socket_map);

    /* Clear the parent frame pointer, because it does not exist in the destination tree. */
    copied_node.parent = nullptr;

    /* Setup input sockets for the copied node. */
    for (const bNodeSocket *src_input_socket : node->input_sockets()) {
      if (!src_input_socket->is_available()) {
        continue;
      }
      bNodeSocket &dst_input_socket = *socket_map.lookup(src_input_socket);
      const SocketInContext input_socket_ctx = {node.context, src_input_socket};
      const SocketValue &value = value_by_socket_.lookup(input_socket_ctx);
      this->set_input_socket_value(*node, copied_node, dst_input_socket, value);
    }
    for (const bNodeSocket *src_output_socket : node->output_sockets()) {
      if (!src_output_socket->is_available()) {
        continue;
      }
      bNodeSocket &dst_output_socket = *socket_map.lookup(src_output_socket);
      const SocketInContext output_socket_ctx = {node.context, src_output_socket};
      this->store_socket_value(output_socket_ctx,
                               {LinkedSocketValue{&copied_node, &dst_output_socket}});
    }
    return copied_node;
  }

  /** Converts the given socket value if necessary. */
  SocketValue handle_implicit_conversion(const SocketValue &src_value,
                                         const bke::bNodeSocketType &from_socket_type,
                                         const bke::bNodeSocketType &to_socket_type)
  {
    if (from_socket_type.type == to_socket_type.type) {
      return src_value;
    }
    if (std::get_if<LinkedSocketValue>(&src_value.value)) {
      return src_value;
    }
    const std::optional<PrimitiveSocketValue> src_primitive_value = src_value.to_primitive(
        from_socket_type);
    if (src_primitive_value && to_socket_type.base_cpp_type) {
      if (data_type_conversions_.is_convertible(*from_socket_type.base_cpp_type,
                                                *to_socket_type.base_cpp_type))
      {
        const void *src_buffer = src_primitive_value->buffer();
        BUFFER_FOR_CPP_TYPE_VALUE(*to_socket_type.base_cpp_type, dst_buffer);
        data_type_conversions_.convert_to_uninitialized(*from_socket_type.base_cpp_type,
                                                        *to_socket_type.base_cpp_type,
                                                        src_buffer,
                                                        dst_buffer);
        return {
            PrimitiveSocketValue::from_value(GPointer{to_socket_type.base_cpp_type, dst_buffer})};
      }
    }
    if (src_primitive_value && to_socket_type.type == SOCK_SHADER) {
      /* Insert a Color node when converting a primitive value to a shader. */
      bNode *color_node = this->add_node("ShaderNodeRGB");
      const void *src_buffer = src_primitive_value->buffer();
      ColorGeometry4f color;
      data_type_conversions_.convert_to_uninitialized(
          *from_socket_type.base_cpp_type, CPPType::get<ColorGeometry4f>(), src_buffer, &color);
      bNodeSocket *output_socket = static_cast<bNodeSocket *>(color_node->outputs.first);
      auto *socket_storage = static_cast<bNodeSocketValueRGBA *>(output_socket->default_value);
      copy_v3_v3(socket_storage->value, color);
      socket_storage->value[3] = 1.0f;
      return {LinkedSocketValue{color_node, output_socket}};
    }

    return SocketValue{FallbackValue{}};
  }

  void set_input_socket_value(const bNode &original_node,
                              bNode &dst_node,
                              bNodeSocket &dst_socket,
                              const SocketValue &value)
  {
    BLI_assert(dst_socket.is_input());
    if (dst_socket.flag & SOCK_HIDE_VALUE) {
      if (const auto *input_socket_value = std::get_if<InputSocketValue>(&value.value)) {
        if (input_socket_value->socket->flag & SOCK_HIDE_VALUE) {
          /* Don't add a value or link of the source and destination sockets don't have a value. */
          return;
        }
      }
    }
    if (const std::optional<PrimitiveSocketValue> primitive_value = value.to_primitive(
            *dst_socket.typeinfo))
    {
      if (dst_socket.flag & SOCK_HIDE_VALUE) {
        /* Can't store the primitive value directly on the socket. So create a new input node and
         * link it instead. */
        const NodeAndSocket node_and_socket = this->primitive_value_to_output_socket(
            *primitive_value);
        if (dst_tree_.typeinfo->validate_link(node_and_socket.socket->typeinfo->type,
                                              dst_socket.typeinfo->type))
        {
          bke::node_add_link(
              dst_tree_, *node_and_socket.node, *node_and_socket.socket, dst_node, dst_socket);
        }
      }
      else {
        this->set_primitive_value_on_socket(dst_socket, *primitive_value);
      }
      return;
    }
    if (!params_.allow_preserving_repeat_zones) {
      const bool is_iterations_input = dst_node.inputs.first == &dst_socket &&
                                       dst_node.is_type("GeometryNodeRepeatInput");
      if (is_iterations_input) {
        this->add_dynamic_repeat_zone_iterations_error(original_node);
        this->set_primitive_value_on_socket(dst_socket, PrimitiveSocketValue{0});
        return;
      }
    }
    if (std::get_if<InputSocketValue>(&value.value)) {
      /* Cases were the input has a primitive value are handled above. */
      return;
    }
    if (std::get_if<FallbackValue>(&value.value)) {
      /* Cases were the input has a primitive fallback value are handled above. */
      return;
    }
    if (std::get_if<DanglingValue>(&value.value)) {
      /* Input sockets should never have a dangling value, because they are replaced by the socket
       * value in #handle_input_socket. */
      BLI_assert_unreachable();
      return;
    }
    if (std::get_if<BundleSocketValuePtr>(&value.value)) {
      /* This type can't be assigned to a socket. The bundle has to be separated first. */
      BLI_assert_unreachable();
      return;
    }
    if (std::get_if<ClosureZoneValue>(&value.value)) {
      /* This type can't be assigned to a socket. One has to evaluate a closure. */
      BLI_assert_unreachable();
      return;
    }
    if (const auto *src_socket_value = std::get_if<LinkedSocketValue>(&value.value)) {
      if (dst_tree_.typeinfo->validate_link(src_socket_value->socket->typeinfo->type,
                                            dst_socket.typeinfo->type))
      {
        bke::node_add_link(
            dst_tree_, *src_socket_value->node, *src_socket_value->socket, dst_node, dst_socket);
      }
      return;
    }
    BLI_assert_unreachable();
  }

  NodeAndSocket primitive_value_to_output_socket(const PrimitiveSocketValue &value)
  {
    if (const float *value_float = std::get_if<float>(&value.value)) {
      bNode *node = this->add_node("ShaderNodeValue");
      bNodeSocket *socket = static_cast<bNodeSocket *>(node->outputs.first);
      socket->default_value_typed<bNodeSocketValueFloat>()->value = *value_float;
      return {node, socket};
    }
    if (const int *value_int = std::get_if<int>(&value.value)) {
      bNode *node = this->add_node("ShaderNodeValue");
      bNodeSocket *socket = static_cast<bNodeSocket *>(node->outputs.first);
      socket->default_value_typed<bNodeSocketValueFloat>()->value = *value_int;
      return {node, socket};
    }
    if (const bool *value_bool = std::get_if<bool>(&value.value)) {
      bNode *node = this->add_node("ShaderNodeValue");
      bNodeSocket *socket = static_cast<bNodeSocket *>(node->outputs.first);
      socket->default_value_typed<bNodeSocketValueFloat>()->value = *value_bool;
      return {node, socket};
    }
    if (const float3 *value_float3 = std::get_if<float3>(&value.value)) {
      bNode *node = this->add_node("ShaderNodeCombineXYZ");
      bNodeSocket *output_socket = static_cast<bNodeSocket *>(node->outputs.first);
      bNodeSocket *input_x = static_cast<bNodeSocket *>(node->inputs.first);
      bNodeSocket *input_y = input_x->next;
      bNodeSocket *input_z = input_y->next;
      input_x->default_value_typed<bNodeSocketValueFloat>()->value = value_float3->x;
      input_y->default_value_typed<bNodeSocketValueFloat>()->value = value_float3->y;
      input_z->default_value_typed<bNodeSocketValueFloat>()->value = value_float3->z;
      return {node, output_socket};
    }
    if (const ColorGeometry4f *value_color = std::get_if<ColorGeometry4f>(&value.value)) {
      bNode *node = this->add_node("ShaderNodeRGB");
      bNodeSocket *output_socket = static_cast<bNodeSocket *>(node->outputs.first);
      auto *socket_storage = static_cast<bNodeSocketValueRGBA *>(output_socket->default_value);
      copy_v3_v3(socket_storage->value, *value_color);
      socket_storage->value[3] = 1.0f;
      return {node, output_socket};
    }
    BLI_assert_unreachable();
    return {};
  }

  bNode *add_node(const StringRefNull idname)
  {
    return bke::node_add_node(nullptr, dst_tree_, idname, this->get_next_node_identifier());
  }

  int get_next_node_identifier()
  {
    return ++dst_node_counter_;
  }

  void set_primitive_value_on_socket(bNodeSocket &socket, const PrimitiveSocketValue &value)
  {
    switch (socket.type) {
      case SOCK_FLOAT: {
        socket.default_value_typed<bNodeSocketValueFloat>()->value = std::get<float>(value.value);
        break;
      }
      case SOCK_INT: {
        socket.default_value_typed<bNodeSocketValueInt>()->value = std::get<int>(value.value);
        break;
      }
      case SOCK_BOOLEAN: {
        socket.default_value_typed<bNodeSocketValueBoolean>()->value = std::get<bool>(value.value);
        break;
      }
      case SOCK_VECTOR: {
        copy_v3_v3(socket.default_value_typed<bNodeSocketValueVector>()->value,
                   std::get<float3>(value.value));
        break;
      }
      case SOCK_RGBA: {
        copy_v4_v4(socket.default_value_typed<bNodeSocketValueRGBA>()->value,
                   std::get<ColorGeometry4f>(value.value));
        break;
      }
      default: {
        BLI_assert_unreachable();
        break;
      }
    }
  }

  void restore_zones_in_output_tree()
  {
    for (const PreservedZone &copied_zone : copied_zone_by_zone_output_node_.values()) {
      if (!copied_zone.input_node || !copied_zone.output_node) {
        continue;
      }
      const bke::bNodeZoneType *zone_type = bke::zone_type_by_node_type(
          copied_zone.input_node->type_legacy);
      if (!zone_type) {
        continue;
      }
      int &output_id = zone_type->get_corresponding_output_id(*copied_zone.input_node);
      output_id = copied_zone.output_node->identifier;
    }
  }

  void position_nodes_in_output_tree()
  {
    bNodeTree &tree = dst_tree_;
    tree.ensure_topology_cache();

    Map<int, int> num_by_depth;
    Map<bNode *, int> depth_by_node;

    /* Simple algorithm that does a very rough layout of the generated tree. This does not produce
     * great results generally, but is usually good enough when debugging smaller node trees. */
    for (bNode *node : tree.toposort_right_to_left()) {
      int depth = 0;
      for (bNodeSocket *socket : node->output_sockets()) {
        for (bNodeSocket *target : socket->directly_linked_sockets()) {
          depth = std::max(depth, depth_by_node.lookup(&target->owner_node()) + 1);
        }
      }
      depth_by_node.add_new(node, depth);
      const int index_at_depth = num_by_depth.lookup_or_add(depth, 0)++;
      node->location[0] = 200 - depth * 200;
      node->location[1] = -index_at_depth * 300;
    }
  }

  /**
   * Utility to that copies the value of the origin socket to the current socket. If the origin
   * value does not exist yet, the origin socket is only scheduled.
   */
  void forward_value_or_schedule(const SocketInContext &socket, const SocketInContext &origin)
  {
    if (const SocketValue *value = value_by_socket_.lookup_ptr(origin)) {
      if (socket->type == origin->type) {
        this->store_socket_value(socket, *value);
        return;
      }
      this->store_socket_value(
          socket, this->handle_implicit_conversion(*value, *origin->typeinfo, *socket->typeinfo));
      return;
    }
    this->schedule_socket(origin);
  }

  void store_socket_value(const SocketInContext &socket, SocketValue value)
  {
    value_by_socket_.add_new(socket, std::move(value));
  }

  void store_socket_value_fallback(const SocketInContext &socket)
  {
    value_by_socket_.add_new(socket, {FallbackValue{}});
  }

  void store_socket_value_dangling(const SocketInContext &socket)
  {
    value_by_socket_.add_new(socket, {DanglingValue{}});
  }

  void schedule_socket(const SocketInContext &socket)
  {
    scheduled_sockets_stack_.push(socket);
  }

  int node_copy_flag() const
  {
    const bool use_refcounting = !(dst_tree_.id.tag & ID_TAG_NO_MAIN);
    return use_refcounting ? 0 : LIB_ID_CREATE_NO_USER_REFCOUNT;
  }
};

}  // namespace

bool inline_shader_node_tree(const bNodeTree &src_tree,
                             bNodeTree &dst_tree,
                             InlineShaderNodeTreeParams &params)
{
  ShaderNodesInliner inliner(src_tree, dst_tree, params);

  if (inliner.do_inline()) {
    /* Update deprecated bNodeSocket.link pointers because some code still depends on it. */
    LISTBASE_FOREACH (bNode *, node, &dst_tree.nodes) {
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->inputs) {
        sock->link = nullptr;
      }
      LISTBASE_FOREACH (bNodeSocket *, sock, &node->outputs) {
        sock->link = nullptr;
      }
    }
    LISTBASE_FOREACH (bNodeLink *, link, &dst_tree.links) {
      link->tosock->link = link;
      BLI_assert(dst_tree.typeinfo->validate_link(link->fromsock->typeinfo->type,
                                                  link->tosock->typeinfo->type));
      link->flag |= NODE_LINK_VALID;
    }
    return true;
  }

  return false;
}

}  // namespace blender::nodes

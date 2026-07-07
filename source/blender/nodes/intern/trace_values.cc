/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_stack.hh"

#include "NOD_geometry_nodes_bundle_signature.hh"
#include "NOD_geometry_nodes_closure_location.hh"
#include "NOD_geometry_nodes_closure_signature.hh"
#include "NOD_node_in_compute_context.hh"
#include "NOD_socket_declarations.hh"
#include "NOD_trace_values.hh"

#include "BKE_compute_context_cache.hh"
#include "BKE_node_tree_zones.hh"

#include "ED_node.hh"

namespace blender::nodes {

static bool is_evaluate_closure_node_input(const SocketInContext &socket)
{
  return socket->is_input() && socket->index() == 0 &&
         socket.owner_node()->is_type("NodeEvaluateClosure");
}

static bool is_closure_zone_output_socket(const SocketInContext &socket)
{
  return socket->owner_node().is_type("NodeClosureOutput") && socket->is_output();
}

static bool use_link_for_tracing(const bNodeLink &link)
{
  if (!link.is_used()) {
    return false;
  }
  const bNodeTree &tree = link.fromnode->owner_tree();
  if (tree.typeinfo->validate_link &&
      !tree.typeinfo->validate_link(eNodeSocketDatatype(link.fromsock->type),
                                    eNodeSocketDatatype(link.tosock->type)))
  {
    return false;
  }
  return true;
}

static Vector<SocketInContext> find_origin_sockets_through_contexts(
    SocketInContext start_socket,
    bke::ComputeContextCache &compute_context_cache,
    FunctionRef<bool(const SocketInContext &)> handle_possible_origin_socket_fn,
    bool find_all);

static Vector<SocketInContext> find_target_sockets_through_contexts(
    const SocketInContext start_socket,
    bke::ComputeContextCache &compute_context_cache,
    const FunctionRef<bool(const SocketInContext &)> handle_possible_target_socket_fn,
    const bool find_all)
{
  using BundlePath = Vector<std::string, 0>;

  struct SocketToCheck {
    SocketInContext socket;
    BundlePath bundle_path;
  };

  Stack<SocketToCheck> sockets_to_check;
  Set<SocketInContext> added_sockets;

  auto add_if_new = [&](const SocketInContext &socket, BundlePath bundle_path) {
    if (added_sockets.add(socket)) {
      sockets_to_check.push({socket, std::move(bundle_path)});
    }
  };

  add_if_new(start_socket, {});

  VectorSet<SocketInContext> found_targets;

  while (!sockets_to_check.is_empty()) {
    const SocketToCheck socket_to_check = sockets_to_check.pop();
    const SocketInContext socket = socket_to_check.socket;
    const BundlePath &bundle_path = socket_to_check.bundle_path;
    const NodeInContext &node = socket.owner_node();
    if (socket->is_input()) {
      if (node->is_muted()) {
        for (const bNodeLink &link : node->internal_links()) {
          if (link.fromsock == socket.socket) {
            add_if_new({socket.context, link.tosock}, bundle_path);
          }
        }
        continue;
      }
      if (bundle_path.is_empty() && handle_possible_target_socket_fn(socket)) {
        found_targets.add(socket);
        if (!find_all) {
          break;
        }
        continue;
      }
      if (node->is_reroute()) {
        add_if_new(node.output_socket(0), bundle_path);
        continue;
      }
      if (node->is_group()) {
        if (const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node->id)) {
          group->ensure_topology_cache();
          const ComputeContext &group_compute_context = compute_context_cache.for_group_node(
              socket.context, node->identifier, &node->owner_tree());
          for (const bNode *input_node : group->group_input_nodes()) {
            if (const bNodeSocket *group_input_socket = input_node->output_by_identifier(
                    socket->identifier))
            {
              if (group_input_socket->is_directly_linked()) {
                add_if_new({&group_compute_context, group_input_socket}, bundle_path);
              }
            }
          }
        }
        continue;
      }
      if (node->is_group_output()) {
        if (const auto *group_context = dynamic_cast<const bke::GroupNodeComputeContext *>(
                socket.context))
        {
          const bNodeTree *caller_group = group_context->tree();
          const bNode *caller_group_node = group_context->node();
          if (caller_group && caller_group_node) {
            caller_group->ensure_topology_cache();
            if (const bNodeSocket *output_socket = caller_group_node->output_by_identifier(
                    socket->identifier))
            {
              add_if_new({group_context->parent(), output_socket}, bundle_path);
            }
          }
        }
        continue;
      }
      if (node->is_type("NodeCombineBundle")) {
        const auto &storage = *static_cast<const NodeCombineBundle *>(node->storage);
        BundlePath new_bundle_path = bundle_path;
        new_bundle_path.append(storage.items[socket->index()].name);
        add_if_new(node.output_socket(0), std::move(new_bundle_path));
        continue;
      }
      if (node->is_type("NodeSeparateBundle")) {
        if (bundle_path.is_empty()) {
          continue;
        }
        const StringRef last_key = bundle_path.last();
        const auto &storage = *static_cast<const NodeSeparateBundle *>(node->storage);
        for (const int output_i : IndexRange(storage.items_num)) {
          if (last_key == storage.items[output_i].name) {
            add_if_new(node.output_socket(output_i), bundle_path.as_span().drop_back(1));
          }
        }
        continue;
      }
      if (node->is_type("NodeClosureOutput")) {
        const auto &closure_storage = *static_cast<const NodeClosureOutput *>(node->storage);
        const StringRef key = closure_storage.output_items.items[socket->index()].name;
        const Vector<SocketInContext> target_sockets = find_target_sockets_through_contexts(
            node.output_socket(0), compute_context_cache, is_evaluate_closure_node_input, true);
        for (const auto &target_socket : target_sockets) {
          const NodeInContext evaluate_node = target_socket.owner_node();
          const auto &evaluate_storage = *static_cast<const NodeEvaluateClosure *>(
              evaluate_node->storage);
          for (const int i : IndexRange(evaluate_storage.output_items.items_num)) {
            const NodeEvaluateClosureOutputItem &item = evaluate_storage.output_items.items[i];
            if (key == item.name) {
              add_if_new(evaluate_node.output_socket(i), bundle_path);
            }
          }
        }
        continue;
      }
      if (node->is_type("NodeEvaluateClosure")) {
        if (socket->index() == 0) {
          continue;
        }
        const auto &evaluate_storage = *static_cast<const NodeEvaluateClosure *>(node->storage);
        const StringRef key = evaluate_storage.input_items.items[socket->index() - 1].name;
        const Vector<SocketInContext> origin_sockets = find_origin_sockets_through_contexts(
            node.input_socket(0), compute_context_cache, is_closure_zone_output_socket, true);
        for (const SocketInContext origin_socket : origin_sockets) {
          const bNodeTree &closure_tree = origin_socket->owner_tree();
          const bke::bNodeTreeZones *closure_tree_zones = closure_tree.zones();
          if (!closure_tree_zones) {
            continue;
          }
          const auto &closure_output_node = origin_socket.owner_node();
          const bke::bNodeTreeZone *closure_zone = closure_tree_zones->get_zone_by_node(
              closure_output_node->identifier);
          if (!closure_zone) {
            continue;
          }
          const bNode *closure_input_node = closure_zone->input_node();
          if (!closure_input_node) {
            continue;
          }
          const bke::EvaluateClosureComputeContext &closure_context =
              compute_context_cache.for_evaluate_closure(
                  node.context,
                  node->identifier,
                  &node->owner_tree(),
                  ClosureSourceLocation{&closure_tree,
                                        closure_output_node->identifier,
                                        origin_socket.context_hash(),
                                        origin_socket.context});
          if (closure_context.is_recursive()) {
            continue;
          }
          const auto &closure_output_storage = *static_cast<const NodeClosureOutput *>(
              closure_output_node->storage);
          for (const int i : IndexRange(closure_output_storage.input_items.items_num)) {
            const NodeClosureInputItem &item = closure_output_storage.input_items.items[i];
            if (key == item.name) {
              add_if_new({&closure_context, &closure_input_node->output_socket(i)}, bundle_path);
            }
          }
        }
        continue;
      }
      if (node->is_type("GeometryNodeSimulationInput")) {
        const ComputeContext &simulation_compute_context =
            compute_context_cache.for_simulation_zone(socket.context, *node);
        add_if_new({&simulation_compute_context, &node->output_socket(socket->index() + 1)},
                   bundle_path);
        continue;
      }
      if (node->is_type("GeometryNodeSimulationOutput")) {
        const int output_index = socket->index();
        if (output_index >= 1) {
          BLI_assert(dynamic_cast<const bke::SimulationZoneComputeContext *>(socket.context));
          add_if_new({socket.context->parent(), &node->output_socket(output_index - 1)},
                     bundle_path);
        }
        continue;
      }
      if (node->is_type("GeometryNodeRepeatInput")) {
        const int index = socket->index();
        if (index >= 1) {
          const ComputeContext &repeat_compute_context = compute_context_cache.for_repeat_zone(
              socket.context, *node, 0);
          add_if_new({&repeat_compute_context, &node->output_socket(index)}, bundle_path);
          const auto &storage = *static_cast<NodeGeometryRepeatInput *>(node->storage);
          if (const bNode *repeat_output_node = node->owner_tree().node_by_id(
                  storage.output_node_id))
          {
            add_if_new({socket.context, &repeat_output_node->output_socket(index - 1)},
                       bundle_path);
          }
        }
        continue;
      }
      if (node->is_type("GeometryNodeRepeatOutput")) {
        BLI_assert(dynamic_cast<const bke::RepeatZoneComputeContext *>(socket.context));
        add_if_new({socket.context->parent(), &node->output_socket(socket->index())}, bundle_path);
        continue;
      }
      for (const bNodeSocket *output_socket : node->output_sockets()) {
        const SocketDeclaration *output_decl = output_socket->runtime->declaration;
        if (!output_decl) {
          continue;
        }
        if (const decl::Bundle *bundle_decl = dynamic_cast<const decl::Bundle *>(output_decl)) {
          if (bundle_decl->pass_through_input_index == socket->index()) {
            add_if_new({socket.context, output_socket}, bundle_path);
          }
        }
      }
    }
    else {
      const bke::bNodeTreeZones *zones = node->owner_tree().zones();
      if (!zones) {
        continue;
      }
      const bke::bNodeTreeZone *from_zone = zones->get_zone_by_socket(*socket.socket);
      for (const bNodeLink *link : socket->directly_linked_links()) {
        if (!use_link_for_tracing(*link)) {
          continue;
        }
        bNodeSocket *to_socket = link->tosock;
        const bke::bNodeTreeZone *to_zone = zones->get_zone_by_socket(*to_socket);
        if (!zones->link_between_zones_is_allowed(from_zone, to_zone)) {
          continue;
        }
        const Vector<const bke::bNodeTreeZone *> zones_to_enter = zones->get_zones_to_enter(
            from_zone, to_zone);
        const ComputeContext *compute_context = ed::space_node::compute_context_for_zones(
            zones_to_enter, compute_context_cache, socket.context);
        if (!compute_context) {
          continue;
        }
        add_if_new({compute_context, to_socket}, bundle_path);
      }
    }
  }
  return found_targets.extract_vector();
}

[[nodiscard]] const ComputeContext *compute_context_for_closure_evaluation(
    const ComputeContext *closure_socket_context,
    const bNodeSocket &closure_socket,
    bke::ComputeContextCache &compute_context_cache,
    const std::optional<ClosureSourceLocation> &source_location)
{
  const Vector<SocketInContext> target_sockets = find_target_sockets_through_contexts(
      {closure_socket_context, &closure_socket},
      compute_context_cache,
      is_evaluate_closure_node_input,
      false);
  if (target_sockets.is_empty()) {
    return nullptr;
  }
  const SocketInContext target_socket = target_sockets[0];
  const NodeInContext target_node = target_socket.owner_node();
  return &compute_context_cache.for_evaluate_closure(target_socket.context,
                                                     target_node->identifier,
                                                     &target_socket->owner_tree(),
                                                     source_location);
}

static Vector<SocketInContext> find_origin_sockets_through_contexts(
    const SocketInContext start_socket,
    bke::ComputeContextCache &compute_context_cache,
    const FunctionRef<bool(const SocketInContext &)> handle_possible_origin_socket_fn,
    const bool find_all)
{
  using BundlePath = Vector<std::string, 0>;

  struct SocketToCheck {
    SocketInContext socket;
    BundlePath bundle_path;
  };

  Stack<SocketToCheck> sockets_to_check;
  Set<SocketInContext> added_sockets;

  auto add_if_new = [&](const SocketInContext &socket, BundlePath bundle_path) {
    if (added_sockets.add(socket)) {
      sockets_to_check.push({socket, std::move(bundle_path)});
    }
  };

  add_if_new(start_socket, {});

  VectorSet<SocketInContext> found_origins;

  while (!sockets_to_check.is_empty()) {
    const SocketToCheck socket_to_check = sockets_to_check.pop();
    const SocketInContext socket = socket_to_check.socket;
    const BundlePath &bundle_path = socket_to_check.bundle_path;
    const NodeInContext &node = socket.owner_node();
    const SocketDeclaration *socket_decl = socket->runtime->declaration;
    if (socket->is_input()) {
      if (bundle_path.is_empty() && handle_possible_origin_socket_fn(socket)) {
        found_origins.add(socket);
        if (!find_all) {
          break;
        }
        continue;
      }
      const bke::bNodeTreeZones *zones = node->owner_tree().zones();
      if (!zones) {
        continue;
      }
      const bke::bNodeTreeZone *to_zone = zones->get_zone_by_socket(*socket.socket);
      for (const bNodeLink *link : socket->directly_linked_links()) {
        if (!use_link_for_tracing(*link)) {
          continue;
        }
        const bNodeSocket *from_socket = link->fromsock;
        const bke::bNodeTreeZone *from_zone = zones->get_zone_by_socket(*from_socket);
        if (!zones->link_between_zones_is_allowed(from_zone, to_zone)) {
          continue;
        }
        const ComputeContext *compute_context = socket.context;
        for (const bke::bNodeTreeZone *zone = to_zone; zone != from_zone; zone = zone->parent_zone)
        {
          if (const auto *evaluate_closure_context =
                  dynamic_cast<const bke::EvaluateClosureComputeContext *>(compute_context))
          {
            const std::optional<nodes::ClosureSourceLocation> &source_location =
                evaluate_closure_context->closure_source_location();
            /* This is expected to be available during value tracing. */
            BLI_assert(source_location);
            BLI_assert(source_location->compute_context);
            compute_context = source_location->compute_context;
          }
          else {
            compute_context = compute_context->parent();
          }
        }
        add_if_new({compute_context, from_socket}, bundle_path);
      }
    }
    else {
      if (node->is_muted()) {
        for (const bNodeLink &link : node->internal_links()) {
          if (link.tosock == socket.socket) {
            add_if_new({socket.context, link.fromsock}, bundle_path);
          }
        }
        continue;
      }
      if (bundle_path.is_empty() && handle_possible_origin_socket_fn(socket)) {
        found_origins.add(socket);
        if (!find_all) {
          break;
        }
        continue;
      }
      if (node->is_reroute()) {
        add_if_new(node.input_socket(0), bundle_path);
        continue;
      }
      if (node->is_group()) {
        if (const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node->id)) {
          group->ensure_topology_cache();
          if (const bNode *group_output_node = group->group_output_node()) {
            const ComputeContext &group_compute_context = compute_context_cache.for_group_node(
                socket.context, node->identifier, &node->owner_tree());
            if (const bNodeSocket *group_output_socket = group_output_node->input_by_identifier(
                    socket->identifier))
            {
              add_if_new({&group_compute_context, group_output_socket}, bundle_path);
            }
          }
        }
        continue;
      }
      if (node->is_group_input()) {
        if (const auto *group_context = dynamic_cast<const bke::GroupNodeComputeContext *>(
                socket.context))
        {
          const bNodeTree *caller_group = group_context->tree();
          const bNode *caller_group_node = group_context->node();
          if (caller_group && caller_group_node) {
            caller_group->ensure_topology_cache();
            if (const bNodeSocket *input_socket = caller_group_node->input_by_identifier(
                    socket->identifier))
            {
              add_if_new({group_context->parent(), input_socket}, bundle_path);
            }
          }
        }
        continue;
      }
      if (node->is_type("NodeJoinBundle")) {
        add_if_new(node.input_socket(0), bundle_path);
        continue;
      }
      if (node->is_type("NodeEvaluateClosure")) {
        const auto &evaluate_storage = *static_cast<const NodeEvaluateClosure *>(node->storage);
        const StringRef key = evaluate_storage.output_items.items[socket->index()].name;
        const Vector<SocketInContext> origin_sockets = find_origin_sockets_through_contexts(
            node.input_socket(0), compute_context_cache, is_closure_zone_output_socket, true);
        for (const SocketInContext origin_socket : origin_sockets) {
          const bNodeTree &closure_tree = origin_socket->owner_tree();
          const NodeInContext closure_output_node = origin_socket.owner_node();
          const auto &closure_storage = *static_cast<const NodeClosureOutput *>(
              closure_output_node->storage);
          const bke::EvaluateClosureComputeContext &closure_context =
              compute_context_cache.for_evaluate_closure(
                  node.context,
                  node->identifier,
                  &node->owner_tree(),
                  ClosureSourceLocation{&closure_tree,
                                        closure_output_node->identifier,
                                        origin_socket.context_hash(),
                                        origin_socket.context});
          if (closure_context.is_recursive()) {
            continue;
          }
          for (const int i : IndexRange(closure_storage.output_items.items_num)) {
            const NodeClosureOutputItem &item = closure_storage.output_items.items[i];
            if (key == item.name) {
              add_if_new({&closure_context, &closure_output_node->input_socket(i)}, bundle_path);
            }
          }
        }
        continue;
      }
      if (node->is_type("NodeClosureInput")) {
        const auto &input_storage = *static_cast<const NodeClosureInput *>(node->storage);
        const bNode *closure_output_node = node->owner_tree().node_by_id(
            input_storage.output_node_id);
        if (!closure_output_node) {
          continue;
        }
        const auto &output_storage = *static_cast<const NodeClosureOutput *>(
            closure_output_node->storage);
        const StringRef key = output_storage.input_items.items[socket->index()].name;
        const bNodeSocket &closure_output_socket = closure_output_node->output_socket(0);
        const Vector<SocketInContext> target_sockets = find_target_sockets_through_contexts(
            {socket.context, &closure_output_socket},
            compute_context_cache,
            is_evaluate_closure_node_input,
            true);
        for (const SocketInContext &target_socket : target_sockets) {
          const NodeInContext target_node = target_socket.owner_node();
          const auto &evaluate_storage = *static_cast<const NodeEvaluateClosure *>(
              target_node.node->storage);
          for (const int i : IndexRange(evaluate_storage.input_items.items_num)) {
            const NodeEvaluateClosureInputItem &item = evaluate_storage.input_items.items[i];
            if (key == item.name) {
              add_if_new(target_node.input_socket(i + 1), bundle_path);
            }
          }
        }
        continue;
      }
      if (node->is_type("NodeCombineBundle")) {
        if (bundle_path.is_empty()) {
          continue;
        }
        const StringRef last_key = bundle_path.last();
        const auto &storage = *static_cast<const NodeCombineBundle *>(node->storage);
        for (const int input_i : IndexRange(storage.items_num)) {
          if (last_key == storage.items[input_i].name) {
            add_if_new(node.input_socket(input_i), bundle_path.as_span().drop_back(1));
          }
        }
        continue;
      }
      if (node->is_type("NodeSeparateBundle")) {
        const auto &storage = *static_cast<const NodeSeparateBundle *>(node->storage);
        BundlePath new_bundle_path = bundle_path;
        new_bundle_path.append(storage.items[socket->index()].name);
        add_if_new(node.input_socket(0), std::move(new_bundle_path));
        continue;
      }
      if (node->is_type("GeometryNodeSimulationInput")) {
        const int output_index = socket->index();
        if (output_index >= 1) {
          BLI_assert(dynamic_cast<const bke::SimulationZoneComputeContext *>(socket.context));
          add_if_new({socket.context->parent(), &node->input_socket(output_index - 1)},
                     bundle_path);
        }
        continue;
      }
      if (node->is_type("GeometryNodeSimulationOutput")) {
        const ComputeContext &simulation_compute_context =
            compute_context_cache.for_simulation_zone(socket.context, *node);
        add_if_new({&simulation_compute_context, &node->input_socket(socket->index() + 1)},
                   bundle_path);
        continue;
      }
      if (node->is_type("GeometryNodeRepeatInput")) {
        const int index = socket->index();
        if (index >= 1) {
          BLI_assert(dynamic_cast<const bke::RepeatZoneComputeContext *>(socket.context));
          add_if_new({socket.context->parent(), &node->input_socket(index)}, bundle_path);
        }
        continue;
      }
      if (node->is_type("GeometryNodeRepeatOutput")) {
        const int index = socket->index();
        const ComputeContext &repeat_compute_context = compute_context_cache.for_repeat_zone(
            socket.context, *node, 0);
        add_if_new({&repeat_compute_context, &node->input_socket(index)}, bundle_path);
        const bke::bNodeZoneType &zone_type = *bke::zone_type_by_node_type(node->type_legacy);
        if (const bNode *repeat_input_node = zone_type.get_corresponding_input(node->owner_tree(),
                                                                               *node))
        {
          add_if_new({socket.context, &repeat_input_node->input_socket(index + 1)}, bundle_path);
        }
        continue;
      }
      if (socket_decl) {
        if (const decl::Bundle *bundle_decl = dynamic_cast<const decl::Bundle *>(socket_decl)) {
          if (bundle_decl->pass_through_input_index) {
            const int input_index = *bundle_decl->pass_through_input_index;
            add_if_new(node.input_socket(input_index), bundle_path);
          }
        }
      }
    }
  }

  return found_origins.extract_vector();
}

LinkedBundleSignatures gather_linked_target_bundle_signatures(
    const ComputeContext *bundle_socket_context,
    const bNodeSocket &bundle_socket,
    bke::ComputeContextCache &compute_context_cache)
{
  LinkedBundleSignatures result;
  find_target_sockets_through_contexts(
      {bundle_socket_context, &bundle_socket},
      compute_context_cache,
      [&](const SocketInContext &socket) {
        const bNode &node = socket->owner_node();
        if (socket->is_input() && node.is_type("NodeSeparateBundle")) {
          const auto &storage = *static_cast<const NodeSeparateBundle *>(node.storage);
          result.items.append({BundleSignature::from_separate_bundle_node(node, false),
                               bool(storage.flag & NODE_SEPARATE_BUNDLE_FLAG_DEFINE_SIGNATURE),
                               socket});
          return true;
        }
        return false;
      },
      true);
  return result;
}

LinkedBundleSignatures gather_linked_origin_bundle_signatures(
    const ComputeContext *bundle_socket_context,
    const bNodeSocket &bundle_socket,
    bke::ComputeContextCache &compute_context_cache)
{
  LinkedBundleSignatures result;
  find_origin_sockets_through_contexts(
      {bundle_socket_context, &bundle_socket},
      compute_context_cache,
      [&](const SocketInContext &socket) {
        const NodeInContext node = socket.owner_node();
        if (socket->is_output()) {
          if (node->is_type("NodeCombineBundle")) {
            const auto &storage = *static_cast<const NodeCombineBundle *>(node->storage);
            result.items.append({BundleSignature::from_combine_bundle_node(*node, false),
                                 bool(storage.flag & NODE_COMBINE_BUNDLE_FLAG_DEFINE_SIGNATURE),
                                 socket});
            return true;
          }
        }
        if (node->is_type("NodeJoinBundle")) {
          const SocketInContext input_socket = node.input_socket(0);
          BundleSignature joined_signature;
          bool is_signature_definition = true;
          for (const bNodeLink *link : input_socket->directly_linked_links()) {
            if (!link->is_used()) {
              continue;
            }
            const bNodeSocket *socket_from = link->fromsock;
            const LinkedBundleSignatures sub_signatures = gather_linked_origin_bundle_signatures(
                node.context, *socket_from, compute_context_cache);
            for (const LinkedBundleSignatures::Item &sub_signature : sub_signatures.items) {
              if (!sub_signature.is_signature_definition) {
                is_signature_definition = false;
              }
              for (const BundleSignature::Item &item : sub_signature.signature.items) {
                joined_signature.items.add(item);
              }
            }
          }
          result.items.append({joined_signature, is_signature_definition, socket});
          return true;
        }
        return false;
      },
      true);
  return result;
}

LinkedClosureSignatures gather_linked_target_closure_signatures(
    const ComputeContext *closure_socket_context,
    const bNodeSocket &closure_socket,
    bke::ComputeContextCache &compute_context_cache)
{
  LinkedClosureSignatures result;
  find_target_sockets_through_contexts(
      {closure_socket_context, &closure_socket},
      compute_context_cache,
      [&](const SocketInContext &socket) {
        const bNode &node = socket->owner_node();
        if (is_evaluate_closure_node_input(socket)) {
          const auto &storage = *static_cast<const NodeEvaluateClosure *>(node.storage);
          result.items.append({ClosureSignature::from_evaluate_closure_node(node, false),
                               bool(storage.flag & NODE_EVALUATE_CLOSURE_FLAG_DEFINE_SIGNATURE),
                               socket});
          return true;
        }
        return false;
      },
      true);
  return result;
}

LinkedClosureSignatures gather_linked_origin_closure_signatures(
    const ComputeContext *closure_socket_context,
    const bNodeSocket &closure_socket,
    bke::ComputeContextCache &compute_context_cache)
{
  LinkedClosureSignatures result;
  find_origin_sockets_through_contexts(
      {closure_socket_context, &closure_socket},
      compute_context_cache,
      [&](const SocketInContext &socket) {
        const bNode &node = socket->owner_node();
        if (is_closure_zone_output_socket(socket)) {
          const auto &storage = *static_cast<const NodeClosureOutput *>(node.storage);
          result.items.append({ClosureSignature::from_closure_output_node(node, false),
                               bool(storage.flag & NODE_CLOSURE_FLAG_DEFINE_SIGNATURE),
                               socket});
          return true;
        }
        return false;
      },
      true);
  return result;
}

std::optional<NodeInContext> find_origin_index_menu_switch(
    const SocketInContext &src_socket, bke::ComputeContextCache &compute_context_cache)
{
  std::optional<NodeInContext> result;
  find_origin_sockets_through_contexts(
      src_socket,
      compute_context_cache,
      [&](const SocketInContext &socket) {
        if (socket->is_input()) {
          return false;
        }
        const NodeInContext node = socket.owner_node();
        if (!node->is_type("GeometryNodeMenuSwitch")) {
          return false;
        }
        const auto &storage = *static_cast<const NodeMenuSwitch *>(node->storage);
        if (storage.data_type != SOCK_INT) {
          return false;
        }
        result = socket.owner_node();
        return true;
      },
      false);
  return result;
}

}  // namespace blender::nodes

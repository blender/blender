/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "DNA_node_types.h"
#include "DNA_space_types.h"

#include "RNA_access.hh"

#include "WM_api.hh"

#include "BKE_compute_context_cache.hh"
#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_report.hh"
#include "BKE_workspace.hh"

#include "ED_node.hh"
#include "ED_screen.hh"

#include "BLI_listbase.h"

#include "BLT_translation.hh"

#include "NOD_geo_bundle.hh"
#include "NOD_geo_closure.hh"
#include "NOD_socket_items.hh"
#include "NOD_sync_sockets.hh"
#include "NOD_trace_values.hh"

namespace blender::nodes {

enum class NodeSyncState {
  Synced,
  CanBeSynced,
  NoSyncSource,
  ConflictingSyncSources,
};

struct BundleSyncState {
  NodeSyncState state;
  std::optional<nodes::BundleSignature> source_signature;
};

struct ClosureSyncState {
  NodeSyncState state;
  std::optional<nodes::ClosureSignature> source_signature;
};

static BundleSyncState get_sync_state_separate_bundle(
    const SpaceNode &snode,
    const bNode &separate_bundle_node,
    const bNodeSocket *src_bundle_socket = nullptr)
{
  BLI_assert(separate_bundle_node.is_type("NodeSeparateBundle"));
  snode.edittree->ensure_topology_cache();
  if (!src_bundle_socket) {
    src_bundle_socket = &separate_bundle_node.input_socket(0);
  }
  BLI_assert(src_bundle_socket->type == SOCK_BUNDLE);

  bke::ComputeContextCache compute_context_cache;
  const ComputeContext *current_context = ed::space_node::compute_context_for_edittree_socket(
      snode, compute_context_cache, *src_bundle_socket);
  if (!current_context) {
    return {NodeSyncState::NoSyncSource};
  }
  const LinkedBundleSignatures linked_signatures = gather_linked_origin_bundle_signatures(
      current_context, *src_bundle_socket, compute_context_cache);
  if (linked_signatures.items.is_empty()) {
    return {NodeSyncState::NoSyncSource};
  }
  std::optional<BundleSignature> merged_signature = linked_signatures.get_merged_signature();
  if (!merged_signature.has_value()) {
    return {NodeSyncState::ConflictingSyncSources};
  }
  if (!linked_signatures.has_type_definition()) {
    merged_signature->set_auto_structure_types();
  }
  const nodes::BundleSignature &current_signature =
      nodes::BundleSignature::from_separate_bundle_node(separate_bundle_node, true);
  if (*merged_signature != current_signature) {
    return {NodeSyncState::CanBeSynced, std::move(merged_signature)};
  }
  return {NodeSyncState::Synced};
}

static BundleSyncState get_sync_state_combine_bundle(
    const SpaceNode &snode,
    const bNode &combine_bundle_node,
    const bNodeSocket *src_bundle_socket = nullptr)
{
  BLI_assert(combine_bundle_node.is_type("NodeCombineBundle"));
  snode.edittree->ensure_topology_cache();
  if (!src_bundle_socket) {
    src_bundle_socket = &combine_bundle_node.output_socket(0);
  }
  BLI_assert(src_bundle_socket->type == SOCK_BUNDLE);

  bke::ComputeContextCache compute_context_cache;
  const ComputeContext *current_context = ed::space_node::compute_context_for_edittree_socket(
      snode, compute_context_cache, *src_bundle_socket);
  if (!current_context) {
    return {NodeSyncState::NoSyncSource};
  }
  const LinkedBundleSignatures linked_signatures = gather_linked_target_bundle_signatures(
      current_context, *src_bundle_socket, compute_context_cache);
  if (linked_signatures.items.is_empty()) {
    return {NodeSyncState::NoSyncSource};
  }
  std::optional<BundleSignature> merged_signature = linked_signatures.get_merged_signature();
  if (!merged_signature.has_value()) {
    return {NodeSyncState::ConflictingSyncSources};
  }
  if (!linked_signatures.has_type_definition()) {
    merged_signature->set_auto_structure_types();
  }
  const nodes::BundleSignature &current_signature =
      nodes::BundleSignature::from_combine_bundle_node(combine_bundle_node, true);
  if (*merged_signature != current_signature) {
    return {NodeSyncState::CanBeSynced, std::move(merged_signature)};
  }
  return {NodeSyncState::Synced};
}

static ClosureSyncState get_sync_state_closure_output(
    const SpaceNode &snode,
    const bNode &closure_output_node,
    const bNodeSocket *src_closure_socket = nullptr)
{
  snode.edittree->ensure_topology_cache();
  if (!src_closure_socket) {
    src_closure_socket = &closure_output_node.output_socket(0);
  }
  BLI_assert(src_closure_socket->type == SOCK_CLOSURE);

  bke::ComputeContextCache compute_context_cache;
  const ComputeContext *current_context = ed::space_node::compute_context_for_edittree_socket(
      snode, compute_context_cache, *src_closure_socket);
  if (!current_context) {
    return {NodeSyncState::NoSyncSource};
  }
  const LinkedClosureSignatures linked_signatures = gather_linked_target_closure_signatures(
      current_context, *src_closure_socket, compute_context_cache);
  if (linked_signatures.items.is_empty()) {
    return {NodeSyncState::NoSyncSource};
  }
  std::optional<ClosureSignature> merged_signature = linked_signatures.get_merged_signature();
  if (!merged_signature.has_value()) {
    return {NodeSyncState::ConflictingSyncSources};
  }
  if (!linked_signatures.has_type_definition()) {
    merged_signature->set_auto_structure_types();
  }
  const nodes::ClosureSignature &current_signature =
      nodes::ClosureSignature::from_closure_output_node(closure_output_node, true);
  if (*merged_signature != current_signature) {
    return {NodeSyncState::CanBeSynced, merged_signature};
  }
  return {NodeSyncState::Synced};
}

static ClosureSyncState get_sync_state_evaluate_closure(
    const SpaceNode &snode,
    const bNode &evaluate_closure_node,
    const bNodeSocket *src_closure_socket = nullptr)
{
  snode.edittree->ensure_topology_cache();
  if (!src_closure_socket) {
    src_closure_socket = &evaluate_closure_node.input_socket(0);
  }
  BLI_assert(src_closure_socket->type == SOCK_CLOSURE);

  bke::ComputeContextCache compute_context_cache;
  const ComputeContext *current_context = ed::space_node::compute_context_for_edittree_socket(
      snode, compute_context_cache, *src_closure_socket);
  if (!current_context) {
    return {NodeSyncState::NoSyncSource};
  }
  const LinkedClosureSignatures linked_signatures = gather_linked_origin_closure_signatures(
      current_context, *src_closure_socket, compute_context_cache);
  if (linked_signatures.items.is_empty()) {
    return {NodeSyncState::NoSyncSource};
  }
  std::optional<ClosureSignature> merged_signature = linked_signatures.get_merged_signature();
  if (!merged_signature.has_value()) {
    return {NodeSyncState::ConflictingSyncSources};
  }
  if (!linked_signatures.has_type_definition()) {
    merged_signature->set_auto_structure_types();
  }
  const nodes::ClosureSignature &current_signature =
      nodes::ClosureSignature::from_evaluate_closure_node(evaluate_closure_node, true);
  if (*merged_signature != current_signature) {
    return {NodeSyncState::CanBeSynced, merged_signature};
  }
  return {NodeSyncState::Synced};
}

void sync_sockets_separate_bundle(SpaceNode &snode,
                                  bNode &separate_bundle_node,
                                  ReportList *reports,
                                  const bNodeSocket *src_bundle_socket)
{
  const BundleSyncState sync_state = get_sync_state_separate_bundle(
      snode, separate_bundle_node, src_bundle_socket);
  switch (sync_state.state) {
    case NodeSyncState::Synced:
      return;
    case NodeSyncState::NoSyncSource:
      BKE_report(reports, RPT_INFO, "No bundle signature found");
      return;
    case NodeSyncState::ConflictingSyncSources:
      BKE_report(reports, RPT_INFO, "Found conflicting bundle signatures");
      return;
    case NodeSyncState::CanBeSynced:
      break;
  }

  auto &storage = *static_cast<NodeSeparateBundle *>(separate_bundle_node.storage);

  Map<std::string, int> old_identifiers;
  for (const int i : IndexRange(storage.items_num)) {
    const NodeSeparateBundleItem &item = storage.items[i];
    old_identifiers.add_new(StringRef(item.name), item.identifier);
  }

  nodes::socket_items::clear<nodes::SeparateBundleItemsAccessor>(separate_bundle_node);
  for (const nodes::BundleSignature::Item &item : sync_state.source_signature->items) {
    NodeSeparateBundleItem &new_item = *nodes::socket_items::add_item_with_socket_type_and_name<
        nodes ::SeparateBundleItemsAccessor>(
        *snode.edittree, separate_bundle_node, item.type->type, item.key.c_str());
    new_item.structure_type = int(item.structure_type);
    if (const std::optional<int> old_identifier = old_identifiers.lookup_try(item.key)) {
      new_item.identifier = *old_identifier;
    }
  }
  BKE_ntree_update_tag_node_property(snode.edittree, &separate_bundle_node);
}

void sync_sockets_combine_bundle(SpaceNode &snode,
                                 bNode &combine_bundle_node,
                                 ReportList *reports,
                                 const bNodeSocket *src_bundle_socket)
{
  const BundleSyncState sync_state = get_sync_state_combine_bundle(
      snode, combine_bundle_node, src_bundle_socket);
  switch (sync_state.state) {
    case NodeSyncState::Synced:
      return;
    case NodeSyncState::NoSyncSource:
      BKE_report(reports, RPT_INFO, "No bundle signature found");
      return;
    case NodeSyncState::ConflictingSyncSources:
      BKE_report(reports, RPT_INFO, "Found conflicting bundle signatures");
      return;
    case NodeSyncState::CanBeSynced:
      break;
  }

  auto &storage = *static_cast<NodeCombineBundle *>(combine_bundle_node.storage);

  Map<std::string, int> old_identifiers;
  for (const int i : IndexRange(storage.items_num)) {
    const NodeCombineBundleItem &item = storage.items[i];
    old_identifiers.add_new(StringRef(item.name), item.identifier);
  }

  nodes::socket_items::clear<nodes::CombineBundleItemsAccessor>(combine_bundle_node);
  for (const nodes::BundleSignature::Item &item : sync_state.source_signature->items) {
    NodeCombineBundleItem &new_item = *nodes::socket_items::add_item_with_socket_type_and_name<
        nodes ::CombineBundleItemsAccessor>(
        *snode.edittree, combine_bundle_node, item.type->type, item.key.c_str());
    new_item.structure_type = int(item.structure_type);
    if (const std::optional<int> old_identifier = old_identifiers.lookup_try(item.key)) {
      new_item.identifier = *old_identifier;
    }
  }

  BKE_ntree_update_tag_node_property(snode.edittree, &combine_bundle_node);
}

void sync_sockets_evaluate_closure(SpaceNode &snode,
                                   bNode &evaluate_closure_node,
                                   ReportList *reports,
                                   const bNodeSocket *src_closure_socket)
{
  const ClosureSyncState sync_state = get_sync_state_evaluate_closure(
      snode, evaluate_closure_node, src_closure_socket);
  switch (sync_state.state) {
    case NodeSyncState::Synced:
      return;
    case NodeSyncState::NoSyncSource:
      BKE_report(reports, RPT_INFO, "No closure signature found");
      return;
    case NodeSyncState::ConflictingSyncSources:
      BKE_report(reports, RPT_INFO, "Found conflicting closure signatures");
      return;
    case NodeSyncState::CanBeSynced:
      break;
  }

  auto &storage = *static_cast<NodeEvaluateClosure *>(evaluate_closure_node.storage);

  Map<std::string, int> old_input_identifiers;
  Map<std::string, int> old_output_identifiers;
  for (const int i : IndexRange(storage.input_items.items_num)) {
    const NodeEvaluateClosureInputItem &item = storage.input_items.items[i];
    old_input_identifiers.add_new(StringRef(item.name), item.identifier);
  }
  for (const int i : IndexRange(storage.output_items.items_num)) {
    const NodeEvaluateClosureOutputItem &item = storage.output_items.items[i];
    old_output_identifiers.add_new(StringRef(item.name), item.identifier);
  }

  nodes::socket_items::clear<nodes::EvaluateClosureInputItemsAccessor>(evaluate_closure_node);
  nodes::socket_items::clear<nodes::EvaluateClosureOutputItemsAccessor>(evaluate_closure_node);

  for (const nodes::ClosureSignature::Item &item : sync_state.source_signature->inputs) {
    NodeEvaluateClosureInputItem &new_item =
        *nodes::socket_items::add_item_with_socket_type_and_name<
            nodes::EvaluateClosureInputItemsAccessor>(
            *snode.edittree, evaluate_closure_node, item.type->type, item.key.c_str());
    new_item.structure_type = int(item.structure_type);
    if (const std::optional<int> old_identifier = old_input_identifiers.lookup_try(item.key)) {
      new_item.identifier = *old_identifier;
    }
  }
  for (const nodes::ClosureSignature::Item &item : sync_state.source_signature->outputs) {
    NodeEvaluateClosureOutputItem &new_item =
        *nodes::socket_items::add_item_with_socket_type_and_name<
            nodes::EvaluateClosureOutputItemsAccessor>(
            *snode.edittree, evaluate_closure_node, item.type->type, item.key.c_str());
    new_item.structure_type = int(item.structure_type);
    if (const std::optional<int> old_identifier = old_output_identifiers.lookup_try(item.key)) {
      new_item.identifier = *old_identifier;
    }
  }
  BKE_ntree_update_tag_node_property(snode.edittree, &evaluate_closure_node);
}

void sync_sockets_closure(SpaceNode &snode,
                          bNode &closure_input_node,
                          bNode &closure_output_node,
                          ReportList *reports,
                          const bNodeSocket *src_closure_socket)
{
  const ClosureSyncState sync_state = get_sync_state_closure_output(
      snode, closure_output_node, src_closure_socket);
  switch (sync_state.state) {
    case NodeSyncState::Synced:
      return;
    case NodeSyncState::NoSyncSource:
      BKE_report(reports, RPT_INFO, "No closure signature found");
      return;
    case NodeSyncState::ConflictingSyncSources:
      BKE_report(reports, RPT_INFO, "Found conflicting closure signatures");
      return;
    case NodeSyncState::CanBeSynced:
      break;
  }
  const nodes::ClosureSignature &signature = *sync_state.source_signature;

  auto &storage = *static_cast<NodeClosureOutput *>(closure_output_node.storage);

  Map<std::string, int> old_input_identifiers;
  Map<std::string, int> old_output_identifiers;
  for (const int i : IndexRange(storage.input_items.items_num)) {
    const NodeClosureInputItem &item = storage.input_items.items[i];
    old_input_identifiers.add_new(StringRef(item.name), item.identifier);
  }
  for (const int i : IndexRange(storage.output_items.items_num)) {
    const NodeClosureOutputItem &item = storage.output_items.items[i];
    old_output_identifiers.add_new(StringRef(item.name), item.identifier);
  }

  nodes::socket_items::clear<nodes::ClosureInputItemsAccessor>(closure_output_node);
  nodes::socket_items::clear<nodes::ClosureOutputItemsAccessor>(closure_output_node);

  for (const nodes::ClosureSignature::Item &item : signature.inputs) {
    NodeClosureInputItem &new_item =
        *nodes::socket_items::add_item_with_socket_type_and_name<nodes::ClosureInputItemsAccessor>(
            *snode.edittree, closure_output_node, item.type->type, item.key.c_str());
    new_item.structure_type = int(item.structure_type);
    if (const std::optional<int> old_identifier = old_input_identifiers.lookup_try(item.key)) {
      new_item.identifier = *old_identifier;
    }
  }
  for (const nodes::ClosureSignature::Item &item : signature.outputs) {
    NodeClosureOutputItem &new_item = *nodes::socket_items::add_item_with_socket_type_and_name<
        nodes::ClosureOutputItemsAccessor>(
        *snode.edittree, closure_output_node, item.type->type, item.key.c_str());
    new_item.structure_type = int(item.structure_type);
    if (const std::optional<int> old_identifier = old_output_identifiers.lookup_try(item.key)) {
      new_item.identifier = *old_identifier;
    }
  }
  BKE_ntree_update_tag_node_property(snode.edittree, &closure_input_node);
  BKE_ntree_update_tag_node_property(snode.edittree, &closure_output_node);

  nodes::update_node_declaration_and_sockets(*snode.edittree, closure_input_node);
  nodes::update_node_declaration_and_sockets(*snode.edittree, closure_output_node);

  /* Create internal zone links for newly created sockets. */
  snode.edittree->ensure_topology_cache();
  Vector<std::pair<bNodeSocket *, bNodeSocket *>> internal_links;
  for (const int input_i : signature.inputs.index_range()) {
    const nodes::ClosureSignature::Item &input_item = signature.inputs[input_i];
    if (old_input_identifiers.contains(input_item.key)) {
      continue;
    }
    for (const int output_i : signature.outputs.index_range()) {
      const nodes::ClosureSignature::Item &output_item = signature.outputs[output_i];
      if (old_output_identifiers.contains(output_item.key)) {
        continue;
      }
      if (input_item.key == output_item.key) {
        internal_links.append({&closure_input_node.output_socket(input_i),
                               &closure_output_node.input_socket(output_i)});
      }
    };
  }
  for (auto &&[from_socket, to_socket] : internal_links) {
    if (!snode.edittree->typeinfo->validate_link ||
        snode.edittree->typeinfo->validate_link(from_socket->typeinfo->type,
                                                to_socket->typeinfo->type))
    {
      bke::node_add_link(
          *snode.edittree, closure_input_node, *from_socket, closure_output_node, *to_socket);
    }
  }
}

static std::string get_bundle_sync_tooltip(const nodes::BundleSignature &old_signature,
                                           const nodes::BundleSignature &new_signature)
{
  Vector<StringRef> added_items;
  Vector<StringRef> removed_items;
  Vector<StringRef> changed_items;
  bool order_changed = false;

  for (const int new_item_i : new_signature.items.index_range()) {
    const BundleSignature::Item &new_item = new_signature.items[new_item_i];
    const int old_item_i = old_signature.items.index_of_try_as(new_item.key);
    if (old_item_i == -1) {
      added_items.append(new_item.key);
    }
    else {
      const BundleSignature::Item &old_item = old_signature.items[old_item_i];
      if (new_item != old_item) {
        changed_items.append(new_item.key);
      }
      if (old_item_i != new_item_i) {
        order_changed = true;
      }
    }
  }
  for (const nodes::BundleSignature::Item &old_item : old_signature.items) {
    if (!new_signature.items.contains_as(old_item.key)) {
      removed_items.append(old_item.key);
    }
  }

  fmt::memory_buffer string_buffer;
  auto buf = fmt::appender(string_buffer);
  if (!added_items.is_empty()) {
    fmt::format_to(buf, "\u2022 {}: {}\n", TIP_("Add"), fmt::join(added_items, ", "));
  }
  if (!removed_items.is_empty()) {
    fmt::format_to(buf, "\u2022 {}: {}\n", TIP_("Remove"), fmt::join(removed_items, ", "));
  }
  if (!changed_items.is_empty()) {
    fmt::format_to(buf, "\u2022 {}: {}\n", TIP_("Change"), fmt::join(changed_items, ", "));
  }
  if (order_changed) {
    fmt::format_to(buf, "\u2022 {}", TIP_("Reorder"));
  }
  fmt::format_to(buf, "\n{}", TIP_("Update based on linked bundle signature"));

  return fmt::to_string(string_buffer);
}

static std::string get_closure_sync_tooltip(const nodes::ClosureSignature &old_signature,
                                            const nodes::ClosureSignature &new_signature)
{
  Vector<StringRef> added_inputs;
  Vector<StringRef> removed_inputs;
  Vector<StringRef> changed_inputs;
  bool input_order = false;

  Vector<StringRef> added_outputs;
  Vector<StringRef> removed_outputs;
  Vector<StringRef> changed_outputs;
  bool output_order = false;

  for (const int new_item_i : new_signature.inputs.index_range()) {
    const nodes::ClosureSignature::Item &new_item = new_signature.inputs[new_item_i];
    const int old_item_i = old_signature.inputs.index_of_try_as(new_item.key);
    if (old_item_i == -1) {
      added_inputs.append(new_item.key);
    }
    else {
      const nodes::ClosureSignature::Item &old_item = old_signature.inputs[old_item_i];
      if (new_item != old_item) {
        changed_inputs.append(new_item.key);
      }
      if (old_item_i != new_item_i) {
        input_order = true;
      }
    }
  }
  for (const nodes::ClosureSignature::Item &old_item : old_signature.inputs) {
    if (!new_signature.inputs.contains_as(old_item.key)) {
      removed_inputs.append(old_item.key);
    }
  }
  for (const int new_item_i : new_signature.outputs.index_range()) {
    const nodes::ClosureSignature::Item &new_item = new_signature.outputs[new_item_i];
    const int old_item_i = old_signature.outputs.index_of_try_as(new_item.key);
    if (old_item_i == -1) {
      added_outputs.append(new_item.key);
    }
    else {
      const nodes::ClosureSignature::Item &old_item = old_signature.outputs[old_item_i];
      if (new_item != old_item) {
        changed_outputs.append(new_item.key);
      }
      if (old_item_i != new_item_i) {
        output_order = true;
      }
    }
  }
  for (const nodes::ClosureSignature::Item &old_item : old_signature.outputs) {
    if (!new_signature.outputs.contains_as(old_item.key)) {
      removed_outputs.append(old_item.key);
    }
  }

  fmt::memory_buffer string_buffer;
  auto buf = fmt::appender(string_buffer);
  if (!added_inputs.is_empty()) {
    fmt::format_to(buf, "\u2022 {}: {}\n", TIP_("Add Inputs"), fmt::join(added_inputs, ", "));
  }
  if (!removed_inputs.is_empty()) {
    fmt::format_to(buf, "\u2022 {}: {}\n", TIP_("Remove Inputs"), fmt::join(removed_inputs, ", "));
  }
  if (!changed_inputs.is_empty()) {
    fmt::format_to(buf, "\u2022 {}: {}\n", TIP_("Change Inputs"), fmt::join(changed_inputs, ", "));
  }
  if (input_order) {
    fmt::format_to(buf, "\u2022 {}\n", TIP_("Reorder Inputs"));
  }
  if (!added_outputs.is_empty()) {
    fmt::format_to(buf, "\u2022 {}: {}\n", TIP_("Add Outputs"), fmt::join(added_outputs, ", "));
  }
  if (!removed_outputs.is_empty()) {
    fmt::format_to(
        buf, "\u2022 {}: {}\n", TIP_("Remove Outputs"), fmt::join(removed_outputs, ", "));
  }
  if (!changed_outputs.is_empty()) {
    fmt::format_to(
        buf, "\u2022 {}: {}\n", TIP_("Change Outputs"), fmt::join(changed_outputs, ", "));
  }
  if (output_order) {
    fmt::format_to(buf, "\u2022 {}\n", TIP_("Reorder Outputs"));
  }
  fmt::format_to(buf, "\n{}", TIP_("Update based on linked closure signature"));

  return fmt::to_string(string_buffer);
}

void sync_node(bContext &C, bNode &node, ReportList *reports)
{
  const bke::bNodeZoneType &closure_zone_type = *bke::zone_type_by_node_type(NODE_CLOSURE_OUTPUT);
  SpaceNode &snode = *CTX_wm_space_node(&C);
  if (node.is_type("NodeEvaluateClosure")) {
    sync_sockets_evaluate_closure(snode, node, reports);
  }
  else if (node.is_type("NodeSeparateBundle")) {
    sync_sockets_separate_bundle(snode, node, reports);
  }
  else if (node.is_type("NodeCombineBundle")) {
    sync_sockets_combine_bundle(snode, node, reports);
  }
  else if (node.is_type("NodeClosureInput")) {
    bNode &closure_input_node = node;
    if (bNode *closure_output_node = closure_zone_type.get_corresponding_output(
            *snode.edittree, closure_input_node))
    {
      sync_sockets_closure(snode, closure_input_node, *closure_output_node, reports);
    }
  }
  else if (node.is_type("NodeClosureOutput")) {
    bNode &closure_output_node = node;
    if (bNode *closure_input_node = closure_zone_type.get_corresponding_input(*snode.edittree,
                                                                              closure_output_node))
    {
      sync_sockets_closure(snode, *closure_input_node, closure_output_node, reports);
    }
  }
}

std::string sync_node_description_get(const bContext &C, const bNode &node)
{
  const SpaceNode *snode = CTX_wm_space_node(&C);
  if (!snode) {
    return "";
  }

  if (node.is_type("NodeSeparateBundle")) {
    const nodes::BundleSignature old_signature = nodes::BundleSignature::from_separate_bundle_node(
        node, true);
    if (const std::optional<nodes::BundleSignature> new_signature =
            get_sync_state_separate_bundle(*snode, node).source_signature)
    {
      return get_bundle_sync_tooltip(old_signature, *new_signature);
    }
  }
  else if (node.is_type("NodeCombineBundle")) {
    const nodes::BundleSignature old_signature = nodes::BundleSignature::from_combine_bundle_node(
        node, true);
    if (const std::optional<nodes::BundleSignature> new_signature =
            get_sync_state_combine_bundle(*snode, node).source_signature)
    {
      return get_bundle_sync_tooltip(old_signature, *new_signature);
    }
  }
  else if (node.is_type("NodeEvaluateClosure")) {
    const nodes::ClosureSignature old_signature =
        nodes::ClosureSignature::from_evaluate_closure_node(node, true);
    if (const std::optional<nodes::ClosureSignature> new_signature =
            get_sync_state_evaluate_closure(*snode, node).source_signature)
    {
      return get_closure_sync_tooltip(old_signature, *new_signature);
    }
  }
  else if (node.is_type("NodeClosureOutput")) {
    const nodes::ClosureSignature old_signature =
        nodes::ClosureSignature::from_closure_output_node(node, true);
    if (const std::optional<nodes::ClosureSignature> new_signature =
            get_sync_state_closure_output(*snode, node).source_signature)
    {
      return get_closure_sync_tooltip(old_signature, *new_signature);
    }
  }
  return "";
}

bool node_can_sync_sockets(const bContext &C, const bNodeTree & /*tree*/, const bNode &node)
{
  SpaceNode *snode = CTX_wm_space_node(&C);
  if (!snode) {
    return false;
  }
  Map<int, bool> &cache = ed::space_node::node_can_sync_cache_get(*snode);
  const bool can_sync = cache.lookup_or_add_cb(node.identifier, [&]() {
    if (node.is_type("NodeEvaluateClosure")) {
      return get_sync_state_evaluate_closure(*snode, node).source_signature.has_value();
    }
    if (node.is_type("NodeClosureOutput")) {
      return get_sync_state_closure_output(*snode, node).source_signature.has_value();
    }
    if (node.is_type("NodeCombineBundle")) {
      return get_sync_state_combine_bundle(*snode, node).source_signature.has_value();
    }
    if (node.is_type("NodeSeparateBundle")) {
      return get_sync_state_separate_bundle(*snode, node).source_signature.has_value();
    }
    return false;
  });
  return can_sync;
}

void node_can_sync_cache_clear(Main &bmain)
{
  if (wmWindowManager *wm = static_cast<wmWindowManager *>(bmain.wm.first)) {
    LISTBASE_FOREACH (wmWindow *, window, &wm->windows) {
      bScreen *screen = BKE_workspace_active_screen_get(window->workspace_hook);
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
        if (sl->spacetype == SPACE_NODE) {
          SpaceNode *snode = reinterpret_cast<SpaceNode *>(sl);
          Map<int, bool> &cache = ed::space_node::node_can_sync_cache_get(*snode);
          cache.clear();
        }
      }
    }
  }
}

}  // namespace blender::nodes

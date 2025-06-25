/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_node_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_enums.h"

#include "WM_api.hh"

#include "BKE_compute_context_cache.hh"
#include "BKE_context.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_report.hh"

#include "ED_node.hh"
#include "ED_screen.hh"

#include "BLI_listbase.h"

#include "NOD_geo_bundle.hh"
#include "NOD_geo_closure.hh"
#include "NOD_socket_items.hh"

#include "node_intern.hh"

namespace blender::ed::space_node {

void sync_sockets_evaluate_closure(SpaceNode &snode,
                                   bNode &evaluate_closure_node,
                                   ReportList *reports)
{
  snode.edittree->ensure_topology_cache();
  bNodeSocket &closure_socket = evaluate_closure_node.input_socket(0);

  bke::ComputeContextCache compute_context_cache;
  const ComputeContext *current_context = ed::space_node::compute_context_for_edittree_socket(
      snode, compute_context_cache, closure_socket);
  const Vector<nodes::ClosureSignature> signatures =
      ed::space_node::gather_linked_origin_closure_signatures(
          current_context, closure_socket, compute_context_cache);
  if (signatures.is_empty()) {
    BKE_report(reports, RPT_INFO, "No closure signature found");
    return;
  }

  bool all_matching = true;
  for (const int i : IndexRange(signatures.size() - 1)) {
    const nodes::ClosureSignature &signature = signatures[i];
    if (!signature.matches_exactly(signatures[i + 1])) {
      all_matching = false;
      break;
    }
  }
  if (!all_matching) {
    BKE_report(reports, RPT_INFO, "Found conflicting closure signatures");
    return;
  }
  const nodes::ClosureSignature &signature = signatures[0];

  auto &storage = *static_cast<NodeGeometryEvaluateClosure *>(evaluate_closure_node.storage);

  Map<std::string, int> old_input_identifiers;
  Map<std::string, int> old_output_identifiers;
  for (const int i : IndexRange(storage.input_items.items_num)) {
    const NodeGeometryEvaluateClosureInputItem &item = storage.input_items.items[i];
    old_input_identifiers.add_new(StringRef(item.name), item.identifier);
  }
  for (const int i : IndexRange(storage.output_items.items_num)) {
    const NodeGeometryEvaluateClosureOutputItem &item = storage.output_items.items[i];
    old_output_identifiers.add_new(StringRef(item.name), item.identifier);
  }

  nodes::socket_items::clear<nodes::EvaluateClosureInputItemsAccessor>(evaluate_closure_node);
  nodes::socket_items::clear<nodes::EvaluateClosureOutputItemsAccessor>(evaluate_closure_node);

  for (const nodes::ClosureSignature::Item &item : signature.inputs) {
    const StringRefNull name = item.key.identifiers()[0];
    NodeGeometryEvaluateClosureInputItem &new_item =
        *nodes::socket_items::add_item_with_socket_type_and_name<
            nodes::EvaluateClosureInputItemsAccessor>(
            evaluate_closure_node, item.type->type, name.c_str());
    if (const std::optional<int> old_identifier = old_input_identifiers.lookup_try(name)) {
      new_item.identifier = *old_identifier;
    }
  }
  for (const nodes::ClosureSignature::Item &item : signature.outputs) {
    const StringRefNull name = item.key.identifiers()[0];
    NodeGeometryEvaluateClosureOutputItem &new_item =
        *nodes::socket_items::add_item_with_socket_type_and_name<
            nodes::EvaluateClosureOutputItemsAccessor>(
            evaluate_closure_node, item.type->type, name.c_str());
    if (const std::optional<int> old_identifier = old_output_identifiers.lookup_try(name)) {
      new_item.identifier = *old_identifier;
    }
  }
  BKE_ntree_update_tag_node_property(snode.edittree, &evaluate_closure_node);
}

void sync_sockets_separate_bundle(SpaceNode &snode,
                                  bNode &separate_bundle_node,
                                  ReportList *reports)
{
  snode.edittree->ensure_topology_cache();
  bNodeSocket &bundle_socket = separate_bundle_node.input_socket(0);

  bke::ComputeContextCache compute_context_cache;
  const ComputeContext *current_context = ed::space_node::compute_context_for_edittree_socket(
      snode, compute_context_cache, bundle_socket);
  const Vector<nodes::BundleSignature> signatures =
      ed::space_node::gather_linked_origin_bundle_signatures(
          current_context, bundle_socket, compute_context_cache);
  if (signatures.is_empty()) {
    BKE_report(reports, RPT_INFO, "No bundle signature found");
    return;
  }

  bool all_matching = true;
  for (const int i : IndexRange(signatures.size() - 1)) {
    const nodes::BundleSignature &signature = signatures[i];
    if (!signature.matches_exactly(signatures[i + 1])) {
      all_matching = false;
      break;
    }
  }
  if (!all_matching) {
    BKE_report(reports, RPT_INFO, "Found conflicting bundle signatures");
    return;
  }
  const nodes::BundleSignature &signature = signatures[0];

  auto &storage = *static_cast<NodeGeometrySeparateBundle *>(separate_bundle_node.storage);

  Map<std::string, int> old_identifiers;
  for (const int i : IndexRange(storage.items_num)) {
    const NodeGeometrySeparateBundleItem &item = storage.items[i];
    old_identifiers.add_new(StringRef(item.name), item.identifier);
  }

  nodes::socket_items::clear<nodes::SeparateBundleItemsAccessor>(separate_bundle_node);
  for (const nodes::BundleSignature::Item &item : signature.items) {
    const StringRefNull name = item.key.identifiers()[0];
    NodeGeometrySeparateBundleItem &new_item =
        *nodes::socket_items::add_item_with_socket_type_and_name<
            nodes ::SeparateBundleItemsAccessor>(
            separate_bundle_node, item.type->type, name.c_str());
    if (const std::optional<int> old_identifier = old_identifiers.lookup_try(name)) {
      new_item.identifier = *old_identifier;
    }
  }
  BKE_ntree_update_tag_node_property(snode.edittree, &separate_bundle_node);
}

void sync_sockets_combine_bundle(SpaceNode &snode, bNode &combine_bundle_node, ReportList *reports)
{
  snode.edittree->ensure_topology_cache();
  bNodeSocket &bundle_socket = combine_bundle_node.output_socket(0);

  bke::ComputeContextCache compute_context_cache;
  const ComputeContext *current_context = ed::space_node::compute_context_for_edittree_socket(
      snode, compute_context_cache, bundle_socket);
  const Vector<nodes::BundleSignature> signatures =
      ed::space_node::gather_linked_target_bundle_signatures(
          current_context, bundle_socket, compute_context_cache);
  if (signatures.is_empty()) {
    BKE_report(reports, RPT_INFO, "No bundle signature found");
    return;
  }

  bool all_matching = true;
  for (const int i : IndexRange(signatures.size() - 1)) {
    const nodes::BundleSignature &signature = signatures[i];
    if (!signature.matches_exactly(signatures[i + 1])) {
      all_matching = false;
      break;
    }
  }
  if (!all_matching) {
    BKE_report(reports, RPT_INFO, "Found conflicting bundle signatures");
    return;
  }
  const nodes::BundleSignature &signature = signatures[0];

  auto &storage = *static_cast<NodeGeometryCombineBundle *>(combine_bundle_node.storage);

  Map<std::string, int> old_identifiers;
  for (const int i : IndexRange(storage.items_num)) {
    const NodeGeometryCombineBundleItem &item = storage.items[i];
    old_identifiers.add_new(StringRef(item.name), item.identifier);
  }

  nodes::socket_items::clear<nodes::CombineBundleItemsAccessor>(combine_bundle_node);
  for (const nodes::BundleSignature::Item &item : signature.items) {
    const StringRefNull name = item.key.identifiers()[0];
    NodeGeometryCombineBundleItem &new_item =
        *nodes::socket_items::add_item_with_socket_type_and_name<
            nodes ::CombineBundleItemsAccessor>(
            combine_bundle_node, item.type->type, name.c_str());
    if (const std::optional<int> old_identifier = old_identifiers.lookup_try(name)) {
      new_item.identifier = *old_identifier;
    }
  }

  BKE_ntree_update_tag_node_property(snode.edittree, &combine_bundle_node);
}

void sync_sockets_closure(SpaceNode &snode,
                          bNode &closure_input_node,
                          bNode &closure_output_node,
                          const bool initialize_internal_links,
                          ReportList *reports)
{
  snode.edittree->ensure_topology_cache();
  bNodeSocket &closure_socket = closure_output_node.output_socket(0);

  bke::ComputeContextCache compute_context_cache;
  const ComputeContext *current_context = ed::space_node::compute_context_for_edittree_socket(
      snode, compute_context_cache, closure_socket);
  const Vector<nodes::ClosureSignature> signatures =
      ed::space_node::gather_linked_target_closure_signatures(
          current_context, closure_socket, compute_context_cache);
  if (signatures.is_empty()) {
    BKE_report(reports, RPT_INFO, "No closure signature found");
    return;
  }

  bool all_matching = true;
  for (const int i : IndexRange(signatures.size() - 1)) {
    const nodes::ClosureSignature &signature = signatures[i];
    if (!signature.matches_exactly(signatures[i + 1])) {
      all_matching = false;
      break;
    }
  }
  if (!all_matching) {
    BKE_report(reports, RPT_INFO, "Found conflicting closure signatures");
    return;
  }
  const nodes::ClosureSignature &signature = signatures[0];

  auto &storage = *static_cast<NodeGeometryClosureOutput *>(closure_output_node.storage);

  Map<std::string, int> old_input_identifiers;
  Map<std::string, int> old_output_identifiers;
  for (const int i : IndexRange(storage.input_items.items_num)) {
    const NodeGeometryClosureInputItem &item = storage.input_items.items[i];
    old_input_identifiers.add_new(StringRef(item.name), item.identifier);
  }
  for (const int i : IndexRange(storage.output_items.items_num)) {
    const NodeGeometryClosureOutputItem &item = storage.output_items.items[i];
    old_output_identifiers.add_new(StringRef(item.name), item.identifier);
  }

  nodes::socket_items::clear<nodes::ClosureInputItemsAccessor>(closure_output_node);
  nodes::socket_items::clear<nodes::ClosureOutputItemsAccessor>(closure_output_node);

  for (const nodes::ClosureSignature::Item &item : signature.inputs) {
    const StringRefNull name = item.key.identifiers()[0];
    NodeGeometryClosureInputItem &new_item =
        *nodes::socket_items::add_item_with_socket_type_and_name<nodes::ClosureInputItemsAccessor>(
            closure_output_node, item.type->type, name.c_str());
    if (item.structure_type) {
      new_item.structure_type = int(*item.structure_type);
    }
    if (const std::optional<int> old_identifier = old_input_identifiers.lookup_try(name)) {
      new_item.identifier = *old_identifier;
    }
  }
  for (const nodes::ClosureSignature::Item &item : signature.outputs) {
    const StringRefNull name = item.key.identifiers()[0];
    NodeGeometryClosureOutputItem &new_item =
        *nodes::socket_items::add_item_with_socket_type_and_name<
            nodes::ClosureOutputItemsAccessor>(closure_output_node, item.type->type, name.c_str());
    if (const std::optional<int> old_identifier = old_output_identifiers.lookup_try(name)) {
      new_item.identifier = *old_identifier;
    }
  }
  BKE_ntree_update_tag_node_property(snode.edittree, &closure_input_node);
  BKE_ntree_update_tag_node_property(snode.edittree, &closure_output_node);

  if (initialize_internal_links) {
    nodes::update_node_declaration_and_sockets(*snode.edittree, closure_input_node);
    nodes::update_node_declaration_and_sockets(*snode.edittree, closure_output_node);

    snode.edittree->ensure_topology_cache();
    Vector<std::pair<bNodeSocket *, bNodeSocket *>> internal_links;
    for (const int input_i : signature.inputs.index_range()) {
      const nodes::ClosureSignature::Item &input_item = signature.inputs[input_i];
      for (const int output_i : signature.outputs.index_range()) {
        const nodes::ClosureSignature::Item &output_item = signature.outputs[output_i];
        if (input_item.key.matches(output_item.key)) {
          internal_links.append({&closure_input_node.output_socket(input_i),
                                 &closure_output_node.input_socket(output_i)});
        }
      };
    }
    for (auto &&[from_socket, to_socket] : internal_links) {
      bke::node_add_link(
          *snode.edittree, closure_input_node, *from_socket, closure_output_node, *to_socket);
    }
  }
}

static wmOperatorStatus sockets_sync_exec(bContext *C, wmOperator *op)
{
  Main &bmain = *CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  if (!snode.edittree) {
    return OPERATOR_CANCELLED;
  }

  bNodeTree &tree = *snode.edittree;
  const bke::bNodeZoneType &closure_zone_type = *bke::zone_type_by_node_type(
      GEO_NODE_CLOSURE_OUTPUT);

  for (bNode *node : tree.all_nodes()) {
    if (!(node->flag & NODE_SELECT)) {
      continue;
    }
    if (node->is_type("GeometryNodeEvaluateClosure")) {
      sync_sockets_evaluate_closure(snode, *node, op->reports);
    }
    else if (node->is_type("GeometryNodeSeparateBundle")) {
      sync_sockets_separate_bundle(snode, *node, op->reports);
    }
    else if (node->is_type("GeometryNodeCombineBundle")) {
      sync_sockets_combine_bundle(snode, *node, op->reports);
    }
    else if (node->is_type("GeometryNodeClosureInput")) {
      bNode &closure_input_node = *node;
      if (bNode *closure_output_node = closure_zone_type.get_corresponding_output(
              tree, closure_input_node))
      {
        sync_sockets_closure(snode, closure_input_node, *closure_output_node, false, op->reports);
      }
    }
    else if (node->is_type("GeometryNodeClosureOutput")) {
      bNode &closure_output_node = *node;
      if (bNode *closure_input_node = closure_zone_type.get_corresponding_input(
              tree, closure_output_node))
      {
        sync_sockets_closure(snode, *closure_input_node, closure_output_node, false, op->reports);
      }
    }
  }
  BKE_main_ensure_invariants(bmain, tree.id);
  return OPERATOR_FINISHED;
}

void NODE_OT_sockets_sync(wmOperatorType *ot)
{
  ot->name = "Sync Sockets";
  ot->idname = "NODE_OT_sockets_sync";
  ot->description = "Update sockets to match what is actually used";

  ot->poll = ED_operator_node_editable;
  ot->exec = sockets_sync_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

}  // namespace blender::ed::space_node

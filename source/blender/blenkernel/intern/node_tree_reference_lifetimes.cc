/* SPDX-FileCopyrightText: 2024 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <iostream>

#include <fmt/format.h>

#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_dot_export.hh"
#include "BKE_node_tree_reference_lifetimes.hh"
#include "BKE_node_tree_zones.hh"

#include "BLI_bit_group_vector.hh"
#include "BLI_bit_span_ops.hh"

#include "NOD_node_declaration.hh"
#include "NOD_socket.hh"

#include "BLI_resource_scope.hh"

namespace blender::bke::node_tree_reference_lifetimes {

using bits::BitInt;
using nodes::NodeDeclaration;
namespace aal = nodes::aal;

std::ostream &operator<<(std::ostream &stream, const ReferenceSetInfo &info)
{
  switch (info.type) {
    case ReferenceSetType::GroupOutputData:
      stream << "Group Output Data: " << info.index;
      break;
    case ReferenceSetType::GroupInputReferenceSet:
      stream << "Group Input Reference: " << info.index;
      break;
    case ReferenceSetType::LocalReferenceSet:
      stream << "Local: " << info.socket->name;
      break;
    case ReferenceSetType::ClosureInputReferenceSet:
      stream << "Closure Input Reference: " << info.socket->name;
      break;
    case ReferenceSetType::ClosureOutputData:
      stream << "Closure Output Data: " << info.socket->name;
      break;
  }
  stream << " (";
  for (const bNodeSocket *socket : info.potential_data_origins) {
    stream << socket->name << ", ";
  }
  stream << ")";
  return stream;
}

static bool socket_may_have_reference(const bNodeSocket &socket)
{
  return socket.may_be_field() || ELEM(socket.type, SOCK_BUNDLE, SOCK_CLOSURE);
}

static bool or_into_each_other_masked(MutableBoundedBitSpan a,
                                      MutableBoundedBitSpan b,
                                      const BoundedBitSpan mask)
{
  if (bits::spans_equal_masked(a, b, mask)) {
    return false;
  }
  bits::inplace_or_masked(a, mask, b);
  bits::inplace_or_masked(b, mask, a);
  return true;
}

static bool or_into_each_other(MutableBoundedBitSpan a, MutableBoundedBitSpan b)
{
  if (bits::spans_equal(a, b)) {
    return false;
  }
  a |= b;
  b |= a;
  return true;
}

bool can_contain_reference(const eNodeSocketDatatype socket_type)
{
  return nodes::socket_type_supports_fields(socket_type) ||
         ELEM(socket_type, SOCK_BUNDLE, SOCK_CLOSURE);
}

bool can_contain_referenced_data(const eNodeSocketDatatype socket_type)
{
  return ELEM(socket_type, SOCK_GEOMETRY, SOCK_BUNDLE, SOCK_CLOSURE);
}

static const bNodeTreeZone *get_zone_of_node_if_full(const bNodeTreeZones *zones,
                                                     const bNode &node)
{
  if (!zones) {
    return nullptr;
  }
  const bNodeTreeZone *zone = zones->get_zone_by_node(node.identifier);
  if (!zone) {
    return nullptr;
  }
  if (!zone->input_node_id || !zone->output_node_id) {
    return nullptr;
  }
  return zone;
}

static Array<const aal::RelationsInNode *> prepare_relations_by_node(const bNodeTree &tree,
                                                                     ResourceScope &scope)
{
  Array<const aal::RelationsInNode *> relations_by_node(tree.all_nodes().size());
  for (const bNode *node : tree.all_nodes()) {
    const aal::RelationsInNode *node_relations = nullptr;
    switch (node->type_legacy) {
      case GEO_NODE_SIMULATION_INPUT:
      case GEO_NODE_SIMULATION_OUTPUT:
      case GEO_NODE_BAKE: {
        /* The relations of these nodes depend on field evaluation to avoid unnecessary
         * relations, but besides that they don't need special handling. */
        aal::RelationsInNode &relations = scope.construct<aal::RelationsInNode>();
        {
          /* Add eval relations. */
          int prev_geometry_index = -1;
          for (const int i : node->input_sockets().index_range()) {
            const bNodeSocket &socket = node->input_socket(i);
            if (socket.type == SOCK_GEOMETRY) {
              prev_geometry_index = i;
              continue;
            }
            if (prev_geometry_index == -1) {
              continue;
            }
            if (socket_may_have_reference(socket)) {
              relations.eval_relations.append({i, prev_geometry_index});
            }
          }
        }
        {
          /* Add available relations. */
          int prev_geometry_index = -1;
          for (const int i : node->output_sockets().index_range()) {
            const bNodeSocket &socket = node->output_socket(i);
            if (socket.type == SOCK_GEOMETRY) {
              prev_geometry_index = i;
            }
            if (prev_geometry_index == -1) {
              continue;
            }
            if (socket_may_have_reference(socket)) {
              relations.available_relations.append({i, prev_geometry_index});
            }
          }
        }
        node_relations = &relations;
        break;
      }
      case GEO_NODE_REPEAT_INPUT:
      case GEO_NODE_REPEAT_OUTPUT: {
        aal::RelationsInNode &relations = scope.construct<aal::RelationsInNode>();
        for (const bNodeSocket *socket : node->output_sockets()) {
          if (can_contain_referenced_data(eNodeSocketDatatype(socket->type))) {
            for (const bNodeSocket *other_output : node->output_sockets()) {
              if (socket_may_have_reference(*other_output)) {
                relations.available_relations.append({other_output->index(), socket->index()});
              }
            }
          }
        }
        for (const bNodeSocket *socket : node->input_sockets()) {
          if (can_contain_referenced_data(eNodeSocketDatatype(socket->type))) {
            for (const bNodeSocket *other_input : node->input_sockets()) {
              if (socket_may_have_reference(*other_input)) {
                relations.eval_relations.append({other_input->index(), socket->index()});
              }
            }
          }
        }
        /* Only create propagate and reference relations for the input node. The output node
         * needs special handling because attributes created inside of the zone are not directly
         * referenced on the outside. */
        if (node->type_legacy == GEO_NODE_REPEAT_INPUT) {
          const int input_items_start = 1;
          const int output_items_start = 1;
          const int items_num = node->output_sockets().size() - 1 - output_items_start;
          for (const int i : IndexRange(items_num)) {
            const int input_index = input_items_start + i;
            const int output_index = output_items_start + i;
            const bNodeSocket &input_socket = node->input_socket(input_index);
            if (can_contain_referenced_data(eNodeSocketDatatype(input_socket.type))) {
              relations.propagate_relations.append({input_index, output_index});
            }
            else if (socket_may_have_reference(input_socket)) {
              relations.reference_relations.append({input_index, output_index});
            }
          }
        }
        node_relations = &relations;
        break;
      }
      case NODE_REROUTE: {
        static const aal::RelationsInNode reroute_relations = []() {
          aal::RelationsInNode relations;
          relations.propagate_relations.append({0, 0});
          relations.reference_relations.append({0, 0});
          return relations;
        }();
        node_relations = &reroute_relations;
        break;
      }
      case NODE_GROUP:
      case NODE_CUSTOM_GROUP: {
        if (const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node->id)) {
          if (group->runtime->reference_lifetimes_info) {
            node_relations = &group->runtime->reference_lifetimes_info->tree_relations;
          }
        }
        break;
      }
      default: {
        if (const NodeDeclaration *node_decl = node->declaration()) {
          node_relations = node_decl->anonymous_attribute_relations();
        }
        break;
      }
    }
    relations_by_node[node->index()] = node_relations;
  }
  return relations_by_node;
}

static Vector<ReferenceSetInfo> find_reference_sets(
    const bNodeTree &tree,
    const Span<const aal::RelationsInNode *> &relations_by_node,
    Vector<int> &r_group_output_reference_sets,
    MultiValueMap<const bNodeTreeZone *, int> &r_output_set_sources_by_closure_zone)
{
  Vector<ReferenceSetInfo> reference_sets;
  const Span<const bNodeTreeInterfaceSocket *> interface_inputs = tree.interface_inputs();
  const Span<const bNodeTreeInterfaceSocket *> interface_outputs = tree.interface_outputs();

  /* Handle references coming from field inputs. */
  for (const int input_i : interface_inputs.index_range()) {
    const bNodeTreeInterfaceSocket &interface_input = *interface_inputs[input_i];
    const bNodeSocketType *stype = interface_input.socket_typeinfo();
    const eNodeSocketDatatype socket_type = stype ? stype->type : SOCK_CUSTOM;
    if (can_contain_reference(socket_type)) {
      reference_sets.append({ReferenceSetType::GroupInputReferenceSet, input_i});
    }
  }
  /* Handle references required by output geometries. */
  for (const int output_i : interface_outputs.index_range()) {
    const bNodeTreeInterfaceSocket &interface_output = *interface_outputs[output_i];
    const bNodeSocketType *stype = interface_output.socket_typeinfo();
    const eNodeSocketDatatype socket_type = stype ? stype->type : SOCK_CUSTOM;
    if (can_contain_referenced_data(socket_type)) {
      r_group_output_reference_sets.append(
          reference_sets.append_and_get_index({ReferenceSetType::GroupOutputData, output_i}));
    }
  }
  /* All references referenced by the sources found so far can exist on all geometry inputs. */
  for (const int input_i : interface_inputs.index_range()) {
    const bNodeTreeInterfaceSocket &interface_input = *interface_inputs[input_i];
    const bNodeSocketType *stype = interface_input.socket_typeinfo();
    const eNodeSocketDatatype socket_type = stype ? stype->type : SOCK_CUSTOM;
    if (can_contain_referenced_data(socket_type)) {
      for (const bNode *node : tree.group_input_nodes()) {
        const bNodeSocket &socket = node->output_socket(input_i);
        for (ReferenceSetInfo &source : reference_sets) {
          source.potential_data_origins.append(&socket);
        }
      }
    }
  }
  /* Handle references created by nodes in the current tree. */
  for (const bNode *node : tree.all_nodes()) {
    if (node->is_muted()) {
      continue;
    }
    if (const aal::RelationsInNode *relations = relations_by_node[node->index()]) {
      for (const aal::AvailableRelation &relation : relations->available_relations) {
        const bNodeSocket &data_socket = node->output_socket(relation.geometry_output);
        const bNodeSocket &reference_socket = node->output_socket(relation.field_output);
        if (!reference_socket.is_available() || !reference_socket.is_available()) {
          continue;
        }
        if (!reference_socket.is_directly_linked() || !data_socket.is_directly_linked()) {
          continue;
        }
        reference_sets.append({ReferenceSetType::LocalReferenceSet, &reference_socket});
        reference_sets.last().potential_data_origins.append(&data_socket);
      }
    }
  }
  /* Each output of the Evaluate Closure node may reference data in any other output. We can't know
   * exactly what references what here. */
  for (const bNode *node : tree.nodes_by_type("NodeEvaluateClosure")) {
    const auto &storage = *static_cast<NodeEvaluateClosure *>(node->storage);
    Vector<const bNodeSocket *> reference_outputs;
    for (const int i : IndexRange(storage.output_items.items_num)) {
      const NodeEvaluateClosureOutputItem &item = storage.output_items.items[i];
      if (can_contain_referenced_data(eNodeSocketDatatype(item.socket_type))) {
        reference_outputs.append(&node->output_socket(i));
      }
    }
    if (!reference_outputs.is_empty()) {
      for (const int i : IndexRange(storage.output_items.items_num)) {
        const NodeEvaluateClosureOutputItem &item = storage.output_items.items[i];
        if (can_contain_reference(eNodeSocketDatatype(item.socket_type))) {
          reference_sets.append({ReferenceSetType::LocalReferenceSet, &node->output_socket(i)});
          reference_sets.last().potential_data_origins.extend(reference_outputs);
        }
      }
    }
  }

  const bNodeTreeZones *zones = tree.zones();
  if (!zones) {
    return reference_sets;
  }
  for (const bNodeTreeZone *zone : zones->zones) {
    const bNode &input_node = *zone->input_node();
    const bNode &output_node = *zone->output_node();
    if (output_node.type_legacy != NODE_CLOSURE_OUTPUT) {
      continue;
    }
    const auto &storage = *static_cast<const NodeClosureOutput *>(output_node.storage);
    const int old_reference_sets_count = reference_sets.size();
    /* Handle references coming from field inputs in the closure. */
    for (const int input_i : IndexRange(storage.input_items.items_num)) {
      const bNodeSocket &socket = input_node.output_socket(input_i);
      if (can_contain_reference(eNodeSocketDatatype(socket.type))) {
        reference_sets.append({ReferenceSetType::ClosureInputReferenceSet, &socket});
      }
    }
    /* Handle references required by output geometries in the closure. */
    for (const int output_i : IndexRange(storage.output_items.items_num)) {
      const bNodeSocket &socket = output_node.input_socket(output_i);
      if (can_contain_referenced_data(eNodeSocketDatatype(socket.type))) {
        r_output_set_sources_by_closure_zone.add(
            zone,
            reference_sets.append_and_get_index({ReferenceSetType::ClosureOutputData, &socket}));
      }
    }
    /* All references referenced passed into this zone may exist on the geometry inputs. */
    MutableSpan<ReferenceSetInfo> new_reference_sets = reference_sets.as_mutable_span().drop_front(
        old_reference_sets_count);
    for (const int input_i : IndexRange(storage.input_items.items_num)) {
      const bNodeSocket &socket = input_node.output_socket(input_i);
      if (can_contain_referenced_data(eNodeSocketDatatype(socket.type))) {
        for (ReferenceSetInfo &source : new_reference_sets) {
          source.potential_data_origins.append(&socket);
        }
      }
    }
  }
  return reference_sets;
}

static void set_initial_data_and_reference_bits(const bNodeTree &tree,
                                                const Span<ReferenceSetInfo> reference_sets,
                                                BitGroupVector<> &r_potential_data_by_socket,
                                                BitGroupVector<> &r_potential_reference_by_socket)
{
  for (const int reference_set_i : reference_sets.index_range()) {
    const ReferenceSetInfo &reference_set = reference_sets[reference_set_i];
    for (const bNodeSocket *socket : reference_set.potential_data_origins) {
      r_potential_data_by_socket[socket->index_in_tree()][reference_set_i].set();
    }
    switch (reference_set.type) {
      case ReferenceSetType::LocalReferenceSet:
      case ReferenceSetType::ClosureInputReferenceSet: {
        r_potential_reference_by_socket[reference_set.socket->index_in_tree()][reference_set_i]
            .set();
        break;
      }
      case ReferenceSetType::GroupInputReferenceSet: {
        for (const bNode *node : tree.group_input_nodes()) {
          const bNodeSocket &socket = node->output_socket(reference_set.index);
          r_potential_reference_by_socket[socket.index_in_tree()][reference_set_i].set();
        }
        break;
      }
      case ReferenceSetType::GroupOutputData:
      case ReferenceSetType::ClosureOutputData: {
        /* Nothing to do. */
        break;
      }
    }
  }
}

static BitVector<> get_references_coming_from_outside_zone(
    const bNodeTreeZone &zone, const Span<const BitGroupVector<> *> sources)
{
  BitVector<> found(sources.first()->group_size(), false);
  /* Gather references that are passed into the zone from the outside, either through the input
   * node or border links. */
  for (const bNodeSocket *socket : zone.input_node()->input_sockets()) {
    const int src = socket->index_in_tree();
    for (const BitGroupVector<> *source : sources) {
      found |= (*source)[src];
    }
  }
  for (const bNodeLink *link : zone.border_links) {
    const int src = link->fromsock->index_in_tree();
    for (const BitGroupVector<> *source : sources) {
      found |= (*source)[src];
    }
  }
  return found;
}

/**
 * \return True when propagation needs to be done again.
 */
static bool pass_left_to_right(const bNodeTree &tree,
                               const Span<const aal::RelationsInNode *> &relations_by_node,
                               BitGroupVector<> &r_potential_data_by_socket,
                               BitGroupVector<> &r_potential_reference_by_socket)
{
  bool needs_extra_pass = false;
  const bNodeTreeZones *zones = tree.zones();
  for (const bNode *node : tree.toposort_left_to_right()) {
    for (const bNodeSocket *socket : node->input_sockets()) {
      if (!socket->is_available()) {
        continue;
      }
      const int dst_index = socket->index_in_tree();
      for (const bNodeLink *link : socket->directly_linked_links()) {
        if (!link->is_used()) {
          continue;
        }
        const int src_index = link->fromsock->index_in_tree();
        r_potential_data_by_socket[dst_index] |= r_potential_data_by_socket[src_index];
        r_potential_reference_by_socket[dst_index] |= r_potential_reference_by_socket[src_index];
      }
    }
    if (node->is_muted()) {
      for (const bNodeLink &link : node->internal_links()) {
        const bNodeSocket &input_socket = *link.fromsock;
        const bNodeSocket &output_socket = *link.tosock;
        if (!input_socket.is_available() || !output_socket.is_available()) {
          continue;
        }
        const int src_index = input_socket.index_in_tree();
        const int dst_index = output_socket.index_in_tree();
        r_potential_data_by_socket[dst_index] |= r_potential_data_by_socket[src_index];
        r_potential_reference_by_socket[dst_index] |= r_potential_reference_by_socket[src_index];
      }
      continue;
    }
    if (const aal::RelationsInNode *relations = relations_by_node[node->index()]) {
      /* Propagate references. */
      for (const aal::ReferenceRelation &relation : relations->reference_relations) {
        const bNodeSocket &from_socket = node->input_socket(relation.from_field_input);
        const bNodeSocket &to_socket = node->output_socket(relation.to_field_output);
        if (!from_socket.is_available() || !to_socket.is_available()) {
          continue;
        }
        const int src_index = from_socket.index_in_tree();
        const int dst_index = to_socket.index_in_tree();
        r_potential_reference_by_socket[dst_index] |= r_potential_reference_by_socket[src_index];
      }
      /* Propagate data. */
      for (const aal::PropagateRelation &relation : relations->propagate_relations) {
        const bNodeSocket &from_socket = node->input_socket(relation.from_geometry_input);
        const bNodeSocket &to_socket = node->output_socket(relation.to_geometry_output);
        if (!from_socket.is_available() || !to_socket.is_available()) {
          continue;
        }
        const int src_index = from_socket.index_in_tree();
        const int dst_index = to_socket.index_in_tree();
        r_potential_data_by_socket[dst_index] |= r_potential_data_by_socket[src_index];
      }
    }
    switch (node->type_legacy) {
      /* This zone needs additional special handling because attributes from the input geometry
       * are propagated to the output node. */
      case GEO_NODE_FOREACH_GEOMETRY_ELEMENT_OUTPUT: {
        const bNodeTreeZone *zone = get_zone_of_node_if_full(zones, *node);
        if (!zone) {
          break;
        }
        const bNode *input_node = zone->input_node();
        const bNode *output_node = node;
        const auto *storage = static_cast<const NodeGeometryForeachGeometryElementOutput *>(
            node->storage);
        const int src_index = input_node->input_socket(0).index_in_tree();
        for (const bNodeSocket *output_socket : output_node->output_sockets()) {
          if (output_socket->type == SOCK_GEOMETRY) {
            const int dst_index = output_socket->index_in_tree();
            r_potential_data_by_socket[dst_index] |= r_potential_data_by_socket[src_index];
          }
        }

        /* Propagate references from the inside to the outside. Like in the repeat zone, new
         * references created in the zone stay local inside the zone and are not propagated to the
         * outside. Instead, the foreach-element output node creates new references. */
        const BitVector<> outside_references = get_references_coming_from_outside_zone(
            *zone, {&r_potential_data_by_socket, &r_potential_reference_by_socket});
        for (const int item_i : IndexRange(storage->generation_items.items_num)) {
          const int src_index =
              node->input_socket(storage->main_items.items_num + item_i).index_in_tree();
          const int dst_index =
              node->output_socket(1 + storage->main_items.items_num + item_i).index_in_tree();
          bits::inplace_or_masked(r_potential_data_by_socket[dst_index],
                                  outside_references,
                                  r_potential_data_by_socket[src_index]);
          bits::inplace_or_masked(r_potential_reference_by_socket[dst_index],
                                  outside_references,
                                  r_potential_reference_by_socket[src_index]);
        }
        break;
      }
      case NODE_CLOSURE_INPUT: {
        const bNodeTreeZone *zone = get_zone_of_node_if_full(zones, *node);
        if (!zone) {
          break;
        }
        const bNode &input_node = *zone->input_node();
        /* Data referenced by border links may also be passed into the closure as input. */
        const BitVector<> outside_references = get_references_coming_from_outside_zone(
            *zone, {&r_potential_data_by_socket, &r_potential_reference_by_socket});
        for (const int i : node->output_sockets().index_range()) {
          const int dst_index = input_node.output_socket(i).index_in_tree();
          r_potential_data_by_socket[dst_index] |= outside_references;
          r_potential_reference_by_socket[dst_index] |= outside_references;
        }
        break;
      }
      case NODE_CLOSURE_OUTPUT: {
        const bNodeTreeZone *zone = get_zone_of_node_if_full(zones, *node);
        if (!zone) {
          break;
        }
        const bNode &output_node = *zone->output_node();
        /* References passed through border links are referenced by the closure. */
        const BitVector<> passed_in_references = get_references_coming_from_outside_zone(
            *zone, {&r_potential_reference_by_socket});
        const BitVector<> passed_in_data = get_references_coming_from_outside_zone(
            *zone, {&r_potential_data_by_socket});
        const int dst_index = output_node.output_socket(0).index_in_tree();
        for ([[maybe_unused]] const int i : node->input_sockets().index_range()) {
          r_potential_data_by_socket[dst_index] |= passed_in_data;
          r_potential_reference_by_socket[dst_index] |= passed_in_references;
        }
        break;
      }
      case NODE_EVALUATE_CLOSURE: {
        BitVector<> potential_input_references(r_potential_reference_by_socket.group_size());
        BitVector<> potential_input_data(r_potential_data_by_socket.group_size());
        /* Gather all references and data from all inputs, including the once on the closure input.
         * The output may reference any of those. */
        for (const bNodeSocket *socket : node->input_sockets()) {
          const int src_index = socket->index_in_tree();
          potential_input_references |= r_potential_reference_by_socket[src_index];
          potential_input_data |= r_potential_data_by_socket[src_index];
        }
        for (const bNodeSocket *out_socket : node->output_sockets()) {
          const int dst_index = out_socket->index_in_tree();
          r_potential_reference_by_socket[dst_index] |= potential_input_references;
          r_potential_data_by_socket[dst_index] |= potential_input_data;
        }
        break;
      }
      case GEO_NODE_REPEAT_OUTPUT: {
        const bNodeTreeZone *zone = get_zone_of_node_if_full(zones, *node);
        if (!zone) {
          break;
        }
        const bNode &input_node = *zone->input_node();
        const bNode &output_node = *zone->output_node();
        const int items_num = output_node.output_sockets().size() - 1;

        /* Handle data propagation in the case when the iteration count is zero. */
        for (const int i : IndexRange(items_num)) {
          const int src_index = input_node.input_socket(i + 1).index_in_tree();
          const int dst_index = output_node.output_socket(i).index_in_tree();
          r_potential_data_by_socket[dst_index] |= r_potential_data_by_socket[src_index];
          r_potential_reference_by_socket[dst_index] |= r_potential_reference_by_socket[src_index];
        }

        const BitVector<> outside_references = get_references_coming_from_outside_zone(
            *zone, {&r_potential_data_by_socket, &r_potential_reference_by_socket});

        /* Propagate within output node. */
        for (const int i : IndexRange(items_num)) {
          const int src_index = output_node.input_socket(i).index_in_tree();
          const int dst_index = output_node.output_socket(i).index_in_tree();
          bits::inplace_or_masked(r_potential_data_by_socket[dst_index],
                                  outside_references,
                                  r_potential_data_by_socket[src_index]);
          bits::inplace_or_masked(r_potential_reference_by_socket[dst_index],
                                  outside_references,
                                  r_potential_reference_by_socket[src_index]);
        }

        /* Ensure that references and data on the input and output node are equal. Since this may
         * propagate information backwards, an additional pass can be necessary. */
        for (const int i : IndexRange(items_num)) {
          const bNodeSocket &body_input_socket = input_node.output_socket(i + 1);
          const bNodeSocket &body_output_socket = output_node.input_socket(i);
          const int in_index = body_output_socket.index_in_tree();
          const int out_index = body_input_socket.index_in_tree();
          needs_extra_pass |= or_into_each_other_masked(r_potential_data_by_socket[in_index],
                                                        r_potential_data_by_socket[out_index],
                                                        outside_references);
          needs_extra_pass |= or_into_each_other_masked(r_potential_reference_by_socket[in_index],
                                                        r_potential_reference_by_socket[out_index],
                                                        outside_references);
        }
        break;
      }
    }
  }
  return needs_extra_pass;
}

static void prepare_required_data_for_group_outputs(
    const bNodeTree &tree,
    const Span<ReferenceSetInfo> reference_sets,
    const Span<int> group_output_set_sources,
    const BitGroupVector<> &potential_data_by_socket,
    const BitGroupVector<> &potential_reference_by_socket,
    BitGroupVector<> &r_required_data_by_socket)
{
  if (const bNode *group_output_node = tree.group_output_node()) {
    const Span<const bNodeSocket *> sockets = group_output_node->input_sockets().drop_back(1);
    for (const int reference_set_i : group_output_set_sources) {
      const ReferenceSetInfo &reference_set = reference_sets[reference_set_i];
      BLI_assert(reference_set.type == ReferenceSetType::GroupOutputData);
      const int index = sockets[reference_set.index]->index_in_tree();
      r_required_data_by_socket[index][reference_set_i].set();
    }
    BitVector<> potential_output_references(reference_sets.size(), false);
    for (const bNodeSocket *socket : sockets) {
      potential_output_references |= potential_reference_by_socket[socket->index_in_tree()];
    }
    for (const bNodeSocket *socket : sockets) {
      if (!can_contain_referenced_data(eNodeSocketDatatype(socket->type))) {
        continue;
      }
      const int index = socket->index_in_tree();
      r_required_data_by_socket[index] |= potential_output_references;
      /* Make sure that only available data is also required. This is enforced in the end anyway,
       * but may reduce some unnecessary work. */
      r_required_data_by_socket[index] &= potential_data_by_socket[index];
    }
  }
}

static void prepare_required_data_for_closure_outputs(
    const bNodeTree &tree,
    const Span<ReferenceSetInfo> reference_sets,
    MultiValueMap<const bNodeTreeZone *, int> &output_set_sources_by_closure_zone,
    const BitGroupVector<> &potential_data_by_socket,
    const BitGroupVector<> &potential_reference_by_socket,
    BitGroupVector<> &r_required_data_by_socket)
{
  const bNodeTreeZones *zones = tree.zones();
  if (!zones) {
    return;
  }
  for (const bNodeTreeZone *zone : zones->zones) {
    if (!zone->input_node_id || !zone->output_node_id) {
      continue;
    }
    const bNode &output_node = *zone->output_node();
    if (output_node.type_legacy != NODE_CLOSURE_OUTPUT) {
      continue;
    }
    const Span<int> closure_output_set_sources = output_set_sources_by_closure_zone.lookup(zone);
    for (const int reference_set_i : closure_output_set_sources) {
      const ReferenceSetInfo &reference_set = reference_sets[reference_set_i];
      BLI_assert(reference_set.type == ReferenceSetType::ClosureOutputData);
      r_required_data_by_socket[reference_set.socket->index_in_tree()][reference_set_i].set();
    }
    BitVector<> potential_output_references(reference_sets.size(), false);
    const Span<const bNodeSocket *> sockets = output_node.input_sockets().drop_back(1);
    for (const bNodeSocket *socket : sockets) {
      potential_output_references |= potential_reference_by_socket[socket->index_in_tree()];
    }
    for (const bNodeSocket *socket : sockets) {
      if (!can_contain_referenced_data(eNodeSocketDatatype(socket->type))) {
        continue;
      }
      const int index = socket->index_in_tree();
      r_required_data_by_socket[index] |= potential_output_references;
      /* Make sure that only available data is also required. This is enforced in the end anyway,
       * but may reduce some unnecessary work. */
      r_required_data_by_socket[index] &= potential_data_by_socket[index];
    }
  }
}

static void prepare_required_data_for_outputs(
    const bNodeTree &tree,
    const Span<ReferenceSetInfo> reference_sets,
    const Span<int> group_output_set_sources,
    MultiValueMap<const bNodeTreeZone *, int> &output_set_sources_by_closure_zone,
    const BitGroupVector<> &potential_data_by_socket,
    const BitGroupVector<> &potential_reference_by_socket,
    BitGroupVector<> &r_required_data_by_socket)
{
  prepare_required_data_for_group_outputs(tree,
                                          reference_sets,
                                          group_output_set_sources,
                                          potential_data_by_socket,
                                          potential_reference_by_socket,
                                          r_required_data_by_socket);
  prepare_required_data_for_closure_outputs(tree,
                                            reference_sets,
                                            output_set_sources_by_closure_zone,
                                            potential_data_by_socket,
                                            potential_reference_by_socket,
                                            r_required_data_by_socket);
}

static bool pass_right_to_left(const bNodeTree &tree,
                               const Span<const aal::RelationsInNode *> &relations_by_node,
                               const BitGroupVector<> &potential_reference_by_socket,
                               BitGroupVector<> &r_required_data_by_socket)
{
  bool needs_extra_pass = false;
  const bNodeTreeZones *zones = tree.zones();
  for (const bNode *node : tree.toposort_right_to_left()) {
    for (const bNodeSocket *socket : node->output_sockets()) {
      if (!socket->is_available()) {
        continue;
      }
      const int dst_index = socket->index_in_tree();
      for (const bNodeLink *link : socket->directly_linked_links()) {
        if (!link->is_used()) {
          continue;
        }
        const int src_index = link->tosock->index_in_tree();
        r_required_data_by_socket[dst_index] |= r_required_data_by_socket[src_index];
      }
    }
    if (node->is_muted()) {
      for (const bNodeLink &link : node->internal_links()) {
        const bNodeSocket &input_socket = *link.fromsock;
        const bNodeSocket &output_socket = *link.tosock;
        if (!input_socket.is_available() || !output_socket.is_available()) {
          continue;
        }
        const int dst_index = input_socket.index_in_tree();
        const int src_index = output_socket.index_in_tree();
        r_required_data_by_socket[dst_index] |= r_required_data_by_socket[src_index];
      }
      continue;
    }
    if (const aal::RelationsInNode *relations = relations_by_node[node->index()]) {
      for (const aal::PropagateRelation &relation : relations->propagate_relations) {
        const bNodeSocket &output_socket = node->output_socket(relation.to_geometry_output);
        const bNodeSocket &input_socket = node->input_socket(relation.from_geometry_input);
        if (!input_socket.is_available() || !output_socket.is_available()) {
          continue;
        }
        const int dst_index = input_socket.index_in_tree();
        const int src_index = output_socket.index_in_tree();
        r_required_data_by_socket[dst_index] |= r_required_data_by_socket[src_index];
      }
      for (const aal::EvalRelation &relation : relations->eval_relations) {
        const bNodeSocket &data_socket = node->input_socket(relation.geometry_input);
        const bNodeSocket &reference_socket = node->input_socket(relation.field_input);
        if (!data_socket.is_available() || !reference_socket.is_available()) {
          continue;
        }
        r_required_data_by_socket[data_socket.index_in_tree()] |=
            potential_reference_by_socket[reference_socket.index_in_tree()];
      }
    }

    switch (node->type_legacy) {
      /* Propagate from the geometry outputs to the geometry input. */
      case GEO_NODE_FOREACH_GEOMETRY_ELEMENT_INPUT: {
        const bNodeTreeZone *zone = get_zone_of_node_if_full(zones, *node);
        if (!zone) {
          break;
        }
        const bNode *input_node = node;
        const bNode *output_node = zone->output_node();
        const int dst_index = input_node->input_socket(0).index_in_tree();
        for (const bNodeSocket *output_socket : output_node->output_sockets()) {
          if (output_socket->type == SOCK_GEOMETRY) {
            const int src_index = output_socket->index_in_tree();
            r_required_data_by_socket[dst_index] |= r_required_data_by_socket[src_index];
          }
        }
        break;
      }
      case GEO_NODE_FOREACH_GEOMETRY_ELEMENT_OUTPUT: {
        const bNode *output_node = node;
        const auto *storage = static_cast<NodeGeometryForeachGeometryElementOutput *>(
            output_node->storage);

        for (const int item_i : IndexRange(storage->generation_items.items_num)) {
          const int src_index =
              node->output_socket(1 + storage->main_items.items_num + item_i).index_in_tree();
          const int dst_index =
              node->input_socket(storage->main_items.items_num + item_i).index_in_tree();
          r_required_data_by_socket[dst_index] |= r_required_data_by_socket[src_index];
        }
        break;
      }
      case GEO_NODE_REPEAT_OUTPUT: {
        const bNodeTreeZone *zone = get_zone_of_node_if_full(zones, *node);
        if (!zone) {
          break;
        }
        /* Propagate within output node. */
        const int items_num = node->output_sockets().size() - 1;
        for (const int i : IndexRange(items_num)) {
          const int src_index = node->output_socket(i).index_in_tree();
          const int dst_index = node->input_socket(i).index_in_tree();
          r_required_data_by_socket[dst_index] |= r_required_data_by_socket[src_index];
        }
        break;
      }
      case GEO_NODE_REPEAT_INPUT: {
        const bNodeTreeZone *zone = get_zone_of_node_if_full(zones, *node);
        if (!zone) {
          break;
        }
        const bNode *input_node = node;
        const bNode *output_node = zone->output_node();
        const int items_num = output_node->output_sockets().size() - 1;
        for (const int i : IndexRange(items_num)) {
          const bNodeSocket &body_input_socket = input_node->output_socket(i + 1);
          const bNodeSocket &body_output_socket = output_node->input_socket(i);
          const int in_index = body_input_socket.index_in_tree();
          const int out_index = body_output_socket.index_in_tree();
          needs_extra_pass |= or_into_each_other(r_required_data_by_socket[in_index],
                                                 r_required_data_by_socket[out_index]);
        }
        break;
      }
      case NODE_EVALUATE_CLOSURE: {
        /* Data referenced by the closure is required on all the other inputs. */
        const bNodeSocket &closure_socket = node->input_socket(0);
        BitVector<> required_data_on_inputs =
            potential_reference_by_socket[closure_socket.index_in_tree()];
        /* Data required on outputs is also required on inputs. */
        for (const bNodeSocket *socket : node->output_sockets()) {
          required_data_on_inputs |= r_required_data_by_socket[socket->index_in_tree()];
        }
        /* References available on inputs are also required on the data inputs because they may be
         * used by the closure. */
        for (const bNodeSocket *socket : node->input_sockets()) {
          if (can_contain_reference(eNodeSocketDatatype(socket->type))) {
            required_data_on_inputs |= potential_reference_by_socket[socket->index_in_tree()];
          }
        }
        for (const bNodeSocket *socket : node->input_sockets()) {
          const int dst_index = socket->index_in_tree();
          r_required_data_by_socket[dst_index] |= required_data_on_inputs;
        }
        break;
      }
      case NODE_CLOSURE_OUTPUT: {
        const bNodeTreeZone *zone = get_zone_of_node_if_full(zones, *node);
        if (!zone) {
          break;
        }
        /* Data that's required on the closure is also required on all inputs of the closure. */
        const bNodeSocket &output_socket = node->output_socket(0);
        const BoundedBitSpan required_data =
            r_required_data_by_socket[output_socket.index_in_tree()];
        for (const bNodeSocket *input_socket : node->input_sockets()) {
          r_required_data_by_socket[input_socket->index_in_tree()] |= required_data;
        }
        break;
      }
    }
  }
  return needs_extra_pass;
}

class bNodeTreeBitGroupVectorOptions : public bNodeTreeToDotOptions {
 private:
  Vector<BitGroupVector<>> bit_groups_;

 public:
  bNodeTreeBitGroupVectorOptions(const Span<BitGroupVector<>> &bit_groups)
      : bit_groups_(bit_groups)
  {
  }

  std::string socket_name(const bNodeSocket &socket) const override
  {
    Vector<std::string> extra_data;
    for (const BitGroupVector<> &bit_group : bit_groups_) {
      const BoundedBitSpan bits = bit_group[socket.index_in_tree()];
      Vector<int> indices;
      bits::foreach_1_index(bits, [&](const int i) { indices.append(i); });
      extra_data.append(fmt::format("({})", fmt::join(indices, ",")));
    }
    return fmt::format("{} {}", socket.name, fmt::join(extra_data, " "));
  }
};

static aal::RelationsInNode get_tree_relations(
    const bNodeTree &tree,
    const Span<ReferenceSetInfo> reference_sets,
    const BitGroupVector<> &potential_data_by_socket,
    const BitGroupVector<> &potential_reference_by_socket,
    const BitGroupVector<> &required_data_by_socket)
{
  aal::RelationsInNode tree_relations;
  const bNode *group_output_node = tree.group_output_node();

  for (const int input_i : tree.interface_inputs().index_range()) {
    const bNodeTreeInterfaceSocket &interface_input = *tree.interface_inputs()[input_i];
    const eNodeSocketDatatype socket_type = interface_input.socket_typeinfo()->type;
    if (can_contain_referenced_data(socket_type)) {
      BitVector<> required_data(required_data_by_socket.group_size(), false);
      for (const bNode *input_node : tree.group_input_nodes()) {
        required_data |=
            required_data_by_socket[input_node->output_socket(input_i).index_in_tree()];
      }
      bits::foreach_1_index(required_data, [&](const int reference_set_i) {
        const ReferenceSetInfo &reference_set = reference_sets[reference_set_i];
        switch (reference_set.type) {
          case ReferenceSetType::GroupOutputData: {
            tree_relations.propagate_relations.append_non_duplicates(
                {input_i, reference_set.index});
            break;
          }
          case ReferenceSetType::GroupInputReferenceSet: {
            tree_relations.eval_relations.append_non_duplicates({reference_set.index, input_i});
            break;
          }
          default:
            break;
        }
      });
    }
  }
  if (group_output_node) {
    for (const int output_i : tree.interface_outputs().index_range()) {
      const bNodeSocket &socket = group_output_node->input_socket(output_i);
      const eNodeSocketDatatype socket_type = eNodeSocketDatatype(socket.type);
      if (can_contain_reference(socket_type)) {
        const BoundedBitSpan potential_references =
            potential_reference_by_socket[socket.index_in_tree()];
        bits::foreach_1_index(potential_references, [&](const int reference_set_i) {
          const ReferenceSetInfo &reference_set = reference_sets[reference_set_i];
          switch (reference_set.type) {
            case ReferenceSetType::GroupInputReferenceSet: {
              tree_relations.reference_relations.append_non_duplicates(
                  {reference_set.index, output_i});
              break;
            }
            default: {
              break;
            }
          }
        });
      }
      if (can_contain_referenced_data(socket_type)) {
        const BoundedBitSpan potential_data = potential_data_by_socket[socket.index_in_tree()];
        bits::foreach_1_index(potential_data, [&](const int reference_set_i) {
          const ReferenceSetInfo &reference_set = reference_sets[reference_set_i];
          switch (reference_set.type) {
            case ReferenceSetType::LocalReferenceSet: {
              for (const bNodeSocket *other_socket :
                   group_output_node->input_sockets().drop_back(1))
              {
                if (!can_contain_reference(eNodeSocketDatatype(other_socket->type))) {
                  continue;
                }
                const BoundedBitSpan potential_references =
                    potential_reference_by_socket[other_socket->index_in_tree()];
                if (potential_references[reference_set_i].test()) {
                  tree_relations.available_relations.append_non_duplicates(
                      {other_socket->index(), output_i});
                }
              }
              break;
            }
            default: {
              break;
            }
          }
        });
      }
    }
  }

  return tree_relations;
}

static std::unique_ptr<ReferenceLifetimesInfo> make_reference_lifetimes_info(const bNodeTree &tree)
{
  tree.ensure_topology_cache();
  tree.ensure_interface_cache();
  if (tree.has_available_link_cycle()) {
    return {};
  }
  const bNodeTreeZones *zones = tree.zones();
  if (zones == nullptr) {
    return {};
  }

  std::unique_ptr<ReferenceLifetimesInfo> reference_lifetimes_info =
      std::make_unique<ReferenceLifetimesInfo>();

  ResourceScope scope;
  Array<const aal::RelationsInNode *> relations_by_node = prepare_relations_by_node(tree, scope);

  Vector<int> group_output_set_sources;
  MultiValueMap<const bNodeTreeZone *, int> output_set_sources_by_closure_zone;
  reference_lifetimes_info->reference_sets = find_reference_sets(
      tree, relations_by_node, group_output_set_sources, output_set_sources_by_closure_zone);
  const Span<ReferenceSetInfo> reference_sets = reference_lifetimes_info->reference_sets;

  const int sockets_num = tree.all_sockets().size();
  const int reference_sets_num = reference_sets.size();

  BitGroupVector<> potential_data_by_socket(sockets_num, reference_sets_num, false);
  BitGroupVector<> potential_reference_by_socket(sockets_num, reference_sets_num, false);
  set_initial_data_and_reference_bits(
      tree, reference_sets, potential_data_by_socket, potential_reference_by_socket);

  /* Propagate data and reference from left to right. This may need to be done multiple times
   * because there may be some back-links. */
  while (pass_left_to_right(
      tree, relations_by_node, potential_data_by_socket, potential_reference_by_socket))
  {
  }

  BitGroupVector<> required_data_by_socket(sockets_num, reference_sets_num, false);
  prepare_required_data_for_outputs(tree,
                                    reference_sets,
                                    group_output_set_sources,
                                    output_set_sources_by_closure_zone,
                                    potential_data_by_socket,
                                    potential_reference_by_socket,
                                    required_data_by_socket);

  while (pass_right_to_left(
      tree, relations_by_node, potential_reference_by_socket, required_data_by_socket))
  {
  }

  /* Make sure that all required data is also potentially available. */
  required_data_by_socket.all_bits() &= potential_data_by_socket.all_bits();

/* Only useful when debugging the reference lifetimes analysis. */
#if 0
  std::cout << "\n\n"
            << node_tree_to_dot(tree,
                                bNodeTreeBitGroupVectorOptions({potential_data_by_socket,
                                                                potential_reference_by_socket,
                                                                required_data_by_socket}))

            << "\n\n";
#endif

  reference_lifetimes_info->tree_relations = get_tree_relations(tree,
                                                                reference_sets,
                                                                potential_data_by_socket,
                                                                potential_reference_by_socket,
                                                                required_data_by_socket);
  reference_lifetimes_info->required_data_by_socket = std::move(required_data_by_socket);
  return reference_lifetimes_info;
}

bool analyse_reference_lifetimes(bNodeTree &tree)
{
  std::unique_ptr<ReferenceLifetimesInfo> reference_lifetimes_info = make_reference_lifetimes_info(
      tree);
  std::unique_ptr<ReferenceLifetimesInfo> &stored_reference_lifetimes_info =
      tree.runtime->reference_lifetimes_info;
  if (!reference_lifetimes_info) {
    if (!tree.runtime->reference_lifetimes_info) {
      return false;
    }
    stored_reference_lifetimes_info.reset();
    return true;
  }

  const bool interface_changed = stored_reference_lifetimes_info &&
                                 stored_reference_lifetimes_info->tree_relations !=
                                     reference_lifetimes_info->tree_relations;
  stored_reference_lifetimes_info = std::move(reference_lifetimes_info);
  return interface_changed;
}

}  // namespace blender::bke::node_tree_reference_lifetimes

/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_zones.hh"

#include "BLI_bit_group_vector.hh"
#include "BLI_bit_span_ops.hh"
#include "BLI_task.hh"
#include "BLI_timeit.hh"

namespace blender::bke::node_tree_zones {

static void update_zone_depths(TreeZone &zone)
{
  if (zone.depth >= 0) {
    return;
  }
  if (zone.parent_zone == nullptr) {
    zone.depth = 0;
    return;
  }
  update_zone_depths(*zone.parent_zone);
  zone.depth = zone.parent_zone->depth + 1;
}

static Vector<std::unique_ptr<TreeZone>> find_zone_nodes(
    const bNodeTree &tree, TreeZones &owner, Map<const bNode *, TreeZone *> &r_zone_by_inout_node)
{
  Vector<std::unique_ptr<TreeZone>> zones;
  for (const bNode *node : tree.nodes_by_type("GeometryNodeSimulationOutput")) {
    auto zone = std::make_unique<TreeZone>();
    zone->owner = &owner;
    zone->index = zones.size();
    zone->output_node = node;
    r_zone_by_inout_node.add(node, zone.get());
    zones.append_and_get_index(std::move(zone));
  }
  for (const bNode *node : tree.nodes_by_type("GeometryNodeSimulationInput")) {
    const auto &storage = *static_cast<NodeGeometrySimulationInput *>(node->storage);
    if (const bNode *sim_output_node = tree.node_by_id(storage.output_node_id)) {
      if (TreeZone *zone = r_zone_by_inout_node.lookup_default(sim_output_node, nullptr)) {
        zone->input_node = node;
        r_zone_by_inout_node.add(node, zone);
      }
    }
  }
  return zones;
}

struct ZoneRelation {
  TreeZone *parent;
  TreeZone *child;
};

static Vector<ZoneRelation> get_direct_zone_relations(
    const Span<std::unique_ptr<TreeZone>> all_zones,
    const BitGroupVector<> &depend_on_input_flag_array)
{
  Vector<ZoneRelation> zone_relations;

  /* Gather all relations, even the transitive once. */
  for (const std::unique_ptr<TreeZone> &zone : all_zones) {
    const int zone_i = zone->index;
    for (const bNode *node : {zone->output_node}) {
      if (node == nullptr) {
        continue;
      }
      const BoundedBitSpan depend_on_input_flags = depend_on_input_flag_array[node->index()];
      bits::foreach_1_index(depend_on_input_flags, [&](const int parent_zone_i) {
        if (parent_zone_i != zone_i) {
          zone_relations.append({all_zones[parent_zone_i].get(), zone.get()});
        }
      });
    }
  }

  /* Remove transitive relations. This is a brute force algorithm currently. */
  Vector<int> transitive_relations;
  for (const int a : zone_relations.index_range()) {
    const ZoneRelation &relation_a = zone_relations[a];
    for (const int b : zone_relations.index_range()) {
      if (a == b) {
        continue;
      }
      const ZoneRelation &relation_b = zone_relations[b];
      for (const int c : zone_relations.index_range()) {
        if (a == c || b == c) {
          continue;
        }
        const ZoneRelation &relation_c = zone_relations[c];
        if (relation_a.child == relation_b.parent && relation_a.parent == relation_c.parent &&
            relation_b.child == relation_c.child)
        {
          transitive_relations.append_non_duplicates(c);
        }
      }
    }
  }
  std::sort(transitive_relations.begin(), transitive_relations.end(), std::greater<>());
  for (const int i : transitive_relations) {
    zone_relations.remove_and_reorder(i);
  }

  return zone_relations;
}

static void update_parent_zone_per_node(const Span<const bNode *> all_nodes,
                                        const Span<std::unique_ptr<TreeZone>> all_zones,
                                        const BitGroupVector<> &depend_on_input_flag_array,
                                        Map<int, int> &r_parent_zone_by_node_id)
{
  for (const int node_i : all_nodes.index_range()) {
    const bNode &node = *all_nodes[node_i];
    const BoundedBitSpan depend_on_input_flags = depend_on_input_flag_array[node_i];
    TreeZone *parent_zone = nullptr;
    bits::foreach_1_index(depend_on_input_flags, [&](const int parent_zone_i) {
      TreeZone *zone = all_zones[parent_zone_i].get();
      if (ELEM(&node, zone->input_node, zone->output_node)) {
        return;
      }
      if (parent_zone == nullptr || zone->depth > parent_zone->depth) {
        parent_zone = zone;
      }
    });
    if (parent_zone != nullptr) {
      r_parent_zone_by_node_id.add(node.identifier, parent_zone->index);
    }
  }
}

static std::unique_ptr<TreeZones> discover_tree_zones(const bNodeTree &tree)
{
  if (tree.has_available_link_cycle()) {
    return {};
  }

  std::unique_ptr<TreeZones> tree_zones = std::make_unique<TreeZones>();

  const Span<const bNode *> all_nodes = tree.all_nodes();
  Map<const bNode *, TreeZone *> zone_by_inout_node;
  tree_zones->zones = find_zone_nodes(tree, *tree_zones, zone_by_inout_node);

  const int zones_num = tree_zones->zones.size();
  const int nodes_num = all_nodes.size();
  /* A bit for every node-zone-combination. The bit is set when the node is in the zone. */
  BitGroupVector<> depend_on_input_flag_array(nodes_num, zones_num, false);
  /* The bit is set when the node depends on the output of the zone. */
  BitGroupVector<> depend_on_output_flag_array(nodes_num, zones_num, false);

  const Span<const bNode *> sorted_nodes = tree.toposort_left_to_right();
  for (const bNode *node : sorted_nodes) {
    const int node_i = node->index();
    MutableBoundedBitSpan depend_on_input_flags = depend_on_input_flag_array[node_i];
    MutableBoundedBitSpan depend_on_output_flags = depend_on_output_flag_array[node_i];

    /* Forward all bits from the nodes to the left. */
    for (const bNodeSocket *input_socket : node->input_sockets()) {
      if (!input_socket->is_available()) {
        continue;
      }
      for (const bNodeLink *link : input_socket->directly_linked_links()) {
        if (link->is_muted()) {
          continue;
        }
        const bNode &from_node = *link->fromnode;
        const int from_node_i = from_node.index();
        depend_on_input_flags |= depend_on_input_flag_array[from_node_i];
        depend_on_output_flags |= depend_on_output_flag_array[from_node_i];
      }
    }
    if (node->type == GEO_NODE_SIMULATION_INPUT) {
      if (const TreeZone *zone = zone_by_inout_node.lookup_default(node, nullptr)) {
        /* Now entering a zone, so set the corresponding bit. */
        depend_on_input_flags[zone->index].set();
      }
    }
    else if (node->type == GEO_NODE_SIMULATION_OUTPUT) {
      if (const TreeZone *zone = zone_by_inout_node.lookup_default(node, nullptr)) {
        /* The output is implicitly linked to the input, so also propagate the bits from there. */
        if (const bNode *zone_input_node = zone->input_node) {
          const int input_node_i = zone_input_node->index();
          depend_on_input_flags |= depend_on_input_flag_array[input_node_i];
          depend_on_output_flags |= depend_on_output_flag_array[input_node_i];
        }
        /* Now exiting a zone, so change the bits accordingly. */
        depend_on_input_flags[zone->index].reset();
        depend_on_output_flags[zone->index].set();
      }
    }

    if (bits::has_common_set_bits(depend_on_input_flags, depend_on_output_flags)) {
      /* A node can not be inside and after a zone at the same time. */
      return {};
    }
  }

  const Vector<ZoneRelation> zone_relations = get_direct_zone_relations(
      tree_zones->zones, depend_on_input_flag_array);

  /* Set parent and child pointers in zones. */
  for (const ZoneRelation &relation : zone_relations) {
    relation.parent->child_zones.append(relation.child);
    BLI_assert(relation.child->parent_zone == nullptr);
    relation.child->parent_zone = relation.parent;
  }

  /* Update depths. */
  for (std::unique_ptr<TreeZone> &zone : tree_zones->zones) {
    update_zone_depths(*zone);
  }

  update_parent_zone_per_node(all_nodes,
                              tree_zones->zones,
                              depend_on_input_flag_array,
                              tree_zones->parent_zone_by_node_id);

  for (const int node_i : all_nodes.index_range()) {
    const bNode *node = all_nodes[node_i];
    const int parent_zone_i = tree_zones->parent_zone_by_node_id.lookup_default(node->identifier,
                                                                                -1);
    if (parent_zone_i != -1) {
      tree_zones->zones[parent_zone_i]->child_nodes.append(node);
    }
  }

  return tree_zones;
}

const TreeZones *get_tree_zones(const bNodeTree &tree)
{
  tree.runtime->tree_zones_cache_mutex.ensure(
      [&]() { tree.runtime->tree_zones = discover_tree_zones(tree); });
  return tree.runtime->tree_zones.get();
}

bool TreeZone::contains_node_recursively(const bNode &node) const
{
  const TreeZones *zones = this->owner;
  const int parent_zone_i = zones->parent_zone_by_node_id.lookup_default(node.identifier, -1);
  if (parent_zone_i == -1) {
    return false;
  }
  for (const TreeZone *zone = zones->zones[parent_zone_i].get(); zone; zone = zone->parent_zone) {
    if (zone == this) {
      return true;
    }
  }
  return false;
}

}  // namespace blender::bke::node_tree_zones

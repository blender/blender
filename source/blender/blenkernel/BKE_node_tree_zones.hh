/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include <iosfwd>

#include "DNA_node_types.h"

#include "BLI_map.hh"
#include "BLI_vector.hh"

namespace blender::bke {

class bNodeTreeZones;

class bNodeTreeZone {
 public:
  bNodeTreeZones *owner = nullptr;
  /** Index of the zone in the array of all zones in a node tree. */
  int index = -1;
  /** Zero for top level zones, one for a nested zone, and so on. */
  int depth = -1;
  /** Input node of the zone. */
  std::optional<int> input_node_id;
  /** Output node of the zone. */
  std::optional<int> output_node_id;
  /** Direct parent of the zone. If this is null, this is a top level zone. */
  bNodeTreeZone *parent_zone = nullptr;
  /** Direct children zones. Does not contain recursively nested zones. */
  Vector<bNodeTreeZone *> child_zones;
  /** Direct children nodes excluding nodes that belong to child zones. */
  Vector<int> child_node_ids;
  /**
   * Links that enter the zone through the zone border and carry information. This excludes muted
   * and unavailable links as well as links that are dangling because they are only connected to a
   * reroute.
   */
  Vector<const bNodeLink *> border_links;

  const bNode *input_node() const;
  const bNode *output_node() const;
  Vector<const bNode *> child_nodes() const;

  bool contains_node_recursively(const bNode &node) const;
  bool contains_zone_recursively(const bNodeTreeZone &other_zone) const;

  friend std::ostream &operator<<(std::ostream &stream, const bNodeTreeZone &zone);
};

class bNodeTreeZones {
 public:
  const bNodeTree *tree = nullptr;
  Vector<std::unique_ptr<bNodeTreeZone>> zones_ptrs;
  /** Same as #zones_ptrs, but usually easier to iterate over. */
  Vector<bNodeTreeZone *> zones;
  Vector<bNodeTreeZone *> root_zones;
  Vector<int> node_ids_outside_zones;
  /**
   * Zone index by node. Nodes that are in no zone, are not included. Nodes that are at the border
   * of a zone (e.g. Simulation Input) are mapped to the zone they create.
   */
  Map<int, int> zone_by_node_id;

  Vector<const bNode *> nodes_outside_zones() const;

  /**
   * Get the deepest zone that a socket is in. Note that the inputs of a Simulation Input node are
   * in a different zone than its output sockets.
   */
  const bNodeTreeZone *get_zone_by_socket(const bNodeSocket &socket) const;

  /**
   * Get the deepest zone that the node is in. Note that the e.g. Simulation Input and Output nodes
   * are considered to be inside of the zone they create.
   */
  const bNodeTreeZone *get_zone_by_node(const int32_t node_id) const;

  /**
   * Check if a link from the first zone to a socket in the second zone is allowed. Either zone
   * input may also be null which represents the root tree outside of any zone. Generally, a link
   * can only go into zones, but not out of zones.
   */
  bool link_between_zones_is_allowed(const bNodeTreeZone *from_zone,
                                     const bNodeTreeZone *to_zone) const;

  /**
   * Check if a link between the given sockets is allowed. It's not allowed if link would go from
   * an inner zone to an outer zone.
   */
  bool link_between_sockets_is_allowed(const bNodeSocket &from, const bNodeSocket &to) const;

  /**
   * Get the ordered list of zones that a link going from an outer to an inner zone has to enter.
   */
  Vector<const bNodeTreeZone *> get_zones_to_enter(const bNodeTreeZone *outer_zone,
                                                   const bNodeTreeZone *inner_zone) const;

  /**
   * Same as #get_zones_to_enter but starts at the top level of the node tree.
   */
  Vector<const bNodeTreeZone *> get_zones_to_enter_from_root(const bNodeTreeZone *zone) const;

  friend std::ostream &operator<<(std::ostream &stream, const bNodeTreeZones &zones);
};

const bNodeTreeZones *get_tree_zones(const bNodeTree &tree);

}  // namespace blender::bke

inline const blender::bke::bNodeTreeZones *bNodeTree::zones() const
{
  return blender::bke::get_tree_zones(*this);
}

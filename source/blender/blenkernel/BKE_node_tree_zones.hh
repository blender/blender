/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "DNA_node_types.h"

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
  const bNode *input_node = nullptr;
  /** Output node of the zone. */
  const bNode *output_node = nullptr;
  /** Direct parent of the zone. If this is null, this is a top level zone. */
  bNodeTreeZone *parent_zone = nullptr;
  /** Direct children zones. Does not contain recursively nested zones. */
  Vector<bNodeTreeZone *> child_zones;
  /** Direct children nodes excluding nodes that belong to child zones. */
  Vector<const bNode *> child_nodes;
  /** Links that enter the zone through the zone border. */
  Vector<const bNodeLink *> border_links;

  bool contains_node_recursively(const bNode &node) const;
  bool contains_zone_recursively(const bNodeTreeZone &other_zone) const;
};

class bNodeTreeZones {
 public:
  Vector<std::unique_ptr<bNodeTreeZone>> zones;
  Vector<bNodeTreeZone *> root_zones;
  Vector<const bNode *> nodes_outside_zones;
  /**
   * Zone index by node. Nodes that are in no zone, are not included. Nodes that are at the border
   * of a zone (e.g. Simulation Input) are mapped to the zone they create.
   */
  Map<int, int> zone_by_node_id;

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
   * Get a sorted list of zones that the node is in. First comes the root zone and last the most
   * nested zone. For nodes that are at the root level, the returned list is empty.
   */
  Vector<const bNodeTreeZone *> get_zone_stack_for_node(const int32_t node_id) const;
};

const bNodeTreeZones *get_tree_zones(const bNodeTree &tree);

}  // namespace blender::bke

inline const blender::bke::bNodeTreeZones *bNodeTree::zones() const
{
  return blender::bke::get_tree_zones(*this);
}

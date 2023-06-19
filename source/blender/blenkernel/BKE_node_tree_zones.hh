/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "DNA_node_types.h"

#include "BLI_vector.hh"

namespace blender::bke::node_tree_zones {

struct TreeZone {
  TreeZones *owner = nullptr;
  /** Index of the zone in the array of all zones in a node tree. */
  int index = -1;
  /** Zero for top level zones, one for a nested zone, and so on. */
  int depth = -1;
  /** Input node of the zone. */
  const bNode *input_node = nullptr;
  /** Output node of the zone. */
  const bNode *output_node = nullptr;
  /** Direct parent of the zone. If this is null, this is a top level zone. */
  TreeZone *parent_zone = nullptr;
  /** Direct children zones. Does not contain recursively nested zones. */
  Vector<TreeZone *> child_zones;
  /** Direct children nodes excluding nodes that belong to child zones. */
  Vector<const bNode *> child_nodes;
  /** Links that enter the zone through the zone border. */
  Vector<const bNodeLink *> border_links;

  bool contains_node_recursively(const bNode &node) const;
  bool contains_zone_recursively(const TreeZone &other_zone) const;
};

class TreeZones {
 public:
  Vector<std::unique_ptr<TreeZone>> zones;
  Vector<TreeZone *> root_zones;
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
  const TreeZone *get_zone_by_socket(const bNodeSocket &socket) const;
};

const TreeZones *get_tree_zones(const bNodeTree &tree);

}  // namespace blender::bke::node_tree_zones

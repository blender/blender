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
  /** Direct children nodes. Does not contain recursively nested nodes. */
  Vector<const bNode *> child_nodes;

  bool contains_node_recursively(const bNode &node) const;
};

class TreeZones {
 public:
  Vector<std::unique_ptr<TreeZone>> zones;
  Map<int, int> parent_zone_by_node_id;
};

const TreeZones *get_tree_zones(const bNodeTree &tree);

}  // namespace blender::bke::node_tree_zones

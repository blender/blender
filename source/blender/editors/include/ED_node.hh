/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "ED_node.h"

struct SpaceNode;
struct ARegion;
struct Main;
struct bNodeTree;

namespace blender::ed::space_node {

void node_insert_on_link_flags_set(SpaceNode &snode, const ARegion &region);

/**
 * Assumes link with #NODE_LINKFLAG_HILITE set.
 */
void node_insert_on_link_flags(Main &bmain, SpaceNode &snode);
void node_insert_on_link_flags_clear(bNodeTree &node_tree);

}  // namespace blender::ed::space_node

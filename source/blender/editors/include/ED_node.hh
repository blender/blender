/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_vector_set.hh"
#include "ED_node_c.hh"

struct SpaceNode;
struct ARegion;
struct Main;
struct bNodeSocket;
struct bNodeTree;
struct rcti;

namespace blender::ed::space_node {

VectorSet<bNode *> get_selected_nodes(bNodeTree &node_tree);

void node_insert_on_link_flags_set(SpaceNode &snode, const ARegion &region);

/**
 * Assumes link with #NODE_LINKFLAG_HILITE set.
 */
void node_insert_on_link_flags(Main &bmain, SpaceNode &snode);
void node_insert_on_link_flags_clear(bNodeTree &node_tree);

/**
 * Draw a single node socket at default size.
 * \note this is only called from external code, internally #node_socket_draw_nested() is used for
 *       optimized drawing of multiple/all sockets of a node.
 */
void node_socket_draw(bNodeSocket *sock, const rcti *rect, const float color[4], float scale);

}  // namespace blender::ed::space_node

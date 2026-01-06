/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

namespace blender {

struct SpaceNode;
struct bNode;
struct ReportList;
struct bContext;
struct bNodeTree;
struct Main;
struct bNodeSocket;

namespace nodes {

/**
 * Sync the sockets of that node if possible. For example, a Separate Bundle node will be updated
 * to match a linked Combine Bundle if one is found.
 */
void sync_node(bContext &C, bNode &node, ReportList *reports);

/**
 * Get a description of what syncing the node would do. This can be used as tooltip.
 */
std::string sync_node_description_get(const bContext &C, const bNode &node);

/** Access (cached) information of whether a specific node can be synced currently. */
bool node_can_sync_sockets(const bContext &C, const bNodeTree &tree, const bNode &node);
void node_can_sync_cache_clear(Main &bmain);

void sync_sockets_evaluate_closure(SpaceNode &snode,
                                   bNode &evaluate_closure_node,
                                   ReportList *reports,
                                   const bNodeSocket *src_closure_socket = nullptr);
void sync_sockets_separate_bundle(SpaceNode &snode,
                                  bNode &separate_bundle_node,
                                  ReportList *reports,
                                  const bNodeSocket *src_bundle_socket = nullptr);
void sync_sockets_combine_bundle(SpaceNode &snode,
                                 bNode &combine_bundle_node,
                                 ReportList *reports,
                                 const bNodeSocket *src_bundle_socket = nullptr);
void sync_sockets_closure(SpaceNode &snode,
                          bNode &closure_input_node,
                          bNode &closure_output_node,
                          ReportList *reports,
                          const bNodeSocket *src_closure_socket = nullptr);

}  // namespace nodes
}  // namespace blender

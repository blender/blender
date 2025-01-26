/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_node_socket_value.hh"

#include "BLI_compute_context.hh"

#include "NOD_geometry_nodes_log.hh"

struct NodesModifierData;

namespace blender::nodes::inverse_eval {

using bke::SocketValueVariant;

/**
 * Utility struct to pack information about a value that is propagated backwards through a node
 * tree.
 */
struct SocketToUpdate {
  const ComputeContext *context = nullptr;
  const bNodeSocket *socket = nullptr;
  /** Only needed if the socket is a multi-input socket. */
  const bNodeLink *multi_input_link = nullptr;
  /** The new value that the socket should have after the backpropagation. */
  SocketValueVariant new_value;
};

/**
 * Try to change socket/node/modifier values so that the given sockets will have a specific value.
 */
bool backpropagate_socket_values(bContext &C,
                                 Object &object,
                                 NodesModifierData &nmd,
                                 geo_eval_log::GeoModifierLog &eval_log,
                                 Span<SocketToUpdate> sockets_to_update);

/**
 * Attempts to get the value for a specific socket from the log.
 */
std::optional<SocketValueVariant> get_logged_socket_value(geo_eval_log::GeoTreeLog &tree_log,
                                                          const bNodeSocket &socket);

/**
 * Performs implicit conversion from the old to the new socket on the given value, if possible.
 */
std::optional<bke::SocketValueVariant> convert_single_socket_value(
    const bNodeSocket &old_socket,
    const bNodeSocket &new_socket,
    const bke::SocketValueVariant &old_value);

}  // namespace blender::nodes::inverse_eval

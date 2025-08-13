/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "NOD_geometry_nodes_closure.hh"

#include "NOD_geometry_nodes_lazy_function.hh"

#include "BKE_node_socket_value.hh"

namespace blender::nodes {

struct ClosureEagerEvalParams {
  struct InputItem {
    std::string key;
    const bke::bNodeSocketType *type = nullptr;
    /** This may be moved from. */
    bke::SocketValueVariant value;
  };

  struct OutputItem {
    std::string key;
    const bke::bNodeSocketType *type = nullptr;
    /**
     * Where the output value should be stored. This is expected to point to uninitialized memory
     * when it's passed into #evaluate_closure_eagerly which will then construct the value inplace.
     */
    bke::SocketValueVariant *value = nullptr;
  };

  Vector<InputItem> inputs;
  Vector<OutputItem> outputs;
  GeoNodesUserData *user_data = nullptr;
};

void evaluate_closure_eagerly(const Closure &closure, ClosureEagerEvalParams &params);

}  // namespace blender::nodes

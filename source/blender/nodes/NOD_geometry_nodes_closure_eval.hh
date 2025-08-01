/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "NOD_geometry_nodes_closure.hh"

#include "NOD_geometry_nodes_lazy_function.hh"

namespace blender::nodes {

struct ClosureEagerEvalParams {
  struct InputItem {
    std::string key;
    const bke::bNodeSocketType *type = nullptr;
    /**
     * The actual socket value of type bNodeSocketType::geometry_nodes_cpp_type.
     * This is not const, because it may be moved from.
     */
    void *value = nullptr;
  };

  struct OutputItem {
    std::string key;
    const bke::bNodeSocketType *type = nullptr;
    /**
     * Where the output value should be stored. This is expected to point to uninitialized memory
     * when it's passed into #evaluate_closure_eagerly which will then construct the value inplace.
     */
    void *value = nullptr;
  };

  Vector<InputItem> inputs;
  Vector<OutputItem> outputs;
  GeoNodesUserData *user_data = nullptr;
};

void evaluate_closure_eagerly(const Closure &closure, ClosureEagerEvalParams &params);

}  // namespace blender::nodes

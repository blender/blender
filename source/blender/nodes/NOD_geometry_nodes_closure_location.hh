/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "BLI_compute_context.hh"
#include "BLI_mutex.hh"
#include "BLI_vector.hh"

struct bNodeTree;
struct bNode;

namespace blender::nodes {

struct ClosureEvalLocation {
  uint32_t orig_node_tree_session_uid;
  int evaluate_closure_node_id;
  ComputeContextHash compute_context_hash;
};

struct ClosureSourceLocation {
  /**
   * Tree where the closure is created. Note that this may be an original or evaluated tree,
   * depending on where it is used.
   */
  const bNodeTree *tree;
  int closure_output_node_id;
  ComputeContextHash compute_context_hash;
  /** Optional actual compute context. If it is set, its hash should be the same as above. */
  const ComputeContext *compute_context = nullptr;
};

struct ClosureEvalLog {
  Mutex mutex;
  Vector<ClosureEvalLocation> evaluations;
};

}  // namespace blender::nodes

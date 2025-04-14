/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>
#include <mutex>

#include "BLI_compute_context.hh"
#include "BLI_vector.hh"

namespace blender::nodes {

struct ClosureEvalLocation {
  uint32_t orig_node_tree_session_uid;
  int evaluate_closure_node_id;
  ComputeContextHash compute_context_hash;
};

struct ClosureSourceLocation {
  uint32_t orig_node_tree_session_uid;
  int closure_output_node_id;
  ComputeContextHash compute_context_hash;
};

struct ClosureEvalLog {
  std::mutex mutex;
  Vector<ClosureEvalLocation> evaluations;
};

}  // namespace blender::nodes

/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_array.hh"

namespace blender::fn::lazy_function::generic_graph_executor {

class GenericGraphExecutor;

/**
 * When a graph is executed, various things have to be allocated (e.g. the state of all nodes).
 * Instead of doing many small allocations, a single bigger allocation is done. This struct
 * contains the preprocessed offsets into that bigger buffer.
 */
struct PreprocessData {
  int node_states_array_offset;
  int loaded_inputs_array_offset;
  Array<int> node_states_offsets;
  int total_size;
};

}  // namespace blender::fn::lazy_function::generic_graph_executor

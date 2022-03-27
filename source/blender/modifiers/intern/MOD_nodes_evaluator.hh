/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_generic_pointer.hh"
#include "BLI_map.hh"

#include "NOD_derived_node_tree.hh"
#include "NOD_geometry_nodes_eval_log.hh"
#include "NOD_multi_function.hh"

#include "DNA_modifier_types.h"

#include "FN_multi_function.hh"

namespace geo_log = blender::nodes::geometry_nodes_eval_log;

namespace blender::modifiers::geometry_nodes {

using namespace nodes::derived_node_tree_types;

struct GeometryNodesEvaluationParams {
  blender::LinearAllocator<> allocator;

  Map<DOutputSocket, GMutablePointer> input_values;
  Vector<DInputSocket> output_sockets;
  /* These sockets will be computed but are not part of the output. Their value can be retrieved in
   * `log_socket_value_fn`. These sockets are not part of `output_sockets` because then the
   * evaluator would have to keep the socket values in memory until the end, which might not be
   * necessary in all cases. Sometimes `log_socket_value_fn` might just want to look at the value
   * and then it can be freed. */
  Vector<DSocket> force_compute_sockets;
  nodes::NodeMultiFunctions *mf_by_node;
  const NodesModifierData *modifier_;
  Depsgraph *depsgraph;
  Object *self_object;
  geo_log::GeoLogger *geo_logger;

  Vector<GMutablePointer> r_output_values;
};

void evaluate_geometry_nodes(GeometryNodesEvaluationParams &params);

}  // namespace blender::modifiers::geometry_nodes

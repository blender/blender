/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#include "BLI_map.hh"

#include "NOD_derived_node_tree.hh"
#include "NOD_geometry_nodes_eval_log.hh"
#include "NOD_node_tree_multi_function.hh"

#include "FN_generic_pointer.hh"

#include "DNA_modifier_types.h"

namespace geo_log = blender::nodes::geometry_nodes_eval_log;

namespace blender::modifiers::geometry_nodes {

using namespace nodes::derived_node_tree_types;
using fn::GMutablePointer;
using fn::GPointer;

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
  nodes::MultiFunctionByNode *mf_by_node;
  const NodesModifierData *modifier_;
  Depsgraph *depsgraph;
  Object *self_object;
  geo_log::GeoLogger *geo_logger;

  Vector<GMutablePointer> r_output_values;
};

void evaluate_geometry_nodes(GeometryNodesEvaluationParams &params);

}  // namespace blender::modifiers::geometry_nodes

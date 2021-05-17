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
#include "NOD_node_tree_multi_function.hh"

#include "FN_generic_pointer.hh"

#include "DNA_modifier_types.h"

namespace blender::modifiers::geometry_nodes {

using namespace nodes::derived_node_tree_types;
using fn::GMutablePointer;
using fn::GPointer;

using LogSocketValueFn = std::function<void(DSocket, Span<GPointer>)>;

struct GeometryNodesEvaluationParams {
  blender::LinearAllocator<> allocator;

  Map<DOutputSocket, GMutablePointer> input_values;
  Vector<DInputSocket> output_sockets;
  nodes::MultiFunctionByNode *mf_by_node;
  const NodesModifierData *modifier_;
  Depsgraph *depsgraph;
  Object *self_object;
  LogSocketValueFn log_socket_value_fn;

  Vector<GMutablePointer> r_output_values;
};

void evaluate_geometry_nodes(GeometryNodesEvaluationParams &params);

}  // namespace blender::modifiers::geometry_nodes

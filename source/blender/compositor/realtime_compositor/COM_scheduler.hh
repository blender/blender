/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_vector_set.hh"

#include "NOD_derived_node_tree.hh"

#include "COM_context.hh"

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;

/* A type representing the ordered set of nodes defining the schedule of node execution. */
using Schedule = VectorSet<DNode>;

/* Computes the execution schedule of the node tree. This is essentially a post-order depth first
 * traversal of the node tree from the output node to the leaf input nodes, with informed order of
 * traversal of dependencies based on a heuristic estimation of the number of needed buffers. */
Schedule compute_schedule(const Context &context, const DerivedNodeTree &tree);

}  // namespace blender::realtime_compositor

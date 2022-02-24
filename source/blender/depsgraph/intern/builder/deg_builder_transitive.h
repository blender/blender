/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2015 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup depsgraph
 */

#pragma once

namespace blender {
namespace deg {

struct Depsgraph;

/* Performs a transitive reduction to remove redundant relations. */
void deg_graph_transitive_reduction(Depsgraph *graph);

}  // namespace deg
}  // namespace blender

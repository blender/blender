/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2015 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#pragma once

namespace blender::deg {

struct Depsgraph;

/* Performs a transitive reduction to remove redundant relations. */
void deg_graph_transitive_reduction(Depsgraph *graph);

}  // namespace blender::deg

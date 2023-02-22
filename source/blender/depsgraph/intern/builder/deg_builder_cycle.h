/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2015 Blender Foundation */

/** \file
 * \ingroup depsgraph
 */

#pragma once

namespace blender::deg {

struct Depsgraph;

/* Detect and solve dependency cycles. */
void deg_graph_detect_cycles(Depsgraph *graph);

}  // namespace blender::deg

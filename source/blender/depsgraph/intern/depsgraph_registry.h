/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "intern/depsgraph_type.h"

struct Main;

namespace blender::deg {

struct Depsgraph;

void register_graph(Depsgraph *depsgraph);
void unregister_graph(Depsgraph *depsgraph);
Span<Depsgraph *> get_all_registered_graphs(Main *bmain);

}  // namespace blender::deg

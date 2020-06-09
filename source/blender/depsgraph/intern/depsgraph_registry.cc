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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/depsgraph_registry.h"

#include "BLI_utildefines.h"

#include "intern/depsgraph.h"

namespace DEG {

static Map<Main *, VectorSet<Depsgraph *>> g_graph_registry;

void register_graph(Depsgraph *depsgraph)
{
  Main *bmain = depsgraph->bmain;
  g_graph_registry.lookup_or_add_default(bmain).add_new(depsgraph);
}

void unregister_graph(Depsgraph *depsgraph)
{
  Main *bmain = depsgraph->bmain;
  VectorSet<Depsgraph *> &graphs = g_graph_registry.lookup(bmain);
  graphs.remove(depsgraph);

  // If this was the last depsgraph associated with the main, remove the main entry as well.
  if (graphs.is_empty()) {
    g_graph_registry.remove(bmain);
  }
}

Span<Depsgraph *> get_all_registered_graphs(Main *bmain)
{
  VectorSet<Depsgraph *> *graphs = g_graph_registry.lookup_ptr(bmain);
  if (graphs != nullptr) {
    return *graphs;
  }
  return {};
}

}  // namespace DEG

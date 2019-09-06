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

typedef set<Depsgraph *> DepsgraphStorage;
typedef map<Main *, DepsgraphStorage> MainDepsgraphMap;

static MainDepsgraphMap g_graph_registry;

void register_graph(Depsgraph *depsgraph)
{
  Main *bmain = depsgraph->bmain;
  MainDepsgraphMap::iterator it = g_graph_registry.find(bmain);
  if (it == g_graph_registry.end()) {
    it = g_graph_registry.insert(make_pair(bmain, DepsgraphStorage())).first;
  }
  DepsgraphStorage &storage = it->second;
  storage.insert(depsgraph);
}

void unregister_graph(Depsgraph *depsgraph)
{
  Main *bmain = depsgraph->bmain;
  MainDepsgraphMap::iterator it = g_graph_registry.find(bmain);
  BLI_assert(it != g_graph_registry.end());

  // Remove dependency graph from storage.
  DepsgraphStorage &storage = it->second;
  storage.erase(depsgraph);

  // If this was the last depsgraph associated with the main, remove the main entry as well.
  if (storage.empty()) {
    g_graph_registry.erase(bmain);
  }
}

const set<Depsgraph *> &get_all_registered_graphs(Main *bmain)
{
  MainDepsgraphMap::iterator it = g_graph_registry.find(bmain);
  if (it == g_graph_registry.end()) {
    static DepsgraphStorage empty_storage;
    return empty_storage;
  }
  return it->second;
}

}  // namespace DEG

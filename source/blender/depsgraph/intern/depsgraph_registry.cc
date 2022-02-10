/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup depsgraph
 */

#include "intern/depsgraph_registry.h"

#include "BLI_utildefines.h"

#include "intern/depsgraph.h"

namespace blender::deg {

using GraphRegistry = Map<Main *, VectorSet<Depsgraph *>>;
static GraphRegistry &get_graph_registry()
{
  static GraphRegistry graph_registry;
  return graph_registry;
}

void register_graph(Depsgraph *depsgraph)
{
  Main *bmain = depsgraph->bmain;
  get_graph_registry().lookup_or_add_default(bmain).add_new(depsgraph);
}

void unregister_graph(Depsgraph *depsgraph)
{
  Main *bmain = depsgraph->bmain;
  GraphRegistry &graph_registry = get_graph_registry();
  VectorSet<Depsgraph *> &graphs = graph_registry.lookup(bmain);
  graphs.remove(depsgraph);

  /* If this was the last depsgraph associated with the main, remove the main entry as well. */
  if (graphs.is_empty()) {
    graph_registry.remove(bmain);
  }
}

Span<Depsgraph *> get_all_registered_graphs(Main *bmain)
{
  VectorSet<Depsgraph *> *graphs = get_graph_registry().lookup_ptr(bmain);
  if (graphs != nullptr) {
    return *graphs;
  }
  return {};
}

}  // namespace blender::deg

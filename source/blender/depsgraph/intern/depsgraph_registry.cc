/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#include <memory>
#include <mutex>

#include "intern/depsgraph_registry.h"

#include "BLI_utildefines.h"

#include "intern/depsgraph.h"

namespace blender::deg {

/* Global registry for dependency graphs associated with a main database.
 *
 * Threads may add or remove depsgraphs for different mains concurrently
 * (for example for preview rendering), but not the same main. */

/* Use pointer for map value to ensure span returned by get_all_registered_graphs
 * remains unchanged as other mains are added or removed. */
typedef std::unique_ptr<VectorSet<Depsgraph *>> GraphSetPtr;
struct GraphRegistry {
  Map<Main *, GraphSetPtr> map;
  std::mutex mutex;
};

static GraphRegistry &get_graph_registry()
{
  static GraphRegistry graph_registry;
  return graph_registry;
}

void register_graph(Depsgraph *depsgraph)
{
  GraphRegistry &graph_registry = get_graph_registry();
  Main *bmain = depsgraph->bmain;

  std::lock_guard<std::mutex> lock{graph_registry.mutex};
  graph_registry.map
      .lookup_or_add_cb(bmain, []() { return std::make_unique<VectorSet<Depsgraph *>>(); })
      ->add_new(depsgraph);
}

void unregister_graph(Depsgraph *depsgraph)
{
  Main *bmain = depsgraph->bmain;
  GraphRegistry &graph_registry = get_graph_registry();

  std::lock_guard<std::mutex> lock{graph_registry.mutex};
  GraphSetPtr &graphs = graph_registry.map.lookup(bmain);
  graphs->remove(depsgraph);

  /* If this was the last depsgraph associated with the main, remove the main entry as well. */
  if (graphs->is_empty()) {
    graph_registry.map.remove(bmain);
  }
}

Span<Depsgraph *> get_all_registered_graphs(Main *bmain)
{
  GraphRegistry &graph_registry = get_graph_registry();
  std::lock_guard<std::mutex> lock{graph_registry.mutex};
  GraphSetPtr *graphs = graph_registry.map.lookup_ptr(bmain);
  if (graphs) {
    return **graphs;
  }
  return {};
}

}  // namespace blender::deg

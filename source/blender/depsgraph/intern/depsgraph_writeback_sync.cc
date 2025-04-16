/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <mutex>

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_writeback_sync.hh"

#include "depsgraph.hh"

namespace blender::deg::sync_writeback {

void add(::Depsgraph &depsgraph, std::function<void()> fn)
{
  deg::Depsgraph &deg_graph = reinterpret_cast<deg::Depsgraph &>(depsgraph);
  if (!deg_graph.is_active || deg_graph.sync_writeback == DEG_EVALUATE_SYNC_WRITEBACK_NO) {
    return;
  }

  std::lock_guard lock{deg_graph.sync_writeback_callbacks_mutex};
  deg_graph.sync_writeback_callbacks.append(std::move(fn));
}

}  // namespace blender::deg::sync_writeback

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Defragment.
 *
 * Defragment the cached page buffer making one continuous array.
 *
 * Also pop_front the cached pages if there is not enough free pages for the needed allocations.
 * Here is an example of the behavior of this buffer during one update cycle:
 *
 *   Initial state: 5 cached pages. Buffer starts at index 2 and ends at 6.
 *     `[--xxxxx---------]`
 *   After page free step: 2 cached pages were removed (r), 3 pages were inserted in the cache (i).
 *     `[--xrxrxiii------]`
 *   After page defragment step: The buffer is compressed into only 6 pages.
 *     `[----xxxxxx------]`
 */

#pragma once
#pragma create_info

#include "draw_shader_shared.hh"
#include "eevee_shadow_page_ops.bsl.hh"

namespace eevee::shadow {

using PageAllocator = eevee::shadow::PageAllocator;
using Statistics = eevee::shadow::Statistics;
using TileMaps = eevee::shadow::TileMaps;

struct Commands {
  [[storage(5, write)]] DispatchCommand &clear_dispatch_buf;
  [[storage(6, write)]] DrawCommandArray &tile_draw_buf;
};

[[compute, local_size(1)]]
void defrag([[resource_table]] PageAllocator &allocator,
            [[resource_table]] Commands &cmds,
            [[resource_table]] Statistics &stats)
{
  allocator.defrag();

  /* Stats. */
  stats.statistics_buf.page_used_count = 0;
  stats.statistics_buf.page_update_count = 0;
  stats.statistics_buf.page_allocated_count = 0;
  stats.statistics_buf.page_rendered_count = 0;
  stats.statistics_buf.view_needed_count = 0;

  /* Reset clear command indirect buffer. */
  cmds.clear_dispatch_buf.num_groups_x = SHADOW_PAGE_RES / SHADOW_PAGE_CLEAR_GROUP_SIZE;
  cmds.clear_dispatch_buf.num_groups_y = SHADOW_PAGE_RES / SHADOW_PAGE_CLEAR_GROUP_SIZE;
  cmds.clear_dispatch_buf.num_groups_z = 0;

  /* Reset TBDR command indirect buffer. */
  cmds.tile_draw_buf.vertex_len = 0u;
  cmds.tile_draw_buf.instance_len = 1u;
  cmds.tile_draw_buf.vertex_first = 0u;
  cmds.tile_draw_buf.instance_first = 0u;
}

PipelineCompute page_defrag(defrag);

}  // namespace eevee::shadow

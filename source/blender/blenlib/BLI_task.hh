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
 */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef WITH_TBB
/* Quiet top level deprecation message, unrelated to API usage here. */
#  define TBB_SUPPRESS_DEPRECATED_MESSAGES 1
#  include <tbb/tbb.h>
#endif

#include "BLI_index_range.hh"
#include "BLI_utildefines.h"

namespace blender {

template<typename Range, typename Function>
void parallel_for_each(Range &range, const Function &function)
{
#ifdef WITH_TBB
  tbb::parallel_for_each(range, function);
#else
  for (auto &value : range) {
    function(value);
  }
#endif
}

template<typename Function>
void parallel_for(IndexRange range, int64_t grain_size, const Function &function)
{
  if (range.size() == 0) {
    return;
  }
#ifdef WITH_TBB
  tbb::parallel_for(tbb::blocked_range<int64_t>(range.first(), range.one_after_last(), grain_size),
                    [&](const tbb::blocked_range<int64_t> &subrange) {
                      function(IndexRange(subrange.begin(), subrange.size()));
                    });
#else
  UNUSED_VARS(grain_size);
  function(range);
#endif
}

}  // namespace blender

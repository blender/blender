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
#  if defined(WIN32) && !defined(NOMINMAX)
/* TBB includes Windows.h which will define min/max macros causing issues
 * when we try to use std::min and std::max later on. */
#    define NOMINMAX
#    define TBB_MIN_MAX_CLEANUP
#  endif
#  include <tbb/blocked_range.h>
#  include <tbb/parallel_for.h>
#  include <tbb/parallel_for_each.h>
#  include <tbb/parallel_reduce.h>
#  include <tbb/task_arena.h>
#  ifdef WIN32
/* We cannot keep this defined, since other parts of the code deal with this on their own, leading
 * to multiple define warnings unless we un-define this, however we can only undefine this if we
 * were the ones that made the definition earlier. */
#    ifdef TBB_MIN_MAX_CLEANUP
#      undef NOMINMAX
#    endif
#  endif
#endif

#include "BLI_index_range.hh"
#include "BLI_utildefines.h"

namespace blender::threading {

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

template<typename Value, typename Function, typename Reduction>
Value parallel_reduce(IndexRange range,
                      int64_t grain_size,
                      const Value &identity,
                      const Function &function,
                      const Reduction &reduction)
{
#ifdef WITH_TBB
  return tbb::parallel_reduce(
      tbb::blocked_range<int64_t>(range.first(), range.one_after_last(), grain_size),
      identity,
      [&](const tbb::blocked_range<int64_t> &subrange, const Value &ident) {
        return function(IndexRange(subrange.begin(), subrange.size()), ident);
      },
      reduction);
#else
  UNUSED_VARS(grain_size, reduction);
  return function(range, identity);
#endif
}

/** See #BLI_task_isolate for a description of what isolating a task means. */
template<typename Function> void isolate_task(const Function &function)
{
#ifdef WITH_TBB
  tbb::this_task_arena::isolate(function);
#else
  function();
#endif
}

}  // namespace blender::threading

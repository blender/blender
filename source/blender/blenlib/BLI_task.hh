/* SPDX-License-Identifier: GPL-2.0-or-later */

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
#  include <tbb/parallel_invoke.h>
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
#include "BLI_lazy_threading.hh"
#include "BLI_utildefines.h"

namespace blender::threading {

template<typename Range, typename Function>
void parallel_for_each(Range &&range, const Function &function)
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
  /* Invoking tbb for small workloads has a large overhead. */
  if (range.size() >= grain_size) {
    lazy_threading::send_hint();
    tbb::parallel_for(
        tbb::blocked_range<int64_t>(range.first(), range.one_after_last(), grain_size),
        [&](const tbb::blocked_range<int64_t> &subrange) {
          function(IndexRange(subrange.begin(), subrange.size()));
        });
    return;
  }
#else
  UNUSED_VARS(grain_size);
#endif
  function(range);
}

/**
 * Same as #parallel_for but tries to make the sub-range sizes multiples of the given alignment.
 * This can improve performance when the range is processed using vectorized and/or unrolled loops,
 * because the fallback loop that processes remaining values is used less often. A disadvantage of
 * using this instead of #parallel_for is that the size differences between sub-ranges can be
 * larger, which means that work is distributed less evenly.
 */
template<typename Function>
void parallel_for_aligned(const IndexRange range,
                          const int64_t grain_size,
                          const int64_t alignment,
                          const Function &function)
{
  const int64_t global_begin = range.start();
  const int64_t global_end = range.one_after_last();
  const int64_t alignment_mask = ~(alignment - 1);
  parallel_for(range, grain_size, [&](const IndexRange unaligned_range) {
    /* Move the sub-range boundaries down to the next aligned index. The "global" begin and end
     * remain fixed though. */
    const int64_t unaligned_begin = unaligned_range.start();
    const int64_t unaligned_end = unaligned_range.one_after_last();
    const int64_t aligned_begin = std::max(global_begin, unaligned_begin & alignment_mask);
    const int64_t aligned_end = unaligned_end == global_end ?
                                    unaligned_end :
                                    std::max(global_begin, unaligned_end & alignment_mask);
    const IndexRange aligned_range{aligned_begin, aligned_end - aligned_begin};
    function(aligned_range);
  });
}

template<typename Value, typename Function, typename Reduction>
Value parallel_reduce(IndexRange range,
                      int64_t grain_size,
                      const Value &identity,
                      const Function &function,
                      const Reduction &reduction)
{
#ifdef WITH_TBB
  if (range.size() >= grain_size) {
    lazy_threading::send_hint();
    return tbb::parallel_reduce(
        tbb::blocked_range<int64_t>(range.first(), range.one_after_last(), grain_size),
        identity,
        [&](const tbb::blocked_range<int64_t> &subrange, const Value &ident) {
          return function(IndexRange(subrange.begin(), subrange.size()), ident);
        },
        reduction);
  }
#else
  UNUSED_VARS(grain_size, reduction);
#endif
  return function(range, identity);
}

/**
 * Execute all of the provided functions. The functions might be executed in parallel or in serial
 * or some combination of both.
 */
template<typename... Functions> void parallel_invoke(Functions &&...functions)
{
#ifdef WITH_TBB
  tbb::parallel_invoke(std::forward<Functions>(functions)...);
#else
  (functions(), ...);
#endif
}

/**
 * Same #parallel_invoke, but allows disabling threading dynamically. This is useful because when
 * the individual functions do very little work, there is a lot of overhead from starting parallel
 * tasks.
 */
template<typename... Functions>
void parallel_invoke(const bool use_threading, Functions &&...functions)
{
  if (use_threading) {
    lazy_threading::send_hint();
    parallel_invoke(std::forward<Functions>(functions)...);
  }
  else {
    (functions(), ...);
  }
}

/** See #BLI_task_isolate for a description of what isolating a task means. */
template<typename Function> void isolate_task(const Function &function)
{
#ifdef WITH_TBB
  lazy_threading::ReceiverIsolation isolation;
  tbb::this_task_arena::isolate(function);
#else
  function();
#endif
}

}  // namespace blender::threading

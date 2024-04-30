/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * Task parallel range functions.
 */

#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_lazy_threading.hh"
#include "BLI_offset_indices.hh"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_threads.h"
#include "BLI_vector.hh"

#include "atomic_ops.h"

#ifdef WITH_TBB
#  include <tbb/blocked_range.h>
#  include <tbb/enumerable_thread_specific.h>
#  include <tbb/parallel_for.h>
#  include <tbb/parallel_reduce.h>
#endif

#ifdef WITH_TBB

/* Functor for running TBB parallel_for and parallel_reduce. */
struct RangeTask {
  TaskParallelRangeFunc func;
  void *userdata;
  const TaskParallelSettings *settings;

  void *userdata_chunk;

  /* Root constructor. */
  RangeTask(TaskParallelRangeFunc func, void *userdata, const TaskParallelSettings *settings)
      : func(func), userdata(userdata), settings(settings)
  {
    init_chunk(settings->userdata_chunk);
  }

  /* Copy constructor. */
  RangeTask(const RangeTask &other)
      : func(other.func), userdata(other.userdata), settings(other.settings)
  {
    init_chunk(settings->userdata_chunk);
  }

  /* Splitting constructor for parallel reduce. */
  RangeTask(RangeTask &other, tbb::split /*unused*/)
      : func(other.func), userdata(other.userdata), settings(other.settings)
  {
    init_chunk(settings->userdata_chunk);
  }

  ~RangeTask()
  {
    if (settings->func_free != nullptr) {
      settings->func_free(userdata, userdata_chunk);
    }
    MEM_SAFE_FREE(userdata_chunk);
  }

  void init_chunk(void *from_chunk)
  {
    if (from_chunk) {
      userdata_chunk = MEM_mallocN(settings->userdata_chunk_size, "RangeTask");
      memcpy(userdata_chunk, from_chunk, settings->userdata_chunk_size);
    }
    else {
      userdata_chunk = nullptr;
    }
  }

  void operator()(const tbb::blocked_range<int> &r) const
  {
    TaskParallelTLS tls;
    tls.userdata_chunk = userdata_chunk;
    for (int i = r.begin(); i != r.end(); ++i) {
      func(userdata, i, &tls);
    }
  }

  void join(const RangeTask &other)
  {
    settings->func_reduce(userdata, userdata_chunk, other.userdata_chunk);
  }
};

#endif

void BLI_task_parallel_range(const int start,
                             const int stop,
                             void *userdata,
                             TaskParallelRangeFunc func,
                             const TaskParallelSettings *settings)
{
#ifdef WITH_TBB
  /* Multithreading. */
  if (settings->use_threading && BLI_task_scheduler_num_threads() > 1) {
    RangeTask task(func, userdata, settings);
    const size_t grainsize = std::max(settings->min_iter_per_thread, 1);
    const tbb::blocked_range<int> range(start, stop, grainsize);

    blender::lazy_threading::send_hint();

    if (settings->func_reduce) {
      parallel_reduce(range, task);
      if (settings->userdata_chunk) {
        memcpy(settings->userdata_chunk, task.userdata_chunk, settings->userdata_chunk_size);
      }
    }
    else {
      parallel_for(range, task);
    }
    return;
  }
#endif

  /* Single threaded. Nothing to reduce as everything is accumulated into the
   * main userdata chunk directly. */
  TaskParallelTLS tls;
  tls.userdata_chunk = settings->userdata_chunk;
  for (int i = start; i < stop; i++) {
    func(userdata, i, &tls);
  }
  if (settings->func_free != nullptr) {
    settings->func_free(userdata, settings->userdata_chunk);
  }
}

int BLI_task_parallel_thread_id(const TaskParallelTLS * /*tls*/)
{
#ifdef WITH_TBB
  /* Get a unique thread ID for texture nodes. In the future we should get rid
   * of the thread ID and change texture evaluation to not require per-thread
   * storage that can't be efficiently allocated on the stack. */
  static tbb::enumerable_thread_specific<int> tbb_thread_id(-1);
  static int tbb_thread_id_counter = 0;

  int &thread_id = tbb_thread_id.local();
  if (thread_id == -1) {
    thread_id = atomic_fetch_and_add_int32(&tbb_thread_id_counter, 1);
    if (thread_id >= BLENDER_MAX_THREADS) {
      BLI_assert_msg(0, "Maximum number of threads exceeded for sculpting");
      thread_id = thread_id % BLENDER_MAX_THREADS;
    }
  }
  return thread_id;
#else
  return 0;
#endif
}

namespace blender::threading::detail {

#ifdef WITH_TBB
static void parallel_for_impl_static_size(const IndexRange range,
                                          const int64_t grain_size,
                                          const FunctionRef<void(IndexRange)> function)
{
  tbb::parallel_for(tbb::blocked_range<int64_t>(range.first(), range.one_after_last(), grain_size),
                    [function](const tbb::blocked_range<int64_t> &subrange) {
                      function(IndexRange(subrange.begin(), subrange.size()));
                    });
}
#endif /* WITH_TBB */

#ifdef WITH_TBB
static void parallel_for_impl_individual_size_lookup(
    const IndexRange range,
    const int64_t grain_size,
    const FunctionRef<void(IndexRange)> function,
    const TaskSizeHints_IndividualLookup &size_hints)
{
  /* Shouldn't be too small, because then there is more overhead when the individual tasks are
   * small. Also shouldn't be too large because then the serial code to split up tasks causes extra
   * overhead. */
  const int64_t outer_grain_size = std::min<int64_t>(grain_size, 512);
  threading::parallel_for(range, outer_grain_size, [&](const IndexRange sub_range) {
    /* Compute the size of every task in the current range. */
    Array<int64_t, 1024> task_sizes(sub_range.size());
    size_hints.lookup_individual_sizes(sub_range, task_sizes);

    /* Split range into multiple segments that have a size that approximates the grain size. */
    Vector<int64_t, 256> offsets_vec;
    offsets_vec.append(0);
    int64_t counter = 0;
    for (const int64_t i : sub_range.index_range()) {
      counter += task_sizes[i];
      if (counter >= grain_size) {
        offsets_vec.append(i + 1);
        counter = 0;
      }
    }
    if (offsets_vec.last() < sub_range.size()) {
      offsets_vec.append(sub_range.size());
    }
    const OffsetIndices<int64_t> offsets = offsets_vec.as_span();

    /* Run the dynamically split tasks in parallel. */
    threading::parallel_for(offsets.index_range(), 1, [&](const IndexRange offsets_range) {
      for (const int64_t i : offsets_range) {
        const IndexRange actual_range = offsets[i].shift(sub_range.start());
        function(actual_range);
      }
    });
  });
}

#endif /* WITH_TBB */

static void parallel_for_impl_accumulated_size_lookup(
    const IndexRange range,
    const int64_t grain_size,
    const FunctionRef<void(IndexRange)> function,
    const TaskSizeHints_AccumulatedLookup &size_hints)
{
  BLI_assert(!range.is_empty());
  if (range.size() == 1) {
    /* Can't subdivide further. */
    function(range);
    return;
  }
  const int64_t total_size = size_hints.lookup_accumulated_size(range);
  if (total_size <= grain_size) {
    function(range);
    return;
  }
  const int64_t middle = range.size() / 2;
  const IndexRange left_range = range.take_front(middle);
  const IndexRange right_range = range.drop_front(middle);
  threading::parallel_invoke(
      [&]() {
        parallel_for_impl_accumulated_size_lookup(left_range, grain_size, function, size_hints);
      },
      [&]() {
        parallel_for_impl_accumulated_size_lookup(right_range, grain_size, function, size_hints);
      });
}

void parallel_for_impl(const IndexRange range,
                       const int64_t grain_size,
                       const FunctionRef<void(IndexRange)> function,
                       const TaskSizeHints &size_hints)
{
#ifdef WITH_TBB
  lazy_threading::send_hint();
  switch (size_hints.type) {
    case TaskSizeHints::Type::Static: {
      const int64_t task_size = static_cast<const detail::TaskSizeHints_Static &>(size_hints).size;
      const int64_t final_grain_size = task_size == 1 ?
                                           grain_size :
                                           std::max<int64_t>(1, grain_size / task_size);
      parallel_for_impl_static_size(range, final_grain_size, function);
      break;
    }
    case TaskSizeHints::Type::IndividualLookup: {
      parallel_for_impl_individual_size_lookup(
          range,
          grain_size,
          function,
          static_cast<const detail::TaskSizeHints_IndividualLookup &>(size_hints));
      break;
    }
    case TaskSizeHints::Type::AccumulatedLookup: {
      parallel_for_impl_accumulated_size_lookup(
          range,
          grain_size,
          function,
          static_cast<const detail::TaskSizeHints_AccumulatedLookup &>(size_hints));
      break;
    }
  }

#else
  UNUSED_VARS(grain_size, size_hints);
  function(range);
#endif
}

void memory_bandwidth_bound_task_impl(const FunctionRef<void()> function)
{
#ifdef WITH_TBB
  /* This is the maximum number of threads that may perform these memory bandwidth bound tasks at
   * the same time. Often fewer threads are already enough to use up the full bandwidth capacity.
   * Additional threads usually have a negligible benefit and can even make performance worse.
   *
   * It's better to use fewer threads here so that the CPU cores can do other tasks at the same
   * time which may be more compute intensive. */
  const int num_threads = 8;
  if (num_threads >= BLI_task_scheduler_num_threads()) {
    /* Avoid overhead of using a task arena when it would not have any effect anyway. */
    function();
    return;
  }
  static tbb::task_arena arena{num_threads};

  /* Make sure the lazy threading hints are send now, because they shouldn't be send out of an
   * isolated region. */
  lazy_threading::send_hint();
  lazy_threading::ReceiverIsolation isolation;

  arena.execute(function);
#else
  function();
#endif
}

}  // namespace blender::threading::detail

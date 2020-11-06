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

/** \file
 * \ingroup bli
 *
 * Task parallel range functions.
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_task.h"
#include "BLI_threads.h"

#include "atomic_ops.h"

#ifdef WITH_TBB
/* Quiet top level deprecation message, unrelated to API usage here. */
#  define TBB_SUPPRESS_DEPRECATED_MESSAGES 1
#  include <tbb/tbb.h>
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
  RangeTask(RangeTask &other, tbb::split /* unused */)
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
    tbb::this_task_arena::isolate([this, r] {
      TaskParallelTLS tls;
      tls.userdata_chunk = userdata_chunk;
      for (int i = r.begin(); i != r.end(); ++i) {
        func(userdata, i, &tls);
      }
    });
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
    const size_t grainsize = MAX2(settings->min_iter_per_thread, 1);
    const tbb::blocked_range<int> range(start, stop, grainsize);

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

int BLI_task_parallel_thread_id(const TaskParallelTLS *UNUSED(tls))
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
      BLI_assert(!"Maximum number of threads exceeded for sculpting");
      thread_id = thread_id % BLENDER_MAX_THREADS;
    }
  }
  return thread_id;
#else
  return 0;
#endif
}

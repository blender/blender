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

#include "MEM_guardedalloc.h"

#include "BLI_task.h"
#include "BLI_threads.h"

#include "BKE_pbvh.h"

#include "atomic_ops.h"

#ifdef WITH_TBB

#  include <tbb/tbb.h>

/* Functor for running TBB parallel_for and parallel_reduce. */
struct PBVHTask {
  PBVHParallelRangeFunc func;
  void *userdata;
  const PBVHParallelSettings *settings;

  void *userdata_chunk;
  bool userdata_chunk_free;

  PBVHTask()
  {
  }

  PBVHTask(const PBVHTask &other)
      : func(other.func),
        userdata(other.userdata),
        settings(other.settings),
        userdata_chunk(0),
        userdata_chunk_free(false)
  {
    if (other.userdata_chunk) {
      userdata_chunk = MEM_mallocN(settings->userdata_chunk_size, "PBVHTask");
      memcpy(userdata_chunk, other.userdata_chunk, settings->userdata_chunk_size);
      userdata_chunk_free = true;
    }
  }

  PBVHTask(PBVHTask &other, tbb::split) : PBVHTask(other)
  {
  }

  ~PBVHTask()
  {
    if (userdata_chunk_free) {
      MEM_freeN(userdata_chunk);
    }
  }

  void operator()(const tbb::blocked_range<int> &r) const
  {
    TaskParallelTLS tls;
    tls.thread_id = get_thread_id();
    tls.userdata_chunk = userdata_chunk;
    for (int i = r.begin(); i != r.end(); ++i) {
      func(userdata, i, &tls);
    }
  }

  void join(const PBVHTask &other)
  {
    settings->func_reduce(userdata, userdata_chunk, other.userdata_chunk);
  }

  int get_thread_id() const
  {
    /* Get a unique thread ID for texture nodes. In the future we should get rid
     * of the thread ID and change texture evaluation to not require per-thread
     * storage that can't be efficiently allocated on the stack. */
    static tbb::enumerable_thread_specific<int> pbvh_thread_id(-1);
    static int pbvh_thread_id_counter = 0;

    int &thread_id = pbvh_thread_id.local();
    if (thread_id == -1) {
      thread_id = atomic_fetch_and_add_int32(&pbvh_thread_id_counter, 1);
      if (thread_id >= BLENDER_MAX_THREADS) {
        BLI_assert(!"Maximum number of threads exceeded for sculpting");
        thread_id = thread_id % BLENDER_MAX_THREADS;
      }
    }
    return thread_id;
  }
};

#endif

void BKE_pbvh_parallel_range(const int start,
                             const int stop,
                             void *userdata,
                             PBVHParallelRangeFunc func,
                             const struct PBVHParallelSettings *settings)
{
#ifdef WITH_TBB
  /* Multithreading. */
  if (settings->use_threading) {
    PBVHTask task;
    task.func = func;
    task.userdata = userdata;
    task.settings = settings;
    task.userdata_chunk = settings->userdata_chunk;
    task.userdata_chunk_free = false;

    if (settings->func_reduce) {
      parallel_reduce(tbb::blocked_range<int>(start, stop), task);
    }
    else {
      parallel_for(tbb::blocked_range<int>(start, stop), task);
    }

    return;
  }
#endif

  /* Single threaded. Nothing to reduce as everything is accumulated into the
   * main userdata chunk directly. */
  TaskParallelTLS tls;
  tls.thread_id = 0;
  tls.userdata_chunk = settings->userdata_chunk;
  for (int i = start; i < stop; i++) {
    func(userdata, i, &tls);
  }
}

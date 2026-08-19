/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * Task scheduler initialization.
 */

#include "MEM_guardedalloc.h"

#include "BLI_lazy_threading.hh"
#include "BLI_simd.hh"
#include "BLI_task_c.hh"
#include "BLI_threads.hh"

#ifdef WITH_TBB
/* Need to include at least one header to get the version define. */
#  include <tbb/blocked_range.h>
#  include <tbb/task_arena.h>
#  include <tbb/task_scheduler_observer.h>
#  if TBB_INTERFACE_VERSION_MAJOR >= 10
#    include <tbb/global_control.h>
#    define WITH_TBB_GLOBAL_CONTROL
#  endif
#endif

namespace blender {

/* Task Scheduler */

static int task_scheduler_num_threads = 1;
#ifdef WITH_TBB_GLOBAL_CONTROL
static tbb::global_control *task_scheduler_global_control = nullptr;
#endif

#ifdef WITH_TBB
/**
 * Enables flush-to-zero and denormals-are-zero on every task scheduler thread.
 *
 * Denormal handling is part of the per-thread floating point state, so it has to be set on each
 * thread rather than once at startup. Applying it consistently everywhere makes results
 * reproducible no matter which thread a computation uses.
 */
class FlushDenormalsObserver : public tbb::task_scheduler_observer {
 public:
  FlushDenormalsObserver()
  {
    this->observe(true);
  }

  ~FlushDenormalsObserver() override
  {
    this->observe(false);
  }

  void on_scheduler_entry(bool /*is_worker*/) override
  {
    SIMD_SET_FLUSH_TO_ZERO;
  }
};

static void ensure_flush_denormals_observer()
{
  static FlushDenormalsObserver observer;
}
#endif

void BLI_task_scheduler_init(const bool use_flush_denormals_to_zero)
{
  if (use_flush_denormals_to_zero) {
    /* The main thread is not observed by the task scheduler, set it directly. */
    SIMD_SET_FLUSH_TO_ZERO;

#ifdef WITH_TBB
    ensure_flush_denormals_observer();
#endif
  }

#ifdef WITH_TBB_GLOBAL_CONTROL
  const int threads_override_num = BLI_system_num_threads_override_get();

  if (threads_override_num > 0) {
    /* Override number of threads. This settings is used within the lifetime
     * of tbb::global_control, so we allocate it on the heap. */
    task_scheduler_global_control = MEM_new<tbb::global_control>(
        __func__, tbb::global_control::max_allowed_parallelism, threads_override_num);
    task_scheduler_num_threads = threads_override_num;
  }
  else {
    /* Let TBB choose the number of threads. For (legacy) code that calls
     * BLI_task_scheduler_num_threads() we provide the system thread count.
     * Ideally such code should be rewritten not to use the number of threads
     * at all. */
    task_scheduler_num_threads = BLI_system_thread_count();
  }
#else
  task_scheduler_num_threads = BLI_system_thread_count();
#endif
}

void BLI_task_scheduler_exit()
{
#ifdef WITH_TBB_GLOBAL_CONTROL
  MEM_delete(task_scheduler_global_control);
#endif
}

int BLI_task_scheduler_num_threads()
{
  return task_scheduler_num_threads;
}

void BLI_task_isolate(void (*func)(void *userdata), void *userdata)
{
#ifdef WITH_TBB
  lazy_threading::ReceiverIsolation isolation;
  tbb::this_task_arena::isolate([&] { func(userdata); });
#else
  func(userdata);
#endif
}

}  // namespace blender

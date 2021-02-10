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
 * Task scheduler initialization.
 */

#include "MEM_guardedalloc.h"

#include "BLI_task.h"
#include "BLI_threads.h"

#ifdef WITH_TBB
/* Need to include at least one header to get the version define. */
#  include <tbb/blocked_range.h>
#  if TBB_INTERFACE_VERSION_MAJOR >= 10
#    include <tbb/global_control.h>
#    define WITH_TBB_GLOBAL_CONTROL
#  endif
#endif

/* Task Scheduler */

static int task_scheduler_num_threads = 1;
#ifdef WITH_TBB_GLOBAL_CONTROL
static tbb::global_control *task_scheduler_global_control = nullptr;
#endif

void BLI_task_scheduler_init()
{
#ifdef WITH_TBB_GLOBAL_CONTROL
  const int num_threads_override = BLI_system_num_threads_override_get();

  if (num_threads_override > 0) {
    /* Override number of threads. This settings is used within the lifetime
     * of tbb::global_control, so we allocate it on the heap. */
    task_scheduler_global_control = OBJECT_GUARDED_NEW(
        tbb::global_control, tbb::global_control::max_allowed_parallelism, num_threads_override);
    task_scheduler_num_threads = num_threads_override;
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
  OBJECT_GUARDED_DELETE(task_scheduler_global_control, tbb::global_control);
#endif
}

int BLI_task_scheduler_num_threads()
{
  return task_scheduler_num_threads;
}

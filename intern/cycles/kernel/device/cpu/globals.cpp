/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "kernel/device/cpu/globals.h"
#include "kernel/osl/globals.h"

#include "util/guiding.h"  // IWYU pragma: keep
#include "util/profiling.h"

CCL_NAMESPACE_BEGIN

ThreadKernelGlobalsCPU::ThreadKernelGlobalsCPU(const KernelGlobalsCPU &kernel_globals,
                                               OSLGlobals *osl_globals,
                                               Profiler &cpu_profiler,
                                               const int thread_index)
    : KernelGlobalsCPU(kernel_globals),
#ifdef WITH_OSL
      osl(osl_globals, thread_index),
#endif
      cpu_profiler_(cpu_profiler)
{
#ifndef WITH_OSL
  (void)thread_index;
  (void)osl_globals;
#endif

#ifdef WITH_PATH_GUIDING
  opgl_path_segment_storage = make_unique<openpgl::cpp::PathSegmentStorage>();
#endif
}

void ThreadKernelGlobalsCPU::start_profiling()
{
  cpu_profiler_.add_state(&profiler);
}

void ThreadKernelGlobalsCPU::stop_profiling()
{
  cpu_profiler_.remove_state(&profiler);
}

CCL_NAMESPACE_END

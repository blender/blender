/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/cpu/kernel_thread_globals.h"

#include "kernel/osl/globals.h"

#include "util/profiling.h"

CCL_NAMESPACE_BEGIN

CPUKernelThreadGlobals::CPUKernelThreadGlobals(const KernelGlobalsCPU &kernel_globals,
                                               void *osl_globals_memory,
                                               Profiler &cpu_profiler)
    : KernelGlobalsCPU(kernel_globals), cpu_profiler_(cpu_profiler)
{
  clear_runtime_pointers();

#ifdef WITH_OSL
  OSLGlobals::thread_init(this, static_cast<OSLGlobals *>(osl_globals_memory));
#else
  (void)osl_globals_memory;
#endif

#ifdef WITH_PATH_GUIDING
  opgl_path_segment_storage = new openpgl::cpp::PathSegmentStorage();
#endif
}

CPUKernelThreadGlobals::CPUKernelThreadGlobals(CPUKernelThreadGlobals &&other) noexcept
    : KernelGlobalsCPU(std::move(other)), cpu_profiler_(other.cpu_profiler_)
{
  other.clear_runtime_pointers();
}

CPUKernelThreadGlobals::~CPUKernelThreadGlobals()
{
#ifdef WITH_OSL
  OSLGlobals::thread_free(this);
#endif

#ifdef WITH_PATH_GUIDING
  delete opgl_path_segment_storage;
  delete opgl_surface_sampling_distribution;
  delete opgl_volume_sampling_distribution;
#endif
}

CPUKernelThreadGlobals &CPUKernelThreadGlobals::operator=(CPUKernelThreadGlobals &&other)
{
  if (this == &other) {
    return *this;
  }

  *static_cast<KernelGlobalsCPU *>(this) = *static_cast<KernelGlobalsCPU *>(&other);

  other.clear_runtime_pointers();

  return *this;
}

void CPUKernelThreadGlobals::clear_runtime_pointers()
{
#ifdef WITH_OSL
  osl = nullptr;
#endif

#ifdef WITH_PATH_GUIDING
  opgl_sample_data_storage = nullptr;
  opgl_guiding_field = nullptr;

  opgl_path_segment_storage = nullptr;
  opgl_surface_sampling_distribution = nullptr;
  opgl_volume_sampling_distribution = nullptr;
#endif
}

void CPUKernelThreadGlobals::start_profiling()
{
  cpu_profiler_.add_state(&profiler);
}

void CPUKernelThreadGlobals::stop_profiling()
{
  cpu_profiler_.remove_state(&profiler);
}

CCL_NAMESPACE_END

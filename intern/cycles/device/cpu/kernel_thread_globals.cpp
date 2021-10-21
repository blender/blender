/*
 * Copyright 2011-2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "device/cpu/kernel_thread_globals.h"

// clang-format off
#include "kernel/osl/osl_shader.h"
#include "kernel/osl/osl_globals.h"
// clang-format on

#include "util/util_profiling.h"

CCL_NAMESPACE_BEGIN

CPUKernelThreadGlobals::CPUKernelThreadGlobals(const KernelGlobalsCPU &kernel_globals,
                                               void *osl_globals_memory,
                                               Profiler &cpu_profiler)
    : KernelGlobalsCPU(kernel_globals), cpu_profiler_(cpu_profiler)
{
  reset_runtime_memory();

#ifdef WITH_OSL
  OSLShader::thread_init(this, reinterpret_cast<OSLGlobals *>(osl_globals_memory));
#else
  (void)osl_globals_memory;
#endif
}

CPUKernelThreadGlobals::CPUKernelThreadGlobals(CPUKernelThreadGlobals &&other) noexcept
    : KernelGlobalsCPU(std::move(other)), cpu_profiler_(other.cpu_profiler_)
{
  other.reset_runtime_memory();
}

CPUKernelThreadGlobals::~CPUKernelThreadGlobals()
{
#ifdef WITH_OSL
  OSLShader::thread_free(this);
#endif
}

CPUKernelThreadGlobals &CPUKernelThreadGlobals::operator=(CPUKernelThreadGlobals &&other)
{
  if (this == &other) {
    return *this;
  }

  *static_cast<KernelGlobalsCPU *>(this) = *static_cast<KernelGlobalsCPU *>(&other);

  other.reset_runtime_memory();

  return *this;
}

void CPUKernelThreadGlobals::reset_runtime_memory()
{
#ifdef WITH_OSL
  osl = nullptr;
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

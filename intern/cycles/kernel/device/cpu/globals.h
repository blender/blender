/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Constant Globals */

#pragma once

#include "kernel/types.h"
#include "kernel/util/profiler.h"

#ifdef __OSL__
#  include "kernel/osl/globals.h"
#endif

#include "util/guiding.h"  // IWYU pragma: keep
#include "util/texture.h"  // IWYU pragma: keep
#include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

struct OSLGlobals;

/* On the CPU, we pass along the struct KernelGlobals to nearly everywhere in
 * the kernel, to access constant data. These are all stored as flat arrays.
 * these are really just standard arrays. We can't use actually globals because
 * multiple renders may be running inside the same process. */

/* Array for kernel data, with size to be able to assert on invalid data access. */
template<typename T> struct kernel_array {
  const ccl_always_inline T &fetch(const int index) const
  {
    kernel_assert(index >= 0 && index < width);
    return data[index];
  }

  T *data = nullptr;
  int width = 0;
};

/* Constant globals shared between all threads. */
struct KernelGlobalsCPU {
#define KERNEL_DATA_ARRAY(type, name) kernel_array<type> name;
#include "kernel/data_arrays.h"

  KernelData data = {};

  ProfilingState profiler;
};

/* Per-thread global state.
 *
 * To avoid pointer indirection, the constant globals are copied to each thread.
 *
 * This may not be ideal for cache pressure. Alternative would be to pass an
 * additional thread index to every function, and potentially to make the shared
 * part an actual global variable. That would match the GPU more closely, but
 * also require mutex locks for multiple Cycles instances. */
struct ThreadKernelGlobalsCPU : public KernelGlobalsCPU {
  ThreadKernelGlobalsCPU(const KernelGlobalsCPU &kernel_globals,
                         OSLGlobals *osl_globals_memory,
                         Profiler &cpu_profiler,
                         const int thread_index);

  ThreadKernelGlobalsCPU(ThreadKernelGlobalsCPU &other) = delete;
  ThreadKernelGlobalsCPU(ThreadKernelGlobalsCPU &&other) noexcept = default;
  ThreadKernelGlobalsCPU &operator=(const ThreadKernelGlobalsCPU &other) = delete;
  ThreadKernelGlobalsCPU &operator=(ThreadKernelGlobalsCPU &&other) = delete;

  void start_profiling();
  void stop_profiling();

#ifdef __OSL__
  OSLThreadData osl;
#endif

#if defined(__PATH_GUIDING__)
  /* Pointers to shared global data structures. */
  openpgl::cpp::SampleStorage *opgl_sample_data_storage = nullptr;
  openpgl::cpp::Field *opgl_guiding_field = nullptr;

  /* Local data structures owned by the thread. */
  unique_ptr<openpgl::cpp::PathSegmentStorage> opgl_path_segment_storage;
  unique_ptr<openpgl::cpp::SurfaceSamplingDistribution> opgl_surface_sampling_distribution;
  unique_ptr<openpgl::cpp::VolumeSamplingDistribution> opgl_volume_sampling_distribution;
#endif

 protected:
  Profiler &cpu_profiler_;
};

using KernelGlobals = const ThreadKernelGlobalsCPU *;

/* Abstraction macros */
#define kernel_data_fetch(name, index) (kg->name.fetch(index))
#define kernel_data_array(name) (kg->name.data)
#define kernel_data (kg->data)
#if defined(WITH_PATH_GUIDING)
#  define guiding_guiding_field kg->opgl_guiding_field
#  define guiding_ssd kg->opgl_surface_sampling_distribution
#  define guiding_vsd kg->opgl_volume_sampling_distribution
#endif

CCL_NAMESPACE_END

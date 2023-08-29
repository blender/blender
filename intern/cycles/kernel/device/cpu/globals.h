/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Constant Globals */

#pragma once

#include "kernel/tables.h"
#include "kernel/types.h"
#include "kernel/util/profiling.h"

#include "util/guiding.h"

CCL_NAMESPACE_BEGIN

/* On the CPU, we pass along the struct KernelGlobals to nearly everywhere in
 * the kernel, to access constant data. These are all stored as flat arrays.
 * these are really just standard arrays. We can't use actually globals because
 * multiple renders may be running inside the same process. */

#ifdef __OSL__
struct OSLGlobals;
struct OSLThreadData;
struct OSLShadingSystem;
#endif

/* Array for kernel data, with size to be able to assert on invalid data access. */
template<typename T> struct kernel_array {
  ccl_always_inline const T &fetch(int index) const
  {
    kernel_assert(index >= 0 && index < width);
    return data[index];
  }

  T *data;
  int width;
};

typedef struct KernelGlobalsCPU {
#define KERNEL_DATA_ARRAY(type, name) kernel_array<type> name;
#include "kernel/data_arrays.h"

  KernelData data;

#ifdef __OSL__
  /* On the CPU, we also have the OSL globals here. Most data structures are shared
   * with SVM, the difference is in the shaders and object/mesh attributes. */
  OSLGlobals *osl = nullptr;
  OSLShadingSystem *osl_ss = nullptr;
  OSLThreadData *osl_tdata = nullptr;
#endif

#ifdef __PATH_GUIDING__
  /* Pointers to global data structures. */
  openpgl::cpp::SampleStorage *opgl_sample_data_storage = nullptr;
  openpgl::cpp::Field *opgl_guiding_field = nullptr;

  /* Local data structures owned by the thread. */
  openpgl::cpp::PathSegmentStorage *opgl_path_segment_storage = nullptr;
  openpgl::cpp::SurfaceSamplingDistribution *opgl_surface_sampling_distribution = nullptr;
  openpgl::cpp::VolumeSamplingDistribution *opgl_volume_sampling_distribution = nullptr;
#endif

  /* **** Run-time data ****  */

  ProfilingState profiler;
} KernelGlobalsCPU;

typedef const KernelGlobalsCPU *ccl_restrict KernelGlobals;

/* Abstraction macros */
#define kernel_data_fetch(name, index) (kg->name.fetch(index))
#define kernel_data_array(name) (kg->name.data)
#define kernel_data (kg->data)

CCL_NAMESPACE_END

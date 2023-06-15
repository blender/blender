/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Constant Globals */

#pragma once

#include "kernel/types.h"

#include "kernel/integrator/state.h"

#include "kernel/util/profiling.h"

CCL_NAMESPACE_BEGIN

/* Not actually used, just a NULL pointer that gets passed everywhere, which we
 * hope gets optimized out by the compiler. */
struct KernelGlobalsGPU {
  int unused[1];
};
typedef ccl_global const KernelGlobalsGPU *ccl_restrict KernelGlobals;

/* Launch parameters */
struct KernelParamsOptiX {
  /* Kernel arguments */
  const int *path_index_array;
  float *render_buffer;
  int offset;

  /* Global scene data and textures */
  KernelData data;
#define KERNEL_DATA_ARRAY(type, name) const type *name;
#include "kernel/data_arrays.h"

  /* Integrator state */
  IntegratorStateGPU integrator_state;
};

#ifdef __NVCC__
extern "C"
#  ifndef __CUDACC_RDC__
    static
#  endif
    __constant__ KernelParamsOptiX kernel_params;
#endif

/* Abstraction macros */
#define kernel_data kernel_params.data
#define kernel_data_array(name) kernel_params.name
#define kernel_data_fetch(name, index) kernel_params.name[(index)]
#define kernel_integrator_state kernel_params.integrator_state

CCL_NAMESPACE_END

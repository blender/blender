/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

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

  /* Global scene data and textures */
  KernelData data;
#define KERNEL_TEX(type, name) const type *name;
#include "kernel/textures.h"

  /* Integrator state */
  IntegratorStateGPU __integrator_state;
};

#ifdef __NVCC__
extern "C" static __constant__ KernelParamsOptiX __params;
#endif

/* Abstraction macros */
#define kernel_data __params.data
#define kernel_tex_array(t) __params.t
#define kernel_tex_fetch(t, index) __params.t[(index)]
#define kernel_integrator_state __params.__integrator_state

CCL_NAMESPACE_END

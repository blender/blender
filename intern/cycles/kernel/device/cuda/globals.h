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

/* Global scene data and textures */
__constant__ KernelData __data;
#define KERNEL_TEX(type, name) const __constant__ __device__ type *name;
#include "kernel/textures.h"

/* Integrator state */
__constant__ IntegratorStateGPU __integrator_state;

/* Abstraction macros */
#define kernel_data __data
#define kernel_tex_fetch(t, index) t[(index)]
#define kernel_tex_array(t) (t)
#define kernel_integrator_state __integrator_state

CCL_NAMESPACE_END

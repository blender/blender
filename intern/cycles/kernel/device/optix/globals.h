/*
 * Copyright 2011-2013 Blender Foundation
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

/* Constant Globals */

#pragma once

#include "kernel/kernel_profiling.h"
#include "kernel/kernel_types.h"

#include "kernel/integrator/integrator_state.h"

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
#include "kernel/kernel_textures.h"

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

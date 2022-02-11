/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

/* Constant Globals */

#include "kernel/types.h"
#include "kernel/util/profiling.h"

#include "kernel/integrator/state.h"

CCL_NAMESPACE_BEGIN

typedef struct KernelParamsMetal {

#define KERNEL_TEX(type, name) ccl_global const type *name;
#include "kernel/textures.h"
#undef KERNEL_TEX

  const IntegratorStateGPU __integrator_state;
  const KernelData data;

} KernelParamsMetal;

typedef struct KernelGlobalsGPU {
  int unused[1];
} KernelGlobalsGPU;

typedef ccl_global const KernelGlobalsGPU *ccl_restrict KernelGlobals;

#define kernel_data launch_params_metal.data
#define kernel_integrator_state launch_params_metal.__integrator_state

/* data lookup defines */

#define kernel_tex_fetch(tex, index) launch_params_metal.tex[index]
#define kernel_tex_array(tex) launch_params_metal.tex

CCL_NAMESPACE_END

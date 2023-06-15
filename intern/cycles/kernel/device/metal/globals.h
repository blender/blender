/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Constant Globals */

#include "kernel/types.h"
#include "kernel/util/profiling.h"

#include "kernel/integrator/state.h"

CCL_NAMESPACE_BEGIN

typedef struct KernelParamsMetal {

#define KERNEL_DATA_ARRAY(type, name) ccl_global const type *name;
#include "kernel/data_arrays.h"
#undef KERNEL_DATA_ARRAY

  const IntegratorStateGPU integrator_state;
  const KernelData data;

} KernelParamsMetal;

typedef struct KernelGlobalsGPU {
  int unused[1];
} KernelGlobalsGPU;

typedef ccl_global const KernelGlobalsGPU *ccl_restrict KernelGlobals;

/* Abstraction macros */
#define kernel_data launch_params_metal.data
#define kernel_data_fetch(name, index) launch_params_metal.name[index]
#define kernel_data_array(name) launch_params_metal.name
#define kernel_integrator_state launch_params_metal.integrator_state

CCL_NAMESPACE_END

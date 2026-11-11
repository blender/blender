/* SPDX-FileCopyrightText: 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Constant Globals */

#pragma once

#include "kernel/types.h"

#include "kernel/integrator/state.h"
#include "kernel/util/profiler.h"

#include "util/color.h"
#include "util/types_image.h"

CCL_NAMESPACE_BEGIN

struct KernelParamsOneAPI;

/* Used in Embree callbacks */
struct KernelGlobalsGPU {
  KernelParamsOneAPI *params;
  sycl::kernel_handler kernel_handler;
};
using KernelGlobals = ccl_global KernelGlobalsGPU *ccl_restrict;

struct KernelParamsOneAPI {
  /* Global scene data and textures */
  KernelData data;
#define KERNEL_DATA_ARRAY(type, name) const type *name;
#define KERNEL_DATA_ARRAY_WRITABLE(type, name) type *name;
#include "kernel/data_arrays.h"

  /* Integrator state */
  IntegratorStateGPU integrator_state;
};

#define kernel_data (this->data)
#define kernel_integrator_state (this->integrator_state)

/* data lookup defines */

#define kernel_data_fetch(name, index) this->name[(index)]
#define kernel_data_array(name) this->name

CCL_NAMESPACE_END

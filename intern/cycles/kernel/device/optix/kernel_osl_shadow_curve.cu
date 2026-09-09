/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#define WITH_OSL

/* Shadow shading raygens for OSL, loaded as a separate optix module so they
 * can be compiled in parallel with the base OSL module. */

#include "kernel/device/optix/compat.h"
#include "kernel/device/optix/globals.h"

#include "kernel/film/data_passes.h"

#include "kernel/bake/bake.h"

extern "C" __global__ void __raygen__kernel_optix_shader_eval_curve_shadow_transparency()
{
  KernelShaderEvalInput *const input = (KernelShaderEvalInput *)kernel_params.path_index_array;
  float *const output = kernel_params.render_buffer;
  uint *const cache_miss = kernel_params.shader_eval_cache_miss;
  const int global_index = kernel_params.shader_eval_offset + optixGetLaunchIndex().x;
  kernel_curve_shadow_transparency_evaluate(nullptr, input, output, cache_miss, global_index);
}

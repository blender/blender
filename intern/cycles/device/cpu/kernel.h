/*
 * Copyright 2011-2021 Blender Foundation
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

#pragma once

#include "device/cpu/kernel_function.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

struct KernelGlobalsCPU;
struct IntegratorStateCPU;
struct TileInfo;

class CPUKernels {
 public:
  /* Integrator. */

  using IntegratorFunction =
      CPUKernelFunction<void (*)(const KernelGlobalsCPU *kg, IntegratorStateCPU *state)>;
  using IntegratorShadeFunction = CPUKernelFunction<void (*)(
      const KernelGlobalsCPU *kg, IntegratorStateCPU *state, ccl_global float *render_buffer)>;
  using IntegratorInitFunction = CPUKernelFunction<bool (*)(const KernelGlobalsCPU *kg,
                                                            IntegratorStateCPU *state,
                                                            KernelWorkTile *tile,
                                                            ccl_global float *render_buffer)>;

  IntegratorInitFunction integrator_init_from_camera;
  IntegratorInitFunction integrator_init_from_bake;
  IntegratorFunction integrator_intersect_closest;
  IntegratorFunction integrator_intersect_shadow;
  IntegratorFunction integrator_intersect_subsurface;
  IntegratorFunction integrator_intersect_volume_stack;
  IntegratorShadeFunction integrator_shade_background;
  IntegratorShadeFunction integrator_shade_light;
  IntegratorShadeFunction integrator_shade_shadow;
  IntegratorShadeFunction integrator_shade_surface;
  IntegratorShadeFunction integrator_shade_volume;
  IntegratorShadeFunction integrator_megakernel;

  /* Shader evaluation. */

  using ShaderEvalFunction = CPUKernelFunction<void (*)(
      const KernelGlobalsCPU *kg, const KernelShaderEvalInput *, float *, const int)>;

  ShaderEvalFunction shader_eval_displace;
  ShaderEvalFunction shader_eval_background;
  ShaderEvalFunction shader_eval_curve_shadow_transparency;

  /* Adaptive stopping. */

  using AdaptiveSamplingConvergenceCheckFunction =
      CPUKernelFunction<bool (*)(const KernelGlobalsCPU *kg,
                                 ccl_global float *render_buffer,
                                 int x,
                                 int y,
                                 float threshold,
                                 bool reset,
                                 int offset,
                                 int stride)>;

  using AdaptiveSamplingFilterXFunction =
      CPUKernelFunction<void (*)(const KernelGlobalsCPU *kg,
                                 ccl_global float *render_buffer,
                                 int y,
                                 int start_x,
                                 int width,
                                 int offset,
                                 int stride)>;

  using AdaptiveSamplingFilterYFunction =
      CPUKernelFunction<void (*)(const KernelGlobalsCPU *kg,
                                 ccl_global float *render_buffer,
                                 int x,
                                 int start_y,
                                 int height,
                                 int offset,
                                 int stride)>;

  AdaptiveSamplingConvergenceCheckFunction adaptive_sampling_convergence_check;

  AdaptiveSamplingFilterXFunction adaptive_sampling_filter_x;
  AdaptiveSamplingFilterYFunction adaptive_sampling_filter_y;

  /* Cryptomatte. */

  using CryptomattePostprocessFunction = CPUKernelFunction<void (*)(
      const KernelGlobalsCPU *kg, ccl_global float *render_buffer, int pixel_index)>;

  CryptomattePostprocessFunction cryptomatte_postprocess;

  CPUKernels();
};

CCL_NAMESPACE_END

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

/* Templated common declaration part of all CPU kernels. */

/* --------------------------------------------------------------------
 * Integrator.
 */

#define KERNEL_INTEGRATOR_FUNCTION(name) \
  void KERNEL_FUNCTION_FULL_NAME(integrator_##name)(const KernelGlobalsCPU *ccl_restrict kg, \
                                                    IntegratorStateCPU *state)

#define KERNEL_INTEGRATOR_SHADE_FUNCTION(name) \
  void KERNEL_FUNCTION_FULL_NAME(integrator_##name)(const KernelGlobalsCPU *ccl_restrict kg, \
                                                    IntegratorStateCPU *state, \
                                                    ccl_global float *render_buffer)

#define KERNEL_INTEGRATOR_INIT_FUNCTION(name) \
  bool KERNEL_FUNCTION_FULL_NAME(integrator_##name)(const KernelGlobalsCPU *ccl_restrict kg, \
                                                    IntegratorStateCPU *state, \
                                                    KernelWorkTile *tile, \
                                                    ccl_global float *render_buffer)

KERNEL_INTEGRATOR_INIT_FUNCTION(init_from_camera);
KERNEL_INTEGRATOR_INIT_FUNCTION(init_from_bake);
KERNEL_INTEGRATOR_FUNCTION(intersect_closest);
KERNEL_INTEGRATOR_FUNCTION(intersect_shadow);
KERNEL_INTEGRATOR_FUNCTION(intersect_subsurface);
KERNEL_INTEGRATOR_FUNCTION(intersect_volume_stack);
KERNEL_INTEGRATOR_SHADE_FUNCTION(shade_background);
KERNEL_INTEGRATOR_SHADE_FUNCTION(shade_light);
KERNEL_INTEGRATOR_SHADE_FUNCTION(shade_shadow);
KERNEL_INTEGRATOR_SHADE_FUNCTION(shade_surface);
KERNEL_INTEGRATOR_SHADE_FUNCTION(shade_volume);
KERNEL_INTEGRATOR_SHADE_FUNCTION(megakernel);

#undef KERNEL_INTEGRATOR_FUNCTION
#undef KERNEL_INTEGRATOR_INIT_FUNCTION
#undef KERNEL_INTEGRATOR_SHADE_FUNCTION

/* --------------------------------------------------------------------
 * Shader evaluation.
 */

void KERNEL_FUNCTION_FULL_NAME(shader_eval_background)(const KernelGlobalsCPU *kg,
                                                       const KernelShaderEvalInput *input,
                                                       float *output,
                                                       const int offset);
void KERNEL_FUNCTION_FULL_NAME(shader_eval_displace)(const KernelGlobalsCPU *kg,
                                                     const KernelShaderEvalInput *input,
                                                     float *output,
                                                     const int offset);
void KERNEL_FUNCTION_FULL_NAME(shader_eval_curve_shadow_transparency)(
    const KernelGlobalsCPU *kg,
    const KernelShaderEvalInput *input,
    float *output,
    const int offset);

/* --------------------------------------------------------------------
 * Adaptive sampling.
 */

bool KERNEL_FUNCTION_FULL_NAME(adaptive_sampling_convergence_check)(
    const KernelGlobalsCPU *kg,
    ccl_global float *render_buffer,
    int x,
    int y,
    float threshold,
    bool reset,
    int offset,
    int stride);

void KERNEL_FUNCTION_FULL_NAME(adaptive_sampling_filter_x)(const KernelGlobalsCPU *kg,
                                                           ccl_global float *render_buffer,
                                                           int y,
                                                           int start_x,
                                                           int width,
                                                           int offset,
                                                           int stride);
void KERNEL_FUNCTION_FULL_NAME(adaptive_sampling_filter_y)(const KernelGlobalsCPU *kg,
                                                           ccl_global float *render_buffer,
                                                           int x,
                                                           int start_y,
                                                           int height,
                                                           int offset,
                                                           int stride);

/* --------------------------------------------------------------------
 * Cryptomatte.
 */

void KERNEL_FUNCTION_FULL_NAME(cryptomatte_postprocess)(const KernelGlobalsCPU *kg,
                                                        ccl_global float *render_buffer,
                                                        int pixel_index);

#undef KERNEL_ARCH

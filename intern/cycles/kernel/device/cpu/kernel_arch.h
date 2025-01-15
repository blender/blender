/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Templated common declaration part of all CPU kernels. */

/* --------------------------------------------------------------------
 * Integrator.
 */

#define KERNEL_INTEGRATOR_FUNCTION(name) \
  void KERNEL_FUNCTION_FULL_NAME(integrator_##name)( \
      const ThreadKernelGlobalsCPU *ccl_restrict kg, IntegratorStateCPU *state)

#define KERNEL_INTEGRATOR_SHADE_FUNCTION(name) \
  void KERNEL_FUNCTION_FULL_NAME(integrator_##name)( \
      const ThreadKernelGlobalsCPU *ccl_restrict kg, \
      IntegratorStateCPU *state, \
      ccl_global float *render_buffer)

#define KERNEL_INTEGRATOR_INIT_FUNCTION(name) \
  bool KERNEL_FUNCTION_FULL_NAME(integrator_##name)( \
      const ThreadKernelGlobalsCPU *ccl_restrict kg, \
      IntegratorStateCPU *state, \
      KernelWorkTile *tile, \
      ccl_global float *render_buffer)

KERNEL_INTEGRATOR_INIT_FUNCTION(init_from_camera);
KERNEL_INTEGRATOR_INIT_FUNCTION(init_from_bake);
KERNEL_INTEGRATOR_SHADE_FUNCTION(megakernel);

#undef KERNEL_INTEGRATOR_FUNCTION
#undef KERNEL_INTEGRATOR_INIT_FUNCTION
#undef KERNEL_INTEGRATOR_SHADE_FUNCTION

#define KERNEL_FILM_CONVERT_FUNCTION(name) \
  void KERNEL_FUNCTION_FULL_NAME(film_convert_##name)(const KernelFilmConvert *kfilm_convert, \
                                                      const float *buffer, \
                                                      float *pixel, \
                                                      const int width, \
                                                      const int buffer_stride, \
                                                      const int pixel_stride); \
  void KERNEL_FUNCTION_FULL_NAME(film_convert_half_rgba_##name)( \
      const KernelFilmConvert *kfilm_convert, \
      const float *buffer, \
      half4 *pixel, \
      const int width, \
      const int buffer_stride);

KERNEL_FILM_CONVERT_FUNCTION(depth)
KERNEL_FILM_CONVERT_FUNCTION(mist)
KERNEL_FILM_CONVERT_FUNCTION(sample_count)
KERNEL_FILM_CONVERT_FUNCTION(float)

KERNEL_FILM_CONVERT_FUNCTION(light_path)
KERNEL_FILM_CONVERT_FUNCTION(float3)

KERNEL_FILM_CONVERT_FUNCTION(motion)
KERNEL_FILM_CONVERT_FUNCTION(cryptomatte)
KERNEL_FILM_CONVERT_FUNCTION(shadow_catcher)
KERNEL_FILM_CONVERT_FUNCTION(shadow_catcher_matte_with_shadow)
KERNEL_FILM_CONVERT_FUNCTION(combined)
KERNEL_FILM_CONVERT_FUNCTION(float4)

#undef KERNEL_FILM_CONVERT_FUNCTION

/* --------------------------------------------------------------------
 * Shader evaluation.
 */

void KERNEL_FUNCTION_FULL_NAME(shader_eval_background)(const ThreadKernelGlobalsCPU *kg,
                                                       const KernelShaderEvalInput *input,
                                                       float *output,
                                                       const int offset);
void KERNEL_FUNCTION_FULL_NAME(shader_eval_displace)(const ThreadKernelGlobalsCPU *kg,
                                                     const KernelShaderEvalInput *input,
                                                     float *output,
                                                     const int offset);
void KERNEL_FUNCTION_FULL_NAME(shader_eval_curve_shadow_transparency)(
    const ThreadKernelGlobalsCPU *kg,
    const KernelShaderEvalInput *input,
    float *output,
    const int offset);

/* --------------------------------------------------------------------
 * Adaptive sampling.
 */

bool KERNEL_FUNCTION_FULL_NAME(adaptive_sampling_convergence_check)(
    const ThreadKernelGlobalsCPU *kg,
    ccl_global float *render_buffer,
    const int x,
    const int y,
    const float threshold,
    const int reset,
    const int offset,
    int stride);

void KERNEL_FUNCTION_FULL_NAME(adaptive_sampling_filter_x)(const ThreadKernelGlobalsCPU *kg,
                                                           ccl_global float *render_buffer,
                                                           const int y,
                                                           const int start_x,
                                                           const int width,
                                                           const int offset,
                                                           int stride);
void KERNEL_FUNCTION_FULL_NAME(adaptive_sampling_filter_y)(const ThreadKernelGlobalsCPU *kg,
                                                           ccl_global float *render_buffer,
                                                           const int x,
                                                           const int start_y,
                                                           const int height,
                                                           const int offset,
                                                           int stride);

/* --------------------------------------------------------------------
 * Cryptomatte.
 */

void KERNEL_FUNCTION_FULL_NAME(cryptomatte_postprocess)(const ThreadKernelGlobalsCPU *kg,
                                                        ccl_global float *render_buffer,
                                                        int pixel_index);

#undef KERNEL_ARCH

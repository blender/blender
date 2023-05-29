/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "device/cpu/kernel_function.h"
#include "util/half.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

struct KernelGlobalsCPU;
struct KernelFilmConvert;
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
  IntegratorShadeFunction integrator_intersect_closest;
  IntegratorFunction integrator_intersect_shadow;
  IntegratorFunction integrator_intersect_subsurface;
  IntegratorFunction integrator_intersect_volume_stack;
  IntegratorFunction integrator_intersect_dedicated_light;
  IntegratorShadeFunction integrator_shade_background;
  IntegratorShadeFunction integrator_shade_light;
  IntegratorShadeFunction integrator_shade_shadow;
  IntegratorShadeFunction integrator_shade_surface;
  IntegratorShadeFunction integrator_shade_volume;
  IntegratorShadeFunction integrator_shade_dedicated_light;
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

  /* Film Convert. */
  using FilmConvertFunction = CPUKernelFunction<void (*)(const KernelFilmConvert *kfilm_convert,
                                                         const float *buffer,
                                                         float *pixel,
                                                         const int width,
                                                         const int buffer_stride,
                                                         const int pixel_stride)>;
  using FilmConvertHalfRGBAFunction =
      CPUKernelFunction<void (*)(const KernelFilmConvert *kfilm_convert,
                                 const float *buffer,
                                 half4 *pixel,
                                 const int width,
                                 const int buffer_stride)>;

#define KERNEL_FILM_CONVERT_FUNCTION(name) \
  FilmConvertFunction film_convert_##name; \
  FilmConvertHalfRGBAFunction film_convert_half_rgba_##name;

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

  CPUKernels();
};

CCL_NAMESPACE_END

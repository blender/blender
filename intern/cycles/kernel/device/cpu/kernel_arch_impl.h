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

/* Templated common implementation part of all CPU kernels.
 *
 * The idea is that particular .cpp files sets needed optimization flags and
 * simply includes this file without worry of copying actual implementation over.
 */

#pragma once

// clang-format off
#include "kernel/device/cpu/compat.h"

#ifndef KERNEL_STUB
#    include "kernel/device/cpu/globals.h"
#    include "kernel/device/cpu/image.h"

#    include "kernel/integrator/state.h"
#    include "kernel/integrator/state_flow.h"
#    include "kernel/integrator/state_util.h"

#    include "kernel/integrator/init_from_camera.h"
#    include "kernel/integrator/init_from_bake.h"
#    include "kernel/integrator/intersect_closest.h"
#    include "kernel/integrator/intersect_shadow.h"
#    include "kernel/integrator/intersect_subsurface.h"
#    include "kernel/integrator/intersect_volume_stack.h"
#    include "kernel/integrator/shade_background.h"
#    include "kernel/integrator/shade_light.h"
#    include "kernel/integrator/shade_shadow.h"
#    include "kernel/integrator/shade_surface.h"
#    include "kernel/integrator/shade_volume.h"
#    include "kernel/integrator/megakernel.h"

#    include "kernel/film/adaptive_sampling.h"
#    include "kernel/film/read.h"
#    include "kernel/film/id_passes.h"

#    include "kernel/bake/bake.h"

#else
#  define STUB_ASSERT(arch, name) \
    assert(!(#name " kernel stub for architecture " #arch " was called!"))
#endif   /* KERNEL_STUB */
// clang-format on

CCL_NAMESPACE_BEGIN

/* --------------------------------------------------------------------
 * Integrator.
 */

#ifdef KERNEL_STUB
#  define KERNEL_INVOKE(name, ...) (STUB_ASSERT(KERNEL_ARCH, name), 0)
#else
#  define KERNEL_INVOKE(name, ...) integrator_##name(__VA_ARGS__)
#endif

/* TODO: Either use something like get_work_pixel(), or simplify tile which is passed here, so
 * that it does not contain unused fields. */
#define DEFINE_INTEGRATOR_INIT_KERNEL(name) \
  bool KERNEL_FUNCTION_FULL_NAME(integrator_##name)(const KernelGlobalsCPU *kg, \
                                                    IntegratorStateCPU *state, \
                                                    KernelWorkTile *tile, \
                                                    ccl_global float *render_buffer) \
  { \
    return KERNEL_INVOKE( \
        name, kg, state, tile, render_buffer, tile->x, tile->y, tile->start_sample); \
  }

#define DEFINE_INTEGRATOR_KERNEL(name) \
  void KERNEL_FUNCTION_FULL_NAME(integrator_##name)(const KernelGlobalsCPU *kg, \
                                                    IntegratorStateCPU *state) \
  { \
    KERNEL_INVOKE(name, kg, state); \
  }

#define DEFINE_INTEGRATOR_SHADE_KERNEL(name) \
  void KERNEL_FUNCTION_FULL_NAME(integrator_##name)( \
      const KernelGlobalsCPU *kg, IntegratorStateCPU *state, ccl_global float *render_buffer) \
  { \
    KERNEL_INVOKE(name, kg, state, render_buffer); \
  }

#define DEFINE_INTEGRATOR_SHADOW_KERNEL(name) \
  void KERNEL_FUNCTION_FULL_NAME(integrator_##name)(const KernelGlobalsCPU *kg, \
                                                    IntegratorStateCPU *state) \
  { \
    KERNEL_INVOKE(name, kg, &state->shadow); \
  }

#define DEFINE_INTEGRATOR_SHADOW_SHADE_KERNEL(name) \
  void KERNEL_FUNCTION_FULL_NAME(integrator_##name)( \
      const KernelGlobalsCPU *kg, IntegratorStateCPU *state, ccl_global float *render_buffer) \
  { \
    KERNEL_INVOKE(name, kg, &state->shadow, render_buffer); \
  }

DEFINE_INTEGRATOR_INIT_KERNEL(init_from_camera)
DEFINE_INTEGRATOR_INIT_KERNEL(init_from_bake)
DEFINE_INTEGRATOR_KERNEL(intersect_closest)
DEFINE_INTEGRATOR_KERNEL(intersect_subsurface)
DEFINE_INTEGRATOR_KERNEL(intersect_volume_stack)
DEFINE_INTEGRATOR_SHADE_KERNEL(shade_background)
DEFINE_INTEGRATOR_SHADE_KERNEL(shade_light)
DEFINE_INTEGRATOR_SHADE_KERNEL(shade_surface)
DEFINE_INTEGRATOR_SHADE_KERNEL(shade_volume)
DEFINE_INTEGRATOR_SHADE_KERNEL(megakernel)
DEFINE_INTEGRATOR_SHADOW_KERNEL(intersect_shadow)
DEFINE_INTEGRATOR_SHADOW_SHADE_KERNEL(shade_shadow)

/* --------------------------------------------------------------------
 * Shader evaluation.
 */

void KERNEL_FUNCTION_FULL_NAME(shader_eval_displace)(const KernelGlobalsCPU *kg,
                                                     const KernelShaderEvalInput *input,
                                                     float *output,
                                                     const int offset)
{
#ifdef KERNEL_STUB
  STUB_ASSERT(KERNEL_ARCH, shader_eval_displace);
#else
  kernel_displace_evaluate(kg, input, output, offset);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(shader_eval_background)(const KernelGlobalsCPU *kg,
                                                       const KernelShaderEvalInput *input,
                                                       float *output,
                                                       const int offset)
{
#ifdef KERNEL_STUB
  STUB_ASSERT(KERNEL_ARCH, shader_eval_background);
#else
  kernel_background_evaluate(kg, input, output, offset);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(shader_eval_curve_shadow_transparency)(
    const KernelGlobalsCPU *kg,
    const KernelShaderEvalInput *input,
    float *output,
    const int offset)
{
#ifdef KERNEL_STUB
  STUB_ASSERT(KERNEL_ARCH, shader_eval_curve_shadow_transparency);
#else
  kernel_curve_shadow_transparency_evaluate(kg, input, output, offset);
#endif
}

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
    int stride)
{
#ifdef KERNEL_STUB
  STUB_ASSERT(KERNEL_ARCH, adaptive_sampling_convergence_check);
  return false;
#else
  return kernel_adaptive_sampling_convergence_check(
      kg, render_buffer, x, y, threshold, reset, offset, stride);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(adaptive_sampling_filter_x)(const KernelGlobalsCPU *kg,
                                                           ccl_global float *render_buffer,
                                                           int y,
                                                           int start_x,
                                                           int width,
                                                           int offset,
                                                           int stride)
{
#ifdef KERNEL_STUB
  STUB_ASSERT(KERNEL_ARCH, adaptive_sampling_filter_x);
#else
  kernel_adaptive_sampling_filter_x(kg, render_buffer, y, start_x, width, offset, stride);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(adaptive_sampling_filter_y)(const KernelGlobalsCPU *kg,
                                                           ccl_global float *render_buffer,
                                                           int x,
                                                           int start_y,
                                                           int height,
                                                           int offset,
                                                           int stride)
{
#ifdef KERNEL_STUB
  STUB_ASSERT(KERNEL_ARCH, adaptive_sampling_filter_y);
#else
  kernel_adaptive_sampling_filter_y(kg, render_buffer, x, start_y, height, offset, stride);
#endif
}

/* --------------------------------------------------------------------
 * Cryptomatte.
 */

void KERNEL_FUNCTION_FULL_NAME(cryptomatte_postprocess)(const KernelGlobalsCPU *kg,
                                                        ccl_global float *render_buffer,
                                                        int pixel_index)
{
#ifdef KERNEL_STUB
  STUB_ASSERT(KERNEL_ARCH, cryptomatte_postprocess);
#else
  kernel_cryptomatte_post(kg, render_buffer, pixel_index);
#endif
}

#undef KERNEL_INVOKE
#undef DEFINE_INTEGRATOR_KERNEL
#undef DEFINE_INTEGRATOR_SHADE_KERNEL
#undef DEFINE_INTEGRATOR_INIT_KERNEL

#undef KERNEL_STUB
#undef STUB_ASSERT
#undef KERNEL_ARCH

CCL_NAMESPACE_END

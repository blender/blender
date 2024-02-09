/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/cpu/kernel.h"

#include "kernel/device/cpu/kernel.h"

CCL_NAMESPACE_BEGIN

#define KERNEL_FUNCTIONS(name) \
  KERNEL_NAME_EVAL(cpu, name), KERNEL_NAME_EVAL(cpu_sse2, name), \
      KERNEL_NAME_EVAL(cpu_sse42, name), KERNEL_NAME_EVAL(cpu_avx2, name)

#define REGISTER_KERNEL(name) name(KERNEL_FUNCTIONS(name))
#define REGISTER_KERNEL_FILM_CONVERT(name) \
  film_convert_##name(KERNEL_FUNCTIONS(film_convert_##name)), \
      film_convert_half_rgba_##name(KERNEL_FUNCTIONS(film_convert_half_rgba_##name))

CPUKernels::CPUKernels()
    : /* Integrator. */
      REGISTER_KERNEL(integrator_init_from_camera),
      REGISTER_KERNEL(integrator_init_from_bake),
      REGISTER_KERNEL(integrator_intersect_closest),
      REGISTER_KERNEL(integrator_intersect_shadow),
      REGISTER_KERNEL(integrator_intersect_subsurface),
      REGISTER_KERNEL(integrator_intersect_volume_stack),
      REGISTER_KERNEL(integrator_intersect_dedicated_light),
      REGISTER_KERNEL(integrator_shade_background),
      REGISTER_KERNEL(integrator_shade_light),
      REGISTER_KERNEL(integrator_shade_shadow),
      REGISTER_KERNEL(integrator_shade_surface),
      REGISTER_KERNEL(integrator_shade_volume),
      REGISTER_KERNEL(integrator_shade_dedicated_light),
      REGISTER_KERNEL(integrator_megakernel),
      /* Shader evaluation. */
      REGISTER_KERNEL(shader_eval_displace),
      REGISTER_KERNEL(shader_eval_background),
      REGISTER_KERNEL(shader_eval_curve_shadow_transparency),
      /* Adaptive sampling. */
      REGISTER_KERNEL(adaptive_sampling_convergence_check),
      REGISTER_KERNEL(adaptive_sampling_filter_x),
      REGISTER_KERNEL(adaptive_sampling_filter_y),
      /* Cryptomatte. */
      REGISTER_KERNEL(cryptomatte_postprocess),
      /* Film Convert. */
      REGISTER_KERNEL_FILM_CONVERT(depth),
      REGISTER_KERNEL_FILM_CONVERT(mist),
      REGISTER_KERNEL_FILM_CONVERT(sample_count),
      REGISTER_KERNEL_FILM_CONVERT(float),
      REGISTER_KERNEL_FILM_CONVERT(light_path),
      REGISTER_KERNEL_FILM_CONVERT(float3),
      REGISTER_KERNEL_FILM_CONVERT(motion),
      REGISTER_KERNEL_FILM_CONVERT(cryptomatte),
      REGISTER_KERNEL_FILM_CONVERT(shadow_catcher),
      REGISTER_KERNEL_FILM_CONVERT(shadow_catcher_matte_with_shadow),
      REGISTER_KERNEL_FILM_CONVERT(combined),
      REGISTER_KERNEL_FILM_CONVERT(float4)
{
}

#undef REGISTER_KERNEL
#undef REGISTER_KERNEL_FILM_CONVERT
#undef KERNEL_FUNCTIONS

CCL_NAMESPACE_END

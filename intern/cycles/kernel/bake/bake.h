/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/camera/projection.h"
#include "kernel/integrator/displacement_shader.h"
#include "kernel/integrator/surface_shader.h"
#include "kernel/integrator/volume_shader.h"

#include "kernel/geom/object.h"
#include "kernel/geom/shader_data.h"

#include "kernel/util/colorspace.h"

CCL_NAMESPACE_BEGIN

ccl_device void kernel_displace_evaluate(KernelGlobals kg,
                                         const ccl_global KernelShaderEvalInput *input,
                                         ccl_global float *output,
                                         const int offset)
{
  /* Setup shader data. */
  const KernelShaderEvalInput in = input[offset];

  ShaderData sd;
  shader_setup_from_displace(kg, &sd, in.object, in.prim, in.u, in.v);

  /* Evaluate displacement shader. */
  ConstIntegratorBakeState state;
  const float3 P = sd.P;
  displacement_shader_eval(kg, state, &sd);
  float3 D = sd.P - P;

  object_inverse_dir_transform(kg, &sd, &D);

#ifdef __KERNEL_DEBUG_NAN__
  if (!isfinite_safe(D)) {
    kernel_assert(!"Cycles displacement with non-finite value detected");
  }
#endif

  /* Ensure finite displacement, preventing BVH from becoming degenerate and avoiding possible
   * traversal issues caused by non-finite math. */
  D = ensure_finite(D);

  /* Write output. */
  output[offset * 3 + 0] += D.x;
  output[offset * 3 + 1] += D.y;
  output[offset * 3 + 2] += D.z;
}

ccl_device void kernel_background_evaluate(KernelGlobals kg,
                                           const ccl_global KernelShaderEvalInput *input,
                                           ccl_global float *output,
                                           const int offset)
{
  /* Setup ray */
  const KernelShaderEvalInput in = input[offset];
  const float3 ray_P = zero_float3();
  const float3 ray_D = equirectangular_to_direction(in.u, in.v);
  const float ray_time = 0.5f;

  /* Setup shader data. */
  ShaderData sd;
  shader_setup_from_background(kg, &sd, ray_P, ray_D, ray_time);

  /* Evaluate shader.
   * This is being evaluated for all BSDFs, so path flag does not contain a specific type.
   * However, we want to flag the ray visibility to ignore the sun in the background map. */
  ConstIntegratorBakeState state;
  const uint32_t path_flag = PATH_RAY_EMISSION | PATH_RAY_IMPORTANCE_BAKE;
  surface_shader_eval<KERNEL_FEATURE_NODE_MASK_SURFACE_LIGHT &
                      ~(KERNEL_FEATURE_NODE_RAYTRACE | KERNEL_FEATURE_NODE_LIGHT_PATH)>(
      kg, state, &sd, nullptr, path_flag);
  Spectrum color = surface_shader_background(&sd);

#ifdef __KERNEL_DEBUG_NAN__
  if (!isfinite_safe(color)) {
    kernel_assert(!"Cycles background with non-finite value detected");
  }
#endif

  /* Ensure finite color, avoiding possible numerical instabilities in the path tracing kernels. */
  color = ensure_finite(color);

  const float3 color_rgb = spectrum_to_rgb(color);

  /* Write output. */
  output[offset * 3 + 0] += color_rgb.x;
  output[offset * 3 + 1] += color_rgb.y;
  output[offset * 3 + 2] += color_rgb.z;
}

ccl_device void kernel_curve_shadow_transparency_evaluate(
    KernelGlobals kg,
    const ccl_global KernelShaderEvalInput *input,
    ccl_global float *output,
    const int offset)
{
#ifdef __HAIR__
  /* Setup shader data. */
  const KernelShaderEvalInput in = input[offset];

  ShaderData sd;
  shader_setup_from_curve(kg, &sd, in.object, in.prim, __float_as_int(in.v), in.u);

  /* Evaluate transparency. */
  ConstIntegratorBakeState state;
  surface_shader_eval<KERNEL_FEATURE_NODE_MASK_SURFACE_SHADOW &
                      ~(KERNEL_FEATURE_NODE_RAYTRACE | KERNEL_FEATURE_NODE_LIGHT_PATH)>(
      kg, state, &sd, nullptr, PATH_RAY_SHADOW);

  /* Write output. */
  output[offset] = clamp(average(surface_shader_transparency(&sd)), 0.0f, 1.0f);
#endif
}

ccl_device void kernel_volume_density_evaluate(KernelGlobals kg,
                                               ccl_global const KernelShaderEvalInput *input,
                                               ccl_global float *output,
                                               const int offset)
{
#ifdef __VOLUME__
  if (input[offset * 2 + 1].object == SHADER_NONE) {
    return;
  }

  KernelShaderEvalInput in = input[offset * 2];

  /* Setup ray. */
  Ray ray;
  ray.P = make_float3(__int_as_float(in.prim), in.u, in.v);
  ray.D = zero_float3();
  ray.tmin = 0.0f;
  /* Motion blur is ignored when computing the extrema of the density, but we also don't expect the
   * value to change a lot in one frame. */
  ray.time = 0.5f;

  /* Setup shader data. */
  ShaderData sd;
  shader_setup_from_volume(&sd, &ray, in.object);
  sd.flag = SD_IS_VOLUME_SHADER_EVAL;
  /* For stochastic texture sampling. */
  sd.lcg_state = lcg_state_init(offset, 0, 0, 0x15b4f88d);

  /* Evaluate extinction and emission without allocating closures. */
  sd.num_closure_left = 0;
  /* Evaluate density for camera ray because it usually makes the most visual impact. For shaders
   * that depends on ray types, the extrema are estimated on the fly. */
  /* TODO(weizhen): Volume invisible to camera ray might appear noisy. We can at least build a
   * separate octree for shadow ray. */
  const uint32_t path_flag = PATH_RAY_CAMERA;

  /* Setup volume stack entry. */
  in = input[offset * 2 + 1];
  const int shader = in.object;
  const VolumeStack entry = {sd.object, shader};

  const float3 voxel_size = make_float3(__int_as_float(in.prim), in.u, in.v);
  Extrema<float> extrema = {FLT_MAX, -FLT_MAX};
  /* For heterogeneous volume, we take 16 samples per grid;
   * for homogeneous volume, only 1 sample is needed. */
  const int num_samples = volume_is_homogeneous(kg, entry) ? 1 : 16;

  const bool need_transformation = !(kernel_data_fetch(object_flag, sd.object) &
                                     SD_OBJECT_TRANSFORM_APPLIED);
  const Transform tfm = need_transformation ?
                            object_fetch_transform(kg, sd.object, OBJECT_TRANSFORM) :
                            Transform();
  for (int sample = 0; sample < num_samples; sample++) {
    /* Blue noise indexing. The sequence length is the number of samples. */
    const uint3 index = make_uint3(sample + offset * num_samples, 0, 0xffffffff);

    /* Sample a random position inside the voxel. */
    const float3 rand_p = sobol_burley_sample_3D(
        index.x, PRNG_BAKE_VOLUME_DENSITY_EVAL, index.y, index.z);
    sd.P = ray.P + rand_p * voxel_size;
    if (need_transformation) {
      /* Convert to world spcace. */
      sd.P = transform_point(&tfm, sd.P);
    }
    sd.closure_transparent_extinction = zero_float3();
    sd.closure_emission_background = zero_float3();

    /* Evaluate volume coefficients. */
    ConstIntegratorBakeState state;
    volume_shader_eval_entry<false,
                             KERNEL_FEATURE_NODE_MASK_VOLUME & ~KERNEL_FEATURE_NODE_LIGHT_PATH>(
        kg, state, &sd, entry, path_flag);

    const float sigma = reduce_max(sd.closure_transparent_extinction);
    const float emission = reduce_max(sd.closure_emission_background);

    extrema = merge(extrema, fmaxf(sigma, emission));
  }

  /* Write output. */
  const float scale = object_volume_density(kg, sd.object);
  output[offset * 2 + 0] = extrema.min / scale;
  output[offset * 2 + 1] = extrema.max / scale;
#endif
}

CCL_NAMESPACE_END

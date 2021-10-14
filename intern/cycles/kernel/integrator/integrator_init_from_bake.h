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

#include "kernel/kernel_accumulate.h"
#include "kernel/kernel_adaptive_sampling.h"
#include "kernel/kernel_camera.h"
#include "kernel/kernel_path_state.h"
#include "kernel/kernel_random.h"

#include "kernel/geom/geom.h"

CCL_NAMESPACE_BEGIN

/* This helps with AA but it's not the real solution as it does not AA the geometry
 * but it's better than nothing, thus committed. */
ccl_device_inline float bake_clamp_mirror_repeat(float u, float max)
{
  /* use mirror repeat (like opengl texture) so that if the barycentric
   * coordinate goes past the end of the triangle it is not always clamped
   * to the same value, gives ugly patterns */
  u /= max;
  float fu = floorf(u);
  u = u - fu;

  return ((((int)fu) & 1) ? 1.0f - u : u) * max;
}

/* Return false to indicate that this pixel is finished.
 * Used by CPU implementation to not attempt to sample pixel for multiple samples once its known
 * that the pixel did converge. */
ccl_device bool integrator_init_from_bake(INTEGRATOR_STATE_ARGS,
                                          ccl_global const KernelWorkTile *ccl_restrict tile,
                                          ccl_global float *render_buffer,
                                          const int x,
                                          const int y,
                                          const int scheduled_sample)
{
  PROFILING_INIT(kg, PROFILING_RAY_SETUP);

  /* Initialize path state to give basic buffer access and allow early outputs. */
  path_state_init(INTEGRATOR_STATE_PASS, tile, x, y);

  /* Check whether the pixel has converged and should not be sampled anymore. */
  if (!kernel_need_sample_pixel(INTEGRATOR_STATE_PASS, render_buffer)) {
    return false;
  }

  /* Always count the sample, even if the camera sample will reject the ray. */
  const int sample = kernel_accum_sample(INTEGRATOR_STATE_PASS, render_buffer, scheduled_sample);

  /* Setup render buffers. */
  const int index = INTEGRATOR_STATE(path, render_pixel_index);
  const int pass_stride = kernel_data.film.pass_stride;
  render_buffer += index * pass_stride;

  ccl_global float *primitive = render_buffer + kernel_data.film.pass_bake_primitive;
  ccl_global float *differential = render_buffer + kernel_data.film.pass_bake_differential;

  const int seed = __float_as_uint(primitive[0]);
  int prim = __float_as_uint(primitive[1]);
  if (prim == -1) {
    return false;
  }

  prim += kernel_data.bake.tri_offset;

  /* Random number generator. */
  const uint rng_hash = hash_uint(seed) ^ kernel_data.integrator.seed;

  float filter_x, filter_y;
  if (sample == 0) {
    filter_x = filter_y = 0.5f;
  }
  else {
    path_rng_2D(kg, rng_hash, sample, PRNG_FILTER_U, &filter_x, &filter_y);
  }

  /* Initialize path state for path integration. */
  path_state_init_integrator(INTEGRATOR_STATE_PASS, sample, rng_hash);

  /* Barycentric UV with sub-pixel offset. */
  float u = primitive[2];
  float v = primitive[3];

  float dudx = differential[0];
  float dudy = differential[1];
  float dvdx = differential[2];
  float dvdy = differential[3];

  if (sample > 0) {
    u = bake_clamp_mirror_repeat(u + dudx * (filter_x - 0.5f) + dudy * (filter_y - 0.5f), 1.0f);
    v = bake_clamp_mirror_repeat(v + dvdx * (filter_x - 0.5f) + dvdy * (filter_y - 0.5f),
                                 1.0f - u);
  }

  /* Position and normal on triangle. */
  const int object = kernel_data.bake.object_index;
  float3 P, Ng;
  int shader;
  triangle_point_normal(kg, object, prim, u, v, &P, &Ng, &shader);

  const int object_flag = kernel_tex_fetch(__object_flag, object);
  if (!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
    Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
    P = transform_point_auto(&tfm, P);
  }

  if (kernel_data.film.pass_background != PASS_UNUSED) {
    /* Environment baking. */

    /* Setup and write ray. */
    Ray ray ccl_optional_struct_init;
    ray.P = zero_float3();
    ray.D = normalize(P);
    ray.t = FLT_MAX;
    ray.time = 0.5f;
    ray.dP = differential_zero_compact();
    ray.dD = differential_zero_compact();
    integrator_state_write_ray(INTEGRATOR_STATE_PASS, &ray);

    /* Setup next kernel to execute. */
    INTEGRATOR_PATH_INIT(DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND);
  }
  else {
    /* Surface baking. */
    float3 N = (shader & SHADER_SMOOTH_NORMAL) ? triangle_smooth_normal(kg, Ng, prim, u, v) : Ng;

    if (!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
      Transform itfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);
      N = normalize(transform_direction_transposed(&itfm, N));
      Ng = normalize(transform_direction_transposed(&itfm, Ng));
    }

    /* Setup ray. */
    Ray ray ccl_optional_struct_init;
    ray.P = P + N;
    ray.D = -N;
    ray.t = FLT_MAX;
    ray.time = 0.5f;

    /* Setup differentials. */
    float3 dPdu, dPdv;
    triangle_dPdudv(kg, prim, &dPdu, &dPdv);
    if (!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
      Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
      dPdu = transform_direction(&tfm, dPdu);
      dPdv = transform_direction(&tfm, dPdv);
    }

    differential3 dP;
    dP.dx = dPdu * dudx + dPdv * dvdx;
    dP.dy = dPdu * dudy + dPdv * dvdy;
    ray.dP = differential_make_compact(dP);
    ray.dD = differential_zero_compact();

    /* Write ray. */
    integrator_state_write_ray(INTEGRATOR_STATE_PASS, &ray);

    /* Setup and write intersection. */
    Intersection isect ccl_optional_struct_init;
    isect.object = kernel_data.bake.object_index;
    isect.prim = prim;
    isect.u = u;
    isect.v = v;
    isect.t = 1.0f;
    isect.type = PRIMITIVE_TRIANGLE;
#ifdef __EMBREE__
    isect.Ng = Ng;
#endif
    integrator_state_write_isect(INTEGRATOR_STATE_PASS, &isect);

    /* Setup next kernel to execute. */
    const int shader_index = shader & SHADER_MASK;
    const int shader_flags = kernel_tex_fetch(__shaders, shader_index).flags;
    if ((shader_flags & SD_HAS_RAYTRACE) || (kernel_data.film.pass_ao != PASS_UNUSED)) {
      INTEGRATOR_PATH_INIT_SORTED(DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE, shader_index);
    }
    else {
      INTEGRATOR_PATH_INIT_SORTED(DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE, shader_index);
    }
  }

  return true;
}

CCL_NAMESPACE_END

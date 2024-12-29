/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/camera/camera.h"

#include "kernel/film/adaptive_sampling.h"
#include "kernel/film/light_passes.h"

#include "kernel/integrator/intersect_closest.h"
#include "kernel/integrator/path_state.h"

#include "kernel/sample/pattern.h"

CCL_NAMESPACE_BEGIN

/* In order to perform anti-aliasing during baking, we jitter the input barycentric coordinates
 * (which are for the center of the texel) within the texel.
 * However, the baking code currently doesn't support going to neighboring triangle, so if the
 * jittered location falls outside of the input triangle, we need to bring it back in somehow.
 * Clamping is a bad choice here since it can produce noticeable artifacts at triangle edges,
 * but properly uniformly sampling the intersection of triangle and texel would be very
 * performance-heavy, so cheat by just trying different jittering until we end up inside the
 * triangle.
 * For triangles that are smaller than a texel, this might take too many attempts, so eventually
 * we just give up and don't jitter in that case.
 * This is not a particularly elegant solution, but it's probably the best we can do. */
ccl_device_inline void bake_jitter_barycentric(ccl_private float &u,
                                               ccl_private float &v,
                                               float2 rand_filter,
                                               const float dudx,
                                               const float dudy,
                                               const float dvdx,
                                               const float dvdy)
{
  for (int i = 0; i < 10; i++) {
    /* Offset UV according to differentials. */
    const float jitterU = u + (rand_filter.x - 0.5f) * dudx + (rand_filter.y - 0.5f) * dudy;
    const float jitterV = v + (rand_filter.x - 0.5f) * dvdx + (rand_filter.y - 0.5f) * dvdy;
    /* If this location is inside the triangle, return. */
    if (jitterU > 0.0f && jitterV > 0.0f && jitterU + jitterV < 1.0f) {
      u = jitterU;
      v = jitterV;
      return;
    }
    /* Retry with new jitter value. */
    rand_filter = hash_float2_to_float2(rand_filter);
  }
  /* Retries exceeded, give up and just use center value. */
}

/* Offset towards center of triangle to avoid ray-tracing precision issues. */
ccl_device float2 bake_offset_towards_center(KernelGlobals kg,
                                             const int prim,
                                             const float u,
                                             const float v)
{
  float3 tri_verts[3];
  triangle_vertices(kg, prim, tri_verts);

  /* Empirically determined values, by no means perfect. */
  const float position_offset = 1e-4f;
  const float uv_offset = 1e-5f;

  /* Offset position towards center, amount relative to absolute size of position coordinates. */
  const float3 P = u * tri_verts[0] + v * tri_verts[1] + (1.0f - u - v) * tri_verts[2];
  const float3 center = (tri_verts[0] + tri_verts[1] + tri_verts[2]) / 3.0f;
  const float3 to_center = center - P;

  const float3 offset_P = P + normalize(to_center) *
                                  min(len(to_center),
                                      max(reduce_max(fabs(P)), 1.0f) * position_offset);

  /* Compute barycentric coordinates at new position. */
  const float3 v1 = tri_verts[1] - tri_verts[0];
  const float3 v2 = tri_verts[2] - tri_verts[0];
  const float3 vP = offset_P - tri_verts[0];

  const float d11 = dot(v1, v1);
  const float d12 = dot(v1, v2);
  const float d22 = dot(v2, v2);
  const float dP1 = dot(vP, v1);
  const float dP2 = dot(vP, v2);

  const float denom = d11 * d22 - d12 * d12;
  if (denom == 0.0f) {
    return make_float2(0.0f, 0.0f);
  }

  const float offset_v = clamp((d22 * dP1 - d12 * dP2) / denom, uv_offset, 1.0f - uv_offset);
  const float offset_w = clamp((d11 * dP2 - d12 * dP1) / denom, uv_offset, 1.0f - uv_offset);
  const float offset_u = clamp(1.0f - offset_v - offset_w, uv_offset, 1.0f - uv_offset);

  return make_float2(offset_u, offset_v);
}

/* Return false to indicate that this pixel is finished.
 * Used by CPU implementation to not attempt to sample pixel for multiple samples once its known
 * that the pixel did converge. */
ccl_device bool integrator_init_from_bake(KernelGlobals kg,
                                          IntegratorState state,
                                          const ccl_global KernelWorkTile *ccl_restrict tile,
                                          ccl_global float *render_buffer,
                                          const int x,
                                          const int y,
                                          const int scheduled_sample)
{
  PROFILING_INIT(kg, PROFILING_RAY_SETUP);

  /* Initialize path state to give basic buffer access and allow early outputs. */
  path_state_init(state, tile, x, y);

  /* Check whether the pixel has converged and should not be sampled anymore. */
  if (!film_need_sample_pixel(kg, state, render_buffer)) {
    return false;
  }

  /* Always count the sample, even if the camera sample will reject the ray. */
  const int sample = film_write_sample(
      kg, state, render_buffer, scheduled_sample, tile->sample_offset);

  /* Setup render buffers. */
  ccl_global float *buffer = film_pass_pixel_render_buffer(kg, state, render_buffer);

  ccl_global float *primitive = buffer + kernel_data.film.pass_bake_primitive;
  ccl_global float *differential = buffer + kernel_data.film.pass_bake_differential;

  int prim = __float_as_uint(primitive[2]);
  if (prim == -1) {
    /* Accumulate transparency for empty pixels. */
    film_write_transparent(kg, state, 0, 1.0f, buffer);
    return true;
  }

  prim += kernel_data.bake.tri_offset;

  /* Random number generator. */
  uint rng_pixel = 0;
  if (kernel_data.film.pass_bake_seed != 0) {
    const uint seed = __float_as_uint(buffer[kernel_data.film.pass_bake_seed]);
    rng_pixel = hash_uint(seed) ^ kernel_data.integrator.seed;
  }
  else {
    rng_pixel = path_rng_pixel_init(kg, sample, x, y);
  }

  const float2 rand_filter = (sample == 0) ? make_float2(0.5f, 0.5f) :
                                             path_rng_2D(kg, rng_pixel, sample, PRNG_FILTER);

  /* Initialize path state for path integration. */
  path_state_init_integrator(kg, state, sample, rng_pixel);

  /* Barycentric UV. */
  float u = primitive[0];
  float v = primitive[1];

  float dudx = differential[0];
  float dudy = differential[1];
  float dvdx = differential[2];
  float dvdy = differential[3];

  /* Exactly at vertex? Nudge inwards to avoid self-intersection. */
  if ((u == 0.0f || u == 1.0f) && (v == 0.0f || v == 1.0f)) {
    const float2 uv = bake_offset_towards_center(kg, prim, u, v);
    u = uv.x;
    v = uv.y;
  }

  /* Sub-pixel offset. */
  bake_jitter_barycentric(u, v, rand_filter, dudx, dudy, dvdx, dvdy);

  /* Convert from Blender to Cycles/Embree/OptiX barycentric convention. */
  const float tmp = u;
  u = v;
  v = 1.0f - tmp - v;

  const float tmpdx = dudx;
  const float tmpdy = dudy;
  dudx = dvdx;
  dudy = dvdy;
  dvdx = -tmpdx - dvdx;
  dvdy = -tmpdy - dvdy;

  /* Position and normal on triangle. */
  const int object = kernel_data.bake.object_index;
  float3 P;
  float3 Ng;
  int shader;
  triangle_point_normal(kg, object, prim, u, v, &P, &Ng, &shader);

  const int object_flag = kernel_data_fetch(object_flag, object);
  if (!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
    const Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
    P = transform_point_auto(&tfm, P);
  }

  if (kernel_data.film.pass_background != PASS_UNUSED) {
    /* Environment baking. */

    /* Setup and write ray. */
    Ray ray ccl_optional_struct_init;
    ray.P = zero_float3();
    ray.D = normalize(P);
    ray.tmin = 0.0f;
    ray.tmax = FLT_MAX;
    ray.time = 0.5f;
    ray.dP = differential_zero_compact();
    ray.dD = differential_zero_compact();
    integrator_state_write_ray(state, &ray);

    /* Setup next kernel to execute. */
    integrator_path_init(kg, state, DEVICE_KERNEL_INTEGRATOR_SHADE_BACKGROUND);
  }
  else {
    /* Surface baking. */
    float3 N = (shader & SHADER_SMOOTH_NORMAL) ? triangle_smooth_normal(kg, Ng, prim, u, v) : Ng;

    if (!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
      const Transform itfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);
      N = normalize(transform_direction_transposed(&itfm, N));
      Ng = normalize(transform_direction_transposed(&itfm, Ng));
    }

    const int shader_index = shader & SHADER_MASK;
    const int shader_flags = kernel_data_fetch(shaders, shader_index).flags;

    /* Fast path for position and normal passes not affected by shaders. */
    if (kernel_data.film.pass_position != PASS_UNUSED) {
      film_write_pass_float3(buffer + kernel_data.film.pass_position, P);
      return true;
    }
    if (kernel_data.film.pass_normal != PASS_UNUSED && !(shader_flags & SD_HAS_BUMP)) {
      film_write_pass_float3(buffer + kernel_data.film.pass_normal, N);
      return true;
    }

    /* Setup ray. */
    Ray ray ccl_optional_struct_init;

    if (kernel_data.bake.use_camera) {
      float3 D = camera_direction_from_point(kg, P);

      const float DN = dot(D, N);

      /* Nudge camera direction, so that the faces facing away from the camera still have
       * somewhat usable shading. (Otherwise, glossy faces would be simply black.)
       *
       * The surface normal offset affects smooth surfaces. Lower values will make
       * smooth surfaces more faceted, but higher values may show up from the camera
       * at grazing angles.
       *
       * This value can actually be pretty high before it's noticeably wrong. */
      const float surface_normal_offset = 0.2f;

      /* Keep the ray direction at least `surface_normal_offset` "above" the smooth normal. */
      if (DN <= surface_normal_offset) {
        D -= N * (DN - surface_normal_offset);
        D = normalize(D);
      }

      /* On the backside, just lerp towards the surface normal for the ray direction,
       * as DN goes from 0.0 to -1.0. */
      if (DN <= 0.0f) {
        D = normalize(mix(D, N, -DN));
      }

      /* We don't want to bake the back face, so make sure the ray direction never
       * goes behind the geometry (flat) normal. This is a fail-safe, and should rarely happen. */
      const float true_normal_epsilon = 0.00001f;

      if (dot(D, Ng) <= true_normal_epsilon) {
        D -= Ng * (dot(D, Ng) - true_normal_epsilon);
        D = normalize(D);
      }

      ray.P = P + D;
      ray.D = -D;
    }
    else {
      ray.P = P + N;
      ray.D = -N;
    }

    ray.tmin = 0.0f;
    ray.tmax = FLT_MAX;
    ray.time = 0.5f;

    /* Setup differentials. */
    float3 dPdu;
    float3 dPdv;
    triangle_dPdudv(kg, prim, &dPdu, &dPdv);
    if (!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
      const Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);
      dPdu = transform_direction(&tfm, dPdu);
      dPdv = transform_direction(&tfm, dPdv);
    }

    differential3 dP;
    dP.dx = dPdu * dudx + dPdv * dvdx;
    dP.dy = dPdu * dudy + dPdv * dvdy;
    ray.dP = differential_make_compact(dP);
    ray.dD = differential_zero_compact();

    /* Write ray. */
    integrator_state_write_ray(state, &ray);

    /* Setup and write intersection. */
    Intersection isect ccl_optional_struct_init;
    isect.object = kernel_data.bake.object_index;
    isect.prim = prim;
    isect.u = u;
    isect.v = v;
    isect.t = 1.0f;
    isect.type = PRIMITIVE_TRIANGLE;
    integrator_state_write_isect(state, &isect);

    /* Setup next kernel to execute. */
    const bool use_caustics = kernel_data.integrator.use_caustics &&
                              (object_flag & SD_OBJECT_CAUSTICS);
    const bool use_raytrace_kernel = (shader_flags & SD_HAS_RAYTRACE);

    if (use_caustics) {
      integrator_path_init_sorted(
          kg, state, DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE, shader_index);
    }
    else if (use_raytrace_kernel) {
      integrator_path_init_sorted(
          kg, state, DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE, shader_index);
    }
    else {
      integrator_path_init_sorted(kg, state, DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE, shader_index);
    }

#ifdef __SHADOW_CATCHER__
    integrator_split_shadow_catcher(kg, state, &isect, render_buffer);
#endif
  }

  return true;
}

CCL_NAMESPACE_END

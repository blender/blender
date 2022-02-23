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

#include "kernel/camera/camera.h"

#include "kernel/film/accumulate.h"
#include "kernel/film/adaptive_sampling.h"

#include "kernel/integrator/path_state.h"

#include "kernel/sample/pattern.h"

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

/* Offset towards center of triangle to avoid ray-tracing precision issues. */
ccl_device const float2 bake_offset_towards_center(KernelGlobals kg,
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
                                  min(len(to_center), max(max3(fabs(P)), 1.0f) * position_offset);

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
                                          ccl_global const KernelWorkTile *ccl_restrict tile,
                                          ccl_global float *render_buffer,
                                          const int x,
                                          const int y,
                                          const int scheduled_sample)
{
  PROFILING_INIT(kg, PROFILING_RAY_SETUP);

  /* Initialize path state to give basic buffer access and allow early outputs. */
  path_state_init(state, tile, x, y);

  /* Check whether the pixel has converged and should not be sampled anymore. */
  if (!kernel_need_sample_pixel(kg, state, render_buffer)) {
    return false;
  }

  /* Always count the sample, even if the camera sample will reject the ray. */
  const int sample = kernel_accum_sample(
      kg, state, render_buffer, scheduled_sample, tile->sample_offset);

  /* Setup render buffers. */
  const int index = INTEGRATOR_STATE(state, path, render_pixel_index);
  const int pass_stride = kernel_data.film.pass_stride;
  ccl_global float *buffer = render_buffer + index * pass_stride;

  ccl_global float *primitive = buffer + kernel_data.film.pass_bake_primitive;
  ccl_global float *differential = buffer + kernel_data.film.pass_bake_differential;

  const int seed = __float_as_uint(primitive[0]);
  int prim = __float_as_uint(primitive[1]);
  if (prim == -1) {
    /* Accumulate transparency for empty pixels. */
    kernel_accum_transparent(kg, state, 0, 1.0f, buffer);
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
  path_state_init_integrator(kg, state, sample, rng_hash);

  /* Barycentric UV. */
  float u = primitive[2];
  float v = primitive[3];

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
    integrator_state_write_ray(kg, state, &ray);

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
    integrator_state_write_ray(kg, state, &ray);

    /* Setup and write intersection. */
    Intersection isect ccl_optional_struct_init;
    isect.object = kernel_data.bake.object_index;
    isect.prim = prim;
    isect.u = u;
    isect.v = v;
    isect.t = 1.0f;
    isect.type = PRIMITIVE_TRIANGLE;
    integrator_state_write_isect(kg, state, &isect);

    /* Setup next kernel to execute. */
    const int shader_index = shader & SHADER_MASK;
    const int shader_flags = kernel_tex_fetch(__shaders, shader_index).flags;
    if (shader_flags & SD_HAS_RAYTRACE) {
      INTEGRATOR_PATH_INIT_SORTED(DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE, shader_index);
    }
    else {
      INTEGRATOR_PATH_INIT_SORTED(DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE, shader_index);
    }
  }

  return true;
}

CCL_NAMESPACE_END

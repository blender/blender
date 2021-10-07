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

#include "kernel/kernel_path_state.h"
#include "kernel/kernel_projection.h"
#include "kernel/kernel_shader.h"

#include "kernel/bvh/bvh.h"

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf_diffuse.h"
#include "kernel/closure/bsdf_principled_diffuse.h"
#include "kernel/closure/bssrdf.h"
#include "kernel/closure/volume.h"

#include "kernel/integrator/integrator_intersect_volume_stack.h"
#include "kernel/integrator/integrator_subsurface_disk.h"
#include "kernel/integrator/integrator_subsurface_random_walk.h"

CCL_NAMESPACE_BEGIN

#ifdef __SUBSURFACE__

ccl_device int subsurface_bounce(INTEGRATOR_STATE_ARGS, ShaderData *sd, const ShaderClosure *sc)
{
  /* We should never have two consecutive BSSRDF bounces, the second one should
   * be converted to a diffuse BSDF to avoid this. */
  kernel_assert(!(INTEGRATOR_STATE(path, flag) & PATH_RAY_DIFFUSE_ANCESTOR));

  /* Setup path state for intersect_subsurface kernel. */
  const Bssrdf *bssrdf = (const Bssrdf *)sc;

  /* Setup ray into surface. */
  INTEGRATOR_STATE_WRITE(ray, P) = sd->P;
  INTEGRATOR_STATE_WRITE(ray, D) = sd->N;
  INTEGRATOR_STATE_WRITE(ray, t) = FLT_MAX;
  INTEGRATOR_STATE_WRITE(ray, dP) = differential_make_compact(sd->dP);
  INTEGRATOR_STATE_WRITE(ray, dD) = differential_zero_compact();

  /* Pass along object info, reusing isect to save memory. */
  INTEGRATOR_STATE_WRITE(isect, Ng) = sd->Ng;
  INTEGRATOR_STATE_WRITE(isect, object) = sd->object;

  /* Pass BSSRDF parameters. */
  const uint32_t path_flag = INTEGRATOR_STATE_WRITE(path, flag);
  INTEGRATOR_STATE_WRITE(path, flag) = (path_flag & ~PATH_RAY_CAMERA) |
                                       ((sc->type == CLOSURE_BSSRDF_BURLEY_ID) ?
                                            PATH_RAY_SUBSURFACE_DISK :
                                            PATH_RAY_SUBSURFACE_RANDOM_WALK);
  INTEGRATOR_STATE_WRITE(path, throughput) *= shader_bssrdf_sample_weight(sd, sc);

  /* Advance random number offset for bounce. */
  INTEGRATOR_STATE_WRITE(path, rng_offset) += PRNG_BOUNCE_NUM;

  if (kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_PASSES) {
    if (INTEGRATOR_STATE(path, bounce) == 0) {
      INTEGRATOR_STATE_WRITE(path, diffuse_glossy_ratio) = one_float3();
    }
  }

  INTEGRATOR_STATE_WRITE(subsurface, albedo) = bssrdf->albedo;
  INTEGRATOR_STATE_WRITE(subsurface, radius) = bssrdf->radius;
  INTEGRATOR_STATE_WRITE(subsurface, roughness) = bssrdf->roughness;
  INTEGRATOR_STATE_WRITE(subsurface, anisotropy) = bssrdf->anisotropy;

  return LABEL_SUBSURFACE_SCATTER;
}

ccl_device void subsurface_shader_data_setup(INTEGRATOR_STATE_ARGS, ShaderData *sd)
{
  /* Get bump mapped normal from shader evaluation at exit point. */
  float3 N = sd->N;
  if (sd->flag & SD_HAS_BSSRDF_BUMP) {
    N = shader_bssrdf_normal(sd);
  }

  /* Setup diffuse BSDF at the exit point. This replaces shader_eval_surface. */
  sd->flag &= ~SD_CLOSURE_FLAGS;
  sd->num_closure = 0;
  sd->num_closure_left = kernel_data.max_closures;

  const float3 weight = one_float3();
  const float roughness = INTEGRATOR_STATE(subsurface, roughness);

#  ifdef __PRINCIPLED__
  if (roughness != FLT_MAX) {
    PrincipledDiffuseBsdf *bsdf = (PrincipledDiffuseBsdf *)bsdf_alloc(
        sd, sizeof(PrincipledDiffuseBsdf), weight);

    if (bsdf) {
      bsdf->N = N;
      bsdf->roughness = roughness;
      sd->flag |= bsdf_principled_diffuse_setup(bsdf);

      /* replace CLOSURE_BSDF_PRINCIPLED_DIFFUSE_ID with this special ID so render passes
       * can recognize it as not being a regular Disney principled diffuse closure */
      bsdf->type = CLOSURE_BSDF_BSSRDF_PRINCIPLED_ID;
    }
  }
  else
#  endif /* __PRINCIPLED__ */
  {
    DiffuseBsdf *bsdf = (DiffuseBsdf *)bsdf_alloc(sd, sizeof(DiffuseBsdf), weight);

    if (bsdf) {
      bsdf->N = N;
      sd->flag |= bsdf_diffuse_setup(bsdf);

      /* replace CLOSURE_BSDF_DIFFUSE_ID with this special ID so render passes
       * can recognize it as not being a regular diffuse closure */
      bsdf->type = CLOSURE_BSDF_BSSRDF_ID;
    }
  }
}

ccl_device_inline bool subsurface_scatter(INTEGRATOR_STATE_ARGS)
{
  RNGState rng_state;
  path_state_rng_load(INTEGRATOR_STATE_PASS, &rng_state);

  Ray ray ccl_optional_struct_init;
  LocalIntersection ss_isect ccl_optional_struct_init;

  if (INTEGRATOR_STATE(path, flag) & PATH_RAY_SUBSURFACE_RANDOM_WALK) {
    if (!subsurface_random_walk(INTEGRATOR_STATE_PASS, rng_state, ray, ss_isect)) {
      return false;
    }
  }
  else {
    if (!subsurface_disk(INTEGRATOR_STATE_PASS, rng_state, ray, ss_isect)) {
      return false;
    }
  }

#  ifdef __VOLUME__
  /* Update volume stack if needed. */
  if (kernel_data.integrator.use_volumes) {
    const int object = ss_isect.hits[0].object;
    const int object_flag = kernel_tex_fetch(__object_flag, object);

    if (object_flag & SD_OBJECT_INTERSECTS_VOLUME) {
      float3 P = INTEGRATOR_STATE(ray, P);
      const float3 Ng = INTEGRATOR_STATE(isect, Ng);
      const float3 offset_P = ray_offset(P, -Ng);

      integrator_volume_stack_update_for_subsurface(INTEGRATOR_STATE_PASS, offset_P, ray.P);
    }
  }
#  endif /* __VOLUME__ */

  /* Pretend ray is coming from the outside towards the exit point. This ensures
   * correct front/back facing normals.
   * TODO: find a more elegant solution? */
  ray.P += ray.D * ray.t * 2.0f;
  ray.D = -ray.D;

  integrator_state_write_isect(INTEGRATOR_STATE_PASS, &ss_isect.hits[0]);
  integrator_state_write_ray(INTEGRATOR_STATE_PASS, &ray);

  /* Advance random number offset for bounce. */
  INTEGRATOR_STATE_WRITE(path, rng_offset) += PRNG_BOUNCE_NUM;

  const int shader = intersection_get_shader(kg, &ss_isect.hits[0]);
  const int shader_flags = kernel_tex_fetch(__shaders, shader).flags;
  if ((shader_flags & SD_HAS_RAYTRACE) || (kernel_data.film.pass_ao != PASS_UNUSED)) {
    INTEGRATOR_PATH_NEXT_SORTED(DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE,
                                DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE,
                                shader);
  }
  else {
    INTEGRATOR_PATH_NEXT_SORTED(DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE,
                                DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE,
                                shader);
  }

  return true;
}

#endif /* __SUBSURFACE__ */

CCL_NAMESPACE_END

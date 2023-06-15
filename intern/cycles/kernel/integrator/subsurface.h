/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/camera/projection.h"

#include "kernel/bvh/bvh.h"

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf_diffuse.h"
#include "kernel/closure/bsdf_principled_diffuse.h"
#include "kernel/closure/bssrdf.h"
#include "kernel/closure/volume.h"

#include "kernel/integrator/intersect_volume_stack.h"
#include "kernel/integrator/path_state.h"
#include "kernel/integrator/subsurface_disk.h"
#include "kernel/integrator/subsurface_random_walk.h"
#include "kernel/integrator/surface_shader.h"

CCL_NAMESPACE_BEGIN

#ifdef __SUBSURFACE__

ccl_device int subsurface_bounce(KernelGlobals kg,
                                 IntegratorState state,
                                 ccl_private ShaderData *sd,
                                 ccl_private const ShaderClosure *sc)
{
  /* We should never have two consecutive BSSRDF bounces, the second one should
   * be converted to a diffuse BSDF to avoid this. */
  kernel_assert(!(INTEGRATOR_STATE(state, path, flag) & PATH_RAY_DIFFUSE_ANCESTOR));

  /* Setup path state for intersect_subsurface kernel. */
  ccl_private const Bssrdf *bssrdf = (ccl_private const Bssrdf *)sc;

  /* Setup ray into surface. */
  INTEGRATOR_STATE_WRITE(state, ray, P) = sd->P;
  INTEGRATOR_STATE_WRITE(state, ray, D) = bssrdf->N;
  INTEGRATOR_STATE_WRITE(state, ray, tmin) = 0.0f;
  INTEGRATOR_STATE_WRITE(state, ray, tmax) = FLT_MAX;
  INTEGRATOR_STATE_WRITE(state, ray, dP) = differential_make_compact(sd->dP);
  INTEGRATOR_STATE_WRITE(state, ray, dD) = differential_zero_compact();

  /* Pass along object info, reusing isect to save memory. */
  INTEGRATOR_STATE_WRITE(state, subsurface, Ng) = sd->Ng;

  uint32_t path_flag = (INTEGRATOR_STATE(state, path, flag) & ~PATH_RAY_CAMERA) |
                       ((sc->type == CLOSURE_BSSRDF_BURLEY_ID) ? PATH_RAY_SUBSURFACE_DISK :
                                                                 PATH_RAY_SUBSURFACE_RANDOM_WALK);

  /* Compute weight, optionally including Fresnel from entry point. */
  Spectrum weight = surface_shader_bssrdf_sample_weight(sd, sc);
  if (bssrdf->roughness != FLT_MAX) {
    path_flag |= PATH_RAY_SUBSURFACE_USE_FRESNEL;
  }

  if (sd->flag & SD_BACKFACING) {
    path_flag |= PATH_RAY_SUBSURFACE_BACKFACING;
  }

  INTEGRATOR_STATE_WRITE(state, path, throughput) *= weight;
  INTEGRATOR_STATE_WRITE(state, path, flag) = path_flag;

  /* Advance random number offset for bounce. */
  INTEGRATOR_STATE_WRITE(state, path, rng_offset) += PRNG_BOUNCE_NUM;

  if (kernel_data.kernel_features & KERNEL_FEATURE_LIGHT_PASSES) {
    if (INTEGRATOR_STATE(state, path, bounce) == 0) {
      INTEGRATOR_STATE_WRITE(state, path, pass_diffuse_weight) = one_spectrum();
      INTEGRATOR_STATE_WRITE(state, path, pass_glossy_weight) = zero_spectrum();
    }
  }

  /* Pass BSSRDF parameters. */
  INTEGRATOR_STATE_WRITE(state, subsurface, albedo) = bssrdf->albedo;
  INTEGRATOR_STATE_WRITE(state, subsurface, radius) = bssrdf->radius;
  INTEGRATOR_STATE_WRITE(state, subsurface, anisotropy) = bssrdf->anisotropy;

  /* Path guiding. */
  guiding_record_bssrdf_weight(kg, state, weight, bssrdf->albedo);

  return LABEL_SUBSURFACE_SCATTER;
}

ccl_device void subsurface_shader_data_setup(KernelGlobals kg,
                                             IntegratorState state,
                                             ccl_private ShaderData *sd,
                                             const uint32_t path_flag)
{
  /* Get bump mapped normal from shader evaluation at exit point. */
  float3 N = sd->N;
  if (sd->flag & SD_HAS_BSSRDF_BUMP) {
    N = surface_shader_bssrdf_normal(sd);
  }

  /* Setup diffuse BSDF at the exit point. This replaces shader_eval_surface. */
  sd->flag &= ~SD_CLOSURE_FLAGS;
  sd->num_closure = 0;
  sd->num_closure_left = kernel_data.max_closures;

  const Spectrum weight = one_spectrum();

  if (path_flag & PATH_RAY_SUBSURFACE_USE_FRESNEL) {
    ccl_private PrincipledDiffuseBsdf *bsdf = (ccl_private PrincipledDiffuseBsdf *)bsdf_alloc(
        sd, sizeof(PrincipledDiffuseBsdf), weight);

    if (bsdf) {
      bsdf->N = N;
      bsdf->roughness = FLT_MAX;
      sd->flag |= bsdf_principled_diffuse_setup(bsdf, PRINCIPLED_DIFFUSE_LAMBERT_EXIT);
    }
  }
  else {
    ccl_private DiffuseBsdf *bsdf = (ccl_private DiffuseBsdf *)bsdf_alloc(
        sd, sizeof(DiffuseBsdf), weight);

    if (bsdf) {
      bsdf->N = N;
      sd->flag |= bsdf_diffuse_setup(bsdf);
    }
  }
}

ccl_device_inline bool subsurface_scatter(KernelGlobals kg, IntegratorState state)
{
  RNGState rng_state;
  path_state_rng_load(state, &rng_state);

  Ray ray ccl_optional_struct_init;
  LocalIntersection ss_isect ccl_optional_struct_init;

  if (INTEGRATOR_STATE(state, path, flag) & PATH_RAY_SUBSURFACE_RANDOM_WALK) {
    if (!subsurface_random_walk(kg, state, rng_state, ray, ss_isect)) {
      return false;
    }
  }
  else {
    if (!subsurface_disk(kg, state, rng_state, ray, ss_isect)) {
      return false;
    }
  }

#  ifdef __VOLUME__
  /* Update volume stack if needed. */
  if (kernel_data.integrator.use_volumes) {
    const int object = ss_isect.hits[0].object;
    const int object_flag = kernel_data_fetch(object_flag, object);

    if (object_flag & SD_OBJECT_INTERSECTS_VOLUME) {
      float3 P = INTEGRATOR_STATE(state, ray, P);

      integrator_volume_stack_update_for_subsurface(kg, state, P, ray.P);
    }
  }
#  endif /* __VOLUME__ */

  /* Pretend ray is coming from the outside towards the exit point. This ensures
   * correct front/back facing normals.
   * TODO: find a more elegant solution? */
  ray.P += ray.D * ray.tmax * 2.0f;
  ray.D = -ray.D;

  integrator_state_write_isect(state, &ss_isect.hits[0]);
  integrator_state_write_ray(state, &ray);

  /* Advance random number offset for bounce. */
  INTEGRATOR_STATE_WRITE(state, path, rng_offset) += PRNG_BOUNCE_NUM;

  const int shader = intersection_get_shader(kg, &ss_isect.hits[0]);
  const int shader_flags = kernel_data_fetch(shaders, shader).flags;
  const int object_flags = intersection_get_object_flags(kg, &ss_isect.hits[0]);
  const bool use_caustics = kernel_data.integrator.use_caustics &&
                            (object_flags & SD_OBJECT_CAUSTICS);
  const bool use_raytrace_kernel = (shader_flags & SD_HAS_RAYTRACE);

  if (use_caustics) {
    integrator_path_next_sorted(kg,
                                state,
                                DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE,
                                DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_MNEE,
                                shader);
  }
  else if (use_raytrace_kernel) {
    integrator_path_next_sorted(kg,
                                state,
                                DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE,
                                DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE_RAYTRACE,
                                shader);
  }
  else {
    integrator_path_next_sorted(kg,
                                state,
                                DEVICE_KERNEL_INTEGRATOR_INTERSECT_SUBSURFACE,
                                DEVICE_KERNEL_INTEGRATOR_SHADE_SURFACE,
                                shader);
  }

  return true;
}

#endif /* __SUBSURFACE__ */

CCL_NAMESPACE_END

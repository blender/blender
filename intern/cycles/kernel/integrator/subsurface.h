/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/camera/projection.h"

#include "kernel/bvh/bvh.h"

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf_diffuse.h"
#include "kernel/closure/bssrdf.h"
#include "kernel/closure/volume.h"

#include "kernel/integrator/intersect_volume_stack.h"
#include "kernel/integrator/path_state.h"
#include "kernel/integrator/subsurface_disk.h"
#include "kernel/integrator/subsurface_random_walk.h"
#include "kernel/integrator/surface_shader.h"

CCL_NAMESPACE_BEGIN

#ifdef __SUBSURFACE__

ccl_device_inline bool subsurface_entry_bounce(KernelGlobals kg,
                                               ccl_private const Bssrdf *bssrdf,
                                               ccl_private ShaderData *sd,
                                               ccl_private RNGState *rng_state,
                                               ccl_private float3 *wo)
{
  float2 rand_bsdf = path_state_rng_2D(kg, rng_state, PRNG_SUBSURFACE_BSDF);

  if (bssrdf->type == CLOSURE_BSSRDF_RANDOM_WALK_ID) {
    /* CLOSURE_BSSRDF_RANDOM_WALK_ID has a 50% chance to sample a diffuse entry bounce.
     * Also, for the refractive entry, it uses a fixed roughness of 1.0. */
    if (rand_bsdf.x < 0.5f) {
      rand_bsdf.x *= 2.0f;
      float pdf;
      sample_cos_hemisphere(-bssrdf->N, rand_bsdf, wo, &pdf);
      return true;
    }
    rand_bsdf.x = 2.0f * (rand_bsdf.x - 0.5f);
  }

  const float cos_NI = dot(bssrdf->N, sd->wi);
  if (cos_NI <= 0.0f) {
    return false;
  }

  float3 X, Y, Z = bssrdf->N;
  make_orthonormals(Z, &X, &Y);

  const float alpha = bssrdf->alpha;
  const float neta = 1.0f / bssrdf->ior;

  /* Sample microfacet normal by transforming to/from local coordinates. */
  const float3 local_I = make_float3(dot(X, sd->wi), dot(Y, sd->wi), cos_NI);
  const float3 local_H = microfacet_ggx_sample_vndf(local_I, alpha, alpha, rand_bsdf);
  const float3 H = X * local_H.x + Y * local_H.y + Z * local_H.z;

  const float cos_HI = dot(H, sd->wi);
  const float arg = 1.0f - (sqr(neta) * (1.0f - sqr(cos_HI)));
  /* We clamp subsurface IOR to be above 1, so there should never be TIR. */
  kernel_assert(arg >= 0.0f);

  const float dnp = max(sqrtf(arg), 1e-7f);
  const float nK = (neta * cos_HI) - dnp;
  *wo = -(neta * sd->wi) + (nK * H);
  return true;
  /* Note: For a proper refractive GGX interface, we should be computing lambdaI and lambdaO
   * and multiplying the throughput by BSDF/pdf, which for VNDF sampling works out to
   * (1 + lambdaI) / (1 + lambdaI + lambdaO).
   * However, this causes darkening due to the single-scattering approximation, which we'd
   * then have to correct with a lookup table.
   * Since we only really care about the directional distribution here, it's much easier to
   * just skip all that instead. */
}

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
  INTEGRATOR_STATE_WRITE(state, ray, tmin) = 0.0f;
  INTEGRATOR_STATE_WRITE(state, ray, tmax) = FLT_MAX;
  INTEGRATOR_STATE_WRITE(state, ray, dP) = differential_make_compact(sd->dP);
  INTEGRATOR_STATE_WRITE(state, ray, dD) = differential_zero_compact();

  /* Advance random number offset for bounce. */
  INTEGRATOR_STATE_WRITE(state, path, rng_offset) += PRNG_BOUNCE_NUM;

  /* Compute weight, optionally including Fresnel from entry point. */
  Spectrum weight = surface_shader_bssrdf_sample_weight(sd, sc);
  INTEGRATOR_STATE_WRITE(state, path, throughput) *= weight;

  uint32_t path_flag = (INTEGRATOR_STATE(state, path, flag) & ~PATH_RAY_CAMERA);
  if (sc->type == CLOSURE_BSSRDF_BURLEY_ID) {
    path_flag |= PATH_RAY_SUBSURFACE_DISK;
    INTEGRATOR_STATE_WRITE(state, subsurface, N) = sd->Ng;
  }
  else {
    path_flag |= PATH_RAY_SUBSURFACE_RANDOM_WALK;

    /* Sample entry bounce into the material. */
    RNGState rng_state;
    path_state_rng_load(state, &rng_state);
    float3 wo;
    if (!subsurface_entry_bounce(kg, bssrdf, sd, &rng_state, &wo) || dot(sd->Ng, wo) >= 0.0f) {
      /* Sampling failed, give up on this bounce. */
      return LABEL_NONE;
    }
    INTEGRATOR_STATE_WRITE(state, ray, D) = wo;
    INTEGRATOR_STATE_WRITE(state, subsurface, N) = sd->N;
  }

  if (sd->flag & SD_BACKFACING) {
    path_flag |= PATH_RAY_SUBSURFACE_BACKFACING;
  }

  INTEGRATOR_STATE_WRITE(state, path, flag) = path_flag;

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

  ccl_private DiffuseBsdf *bsdf = (ccl_private DiffuseBsdf *)bsdf_alloc(
      sd, sizeof(DiffuseBsdf), weight);

  if (bsdf) {
    bsdf->N = N;
    sd->flag |= bsdf_diffuse_setup(bsdf);
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

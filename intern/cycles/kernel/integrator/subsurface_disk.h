/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "kernel/integrator/guiding.h"

CCL_NAMESPACE_BEGIN

/* BSSRDF using disk based importance sampling.
 *
 * BSSRDF Importance Sampling, SIGGRAPH 2013
 * http://library.imageworks.com/pdfs/imageworks-library-BSSRDF-sampling.pdf
 */

ccl_device_inline Spectrum subsurface_disk_eval(const Spectrum radius, float disk_r, float r)
{
  const Spectrum eval = bssrdf_eval(radius, r);
  const float pdf = bssrdf_pdf(radius, disk_r);
  return (pdf > 0.0f) ? eval / pdf : zero_spectrum();
}

/* Subsurface scattering step, from a point on the surface to other
 * nearby points on the same object. */
ccl_device_inline bool subsurface_disk(KernelGlobals kg,
                                       IntegratorState state,
                                       RNGState rng_state,
                                       ccl_private Ray &ray,
                                       ccl_private LocalIntersection &ss_isect)

{
  float2 rand_disk = path_state_rng_2D(kg, &rng_state, PRNG_SUBSURFACE_DISK);

  /* Read shading point info from integrator state. */
  const float3 P = INTEGRATOR_STATE(state, ray, P);
  const float ray_dP = INTEGRATOR_STATE(state, ray, dP);
  const float time = INTEGRATOR_STATE(state, ray, time);
  const float3 Ng = INTEGRATOR_STATE(state, subsurface, Ng);
  const int object = INTEGRATOR_STATE(state, isect, object);
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

  /* Read subsurface scattering parameters. */
  const Spectrum radius = INTEGRATOR_STATE(state, subsurface, radius);

  /* Pick random axis in local frame and point on disk. */
  float3 disk_N, disk_T, disk_B;
  float pick_pdf_N, pick_pdf_T, pick_pdf_B;

  disk_N = Ng;
  make_orthonormals(disk_N, &disk_T, &disk_B);

  if (rand_disk.y < 0.5f) {
    pick_pdf_N = 0.5f;
    pick_pdf_T = 0.25f;
    pick_pdf_B = 0.25f;
    rand_disk.y *= 2.0f;
  }
  else if (rand_disk.y < 0.75f) {
    float3 tmp = disk_N;
    disk_N = disk_T;
    disk_T = tmp;
    pick_pdf_N = 0.25f;
    pick_pdf_T = 0.5f;
    pick_pdf_B = 0.25f;
    rand_disk.y = (rand_disk.y - 0.5f) * 4.0f;
  }
  else {
    float3 tmp = disk_N;
    disk_N = disk_B;
    disk_B = tmp;
    pick_pdf_N = 0.25f;
    pick_pdf_T = 0.25f;
    pick_pdf_B = 0.5f;
    rand_disk.y = (rand_disk.y - 0.75f) * 4.0f;
  }

  /* Sample point on disk. */
  float phi = M_2PI_F * rand_disk.y;
  float disk_height, disk_r;

  bssrdf_sample(radius, rand_disk.x, &disk_r, &disk_height);

  float3 disk_P = (disk_r * cosf(phi)) * disk_T + (disk_r * sinf(phi)) * disk_B;

  /* Create ray. */
  ray.P = P + disk_N * disk_height + disk_P;
  ray.D = -disk_N;
  ray.tmin = 0.0f;
  ray.tmax = 2.0f * disk_height;
  ray.dP = ray_dP;
  ray.dD = differential_zero_compact();
  ray.time = time;
  ray.self.object = OBJECT_NONE;
  ray.self.prim = PRIM_NONE;
  ray.self.light_object = OBJECT_NONE;
  ray.self.light_prim = OBJECT_NONE;

  /* Intersect with the same object. if multiple intersections are found it
   * will use at most BSSRDF_MAX_HITS hits, a random subset of all hits. */
  uint lcg_state = lcg_state_init(
      rng_state.rng_hash, rng_state.rng_offset, rng_state.sample, 0x68bc21eb);
  const int max_hits = BSSRDF_MAX_HITS;

  scene_intersect_local(kg, &ray, &ss_isect, object, &lcg_state, max_hits);
  const int num_eval_hits = min(ss_isect.num_hits, max_hits);
  if (num_eval_hits == 0) {
    return false;
  }

  /* Sort for consistent renders between CPU and GPU, independent of the BVH
   * traversal algorithm. */
  sort_intersections_and_normals(ss_isect.hits, ss_isect.Ng, num_eval_hits);

  Spectrum weights[BSSRDF_MAX_HITS]; /* TODO: zero? */
  float sum_weights = 0.0f;

  for (int hit = 0; hit < num_eval_hits; hit++) {
    /* Get geometric normal. */
    const int object = ss_isect.hits[hit].object;
    const int object_flag = kernel_data_fetch(object_flag, object);
    float3 hit_Ng = ss_isect.Ng[hit];
    if (path_flag & PATH_RAY_SUBSURFACE_BACKFACING) {
      hit_Ng = -hit_Ng;
    }
    if (object_negative_scale_applied(object_flag)) {
      hit_Ng = -hit_Ng;
    }

    if (!(object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
      /* Transform normal to world space. */
      Transform itfm;
      object_fetch_transform_motion_test(kg, object, time, &itfm);
      hit_Ng = normalize(transform_direction_transposed(&itfm, hit_Ng));
    }

    /* Quickly retrieve P and Ng without setting up ShaderData. */
    const float3 hit_P = ray.P + ray.D * ss_isect.hits[hit].t;

    /* Probability densities for local frame axes. */
    const float pdf_N = pick_pdf_N * fabsf(dot(disk_N, hit_Ng));
    const float pdf_T = pick_pdf_T * fabsf(dot(disk_T, hit_Ng));
    const float pdf_B = pick_pdf_B * fabsf(dot(disk_B, hit_Ng));

    /* Multiple importance sample between 3 axes, power heuristic
     * found to be slightly better than balance heuristic. pdf_N
     * in the MIS weight and denominator cancelled out. */
    float w = pdf_N / (sqr(pdf_N) + sqr(pdf_T) + sqr(pdf_B));
    if (ss_isect.num_hits > max_hits) {
      w *= ss_isect.num_hits / (float)max_hits;
    }

    /* Real distance to sampled point. */
    const float r = len(hit_P - P);

    /* Evaluate profiles. */
    const Spectrum weight = subsurface_disk_eval(radius, disk_r, r) * w;

    /* Store result. */
    ss_isect.Ng[hit] = hit_Ng;
    weights[hit] = weight;
    sum_weights += average(fabs(weight));
  }

  if (sum_weights == 0.0f) {
    return false;
  }

  /* Use importance resampling, sampling one of the hits proportional to weight. */
  const float rand_resample = path_state_rng_1D(kg, &rng_state, PRNG_SUBSURFACE_DISK_RESAMPLE);
  const float r = rand_resample * sum_weights;
  float partial_sum = 0.0f;

  for (int hit = 0; hit < num_eval_hits; hit++) {
    const Spectrum weight = weights[hit];
    const float sample_weight = average(fabs(weight));
    float next_sum = partial_sum + sample_weight;

    if (r < next_sum) {
      /* Return exit point. */
      const Spectrum resampled_weight = weight * sum_weights / sample_weight;
      INTEGRATOR_STATE_WRITE(state, path, throughput) *= resampled_weight;
      ss_isect.hits[0] = ss_isect.hits[hit];
      ss_isect.Ng[0] = ss_isect.Ng[hit];

      ray.P = ray.P + ray.D * ss_isect.hits[hit].t;
      ray.D = ss_isect.Ng[hit];
      ray.tmin = 0.0f;
      ray.tmax = 1.0f;

      guiding_record_bssrdf_bounce(
          kg, state, 1.0f, Ng, -Ng, resampled_weight, INTEGRATOR_STATE(state, subsurface, albedo));
      return true;
    }

    partial_sum = next_sum;
  }

  return false;
}

CCL_NAMESPACE_END

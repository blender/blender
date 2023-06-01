/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *  */

/** \file
 * \ingroup eevee
 *
 * Random number generator, contains persistent state and sample count logic.
 */

#include "BLI_rand.h"

#include "eevee_instance.hh"
#include "eevee_sampling.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Sampling
 * \{ */

void Sampling::init(const Scene *scene)
{
  sample_count_ = inst_.is_viewport() ? scene->eevee.taa_samples : scene->eevee.taa_render_samples;

  if (sample_count_ == 0) {
    BLI_assert(inst_.is_viewport());
    sample_count_ = infinite_sample_count_;
  }

  motion_blur_steps_ = !inst_.is_viewport() ? scene->eevee.motion_blur_steps : 1;
  sample_count_ = divide_ceil_u(sample_count_, motion_blur_steps_);

  if (scene->eevee.flag & SCE_EEVEE_DOF_JITTER) {
    if (sample_count_ == infinite_sample_count_) {
      /* Special case for viewport continuous rendering. We clamp to a max sample
       * to avoid the jittered dof never converging. */
      dof_ring_count_ = 6;
    }
    else {
      dof_ring_count_ = sampling_web_ring_count_get(dof_web_density_, sample_count_);
    }
    dof_sample_count_ = sampling_web_sample_count_get(dof_web_density_, dof_ring_count_);
    /* Change total sample count to fill the web pattern entirely. */
    sample_count_ = divide_ceil_u(sample_count_, dof_sample_count_) * dof_sample_count_;
  }
  else {
    dof_ring_count_ = 0;
    dof_sample_count_ = 1;
  }

  /* Only multiply after to have full the full DoF web pattern for each time steps. */
  sample_count_ *= motion_blur_steps_;
}

void Sampling::end_sync()
{
  if (reset_) {
    viewport_sample_ = 0;
  }

  if (inst_.is_viewport()) {

    interactive_mode_ = viewport_sample_ < interactive_mode_threshold;

    bool interactive_mode_disabled = (inst_.scene->eevee.flag & SCE_EEVEE_TAA_REPROJECTION) == 0;
    if (interactive_mode_disabled) {
      interactive_mode_ = false;
      sample_ = viewport_sample_;
    }
    else if (interactive_mode_) {
      int interactive_sample_count = min_ii(interactive_sample_max_, sample_count_);

      if (viewport_sample_ < interactive_sample_count) {
        /* Loop over the same starting samples. */
        sample_ = sample_ % interactive_sample_count;
      }
      else {
        /* Break out of the loop and resume normal pattern. */
        sample_ = interactive_sample_count;
      }
    }
  }
}

void Sampling::step()
{
  {
    /* TODO(fclem) we could use some persistent states to speedup the computation. */
    double2 r, offset = {0, 0};
    /* Using 2,3 primes as per UE4 Temporal AA presentation.
     * http://advances.realtimerendering.com/s2014/epic/TemporalAA.pptx (slide 14) */
    uint2 primes = {2, 3};
    BLI_halton_2d(primes, offset, sample_ + 1, r);
    /* WORKAROUND: We offset the distribution to make the first sample (0,0). This way, we are
     * assured that at least one of the samples inside the TAA rotation will match the one from the
     * draw manager. This makes sure overlays are correctly composited in static scene. */
    data_.dimensions[SAMPLING_FILTER_U] = fractf(r[0] + (1.0 / 2.0));
    data_.dimensions[SAMPLING_FILTER_V] = fractf(r[1] + (2.0 / 3.0));
    /* TODO de-correlate. */
    data_.dimensions[SAMPLING_TIME] = r[0];
    data_.dimensions[SAMPLING_CLOSURE] = r[1];
    data_.dimensions[SAMPLING_RAYTRACE_X] = r[0];
  }
  {
    double2 r, offset = {0, 0};
    uint2 primes = {5, 7};
    BLI_halton_2d(primes, offset, sample_ + 1, r);
    data_.dimensions[SAMPLING_LENS_U] = r[0];
    data_.dimensions[SAMPLING_LENS_V] = r[1];
    /* TODO de-correlate. */
    data_.dimensions[SAMPLING_LIGHTPROBE] = r[0];
    data_.dimensions[SAMPLING_TRANSPARENCY] = r[1];
  }
  {
    /* Using leaped Halton sequence so we can reused the same primes as lens. */
    double3 r, offset = {0, 0, 0};
    uint64_t leap = 11;
    uint3 primes = {5, 4, 7};
    BLI_halton_3d(primes, offset, sample_ * leap, r);
    data_.dimensions[SAMPLING_SHADOW_U] = r[0];
    data_.dimensions[SAMPLING_SHADOW_V] = r[1];
    data_.dimensions[SAMPLING_SHADOW_W] = r[2];
    /* TODO de-correlate. */
    data_.dimensions[SAMPLING_RAYTRACE_U] = r[0];
    data_.dimensions[SAMPLING_RAYTRACE_V] = r[1];
    data_.dimensions[SAMPLING_RAYTRACE_W] = r[2];
  }
  {
    /* Using leaped Halton sequence so we can reused the same primes. */
    double2 r, offset = {0, 0};
    uint64_t leap = 5;
    uint2 primes = {2, 3};
    BLI_halton_2d(primes, offset, sample_ * leap, r);
    data_.dimensions[SAMPLING_SHADOW_X] = r[0];
    data_.dimensions[SAMPLING_SHADOW_Y] = r[1];
    /* TODO de-correlate. */
    data_.dimensions[SAMPLING_SSS_U] = r[0];
    data_.dimensions[SAMPLING_SSS_V] = r[1];
  }

  data_.push_update();

  viewport_sample_++;
  sample_++;

  reset_ = false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sampling patterns
 * \{ */

float3 Sampling::sample_ball(const float3 &rand)
{
  float3 sample;
  sample.z = rand.x * 2.0f - 1.0f; /* cos theta */

  float r = sqrtf(fmaxf(0.0f, 1.0f - square_f(sample.z))); /* sin theta */

  float omega = rand.y * 2.0f * M_PI;
  sample.x = r * cosf(omega);
  sample.y = r * sinf(omega);

  sample *= sqrtf(sqrtf(rand.z));
  return sample;
}

float2 Sampling::sample_disk(const float2 &rand)
{
  float omega = rand.y * 2.0f * M_PI;
  return sqrtf(rand.x) * float2(cosf(omega), sinf(omega));
}

float2 Sampling::sample_spiral(const float2 &rand)
{
  /* Fibonacci spiral. */
  float omega = 4.0f * M_PI * (1.0f + sqrtf(5.0f)) * rand.x;
  float r = sqrtf(rand.x);
  /* Random rotation. */
  omega += rand.y * 2.0f * M_PI;
  return r * float2(cosf(omega), sinf(omega));
}

void Sampling::dof_disk_sample_get(float *r_radius, float *r_theta) const
{
  if (dof_ring_count_ == 0) {
    *r_radius = *r_theta = 0.0f;
    return;
  }

  int s = sample_ - 1;
  int ring = 0;
  int ring_sample_count = 1;
  int ring_sample = 1;

  s = s * (dof_web_density_ - 1);
  s = s % dof_sample_count_;

  /* Choosing sample to we get faster convergence.
   * The issue here is that we cannot map a low discrepancy sequence to this sampling pattern
   * because the same sample could be chosen twice in relatively short intervals. */
  /* For now just use an ascending sequence with an offset. This gives us relatively quick
   * initial coverage and relatively high distance between samples. */
  /* TODO(@fclem) We can try to order samples based on a LDS into a table to avoid duplicates.
   * The drawback would be some memory consumption and initialize time. */
  int samples_passed = 1;
  while (s >= samples_passed) {
    ring++;
    ring_sample_count = ring * dof_web_density_;
    ring_sample = s - samples_passed;
    ring_sample = (ring_sample + 1) % ring_sample_count;
    samples_passed += ring_sample_count;
  }

  *r_radius = ring / float(dof_ring_count_);
  *r_theta = 2.0f * M_PI * ring_sample / float(ring_sample_count);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cumulative Distribution Function (CDF)
 * \{ */

/* Creates a discrete cumulative distribution function table from a given curvemapping.
 * Output cdf vector is expected to already be sized according to the wanted resolution. */
void Sampling::cdf_from_curvemapping(const CurveMapping &curve, Vector<float> &cdf)
{
  BLI_assert(cdf.size() > 1);
  cdf[0] = 0.0f;
  /* Actual CDF evaluation. */
  for (int u : IndexRange(cdf.size() - 1)) {
    float x = float(u + 1) / float(cdf.size() - 1);
    cdf[u + 1] = cdf[u] + BKE_curvemapping_evaluateF(&curve, 0, x);
  }
  /* Normalize the CDF. */
  for (int u : cdf.index_range()) {
    cdf[u] /= cdf.last();
  }
  /* Just to make sure. */
  cdf.last() = 1.0f;
}

/* Inverts a cumulative distribution function.
 * Output vector is expected to already be sized according to the wanted resolution. */
void Sampling::cdf_invert(Vector<float> &cdf, Vector<float> &inverted_cdf)
{
  for (int u : inverted_cdf.index_range()) {
    float x = float(u) / float(inverted_cdf.size() - 1);
    for (int i : cdf.index_range()) {
      if (i == cdf.size() - 1) {
        inverted_cdf[u] = 1.0f;
      }
      else if (cdf[i] >= x) {
        float t = (x - cdf[i]) / (cdf[i + 1] - cdf[i]);
        inverted_cdf[u] = (float(i) + t) / float(cdf.size() - 1);
        break;
      }
    }
  }
}

/** \} */

}  // namespace blender::eevee

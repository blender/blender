/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 */

#include "BLI_vector.hh"

#include "eevee_instance.hh"
#include "eevee_subsurface.hh"

#include <iostream>

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Subsurface
 * \{ */

void SubsurfaceModule::end_sync()
{
  data_.jitter_threshold = inst_.scene->eevee.sss_jitter_threshold;
  if (data_.sample_len != inst_.scene->eevee.sss_samples) {
    /* Convert sample count from old implementation which was using a separable filter. */
    /* TODO(fclem) better remapping. */
    // data_.sample_len = square_f(1 + 2 * inst_.scene->eevee.sss_samples);
    data_.sample_len = 55;
  }

  subsurface_ps_.init();
  subsurface_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_STENCIL_EQUAL |
                           DRW_STATE_BLEND_ADD_FULL);
  subsurface_ps_.state_stencil(0x00u, 0xFFu, CLOSURE_SSS);
  subsurface_ps_.shader_set(inst_.shaders.static_shader_get(SUBSURFACE_EVAL));
  inst_.bind_uniform_data(&subsurface_ps_);
  inst_.hiz_buffer.bind_resources(&subsurface_ps_);
  subsurface_ps_.bind_texture("radiance_tx", &diffuse_light_tx_);
  subsurface_ps_.bind_texture("gbuffer_closure_tx", &inst_.gbuffer.closure_tx);
  subsurface_ps_.bind_texture("gbuffer_color_tx", &inst_.gbuffer.color_tx);
  subsurface_ps_.bind_image(RBUFS_COLOR_SLOT, &inst_.render_buffers.rp_color_tx);
  /** NOTE: Not used in the shader, but we bind it to avoid debug warnings. */
  subsurface_ps_.bind_image(RBUFS_VALUE_SLOT, &inst_.render_buffers.rp_value_tx);

  subsurface_ps_.barrier(GPU_BARRIER_TEXTURE_FETCH);
  subsurface_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void SubsurfaceModule::render(View &view, Framebuffer &fb, Texture &diffuse_light_tx)
{
  precompute_samples_location();

  fb.bind();
  diffuse_light_tx_ = *&diffuse_light_tx;
  inst_.manager->submit(subsurface_ps_, view);
}

void SubsurfaceModule::precompute_samples_location()
{
  /* Precompute sample position with white albedo. */
  float d = burley_setup(float3(1.0f), float3(1.0f)).x;

  float rand_u = inst_.sampling.rng_get(SAMPLING_SSS_U);
  float rand_v = inst_.sampling.rng_get(SAMPLING_SSS_V);

  double golden_angle = M_PI * (3.0 - sqrt(5.0));
  for (auto i : IndexRange(data_.sample_len)) {
    float theta = golden_angle * i + M_PI * 2.0f * rand_u;
    /* Scale using rand_v in order to keep first sample always at center. */
    float x = (1.0f + (rand_v / data_.sample_len)) * (i / float(data_.sample_len));
    float r = SubsurfaceModule::burley_sample(d, x);
    data_.samples[i].x = cosf(theta) * r;
    data_.samples[i].y = sinf(theta) * r;
    data_.samples[i].z = 1.0f / burley_pdf(d, r);
  }

  inst_.push_uniform_data();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Christensen-Burley SSS model
 *
 * Based on: "Approximate Reflectance Profiles for Efficient Subsurface Scattering"
 * by Per Christensen
 * https://graphics.pixar.com/library/ApproxBSSRDF/approxbssrdfslides.pdf
 * \{ */

float SubsurfaceModule::burley_sample(float d, float x_rand)
{
  x_rand *= SSS_BURLEY_TRUNCATE_CDF;

  const float tolerance = 1e-6;
  const int max_iteration_count = 10;
  /* Do initial guess based on manual curve fitting, this allows us to reduce
   * number of iterations to maximum 4 across the [0..1] range. We keep maximum
   * number of iteration higher just to be sure we didn't miss root in some
   * corner case.
   */
  float r;
  if (x_rand <= 0.9) {
    r = exp(x_rand * x_rand * 2.4) - 1.0;
  }
  else {
    /* TODO(sergey): Some nicer curve fit is possible here. */
    r = 15.0;
  }
  /* Solve against scaled radius. */
  for (int i = 0; i < max_iteration_count; i++) {
    float exp_r_3 = exp(-r / 3.0);
    float exp_r = exp_r_3 * exp_r_3 * exp_r_3;
    float f = 1.0 - 0.25 * exp_r - 0.75 * exp_r_3 - x_rand;
    float f_ = 0.25 * exp_r + 0.25 * exp_r_3;

    if (abs(f) < tolerance || f_ == 0.0) {
      break;
    }

    r = r - f / f_;
    if (r < 0.0) {
      r = 0.0;
    }
  }

  return r * d;
}

float SubsurfaceModule::burley_eval(float d, float r)
{
  if (r >= SSS_BURLEY_TRUNCATE * d) {
    return 0.0;
  }
  /* Slide 33. */
  float exp_r_3_d = expf(-r / (3.0f * d));
  float exp_r_d = exp_r_3_d * exp_r_3_d * exp_r_3_d;
  return (exp_r_d + exp_r_3_d) / (8.0f * float(M_PI) * d);
}

float SubsurfaceModule::burley_pdf(float d, float r)
{
  return burley_eval(d, r) / SSS_BURLEY_TRUNCATE_CDF;
}

/** \} */

}  // namespace blender::eevee

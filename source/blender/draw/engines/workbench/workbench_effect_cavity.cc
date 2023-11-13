/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Cavity Effect:
 *
 * We use Screen Space Ambient Occlusion (SSAO) to enhance geometric details of the surfaces.
 * We also use a Curvature effect computed only using the surface normals.
 *
 * This is done as part of the opaque resolve pass. It only affects the opaque surfaces.
 */

#include "BLI_rand.h"
#include "workbench_private.hh"

namespace blender::workbench {

void CavityEffect::init(const SceneState &scene_state, SceneResources &resources)
{
  cavity_enabled_ = scene_state.draw_cavity;
  curvature_enabled_ = scene_state.draw_curvature;

  const int ssao_samples = scene_state.scene->display.matcap_ssao_samples;
  int sample_count = min_ii(scene_state.samples_len * ssao_samples, max_samples_);
  const int max_iter_count = sample_count / ssao_samples;

  sample_ = scene_state.sample % max_iter_count;

  UniformBuffer<WorldData> &world_buf = resources.world_buf;

  world_buf.cavity_sample_start = ssao_samples * sample_;
  world_buf.cavity_sample_end = ssao_samples * (sample_ + 1);

  world_buf.cavity_sample_count_inv = 1.0f / (world_buf.cavity_sample_end -
                                              world_buf.cavity_sample_start);
  world_buf.cavity_jitter_scale = 1.0f / 64.0f;

  world_buf.cavity_valley_factor = scene_state.shading.cavity_valley_factor;
  world_buf.cavity_ridge_factor = scene_state.shading.cavity_ridge_factor;
  world_buf.cavity_attenuation = scene_state.scene->display.matcap_ssao_attenuation;
  world_buf.cavity_distance = scene_state.scene->display.matcap_ssao_distance;

  world_buf.curvature_ridge = 0.5f /
                              max_ff(square_f(scene_state.shading.curvature_ridge_factor), 1e-4f);
  world_buf.curvature_valley = 0.7f / max_ff(square_f(scene_state.shading.curvature_valley_factor),
                                             1e-4f);

  if (cavity_enabled_ && sample_count_ != sample_count) {
    sample_count_ = sample_count;
    load_samples_buf(ssao_samples);
    resources.load_jitter_tx(sample_count_);
  }
}

void CavityEffect::load_samples_buf(int ssao_samples)
{
  const float iteration_samples_inv = 1.0f / ssao_samples;

  /* Create disk samples using Hammersley distribution */
  for (int i : IndexRange(sample_count_)) {
    float it_add = (i / ssao_samples) * 0.499f;
    float r = fmodf((i + 0.5f + it_add) * iteration_samples_inv, 1.0f);
    double dphi;
    BLI_hammersley_1d(i, &dphi);

    float phi = float(dphi) * 2.0f * M_PI + it_add;
    samples_buf[i].x = math::cos(phi);
    samples_buf[i].y = math::sin(phi);
    /* This deliberately distribute more samples
     * at the center of the disk (and thus the shadow). */
    samples_buf[i].z = r;
  }

  samples_buf.push_update();
}

void CavityEffect::setup_resolve_pass(PassSimple &pass, SceneResources &resources)
{
  if (cavity_enabled_) {
    pass.bind_ubo("cavity_samples", samples_buf);
    pass.bind_texture("jitter_tx",
                      &resources.jitter_tx,
                      {GPU_SAMPLER_FILTERING_DEFAULT,
                       GPU_SAMPLER_EXTEND_MODE_REPEAT,
                       GPU_SAMPLER_EXTEND_MODE_REPEAT});
  }
  if (curvature_enabled_) {
    pass.bind_texture("object_id_tx", &resources.object_id_tx);
  }
}

}  // namespace blender::workbench

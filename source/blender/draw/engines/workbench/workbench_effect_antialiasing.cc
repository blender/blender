/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "workbench_private.hh"

#include "BLI_jitter_2d.h"
#include "BLI_smaa_textures.h"

namespace blender::workbench {

class TaaSamples {
  void init_samples(MutableSpan<float2> samples)
  {
    BLI_jitter_init(reinterpret_cast<float(*)[2]>(samples.data()), samples.size());

    /* Find closest element to center */
    int closest_index = 0;
    float closest_squared_distance = 1.0f;
    for (int i : samples.index_range()) {
      const float2 sample = samples[i];
      const float squared_dist = math::length_squared(sample);
      if (squared_dist < closest_squared_distance) {
        closest_squared_distance = squared_dist;
        closest_index = i;
      }
    }

    const float2 closest_sample = samples[closest_index];

    for (float2 &sample : samples) {
      /* Move jitter samples so that closest sample is in center */
      sample -= closest_sample;
      /* Avoid samples outside range (wrap around). */
      sample = {fmodf(sample.x + 0.5f, 1.0f), fmodf(sample.y + 0.5f, 1.0f)};
      /* Recenter the distribution[-1..1]. */
      sample = (sample * 2.0f) - 1.0f;
    }

    /* Swap center sample to the start of the array */
    if (closest_index != 0) {
      std::swap(samples[0], samples[closest_index]);
    }

    /* Sort list based on farthest distance with previous. */
    for (int i = 0; i < samples.size() - 2; i++) {
      float squared_dist = 0.0;
      int index = i;
      for (int j = i + 1; j < samples.size(); j++) {
        const float _squared_dist = math::length_squared(samples[i] - samples[j]);
        if (_squared_dist > squared_dist) {
          squared_dist = _squared_dist;
          index = j;
        }
      }
      std::swap(samples[i + 1], samples[index]);
    }
  }

 public:
  std::array<float2, 5> x5;
  std::array<float2, 8> x8;
  std::array<float2, 11> x11;
  std::array<float2, 16> x16;
  std::array<float2, 32> x32;

  TaaSamples()
  {
    init_samples(x5);
    init_samples(x8);
    init_samples(x11);
    init_samples(x16);
    init_samples(x32);
  }
};

static const TaaSamples &get_taa_samples()
{
  static const TaaSamples taa_samples;
  return taa_samples;
}

static float filter_blackman_harris(float x, const float width)
{
  if (x > width * 0.5f) {
    return 0.0f;
  }
  x = 2.0f * M_PI * clamp_f((x / width + 0.5f), 0.0f, 1.0f);
  return 0.35875f - 0.48829f * math::cos(x) + 0.14128f * math::cos(2.0f * x) -
         0.01168f * math::cos(3.0f * x);
}

/* Compute weights for the 3x3 neighborhood using a 1.5px filter. */
static void setup_taa_weights(const float2 offset, float r_weights[9], float &r_weight_sum)
{
  /* NOTE: If filter width is bigger than 2.0f, then we need to sample more neighborhood. */
  const float filter_width = 2.0f;
  r_weight_sum = 0.0f;
  int i = 0;
  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++, i++) {
      float2 sample_co = float2(x, y) - offset;
      float r = len_v2(sample_co);
      /* fclem: Is radial distance ok here? */
      float weight = filter_blackman_harris(r, filter_width);
      r_weight_sum += weight;
      r_weights[i] = weight;
    }
  }
}

AntiAliasingPass::AntiAliasingPass()
{
  taa_accumulation_sh_ = GPU_shader_create_from_info_name("workbench_taa");
  smaa_edge_detect_sh_ = GPU_shader_create_from_info_name("workbench_smaa_stage_0");
  smaa_aa_weight_sh_ = GPU_shader_create_from_info_name("workbench_smaa_stage_1");
  smaa_resolve_sh_ = GPU_shader_create_from_info_name("workbench_smaa_stage_2");
  overlay_depth_sh_ = GPU_shader_create_from_info_name("workbench_overlay_depth");

  smaa_search_tx_.ensure_2d(
      GPU_R8, {SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT}, GPU_TEXTURE_USAGE_SHADER_READ);
  GPU_texture_update(smaa_search_tx_, GPU_DATA_UBYTE, searchTexBytes);
  GPU_texture_filter_mode(smaa_search_tx_, true);

  smaa_area_tx_.ensure_2d(GPU_RG8, {AREATEX_WIDTH, AREATEX_HEIGHT}, GPU_TEXTURE_USAGE_SHADER_READ);
  GPU_texture_update(smaa_area_tx_, GPU_DATA_UBYTE, areaTexBytes);
  GPU_texture_filter_mode(smaa_area_tx_, true);
}

AntiAliasingPass::~AntiAliasingPass()
{
  DRW_SHADER_FREE_SAFE(taa_accumulation_sh_);
  DRW_SHADER_FREE_SAFE(smaa_edge_detect_sh_);
  DRW_SHADER_FREE_SAFE(smaa_aa_weight_sh_);
  DRW_SHADER_FREE_SAFE(smaa_resolve_sh_);
  DRW_SHADER_FREE_SAFE(overlay_depth_sh_);
}

void AntiAliasingPass::init(const SceneState &scene_state)
{
  enabled_ = scene_state.draw_aa;
  sample_ = scene_state.sample;
  samples_len_ = scene_state.samples_len;
}

void AntiAliasingPass::sync(SceneResources &resources, int2 resolution)
{
  overlay_depth_ps_.init();
  overlay_depth_ps_.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
  overlay_depth_ps_.shader_set(overlay_depth_sh_);
  overlay_depth_ps_.bind_texture("depth_tx", &resources.depth_tx);
  overlay_depth_ps_.bind_texture("stencil_tx", &stencil_tx_);
  overlay_depth_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  if (!enabled_) {
    taa_accumulation_tx_.free();
    sample0_depth_tx_.free();
    return;
  }

  taa_accumulation_tx_.ensure_2d(
      GPU_RGBA16F, resolution, GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);
  sample0_depth_tx_.ensure_2d(GPU_DEPTH24_STENCIL8,
                              resolution,
                              GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);

  taa_accumulation_ps_.init();
  taa_accumulation_ps_.state_set(sample_ == 0 ? DRW_STATE_WRITE_COLOR :
                                                DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);
  taa_accumulation_ps_.shader_set(taa_accumulation_sh_);
  taa_accumulation_ps_.bind_texture("colorBuffer", &resources.color_tx);
  taa_accumulation_ps_.push_constant("samplesWeights", weights_, 9);
  taa_accumulation_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  smaa_edge_detect_ps_.init();
  smaa_edge_detect_ps_.state_set(DRW_STATE_WRITE_COLOR);
  smaa_edge_detect_ps_.shader_set(smaa_edge_detect_sh_);
  smaa_edge_detect_ps_.bind_texture("colorTex", &taa_accumulation_tx_);
  smaa_edge_detect_ps_.push_constant("viewportMetrics", &smaa_viewport_metrics_, 1);
  smaa_edge_detect_ps_.clear_color(float4(0.0f));
  smaa_edge_detect_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  smaa_aa_weight_ps_.init();
  smaa_aa_weight_ps_.state_set(DRW_STATE_WRITE_COLOR);
  smaa_aa_weight_ps_.shader_set(smaa_aa_weight_sh_);
  smaa_aa_weight_ps_.bind_texture("edgesTex", &smaa_edge_tx_);
  smaa_aa_weight_ps_.bind_texture("areaTex", smaa_area_tx_);
  smaa_aa_weight_ps_.bind_texture("searchTex", smaa_search_tx_);
  smaa_aa_weight_ps_.push_constant("viewportMetrics", &smaa_viewport_metrics_, 1);
  smaa_aa_weight_ps_.clear_color(float4(0.0f));
  smaa_aa_weight_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  smaa_resolve_ps_.init();
  smaa_resolve_ps_.state_set(DRW_STATE_WRITE_COLOR);
  smaa_resolve_ps_.shader_set(smaa_resolve_sh_);
  smaa_resolve_ps_.bind_texture("blendTex", &smaa_weight_tx_);
  smaa_resolve_ps_.bind_texture("colorTex", &taa_accumulation_tx_);
  smaa_resolve_ps_.push_constant("viewportMetrics", &smaa_viewport_metrics_, 1);
  smaa_resolve_ps_.push_constant("mixFactor", &smaa_mix_factor_, 1);
  smaa_resolve_ps_.push_constant("taaAccumulatedWeight", &weight_accum_, 1);
  smaa_resolve_ps_.clear_color(float4(0.0f));
  smaa_resolve_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void AntiAliasingPass::setup_view(View &view, int2 resolution)
{
  if (!enabled_) {
    return;
  }

  const TaaSamples &taa_samples = get_taa_samples();

  float2 sample_offset;
  switch (samples_len_) {
    default:
    case 5:
      sample_offset = taa_samples.x5[sample_];
      break;
    case 8:
      sample_offset = taa_samples.x8[sample_];
      break;
    case 11:
      sample_offset = taa_samples.x11[sample_];
      break;
    case 16:
      sample_offset = taa_samples.x16[sample_];
      break;
    case 32:
      sample_offset = taa_samples.x32[sample_];
      break;
  }

  setup_taa_weights(sample_offset, weights_, weights_sum_);

  /* TODO(@pragma37): New API equivalent? */
  const DRWView *default_view = DRW_view_default_get();
  float4x4 winmat, viewmat, persmat;
  /* Construct new matrices from transform delta */
  DRW_view_winmat_get(default_view, winmat.ptr(), false);
  DRW_view_viewmat_get(default_view, viewmat.ptr(), false);
  DRW_view_persmat_get(default_view, persmat.ptr(), false);

  window_translate_m4(
      winmat.ptr(), persmat.ptr(), sample_offset.x / resolution.x, sample_offset.y / resolution.y);

  view.sync(viewmat, winmat);
}

void AntiAliasingPass::draw(Manager &manager,
                            View &view,
                            SceneResources &resources,
                            int2 resolution,
                            GPUTexture *depth_tx,
                            GPUTexture *color_tx)
{
  auto draw_overlay_depth = [&](GPUTexture *target) {
    stencil_tx_ = resources.depth_tx.stencil_view();
    overlay_depth_fb_.ensure(GPU_ATTACHMENT_TEXTURE(target));
    overlay_depth_fb_.bind();
    manager.submit(overlay_depth_ps_);
  };

  if (!enabled_) {
    GPU_texture_copy(color_tx, resources.color_tx);
    draw_overlay_depth(depth_tx);
    return;
  }

  /**
   * We always do SMAA on top of TAA accumulation, unless the number of samples of TAA is already
   * high. This ensure a smoother transition.
   * If TAA accumulation is finished, we only blit the result.
   */
  const bool last_sample = sample_ + 1 == samples_len_;
  const bool taa_finished = sample_ >= samples_len_;

  if (!taa_finished) {
    if (sample_ == 0) {
      weight_accum_ = 0;
    }
    /* Accumulate result to the TAA buffer. */
    taa_accumulation_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(taa_accumulation_tx_));
    taa_accumulation_fb_.bind();
    manager.submit(taa_accumulation_ps_, view);
    weight_accum_ += weights_sum_;
  }

  if (sample_ == 0) {
    draw_overlay_depth(sample0_depth_tx_);
  }
  /* Copy back the saved depth buffer for correct overlays. */
  GPU_texture_copy(depth_tx, sample0_depth_tx_);

  if (!DRW_state_is_image_render() || last_sample) {
    smaa_weight_tx_.acquire(
        resolution, GPU_RGBA8, GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);
    smaa_mix_factor_ = 1.0f - clamp_f(sample_ / 4.0f, 0.0f, 1.0f);
    smaa_viewport_metrics_ = float4(float2(1.0f / float2(resolution)), resolution);

    /* After a certain point SMAA is no longer necessary. */
    if (smaa_mix_factor_ > 0.0f) {
      smaa_edge_tx_.acquire(
          resolution, GPU_RG8, GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);
      smaa_edge_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(smaa_edge_tx_));
      smaa_edge_fb_.bind();
      manager.submit(smaa_edge_detect_ps_, view);

      smaa_weight_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(smaa_weight_tx_));
      smaa_weight_fb_.bind();
      manager.submit(smaa_aa_weight_ps_, view);
      smaa_edge_tx_.release();
    }
    smaa_resolve_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(color_tx));
    smaa_resolve_fb_.bind();
    manager.submit(smaa_resolve_ps_, view);
    smaa_weight_tx_.release();
  }
}

}  // namespace blender::workbench

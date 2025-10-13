/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "workbench_private.hh"

#include "BLI_jitter_2d.h"
#include "BLI_math_geom.h"
#include "BLI_smaa_textures.h"

namespace blender::workbench {

class TaaSamples {
  void init_samples(MutableSpan<float2> samples)
  {
    BLI_jitter_init(reinterpret_cast<float (*)[2]>(samples.data()), samples.size());

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
  smaa_search_tx_.ensure_2d(gpu::TextureFormat::UNORM_8,
                            {SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT},
                            GPU_TEXTURE_USAGE_SHADER_READ);
  GPU_texture_update(smaa_search_tx_, GPU_DATA_UBYTE, searchTexBytes);
  GPU_texture_filter_mode(smaa_search_tx_, true);

  smaa_area_tx_.ensure_2d(gpu::TextureFormat::UNORM_8_8,
                          {AREATEX_WIDTH, AREATEX_HEIGHT},
                          GPU_TEXTURE_USAGE_SHADER_READ);
  GPU_texture_update(smaa_area_tx_, GPU_DATA_UBYTE, areaTexBytes);
  GPU_texture_filter_mode(smaa_area_tx_, true);
}

void AntiAliasingPass::init(const SceneState &scene_state)
{
  enabled_ = scene_state.draw_aa;
}

void AntiAliasingPass::sync(const SceneState &scene_state, SceneResources &resources)
{
  overlay_depth_ps_.init();
  overlay_depth_ps_.state_set(DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS |
                              DRW_STATE_STENCIL_EQUAL);
  overlay_depth_ps_.state_stencil(0x00, 0xFF, uint8_t(StencilBits::OBJECT_IN_FRONT));
  overlay_depth_ps_.shader_set(ShaderCache::get().overlay_depth.get());
  overlay_depth_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  if (!enabled_) {
    taa_accumulation_tx_.free();
    sample0_depth_tx_.free();
    return;
  }

  smaa_viewport_metrics_ = float4(float2(1.0f / float2(scene_state.resolution)),
                                  scene_state.resolution);
  smaa_mix_factor_ = 1.0f - clamp_f(scene_state.sample / 4.0f, 0.0f, 1.0f);

  taa_accumulation_tx_.ensure_2d(gpu::TextureFormat::SFLOAT_16_16_16_16,
                                 scene_state.resolution,
                                 GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);
  sample0_depth_tx_.ensure_2d(gpu::TextureFormat::SFLOAT_32_DEPTH_UINT_8,
                              scene_state.resolution,
                              GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);

  taa_accumulation_ps_.init();
  taa_accumulation_ps_.state_set(scene_state.sample == 0 ?
                                     DRW_STATE_WRITE_COLOR :
                                     DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD_FULL);
  taa_accumulation_ps_.shader_set(ShaderCache::get().taa_accumulation.get());
  taa_accumulation_ps_.bind_texture("color_buffer", &resources.color_tx);
  taa_accumulation_ps_.push_constant("samplesWeights", weights_, 9);
  taa_accumulation_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  smaa_edge_detect_ps_.init();
  smaa_edge_detect_ps_.state_set(DRW_STATE_WRITE_COLOR);
  smaa_edge_detect_ps_.shader_set(ShaderCache::get().smaa_edge_detect.get());
  smaa_edge_detect_ps_.bind_texture("color_tx", &taa_accumulation_tx_);
  smaa_edge_detect_ps_.push_constant("viewport_metrics", &smaa_viewport_metrics_, 1);
  smaa_edge_detect_ps_.clear_color(float4(0.0f));
  smaa_edge_detect_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  smaa_aa_weight_ps_.init();
  smaa_aa_weight_ps_.state_set(DRW_STATE_WRITE_COLOR);
  smaa_aa_weight_ps_.shader_set(ShaderCache::get().smaa_aa_weight.get());
  smaa_aa_weight_ps_.bind_texture("edges_tx", &smaa_edge_tx_);
  smaa_aa_weight_ps_.bind_texture("area_tx", smaa_area_tx_);
  smaa_aa_weight_ps_.bind_texture("search_tx", smaa_search_tx_);
  smaa_aa_weight_ps_.push_constant("viewport_metrics", &smaa_viewport_metrics_, 1);
  smaa_aa_weight_ps_.clear_color(float4(0.0f));
  smaa_aa_weight_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

  smaa_resolve_ps_.init();
  smaa_resolve_ps_.state_set(DRW_STATE_WRITE_COLOR);
  smaa_resolve_ps_.shader_set(ShaderCache::get().smaa_resolve.get());
  smaa_resolve_ps_.bind_texture("blend_tx", &smaa_weight_tx_);
  smaa_resolve_ps_.bind_texture("color_tx", &taa_accumulation_tx_);
  smaa_resolve_ps_.push_constant("viewport_metrics", &smaa_viewport_metrics_, 1);
  smaa_resolve_ps_.push_constant("mix_factor", &smaa_mix_factor_, 1);
  smaa_resolve_ps_.push_constant("taa_accumulated_weight", &weight_accum_, 1);
  smaa_resolve_ps_.clear_color(float4(0.0f));
  smaa_resolve_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void AntiAliasingPass::setup_view(View &view, const SceneState &scene_state)
{
  const View &default_view = View::default_get();
  const float4x4 &viewmat = default_view.viewmat();
  float4x4 winmat = default_view.winmat();
  float4x4 persmat = default_view.persmat();

  if (!enabled_) {
    view.sync(viewmat, winmat);
    return;
  }

  const TaaSamples &taa_samples = get_taa_samples();

  float2 sample_offset;
  switch (scene_state.samples_len) {
    default:
    case 5:
      sample_offset = taa_samples.x5[scene_state.sample];
      break;
    case 8:
      sample_offset = taa_samples.x8[scene_state.sample];
      break;
    case 11:
      sample_offset = taa_samples.x11[scene_state.sample];
      break;
    case 16:
      sample_offset = taa_samples.x16[scene_state.sample];
      break;
    case 32:
      sample_offset = taa_samples.x32[scene_state.sample];
      break;
  }

  setup_taa_weights(sample_offset, weights_, weights_sum_);

  window_translate_m4(winmat.ptr(),
                      persmat.ptr(),
                      sample_offset.x / scene_state.resolution.x,
                      sample_offset.y / scene_state.resolution.y);

  view.sync(viewmat, winmat);
}

void AntiAliasingPass::draw(const DRWContext *draw_ctx,
                            Manager &manager,
                            View &view,
                            const SceneState &scene_state,
                            SceneResources &resources,
                            gpu::Texture *depth_in_front_tx)
{
  if (resources.depth_in_front_tx.is_valid() && scene_state.sample == 0) {
    overlay_depth_fb_.ensure(GPU_ATTACHMENT_TEXTURE(resources.depth_tx));
    overlay_depth_fb_.bind();
    manager.submit(overlay_depth_ps_);
  }

  if (!enabled_) {
    return;
  }

  const bool last_sample = scene_state.sample + 1 == scene_state.samples_len;

  if (scene_state.samples_len > 1) {
    if (scene_state.sample == 0) {
      GPU_texture_copy(sample0_depth_tx_, resources.depth_tx);
      if (resources.depth_in_front_tx.is_valid()) {
        sample0_depth_in_front_tx_.ensure_2d(gpu::TextureFormat::SFLOAT_32_DEPTH_UINT_8,
                                             scene_state.resolution,
                                             GPU_TEXTURE_USAGE_ATTACHMENT);
        GPU_texture_copy(sample0_depth_in_front_tx_, resources.depth_in_front_tx);
      }
      else {
        sample0_depth_in_front_tx_.free();
      }
    }
    else if (!draw_ctx->is_scene_render() || last_sample) {
      /* Copy back the saved depth buffer for correct overlays. */
      GPU_texture_copy(resources.depth_tx, sample0_depth_tx_);
      if (sample0_depth_in_front_tx_.is_valid()) {
        GPU_texture_copy(depth_in_front_tx, sample0_depth_in_front_tx_);
      }
    }
  }

  /**
   * We always do SMAA on top of TAA accumulation, unless the number of samples of TAA is already
   * high. This ensure a smoother transition.
   * If TAA accumulation is finished, we only blit the result.
   */
  const bool taa_finished = scene_state.sample >= scene_state.samples_len;
  if (!taa_finished) {
    if (scene_state.sample == 0) {
      weight_accum_ = 0;
    }
    /* Accumulate result to the TAA buffer. */
    taa_accumulation_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(taa_accumulation_tx_));
    taa_accumulation_fb_.bind();
    manager.submit(taa_accumulation_ps_, view);
    weight_accum_ += weights_sum_;
  }

  /** Always acquire to avoid constant allocation/deallocation. */
  smaa_weight_tx_.acquire(scene_state.resolution,
                          gpu::TextureFormat::UNORM_8_8_8_8,
                          GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);
  smaa_edge_tx_.acquire(scene_state.resolution,
                        gpu::TextureFormat::UNORM_8_8,
                        GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT);

  if (!draw_ctx->is_image_render() || last_sample || taa_finished) {
    /* After a certain point SMAA is no longer necessary. */
    if (smaa_mix_factor_ > 0.0f) {
      smaa_edge_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(smaa_edge_tx_));
      smaa_edge_fb_.bind();
      manager.submit(smaa_edge_detect_ps_, view);

      smaa_weight_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(smaa_weight_tx_));
      smaa_weight_fb_.bind();
      manager.submit(smaa_aa_weight_ps_, view);
    }
    smaa_resolve_fb_.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(resources.color_tx));
    smaa_resolve_fb_.bind();
    manager.submit(smaa_resolve_ps_, view);
  }

  smaa_edge_tx_.release();
  smaa_weight_tx_.release();
}

}  // namespace blender::workbench

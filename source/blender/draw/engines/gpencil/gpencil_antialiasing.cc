/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_rand.h"
#include "DNA_scene_types.h"
#include "DRW_render.hh"

#include "gpencil_engine_private.hh"

#include "BLI_smaa_textures.h"

namespace blender::draw::gpencil {

void Instance::antialiasing_init()
{
  const float2 size_f = this->draw_ctx->viewport_size_get();
  const int2 size(size_f[0], size_f[1]);
  const float4 metrics = {1.0f / size[0], 1.0f / size[1], float(size[0]), float(size[1])};

  if (this->simplify_antialias) {
    /* No AA fallback. */
    PassSimple &pass = this->smaa_resolve_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
    pass.shader_set(ShaderCache::get().antialiasing[2].get());
    pass.bind_texture("blend_tx", &this->color_tx);
    pass.bind_texture("color_tx", &this->color_tx);
    pass.bind_texture("reveal_tx", &this->reveal_tx);
    pass.push_constant("do_anti_aliasing", false);
    pass.push_constant("only_alpha", this->draw_wireframe);
    pass.push_constant("viewport_metrics", metrics);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    return;
  }

  if (!this->smaa_search_tx.is_valid()) {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ;
    this->smaa_search_tx.ensure_2d(
        gpu::TextureFormat::UNORM_8, int2(SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT), usage);
    GPU_texture_update(this->smaa_search_tx, GPU_DATA_UBYTE, searchTexBytes);

    this->smaa_area_tx.ensure_2d(
        gpu::TextureFormat::UNORM_8_8, int2(AREATEX_WIDTH, AREATEX_HEIGHT), usage);
    GPU_texture_update(this->smaa_area_tx, GPU_DATA_UBYTE, areaTexBytes);

    GPU_texture_filter_mode(this->smaa_search_tx, true);
    GPU_texture_filter_mode(this->smaa_area_tx, true);
  }

  {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    this->smaa_edge_tx.acquire(size, gpu::TextureFormat::UNORM_8_8, usage);
    this->smaa_weight_tx.acquire(size, gpu::TextureFormat::UNORM_8_8_8_8, usage);

    this->smaa_edge_fb.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(this->smaa_edge_tx));
    this->smaa_weight_fb.ensure(GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(this->smaa_weight_tx));
  }

  SceneGpencil gpencil_settings = this->scene->grease_pencil_settings;
  const float luma_weight = this->is_viewport ? gpencil_settings.smaa_threshold :
                                                gpencil_settings.smaa_threshold_render;

  {
    /* Stage 1: Edge detection. */
    PassSimple &pass = this->smaa_edge_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR);
    pass.shader_set(ShaderCache::get().antialiasing[0].get());
    pass.bind_texture("color_tx", &this->color_tx);
    pass.bind_texture("reveal_tx", &this->reveal_tx);
    pass.push_constant("viewport_metrics", metrics);
    pass.push_constant("luma_weight", luma_weight);
    pass.clear_color(float4(0.0f));
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
  {
    /* Stage 2: Blend Weight/Coord. */
    PassSimple &pass = this->smaa_weight_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR);
    pass.shader_set(ShaderCache::get().antialiasing[1].get());
    pass.bind_texture("edges_tx", &this->smaa_edge_tx);
    pass.bind_texture("area_tx", &this->smaa_area_tx);
    pass.bind_texture("search_tx", &this->smaa_search_tx);
    pass.push_constant("viewport_metrics", metrics);
    pass.clear_color(float4(0.0f));
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
  {
    /* Stage 3: Resolve. */
    PassSimple &pass = this->smaa_resolve_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_CUSTOM);
    pass.shader_set(ShaderCache::get().antialiasing[2].get());
    pass.bind_texture("blend_tx", &this->smaa_weight_tx);
    pass.bind_texture("color_tx", &this->color_tx);
    pass.bind_texture("reveal_tx", &this->reveal_tx);
    pass.push_constant("do_anti_aliasing", true);
    pass.push_constant("only_alpha", this->draw_wireframe);
    pass.push_constant("viewport_metrics", metrics);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }
}

void Instance::antialiasing_draw(Manager &manager)
{
  if (!this->simplify_antialias) {
    GPU_framebuffer_bind(this->smaa_edge_fb);
    manager.submit(this->smaa_edge_ps);

    GPU_framebuffer_bind(this->smaa_weight_fb);
    manager.submit(this->smaa_weight_ps);
  }

  GPU_framebuffer_bind(this->scene_fb);
  manager.submit(this->smaa_resolve_ps);

  if (this->use_separate_pass) {
    GPU_framebuffer_bind(this->gpencil_pass_fb);
    GPU_framebuffer_clear(this->gpencil_pass_fb, GPU_COLOR_BIT, float4(0, 0, 0, 0), 0, 0);
    manager.submit(this->smaa_resolve_ps);
  }
}

static float erfinv_approx(const float x)
{
  /* From: Approximating the `erfinv` function by Mike Giles. */
  /* To avoid trouble at the limit, clamp input to 1-epsilon. */
  const float a = math::min(fabsf(x), 0.99999994f);
  float w = -logf((1.0f - a) * (1.0f + a));
  float p;
  if (w < 5.0f) {
    w = w - 2.5f;
    p = 2.81022636e-08f;
    p = p * w + 3.43273939e-07f;
    p = p * w + -3.5233877e-06f;
    p = p * w + -4.39150654e-06f;
    p = p * w + 0.00021858087f;
    p = p * w + -0.00125372503f;
    p = p * w + -0.00417768164f;
    p = p * w + 0.246640727f;
    p = p * w + 1.50140941f;
  }
  else {
    w = sqrtf(w) - 3.0f;
    p = -0.000200214257f;
    p = p * w + 0.000100950558f;
    p = p * w + 0.00134934322f;
    p = p * w + -0.00367342844f;
    p = p * w + 0.00573950773f;
    p = p * w + -0.0076224613f;
    p = p * w + 0.00943887047f;
    p = p * w + 1.00167406f;
    p = p * w + 2.83297682f;
  }
  return p * x;
}

float2 Instance::antialiasing_sample_get(const int sample_index, const int sample_count)
{
  if (sample_count < 2) {
    return float2(0.0f);
  }

  double halton[2];
  {
    uint primes[2] = {2, 3};
    double ofs[2] = {0, 0};
    BLI_halton_2d(primes, ofs, sample_index, halton);
  }
  /* Uniform distribution [0..1]. */
  const float2 rand = float2(halton[0], halton[1]);
  /* Uniform distribution [-1..1]. */
  const float2 rand_remap = rand * 2.0f - 1.0f;
  /* Limit sampling region to avoid outliers. */
  const float2 rand_adjusted = rand_remap * 0.93f;
  /* Gaussian distribution [-1..1]. */
  const float2 offset = float2(erfinv_approx(rand_adjusted.x), erfinv_approx(rand_adjusted.y));
  /* Gaussian fitted to Blackman-Harris (follows EEVEE). */
  const float sigma = 0.284f;
  /* NOTE(fclem): Not sure where this sqrt comes from but is needed to match EEVEE. */
  return offset * sqrt(sigma);
}

void Instance::antialiasing_accumulate(Manager &manager, const float alpha)
{
  BLI_assert_msg(this->render_color_tx.gpu_texture() != nullptr,
                 "This should only be called during render");
  const int2 size = this->render_color_tx.size().xy();

  const eGPUTextureUsage usage = GPU_TEXTURE_USAGE_HOST_READ | GPU_TEXTURE_USAGE_SHADER_READ |
                                 GPU_TEXTURE_USAGE_SHADER_WRITE | GPU_TEXTURE_USAGE_ATTACHMENT;
  accumulation_tx.ensure_2d(gpu::TextureFormat::GPENCIL_ACCUM_FORMAT, size, usage);

  {
    PassSimple &pass = this->accumulate_ps;
    pass.init();
    pass.state_set(DRW_STATE_WRITE_DEPTH /* There is no depth, but avoid blank state. */);
    pass.shader_set(ShaderCache::get().accumulation.get());
    pass.bind_image("src_img", &this->render_color_tx);
    pass.bind_image("dst_img", &this->accumulation_tx);
    pass.push_constant("weight_src", alpha);
    pass.push_constant("weight_dst", 1.0f - alpha);
    pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  }

  accumulation_fb.ensure(size);
  GPU_framebuffer_bind(this->accumulation_fb);
  manager.submit(this->accumulate_ps);
}

}  // namespace blender::draw::gpencil

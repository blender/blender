/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Eevee's bloom shader.
 */

#include "DRW_render.hh"

#include "GPU_texture.hh"

#include "DEG_depsgraph_query.hh"

#include "eevee_private.hh"

static const bool use_highres = true;

int EEVEE_bloom_init(EEVEE_ViewLayerData * /*sldata*/, EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_EffectsInfo *effects = stl->effects;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  if (scene_eval->eevee.flag & SCE_EEVEE_BLOOM_ENABLED) {
    const float *viewport_size = DRW_viewport_size_get();

    /* Bloom */
    int blitsize[2], texsize[2];

    /* Blit Buffer */
    effects->source_texel_size[0] = 1.0f / viewport_size[0];
    effects->source_texel_size[1] = 1.0f / viewport_size[1];

    blitsize[0] = int(viewport_size[0]);
    blitsize[1] = int(viewport_size[1]);

    effects->blit_texel_size[0] = 1.0f / float(blitsize[0]);
    effects->blit_texel_size[1] = 1.0f / float(blitsize[1]);

    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    effects->bloom_blit = DRW_texture_pool_query_2d_ex(
        blitsize[0], blitsize[1], GPU_R11F_G11F_B10F, usage, &draw_engine_eevee_type);

    GPU_framebuffer_ensure_config(
        &fbl->bloom_blit_fb, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(effects->bloom_blit)});

    /* Parameters */
    const float threshold = scene_eval->eevee.bloom_threshold;
    const float knee = scene_eval->eevee.bloom_knee;
    const float intensity = scene_eval->eevee.bloom_intensity;
    const float *color = scene_eval->eevee.bloom_color;
    const float radius = scene_eval->eevee.bloom_radius;
    effects->bloom_clamp = scene_eval->eevee.bloom_clamp;

    /* determine the iteration count */
    const float minDim = float(std::min(blitsize[0], blitsize[1]));
    const float maxIter = (radius - 8.0f) + log(minDim) / log(2);
    const int maxIterInt = effects->bloom_iteration_len = int(maxIter);

    CLAMP(effects->bloom_iteration_len, 1, MAX_BLOOM_STEP);

    effects->bloom_sample_scale = 0.5f + maxIter - float(maxIterInt);
    effects->bloom_curve_threshold[0] = threshold - knee;
    effects->bloom_curve_threshold[1] = knee * 2.0f;
    effects->bloom_curve_threshold[2] = 0.25f / max_ff(1e-5f, knee);
    effects->bloom_curve_threshold[3] = threshold;

    mul_v3_v3fl(effects->bloom_color, color, intensity);

    /* Downsample buffers */
    copy_v2_v2_int(texsize, blitsize);
    for (int i = 0; i < effects->bloom_iteration_len; i++) {
      texsize[0] /= 2;
      texsize[1] /= 2;

      texsize[0] = std::max(texsize[0], 2);
      texsize[1] = std::max(texsize[1], 2);

      effects->downsamp_texel_size[i][0] = 1.0f / float(texsize[0]);
      effects->downsamp_texel_size[i][1] = 1.0f / float(texsize[1]);

      eGPUTextureUsage downsample_usage = GPU_TEXTURE_USAGE_SHADER_READ |
                                          GPU_TEXTURE_USAGE_ATTACHMENT;
      effects->bloom_downsample[i] = DRW_texture_pool_query_2d_ex(
          texsize[0], texsize[1], GPU_R11F_G11F_B10F, downsample_usage, &draw_engine_eevee_type);
      GPU_framebuffer_ensure_config(
          &fbl->bloom_down_fb[i],
          {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(effects->bloom_downsample[i])});
    }

    /* Upsample buffers */
    copy_v2_v2_int(texsize, blitsize);
    for (int i = 0; i < effects->bloom_iteration_len - 1; i++) {
      texsize[0] /= 2;
      texsize[1] /= 2;

      texsize[0] = std::max(texsize[0], 2);
      texsize[1] = std::max(texsize[1], 2);

      eGPUTextureUsage upsample_usage = GPU_TEXTURE_USAGE_SHADER_READ |
                                        GPU_TEXTURE_USAGE_ATTACHMENT;

      effects->bloom_upsample[i] = DRW_texture_pool_query_2d_ex(
          texsize[0], texsize[1], GPU_R11F_G11F_B10F, upsample_usage, &draw_engine_eevee_type);
      GPU_framebuffer_ensure_config(
          &fbl->bloom_accum_fb[i],
          {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(effects->bloom_upsample[i])});
    }

    return EFFECT_BLOOM | EFFECT_POST_BUFFER;
  }

  /* Cleanup to release memory */
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->bloom_blit_fb);

  for (int i = 0; i < MAX_BLOOM_STEP - 1; i++) {
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->bloom_down_fb[i]);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->bloom_accum_fb[i]);
  }

  return 0;
}

static DRWShadingGroup *eevee_create_bloom_pass(const char *name,
                                                EEVEE_EffectsInfo *effects,
                                                GPUShader *sh,
                                                DRWPass **pass,
                                                bool upsample,
                                                bool resolve,
                                                bool resolve_add_base)
{
  blender::gpu::Batch *quad = DRW_cache_fullscreen_quad_get();

  *pass = DRW_pass_create(name, DRW_STATE_WRITE_COLOR);

  DRWShadingGroup *grp = DRW_shgroup_create(sh, *pass);
  DRW_shgroup_call(grp, quad, nullptr);
  DRW_shgroup_uniform_texture_ref(grp, "sourceBuffer", &effects->unf_source_buffer);
  DRW_shgroup_uniform_vec2(grp, "sourceBufferTexelSize", effects->unf_source_texel_size, 1);
  if (upsample) {
    DRW_shgroup_uniform_texture_ref(grp, "baseBuffer", &effects->unf_base_buffer);
    DRW_shgroup_uniform_float(grp, "sampleScale", &effects->bloom_sample_scale, 1);
  }
  if (resolve) {
    DRW_shgroup_uniform_vec3(grp, "bloomColor", effects->bloom_color, 1);
    DRW_shgroup_uniform_bool_copy(grp, "bloomAddBase", resolve_add_base);
  }

  return grp;
}

void EEVEE_bloom_cache_init(EEVEE_ViewLayerData * /*sldata*/, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  psl->bloom_accum_ps = nullptr;

  if ((effects->enabled_effects & EFFECT_BLOOM) != 0) {
    /**
     * Bloom Algorithm
     *
     * Overview:
     * - Down-sample the color buffer doing a small blur during each step.
     * - Accumulate bloom color using previously down-sampled color buffers
     *   and do an up-sample blur for each new accumulated layer.
     * - Finally add accumulation buffer onto the source color buffer.
     *
     *  [1/1] is original copy resolution (can be half or quarter res for performance)
     * <pre>
     *                            [DOWNSAMPLE CHAIN]                      [UPSAMPLE CHAIN]
     *
     * Source Color ─ [Blit] ─> Bright Color Extract [1/1]                  Final Color
     *                                    |                                      Λ
     *                            [Downsample First]       Source Color ─> + [Resolve]
     *                                    v                                      |
     *                          Color Downsampled [1/2] ────────────> + Accumulation Buffer [1/2]
     *                                    |                                      Λ
     *                                   ───                                    ───
     *                                  Repeat                                 Repeat
     *                                   ───                                    ───
     *                                    v                                      |
     *                          Color Downsampled [1/N-1] ──────────> + Accumulation Buffer [1/N-1]
     *                                    |                                      Λ
     *                               [Downsample]                            [Upsample]
     *                                    v                                      |
     *                          Color Downsampled [1/N] ─────────────────────────┘
     * </pre>
     */
    DRWShadingGroup *grp;
    const bool use_antiflicker = true;
    eevee_create_bloom_pass("Bloom Downsample First",
                            effects,
                            EEVEE_shaders_bloom_downsample_get(use_antiflicker),
                            &psl->bloom_downsample_first,
                            false,
                            false,
                            false);
    eevee_create_bloom_pass("Bloom Downsample",
                            effects,
                            EEVEE_shaders_bloom_downsample_get(false),
                            &psl->bloom_downsample,
                            false,
                            false,
                            false);
    eevee_create_bloom_pass("Bloom Upsample",
                            effects,
                            EEVEE_shaders_bloom_upsample_get(use_highres),
                            &psl->bloom_upsample,
                            true,
                            false,
                            false);

    grp = eevee_create_bloom_pass("Bloom Blit",
                                  effects,
                                  EEVEE_shaders_bloom_blit_get(use_antiflicker),
                                  &psl->bloom_blit,
                                  false,
                                  false,
                                  false);
    DRW_shgroup_uniform_vec4(grp, "curveThreshold", effects->bloom_curve_threshold, 1);
    DRW_shgroup_uniform_float(grp, "clampIntensity", &effects->bloom_clamp, 1);

    grp = eevee_create_bloom_pass("Bloom Resolve",
                                  effects,
                                  EEVEE_shaders_bloom_resolve_get(use_highres),
                                  &psl->bloom_resolve,
                                  true,
                                  true,
                                  true);
  }
}

void EEVEE_bloom_draw(EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  /* Bloom */
  if ((effects->enabled_effects & EFFECT_BLOOM) != 0) {
    GPUTexture *last;

    /* Extract bright pixels */
    copy_v2_v2(effects->unf_source_texel_size, effects->source_texel_size);
    effects->unf_source_buffer = effects->source_buffer;

    GPU_framebuffer_bind(fbl->bloom_blit_fb);
    DRW_draw_pass(psl->bloom_blit);

    /* Downsample */
    copy_v2_v2(effects->unf_source_texel_size, effects->blit_texel_size);
    effects->unf_source_buffer = effects->bloom_blit;

    GPU_framebuffer_bind(fbl->bloom_down_fb[0]);
    DRW_draw_pass(psl->bloom_downsample_first);

    last = effects->bloom_downsample[0];

    for (int i = 1; i < effects->bloom_iteration_len; i++) {
      copy_v2_v2(effects->unf_source_texel_size, effects->downsamp_texel_size[i - 1]);
      effects->unf_source_buffer = last;

      GPU_framebuffer_bind(fbl->bloom_down_fb[i]);
      DRW_draw_pass(psl->bloom_downsample);

      /* Used in next loop */
      last = effects->bloom_downsample[i];
    }

    /* Upsample and accumulate */
    for (int i = effects->bloom_iteration_len - 2; i >= 0; i--) {
      copy_v2_v2(effects->unf_source_texel_size, effects->downsamp_texel_size[i]);
      effects->unf_source_buffer = last;
      effects->unf_base_buffer = effects->bloom_downsample[i];

      GPU_framebuffer_bind(fbl->bloom_accum_fb[i]);
      DRW_draw_pass(psl->bloom_upsample);

      last = effects->bloom_upsample[i];
    }

    /* Resolve */
    copy_v2_v2(effects->unf_source_texel_size, effects->downsamp_texel_size[0]);
    effects->unf_source_buffer = last;
    effects->unf_base_buffer = effects->source_buffer;

    GPU_framebuffer_bind(effects->target_buffer);
    DRW_draw_pass(psl->bloom_resolve);
    SWAP_BUFFERS();
  }
}

void EEVEE_bloom_output_init(EEVEE_ViewLayerData * /*sldata*/,
                             EEVEE_Data *vedata,
                             uint /*tot_samples*/)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  /* Create FrameBuffer. */
  DRW_texture_ensure_fullscreen_2d(&txl->bloom_accum, GPU_R11F_G11F_B10F, DRWTextureFlag(0));

  GPU_framebuffer_ensure_config(&fbl->bloom_pass_accum_fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->bloom_accum)});

  /* Create Pass and shgroup. */
  eevee_create_bloom_pass("Bloom Accumulate",
                          effects,
                          EEVEE_shaders_bloom_resolve_get(use_highres),
                          &psl->bloom_accum_ps,
                          true,
                          true,
                          false);
}

void EEVEE_bloom_output_accumulate(EEVEE_ViewLayerData * /*sldata*/, EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;

  if (stl->g_data->render_passes & EEVEE_RENDER_PASS_BLOOM) {
    GPU_framebuffer_bind(fbl->bloom_pass_accum_fb);
    DRW_draw_pass(psl->bloom_accum_ps);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

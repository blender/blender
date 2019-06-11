/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 *
 * Eevee's bloom shader.
 */

#include "DRW_render.h"

#include "GPU_texture.h"

#include "DEG_depsgraph_query.h"

#include "eevee_private.h"

static struct {
  /* Bloom */
  struct GPUShader *bloom_blit_sh[2];
  struct GPUShader *bloom_downsample_sh[2];
  struct GPUShader *bloom_upsample_sh[2];
  struct GPUShader *bloom_resolve_sh[2];
} e_data = {{NULL}}; /* Engine data */

extern char datatoc_effect_bloom_frag_glsl[];

static void eevee_create_shader_bloom(void)
{
  e_data.bloom_blit_sh[0] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl,
                                                         "#define STEP_BLIT\n");
  e_data.bloom_blit_sh[1] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl,
                                                         "#define STEP_BLIT\n"
                                                         "#define HIGH_QUALITY\n");

  e_data.bloom_downsample_sh[0] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl,
                                                               "#define STEP_DOWNSAMPLE\n");
  e_data.bloom_downsample_sh[1] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl,
                                                               "#define STEP_DOWNSAMPLE\n"
                                                               "#define HIGH_QUALITY\n");

  e_data.bloom_upsample_sh[0] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl,
                                                             "#define STEP_UPSAMPLE\n");
  e_data.bloom_upsample_sh[1] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl,
                                                             "#define STEP_UPSAMPLE\n"
                                                             "#define HIGH_QUALITY\n");

  e_data.bloom_resolve_sh[0] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl,
                                                            "#define STEP_RESOLVE\n");
  e_data.bloom_resolve_sh[1] = DRW_shader_create_fullscreen(datatoc_effect_bloom_frag_glsl,
                                                            "#define STEP_RESOLVE\n"
                                                            "#define HIGH_QUALITY\n");
}

int EEVEE_bloom_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_EffectsInfo *effects = stl->effects;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  if (scene_eval->eevee.flag & SCE_EEVEE_BLOOM_ENABLED) {
    const float *viewport_size = DRW_viewport_size_get();

    /* Shaders */
    if (!e_data.bloom_blit_sh[0]) {
      eevee_create_shader_bloom();
    }

    /* Bloom */
    int blitsize[2], texsize[2];

    /* Blit Buffer */
    effects->source_texel_size[0] = 1.0f / viewport_size[0];
    effects->source_texel_size[1] = 1.0f / viewport_size[1];

    blitsize[0] = (int)viewport_size[0];
    blitsize[1] = (int)viewport_size[1];

    effects->blit_texel_size[0] = 1.0f / (float)blitsize[0];
    effects->blit_texel_size[1] = 1.0f / (float)blitsize[1];

    effects->bloom_blit = DRW_texture_pool_query_2d(
        blitsize[0], blitsize[1], GPU_R11F_G11F_B10F, &draw_engine_eevee_type);

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
    const float minDim = (float)MIN2(blitsize[0], blitsize[1]);
    const float maxIter = (radius - 8.0f) + log(minDim) / log(2);
    const int maxIterInt = effects->bloom_iteration_len = (int)maxIter;

    CLAMP(effects->bloom_iteration_len, 1, MAX_BLOOM_STEP);

    effects->bloom_sample_scale = 0.5f + maxIter - (float)maxIterInt;
    effects->bloom_curve_threshold[0] = threshold - knee;
    effects->bloom_curve_threshold[1] = knee * 2.0f;
    effects->bloom_curve_threshold[2] = 0.25f / max_ff(1e-5f, knee);
    effects->bloom_curve_threshold[3] = threshold;

    mul_v3_v3fl(effects->bloom_color, color, intensity);

    /* Downsample buffers */
    copy_v2_v2_int(texsize, blitsize);
    for (int i = 0; i < effects->bloom_iteration_len; ++i) {
      texsize[0] /= 2;
      texsize[1] /= 2;

      texsize[0] = MAX2(texsize[0], 2);
      texsize[1] = MAX2(texsize[1], 2);

      effects->downsamp_texel_size[i][0] = 1.0f / (float)texsize[0];
      effects->downsamp_texel_size[i][1] = 1.0f / (float)texsize[1];

      effects->bloom_downsample[i] = DRW_texture_pool_query_2d(
          texsize[0], texsize[1], GPU_R11F_G11F_B10F, &draw_engine_eevee_type);
      GPU_framebuffer_ensure_config(
          &fbl->bloom_down_fb[i],
          {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(effects->bloom_downsample[i])});
    }

    /* Upsample buffers */
    copy_v2_v2_int(texsize, blitsize);
    for (int i = 0; i < effects->bloom_iteration_len - 1; ++i) {
      texsize[0] /= 2;
      texsize[1] /= 2;

      texsize[0] = MAX2(texsize[0], 2);
      texsize[1] = MAX2(texsize[1], 2);

      effects->bloom_upsample[i] = DRW_texture_pool_query_2d(
          texsize[0], texsize[1], GPU_R11F_G11F_B10F, &draw_engine_eevee_type);
      GPU_framebuffer_ensure_config(
          &fbl->bloom_accum_fb[i],
          {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(effects->bloom_upsample[i])});
    }

    return EFFECT_BLOOM | EFFECT_POST_BUFFER;
  }

  /* Cleanup to release memory */
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->bloom_blit_fb);

  for (int i = 0; i < MAX_BLOOM_STEP - 1; ++i) {
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->bloom_down_fb[i]);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->bloom_accum_fb[i]);
  }

  return 0;
}

static DRWShadingGroup *eevee_create_bloom_pass(const char *name,
                                                EEVEE_EffectsInfo *effects,
                                                struct GPUShader *sh,
                                                DRWPass **pass,
                                                bool upsample)
{
  struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();

  *pass = DRW_pass_create(name, DRW_STATE_WRITE_COLOR);

  DRWShadingGroup *grp = DRW_shgroup_create(sh, *pass);
  DRW_shgroup_call(grp, quad, NULL);
  DRW_shgroup_uniform_texture_ref(grp, "sourceBuffer", &effects->unf_source_buffer);
  DRW_shgroup_uniform_vec2(grp, "sourceBufferTexelSize", effects->unf_source_texel_size, 1);
  if (upsample) {
    DRW_shgroup_uniform_texture_ref(grp, "baseBuffer", &effects->unf_base_buffer);
    DRW_shgroup_uniform_float(grp, "sampleScale", &effects->bloom_sample_scale, 1);
  }

  return grp;
}

void EEVEE_bloom_cache_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & EFFECT_BLOOM) != 0) {
    /**  Bloom algorithm
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
    const bool use_highres = true;
    const bool use_antiflicker = true;
    eevee_create_bloom_pass("Bloom Downsample First",
                            effects,
                            e_data.bloom_downsample_sh[use_antiflicker],
                            &psl->bloom_downsample_first,
                            false);
    eevee_create_bloom_pass(
        "Bloom Downsample", effects, e_data.bloom_downsample_sh[0], &psl->bloom_downsample, false);
    eevee_create_bloom_pass("Bloom Upsample",
                            effects,
                            e_data.bloom_upsample_sh[use_highres],
                            &psl->bloom_upsample,
                            true);

    grp = eevee_create_bloom_pass(
        "Bloom Blit", effects, e_data.bloom_blit_sh[use_antiflicker], &psl->bloom_blit, false);
    DRW_shgroup_uniform_vec4(grp, "curveThreshold", effects->bloom_curve_threshold, 1);
    DRW_shgroup_uniform_float(grp, "clampIntensity", &effects->bloom_clamp, 1);

    grp = eevee_create_bloom_pass(
        "Bloom Resolve", effects, e_data.bloom_resolve_sh[use_highres], &psl->bloom_resolve, true);
    DRW_shgroup_uniform_vec3(grp, "bloomColor", effects->bloom_color, 1);
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
    struct GPUTexture *last;

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

    for (int i = 1; i < effects->bloom_iteration_len; ++i) {
      copy_v2_v2(effects->unf_source_texel_size, effects->downsamp_texel_size[i - 1]);
      effects->unf_source_buffer = last;

      GPU_framebuffer_bind(fbl->bloom_down_fb[i]);
      DRW_draw_pass(psl->bloom_downsample);

      /* Used in next loop */
      last = effects->bloom_downsample[i];
    }

    /* Upsample and accumulate */
    for (int i = effects->bloom_iteration_len - 2; i >= 0; --i) {
      copy_v2_v2(effects->unf_source_texel_size, effects->downsamp_texel_size[i]);
      effects->unf_source_buffer = effects->bloom_downsample[i];
      effects->unf_base_buffer = last;

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

void EEVEE_bloom_free(void)
{
  for (int i = 0; i < 2; ++i) {
    DRW_SHADER_FREE_SAFE(e_data.bloom_blit_sh[i]);
    DRW_SHADER_FREE_SAFE(e_data.bloom_downsample_sh[i]);
    DRW_SHADER_FREE_SAFE(e_data.bloom_upsample_sh[i]);
    DRW_SHADER_FREE_SAFE(e_data.bloom_resolve_sh[i]);
  }
}

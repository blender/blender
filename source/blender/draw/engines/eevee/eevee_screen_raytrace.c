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
 * Screen space reflections and refractions techniques.
 */

#include "DRW_render.h"

#include "BLI_dynstr.h"
#include "BLI_string_utils.h"

#include "DEG_depsgraph_query.h"

#include "GPU_texture.h"
#include "eevee_private.h"

static struct {
  /* These are just references, not actually allocated */
  struct GPUTexture *depth_src;
} e_data = {NULL}; /* Engine data */

int EEVEE_screen_raytrace_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_EffectsInfo *effects = stl->effects;
  const float *viewport_size = DRW_viewport_size_get();

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  if (scene_eval->eevee.flag & SCE_EEVEE_SSR_ENABLED) {
    const bool use_refraction = (scene_eval->eevee.flag & SCE_EEVEE_SSR_REFRACTION) != 0;

    const bool is_persp = DRW_view_is_persp_get(NULL);
    if (effects->ssr_was_persp != is_persp) {
      effects->ssr_was_persp = is_persp;
      DRW_viewport_request_redraw();
      EEVEE_temporal_sampling_reset(vedata);
      stl->g_data->valid_double_buffer = false;
    }

    if (!effects->ssr_was_valid_double_buffer) {
      DRW_viewport_request_redraw();
      EEVEE_temporal_sampling_reset(vedata);
    }
    effects->ssr_was_valid_double_buffer = stl->g_data->valid_double_buffer;

    effects->reflection_trace_full = (scene_eval->eevee.flag & SCE_EEVEE_SSR_HALF_RESOLUTION) == 0;
    common_data->ssr_thickness = scene_eval->eevee.ssr_thickness;
    common_data->ssr_border_fac = scene_eval->eevee.ssr_border_fade;
    common_data->ssr_firefly_fac = scene_eval->eevee.ssr_firefly_fac;
    common_data->ssr_max_roughness = scene_eval->eevee.ssr_max_roughness;
    common_data->ssr_quality = 1.0f - 0.95f * scene_eval->eevee.ssr_quality;
    common_data->ssr_brdf_bias = 0.1f + common_data->ssr_quality * 0.6f; /* Range [0.1, 0.7]. */

    if (common_data->ssr_firefly_fac < 1e-8f) {
      common_data->ssr_firefly_fac = FLT_MAX;
    }

    void *owner = (void *)EEVEE_screen_raytrace_init;
    const int divisor = (effects->reflection_trace_full) ? 1 : 2;
    int tracing_res[2] = {(int)viewport_size[0] / divisor, (int)viewport_size[1] / divisor};
    const int size_fs[2] = {(int)viewport_size[0], (int)viewport_size[1]};
    const bool high_qual_input = true; /* TODO dither low quality input */
    const eGPUTextureFormat format = (high_qual_input) ? GPU_RGBA16F : GPU_RGBA8;

    tracing_res[0] = max_ii(1, tracing_res[0]);
    tracing_res[1] = max_ii(1, tracing_res[1]);

    common_data->ssr_uv_scale[0] = size_fs[0] / ((float)tracing_res[0] * divisor);
    common_data->ssr_uv_scale[1] = size_fs[1] / ((float)tracing_res[1] * divisor);

    /* MRT for the shading pass in order to output needed data for the SSR pass. */
    effects->ssr_specrough_input = DRW_texture_pool_query_2d(UNPACK2(size_fs), format, owner);

    GPU_framebuffer_texture_attach(fbl->main_fb, effects->ssr_specrough_input, 2, 0);

    /* Ray-tracing output. */
    effects->ssr_hit_output = DRW_texture_pool_query_2d(UNPACK2(tracing_res), GPU_RGBA16F, owner);
    effects->ssr_hit_depth = DRW_texture_pool_query_2d(UNPACK2(tracing_res), GPU_R16F, owner);

    GPU_framebuffer_ensure_config(&fbl->screen_tracing_fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(effects->ssr_hit_output),
                                      GPU_ATTACHMENT_TEXTURE(effects->ssr_hit_depth),
                                  });

    return EFFECT_SSR | EFFECT_NORMAL_BUFFER | EFFECT_RADIANCE_BUFFER | EFFECT_DOUBLE_BUFFER |
           ((use_refraction) ? EFFECT_REFRACT : 0);
  }

  /* Cleanup to release memory */
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->screen_tracing_fb);
  effects->ssr_specrough_input = NULL;
  effects->ssr_hit_output = NULL;

  return 0;
}

void EEVEE_screen_raytrace_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects = stl->effects;
  LightCache *lcache = stl->g_data->light_cache;

  struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();

  if ((effects->enabled_effects & EFFECT_SSR) != 0) {
    EEVEE_SSRShaderOptions options = (effects->reflection_trace_full) ? SSR_FULL_TRACE : 0;

    struct GPUShader *trace_shader = EEVEE_shaders_effect_screen_raytrace_sh_get(options);
    struct GPUShader *resolve_shader = EEVEE_shaders_effect_screen_raytrace_sh_get(SSR_RESOLVE |
                                                                                   options);

    /** Screen space raytracing overview
     *
     * Following Frostbite stochastic SSR.
     *
     * - First pass Trace rays across the depth buffer. The hit position and PDF are
     *   recorded in a RGBA16F render target for each ray (sample).
     *
     * - We down-sample the previous frame color buffer.
     *
     * - For each final pixel, we gather neighbors rays and choose a color buffer
     *   mipmap for each ray using its PDF. (filtered importance sampling)
     *   We then evaluate the lighting from the probes and mix the results together.
     */
    DRW_PASS_CREATE(psl->ssr_raytrace, DRW_STATE_WRITE_COLOR);
    DRWShadingGroup *grp = DRW_shgroup_create(trace_shader, psl->ssr_raytrace);
    DRW_shgroup_uniform_texture_ref(grp, "normalBuffer", &effects->ssr_normal_input);
    DRW_shgroup_uniform_texture_ref(grp, "specroughBuffer", &effects->ssr_specrough_input);
    DRW_shgroup_uniform_texture_ref(grp, "maxzBuffer", &txl->maxzbuffer);
    DRW_shgroup_uniform_texture_ref(grp, "planarDepth", &vedata->txl->planar_depth);
    DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
    DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);
    DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
    DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
    if (!effects->reflection_trace_full) {
      DRW_shgroup_uniform_ivec2(grp, "halfresOffset", effects->ssr_halfres_ofs, 1);
    }
    DRW_shgroup_call(grp, quad, NULL);

    eGPUSamplerState no_filter = GPU_SAMPLER_DEFAULT;

    DRW_PASS_CREATE(psl->ssr_resolve, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD);
    grp = DRW_shgroup_create(resolve_shader, psl->ssr_resolve);
    DRW_shgroup_uniform_texture_ref(grp, "normalBuffer", &effects->ssr_normal_input);
    DRW_shgroup_uniform_texture_ref(grp, "specroughBuffer", &effects->ssr_specrough_input);
    DRW_shgroup_uniform_texture_ref(grp, "probeCubes", &lcache->cube_tx.tex);
    DRW_shgroup_uniform_texture_ref(grp, "probePlanars", &vedata->txl->planar_pool);
    DRW_shgroup_uniform_texture_ref(grp, "planarDepth", &vedata->txl->planar_depth);
    DRW_shgroup_uniform_texture_ref_ex(grp, "hitBuffer", &effects->ssr_hit_output, no_filter);
    DRW_shgroup_uniform_texture_ref_ex(grp, "hitDepth", &effects->ssr_hit_depth, no_filter);
    DRW_shgroup_uniform_texture_ref(grp, "colorBuffer", &txl->filtered_radiance);
    DRW_shgroup_uniform_texture_ref(grp, "maxzBuffer", &txl->maxzbuffer);
    DRW_shgroup_uniform_texture_ref(grp, "shadowCubeTexture", &sldata->shadow_cube_pool);
    DRW_shgroup_uniform_texture_ref(grp, "shadowCascadeTexture", &sldata->shadow_cascade_pool);
    DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
    DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
    DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);
    DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);
    DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
    DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
    DRW_shgroup_uniform_int(grp, "neighborOffset", &effects->ssr_neighbor_ofs, 1);
    DRW_shgroup_uniform_texture_ref(grp, "horizonBuffer", &effects->gtao_horizons);

    DRW_shgroup_call(grp, quad, NULL);
  }
}

void EEVEE_refraction_compute(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & EFFECT_REFRACT) != 0) {
    EEVEE_effects_downsample_radiance_buffer(vedata, txl->color);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

void EEVEE_reflection_compute(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if (((effects->enabled_effects & EFFECT_SSR) != 0) && stl->g_data->valid_double_buffer) {
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
    e_data.depth_src = dtxl->depth;

    DRW_stats_group_start("SSR");

    /* Raytrace. */
    GPU_framebuffer_bind(fbl->screen_tracing_fb);
    DRW_draw_pass(psl->ssr_raytrace);

    EEVEE_effects_downsample_radiance_buffer(vedata, txl->color_double_buffer);

    /* Resolve at fullres */
    int samp = (DRW_state_is_image_render()) ? effects->taa_render_sample :
                                               effects->taa_current_sample;
    /* Doing a neighbor shift only after a few iteration.
     * We wait for a prime number of cycles to avoid noise correlation.
     * This reduces variance faster. */
    effects->ssr_neighbor_ofs = ((samp / 5) % 8) * 4;
    switch ((samp / 11) % 4) {
      case 0:
        effects->ssr_halfres_ofs[0] = 0;
        effects->ssr_halfres_ofs[1] = 0;
        break;
      case 1:
        effects->ssr_halfres_ofs[0] = 0;
        effects->ssr_halfres_ofs[1] = 1;
        break;
      case 2:
        effects->ssr_halfres_ofs[0] = 1;
        effects->ssr_halfres_ofs[1] = 0;
        break;
      case 4:
        effects->ssr_halfres_ofs[0] = 1;
        effects->ssr_halfres_ofs[1] = 1;
        break;
    }
    GPU_framebuffer_bind(fbl->main_color_fb);
    DRW_draw_pass(psl->ssr_resolve);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);
    DRW_stats_group_end();
  }
}

void EEVEE_reflection_output_init(EEVEE_ViewLayerData *UNUSED(sldata),
                                  EEVEE_Data *vedata,
                                  uint tot_samples)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  /* Create FrameBuffer. */
  const eGPUTextureFormat texture_format = (tot_samples > 256) ? GPU_RGBA32F : GPU_RGBA16F;
  DRW_texture_ensure_fullscreen_2d(&txl->ssr_accum, texture_format, 0);

  GPU_framebuffer_ensure_config(&fbl->ssr_accum_fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->ssr_accum)});

  /* Clear texture. */
  if (effects->taa_current_sample == 1) {
    GPU_framebuffer_bind(fbl->ssr_accum_fb);
    GPU_framebuffer_clear_color(fbl->ssr_accum_fb, clear);
  }
}

void EEVEE_reflection_output_accumulate(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;

  if (stl->g_data->valid_double_buffer) {
    GPU_framebuffer_bind(fbl->ssr_accum_fb);
    DRW_draw_pass(psl->ssr_resolve);
  }
}

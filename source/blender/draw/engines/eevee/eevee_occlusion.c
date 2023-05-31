/* SPDX-FileCopyrightText: 2016 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Implementation of the screen space Ground Truth Ambient Occlusion.
 */

#include "DRW_render.h"

#include "BLI_string_utils.h"

#include "DEG_depsgraph_query.h"

#include "BKE_global.h" /* for G.debug_value */

#include "eevee_private.h"

#include "GPU_capabilities.h"
#include "GPU_platform.h"
#include "GPU_state.h"

static struct {
  struct GPUTexture *dummy_horizon_tx;
} e_data = {NULL}; /* Engine data */

int EEVEE_occlusion_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  if (!e_data.dummy_horizon_tx) {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ;
    const float pixel[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    e_data.dummy_horizon_tx = DRW_texture_create_2d_ex(
        1, 1, GPU_RGBA8, usage, DRW_TEX_WRAP, pixel);
  }

  if (scene_eval->eevee.flag & SCE_EEVEE_GTAO_ENABLED ||
      stl->g_data->render_passes & EEVEE_RENDER_PASS_AO)
  {
    const float *viewport_size = DRW_viewport_size_get();
    const int fs_size[2] = {(int)viewport_size[0], (int)viewport_size[1]};

    common_data->ao_dist = scene_eval->eevee.gtao_distance;
    common_data->ao_factor = max_ff(1e-4f, scene_eval->eevee.gtao_factor);
    common_data->ao_quality = scene_eval->eevee.gtao_quality;

    if (scene_eval->eevee.flag & SCE_EEVEE_GTAO_ENABLED) {
      common_data->ao_settings = 1.0f; /* USE_AO */
    }
    if (scene_eval->eevee.flag & SCE_EEVEE_GTAO_BENT_NORMALS) {
      common_data->ao_settings += 2.0f; /* USE_BENT_NORMAL */
    }
    if (scene_eval->eevee.flag & SCE_EEVEE_GTAO_BOUNCE) {
      common_data->ao_settings += 4.0f; /* USE_DENOISE */
    }

    common_data->ao_bounce_fac = (scene_eval->eevee.flag & SCE_EEVEE_GTAO_BOUNCE) ? 1.0f : 0.0f;

    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ;
    effects->gtao_horizons_renderpass = DRW_texture_pool_query_2d_ex(
        UNPACK2(effects->hiz_size), GPU_RGBA8, usage, &draw_engine_eevee_type);
    GPU_framebuffer_ensure_config(
        &fbl->gtao_fb,
        {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(effects->gtao_horizons_renderpass)});

    if (G.debug_value == 6) {
      effects->gtao_horizons_debug = DRW_texture_pool_query_2d(
          UNPACK2(fs_size), GPU_RGBA8, &draw_engine_eevee_type);
      GPU_framebuffer_ensure_config(
          &fbl->gtao_debug_fb,
          {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(effects->gtao_horizons_debug)});
    }
    else {
      effects->gtao_horizons_debug = NULL;
    }

    effects->gtao_horizons = (scene_eval->eevee.flag & SCE_EEVEE_GTAO_ENABLED) ?
                                 effects->gtao_horizons_renderpass :
                                 e_data.dummy_horizon_tx;

    return EFFECT_GTAO | EFFECT_NORMAL_BUFFER;
  }

  /* Cleanup */
  effects->gtao_horizons_renderpass = e_data.dummy_horizon_tx;
  effects->gtao_horizons = e_data.dummy_horizon_tx;
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->gtao_fb);
  common_data->ao_settings = 0.0f;

  return 0;
}

void EEVEE_occlusion_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, uint tot_samples)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_EffectsInfo *effects = stl->effects;

  const eGPUTextureFormat texture_format = (tot_samples > 128) ? GPU_R32F : GPU_R16F;

  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  /* Should be enough precision for many samples. */
  DRW_texture_ensure_fullscreen_2d(&txl->ao_accum, texture_format, 0);

  GPU_framebuffer_ensure_config(&fbl->ao_accum_fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->ao_accum)});

  /* Accumulation pass */
  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD;
  DRW_PASS_CREATE(psl->ao_accum_ps, state);
  DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_effect_ambient_occlusion_debug_sh_get(),
                                            psl->ao_accum_ps);
  DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
  DRW_shgroup_uniform_texture_ref(grp, "maxzBuffer", &txl->maxzbuffer);
  DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
  DRW_shgroup_uniform_texture_ref(grp, "normalBuffer", &effects->ssr_normal_input);
  DRW_shgroup_uniform_texture_ref(grp, "horizonBuffer", &effects->gtao_horizons_renderpass);
  DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
  DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
  DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), NULL);
}

void EEVEE_occlusion_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects = stl->effects;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  if ((effects->enabled_effects & EFFECT_GTAO) != 0) {
    /**
     * Occlusion Algorithm Overview:
     *
     * We separate the computation into 2 steps.
     *
     * - First we scan the neighborhood pixels to find the maximum horizon angle.
     *   We save this angle in a RG8 array texture.
     *
     * - Then we use this angle to compute occlusion with the shading normal at
     *   the shading stage. This let us do correct shadowing for each diffuse / specular
     *   lobe present in the shader using the correct normal.
     */
    DRW_PASS_CREATE(psl->ao_horizon_search, DRW_STATE_WRITE_COLOR);
    DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_effect_ambient_occlusion_sh_get(),
                                              psl->ao_horizon_search);
    DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
    DRW_shgroup_uniform_texture_ref(grp, "maxzBuffer", &txl->maxzbuffer);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

    if (G.debug_value == 6) {
      DRW_PASS_CREATE(psl->ao_horizon_debug, DRW_STATE_WRITE_COLOR);
      grp = DRW_shgroup_create(EEVEE_shaders_effect_ambient_occlusion_debug_sh_get(),
                               psl->ao_horizon_debug);
      DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
      DRW_shgroup_uniform_texture_ref(grp, "maxzBuffer", &txl->maxzbuffer);
      DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
      DRW_shgroup_uniform_texture_ref(grp, "normalBuffer", &effects->ssr_normal_input);
      DRW_shgroup_uniform_texture_ref(grp, "horizonBuffer", &effects->gtao_horizons_renderpass);
      DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
      DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
      DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
    }
  }
}

void EEVEE_occlusion_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;

  if ((effects->enabled_effects & EFFECT_GTAO) != 0) {
    DRW_stats_group_start("GTAO Horizon Scan");

    GPU_framebuffer_bind(fbl->gtao_fb);

    /** NOTE(fclem): Kind of fragile. We need this to make sure everything lines up
     * nicely during planar reflection. */
    if (common_data->ray_type != EEVEE_RAY_GLOSSY) {
      const float *viewport_size = DRW_viewport_size_get();
      GPU_framebuffer_viewport_set(fbl->gtao_fb, 0, 0, UNPACK2(viewport_size));
    }

    DRW_draw_pass(psl->ao_horizon_search);

    if (common_data->ray_type != EEVEE_RAY_GLOSSY) {
      GPU_framebuffer_viewport_reset(fbl->gtao_fb);
    }

    if (GPU_mip_render_workaround() ||
        GPU_type_matches_ex(GPU_DEVICE_INTEL_UHD, GPU_OS_WIN, GPU_DRIVER_ANY, GPU_BACKEND_OPENGL))
    {
      /* Fix dot corruption on intel HD5XX/HD6XX series. */
      GPU_flush();
    }

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);

    DRW_stats_group_end();
  }
}

void EEVEE_occlusion_draw_debug(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if (((effects->enabled_effects & EFFECT_GTAO) != 0) && (G.debug_value == 6)) {
    DRW_stats_group_start("GTAO Debug");

    GPU_framebuffer_bind(fbl->gtao_debug_fb);
    DRW_draw_pass(psl->ao_horizon_debug);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);

    DRW_stats_group_end();
  }
}

void EEVEE_occlusion_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;

  if (fbl->ao_accum_fb != NULL) {
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

    /* Update the min_max/horizon buffers so the refraction materials appear in it. */
    EEVEE_create_minmax_buffer(vedata, dtxl->depth, -1);
    EEVEE_occlusion_compute(sldata, vedata);

    GPU_framebuffer_bind(fbl->ao_accum_fb);

    /* Clear texture. */
    if (effects->taa_current_sample == 1) {
      const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      GPU_framebuffer_clear_color(fbl->ao_accum_fb, clear);
    }

    DRW_draw_pass(psl->ao_accum_ps);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

void EEVEE_occlusion_free(void)
{
  DRW_TEXTURE_FREE_SAFE(e_data.dummy_horizon_tx);
}

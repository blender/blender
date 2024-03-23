/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Screen space subsurface scattering technique.
 */

#include "DRW_render.hh"

#include "BLI_string_utils.hh"

#include "DEG_depsgraph_query.hh"

#include "GPU_capabilities.hh"
#include "GPU_material.hh"
#include "GPU_texture.hh"

#include "eevee_private.h"

void EEVEE_subsurface_init(EEVEE_ViewLayerData * /*sldata*/, EEVEE_Data * /*vedata*/) {}

void EEVEE_subsurface_draw_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  const float *viewport_size = DRW_viewport_size_get();
  const int fs_size[2] = {int(viewport_size[0]), int(viewport_size[1])};

  if (effects->enabled_effects & EFFECT_SSS) {
    /* NOTE: we need another stencil because the stencil buffer is on the same texture
     * as the depth buffer we are sampling from. This could be avoided if the stencil is
     * a separate texture but that needs OpenGL 4.4 or ARB_texture_stencil8.
     * OR OpenGL 4.3 / ARB_ES3_compatibility if using a render-buffer instead. */
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;

    effects->sss_stencil = DRW_texture_pool_query_2d_ex(
        fs_size[0], fs_size[1], GPU_DEPTH24_STENCIL8, usage, &draw_engine_eevee_type);
    effects->sss_blur = DRW_texture_pool_query_2d_ex(
        fs_size[0], fs_size[1], GPU_R11F_G11F_B10F, usage, &draw_engine_eevee_type);
    effects->sss_irradiance = DRW_texture_pool_query_2d_ex(
        fs_size[0], fs_size[1], GPU_R11F_G11F_B10F, usage, &draw_engine_eevee_type);
    effects->sss_radius = DRW_texture_pool_query_2d_ex(
        fs_size[0], fs_size[1], GPU_R16F, usage, &draw_engine_eevee_type);
    effects->sss_albedo = DRW_texture_pool_query_2d_ex(
        fs_size[0], fs_size[1], GPU_R11F_G11F_B10F, usage, &draw_engine_eevee_type);

    GPUTexture *stencil_tex = effects->sss_stencil;

    if (GPU_depth_blitting_workaround()) {
      /* Blitting stencil buffer does not work on macOS + Radeon Pro.
       * Blit depth instead and use sss_stencil's depth as depth texture,
       * and dtxl->depth as stencil mask. */
      GPU_framebuffer_ensure_config(
          &fbl->sss_blit_fb, {GPU_ATTACHMENT_TEXTURE(effects->sss_stencil), GPU_ATTACHMENT_NONE});

      stencil_tex = dtxl->depth;
    }

    GPU_framebuffer_ensure_config(
        &fbl->sss_blur_fb,
        {GPU_ATTACHMENT_TEXTURE(stencil_tex), GPU_ATTACHMENT_TEXTURE(effects->sss_blur)});

    GPU_framebuffer_ensure_config(
        &fbl->sss_resolve_fb,
        {GPU_ATTACHMENT_TEXTURE(stencil_tex), GPU_ATTACHMENT_TEXTURE(txl->color)});

    GPU_framebuffer_ensure_config(
        &fbl->sss_translucency_fb,
        {GPU_ATTACHMENT_TEXTURE(stencil_tex), GPU_ATTACHMENT_TEXTURE(effects->sss_irradiance)});

    GPU_framebuffer_ensure_config(&fbl->sss_clear_fb,
                                  {GPU_ATTACHMENT_NONE,
                                   GPU_ATTACHMENT_TEXTURE(effects->sss_irradiance),
                                   GPU_ATTACHMENT_TEXTURE(effects->sss_radius)});
    if ((stl->g_data->render_passes & EEVEE_RENDER_PASS_DIFFUSE_LIGHT) != 0) {
      EEVEE_subsurface_output_init(sldata, vedata, 0);
    }
    else {
      GPU_FRAMEBUFFER_FREE_SAFE(fbl->sss_accum_fb);
      DRW_TEXTURE_FREE_SAFE(txl->sss_accum);
    }
  }
  else {
    /* Cleanup to release memory */
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->sss_blur_fb);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->sss_resolve_fb);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->sss_clear_fb);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->sss_accum_fb);
    DRW_TEXTURE_FREE_SAFE(txl->sss_accum);
    effects->sss_stencil = nullptr;
    effects->sss_blur = nullptr;
    effects->sss_irradiance = nullptr;
    effects->sss_radius = nullptr;
  }
}

void EEVEE_subsurface_output_init(EEVEE_ViewLayerData * /*sldata*/,
                                  EEVEE_Data *vedata,
                                  uint /*tot_samples*/)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  const eGPUTextureFormat texture_format_light = GPU_RGBA32F;
  const bool texture_created = txl->sss_accum == nullptr;
  DRW_texture_ensure_fullscreen_2d(&txl->sss_accum, texture_format_light, DRWTextureFlag(0));

  GPUTexture *stencil_tex = effects->sss_stencil;

  if (GPU_depth_blitting_workaround()) {
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
    /* Blitting stencil buffer does not work on macOS + Radeon Pro.
     * Blit depth instead and use sss_stencil's depth as depth texture,
     * and dtxl->depth as stencil mask. */
    stencil_tex = dtxl->depth;
  }

  GPU_framebuffer_ensure_config(
      &fbl->sss_accum_fb,
      {GPU_ATTACHMENT_TEXTURE(stencil_tex), GPU_ATTACHMENT_TEXTURE(txl->sss_accum)});

  /* Clear texture.
   * Due to the late initialization of the SSS it can happen that the `taa_current_sample` is
   * already higher than one. This is noticeable when loading a file that has the diffuse light
   * pass in look-dev mode active. `texture_created` will make sure that newly created textures
   * are cleared. */
  if (effects->taa_current_sample == 1 || texture_created) {
    const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GPU_framebuffer_bind(fbl->sss_accum_fb);
    GPU_framebuffer_clear_color(fbl->sss_accum_fb, clear);
  }
}

void EEVEE_subsurface_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  EEVEE_PassList *psl = vedata->psl;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  effects->sss_sample_count = 1 + scene_eval->eevee.sss_samples * 2;
  effects->sss_surface_count = 0;
  common_data->sss_jitter_threshold = scene_eval->eevee.sss_jitter_threshold;

  /* Screen Space SubSurface Scattering overview.
   * TODO */
  DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_STENCIL_EQUAL;
  DRW_PASS_CREATE(psl->sss_blur_ps, state);
  DRW_PASS_CREATE(psl->sss_resolve_ps, state | DRW_STATE_BLEND_ADD);
  DRW_PASS_CREATE(psl->sss_translucency_ps, state | DRW_STATE_BLEND_ADD);
}

void EEVEE_subsurface_add_pass(EEVEE_ViewLayerData *sldata,
                               EEVEE_Data *vedata,
                               Material *ma,
                               DRWShadingGroup *shgrp,
                               GPUMaterial *gpumat)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  GPUTexture **depth_src = GPU_depth_blitting_workaround() ? &effects->sss_stencil : &dtxl->depth;

  GPUTexture *sss_tex_profile = nullptr;
  GPUUniformBuf *sss_profile = GPU_material_sss_profile_get(
      gpumat, stl->effects->sss_sample_count, &sss_tex_profile);

  if (!sss_profile) {
    BLI_assert_msg(0, "SSS pass requested but no SSS data was found");
    return;
  }

  /* Limit of 8 bit stencil buffer. ID 255 is refraction. */
  if (effects->sss_surface_count >= 254) {
    /* TODO: display message. */
    printf("Error: Too many different Subsurface shader in the scene.\n");
    return;
  }

  int sss_id = ++(effects->sss_surface_count);
  /* Make main pass output stencil mask. */
  DRW_shgroup_stencil_mask(shgrp, sss_id);

  {
    GPUSamplerState state = GPUSamplerState::default_sampler();

    DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_subsurface_first_pass_sh_get(),
                                              psl->sss_blur_ps);
    DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", depth_src);
    DRW_shgroup_uniform_texture_ref_ex(grp, "sssIrradiance", &effects->sss_irradiance, state);
    DRW_shgroup_uniform_texture_ref_ex(grp, "sssRadius", &effects->sss_radius, state);
    DRW_shgroup_uniform_block(grp, "sssProfile", sss_profile);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
    DRW_shgroup_stencil_mask(grp, sss_id);
    DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);

    grp = DRW_shgroup_create(EEVEE_shaders_subsurface_second_pass_sh_get(), psl->sss_resolve_ps);
    DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", depth_src);
    DRW_shgroup_uniform_texture_ref_ex(grp, "sssIrradiance", &effects->sss_blur, state);
    DRW_shgroup_uniform_texture_ref_ex(grp, "sssAlbedo", &effects->sss_albedo, state);
    DRW_shgroup_uniform_texture_ref_ex(grp, "sssRadius", &effects->sss_radius, state);
    DRW_shgroup_uniform_block(grp, "sssProfile", sss_profile);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
    DRW_shgroup_stencil_mask(grp, sss_id);
    DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);
  }

  if (ma->blend_flag & MA_BL_TRANSLUCENCY) {
    DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_subsurface_translucency_sh_get(),
                                              psl->sss_translucency_ps);
    DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
    DRW_shgroup_uniform_texture(grp, "sssTexProfile", sss_tex_profile);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", depth_src);
    DRW_shgroup_uniform_texture_ref(grp, "sssRadius", &effects->sss_radius);
    DRW_shgroup_uniform_texture_ref(grp, "sssShadowCubes", &sldata->shadow_cube_pool);
    DRW_shgroup_uniform_texture_ref(grp, "sssShadowCascades", &sldata->shadow_cascade_pool);
    DRW_shgroup_uniform_block(grp, "sssProfile", sss_profile);
    DRW_shgroup_uniform_block(grp, "light_block", sldata->light_ubo);
    DRW_shgroup_uniform_block(grp, "shadow_block", sldata->shadow_ubo);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
    DRW_shgroup_stencil_mask(grp, sss_id);
    DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);
  }
}

void EEVEE_subsurface_data_render(EEVEE_ViewLayerData * /*sldata*/, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & EFFECT_SSS) != 0) {
    const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    /* Clear sss_data texture only... can this be done in a more clever way? */
    GPU_framebuffer_bind(fbl->sss_clear_fb);
    GPU_framebuffer_clear_color(fbl->sss_clear_fb, clear);

    GPU_framebuffer_ensure_config(&fbl->main_fb,
                                  {GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_TEXTURE(effects->sss_irradiance),
                                   GPU_ATTACHMENT_TEXTURE(effects->sss_radius),
                                   GPU_ATTACHMENT_TEXTURE(effects->sss_albedo)});

    GPU_framebuffer_bind(fbl->main_fb);
    DRW_draw_pass(psl->material_sss_ps);

    /* Restore */
    GPU_framebuffer_ensure_config(&fbl->main_fb,
                                  {GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_NONE,
                                   GPU_ATTACHMENT_NONE,
                                   GPU_ATTACHMENT_NONE});
  }
}

void EEVEE_subsurface_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & EFFECT_SSS) != 0) {
    const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    DRW_stats_group_start("SSS");

    if (GPU_depth_blitting_workaround()) {
      /* Copy depth channel */
      GPU_framebuffer_blit(fbl->main_fb, 0, fbl->sss_blit_fb, 0, GPU_DEPTH_BIT);
    }
    else {
      /* Copy stencil channel, could be avoided (see EEVEE_subsurface_init) */
      GPU_framebuffer_blit(fbl->main_fb, 0, fbl->sss_blur_fb, 0, GPU_STENCIL_BIT);
    }

    if (!DRW_pass_is_empty(psl->sss_translucency_ps)) {
      /* We sample the shadow-maps using normal sampler. We need to disable Comparison mode.
       * TODO(fclem): avoid this by using sampler objects. */
      GPU_texture_compare_mode(sldata->shadow_cube_pool, false);
      GPU_texture_compare_mode(sldata->shadow_cascade_pool, false);

      GPU_framebuffer_bind(fbl->sss_translucency_fb);
      DRW_draw_pass(psl->sss_translucency_ps);

      /* Reset original state. */
      GPU_texture_compare_mode(sldata->shadow_cube_pool, true);
      GPU_texture_compare_mode(sldata->shadow_cascade_pool, true);
    }

    /* 1. horizontal pass */
    GPU_framebuffer_bind(fbl->sss_blur_fb);
    GPU_framebuffer_clear_color(fbl->sss_blur_fb, clear);
    DRW_draw_pass(psl->sss_blur_ps);

    /* 2. vertical pass + Resolve */
    GPU_framebuffer_texture_attach(fbl->sss_resolve_fb, txl->color, 0, 0);
    GPU_framebuffer_bind(fbl->sss_resolve_fb);
    DRW_draw_pass(psl->sss_resolve_ps);

    GPU_framebuffer_bind(fbl->main_fb);
    DRW_stats_group_end();
  }
}

void EEVEE_subsurface_output_accumulate(EEVEE_ViewLayerData * /*sldata*/, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if (((effects->enabled_effects & EFFECT_SSS) != 0) && (fbl->sss_accum_fb != nullptr)) {
    /* Copy stencil channel, could be avoided (see EEVEE_subsurface_init) */
    GPU_framebuffer_blit(fbl->main_fb, 0, fbl->sss_accum_fb, 0, GPU_STENCIL_BIT);

    /* Only do vertical pass + Resolve */
    GPU_framebuffer_bind(fbl->sss_accum_fb);
    DRW_draw_pass(psl->sss_resolve_ps);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

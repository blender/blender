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
 * Screen space subsurface scattering technique.
 */

#include "DRW_render.h"

#include "BLI_string_utils.h"

#include "DEG_depsgraph_query.h"

#include "eevee_private.h"
#include "GPU_texture.h"
#include "GPU_extensions.h"

static struct {
  struct GPUShader *sss_sh[4];
} e_data = {{NULL}}; /* Engine data */

extern char datatoc_common_view_lib_glsl[];
extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_effect_subsurface_frag_glsl[];

static void eevee_create_shader_subsurface(void)
{
  char *frag_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                    datatoc_common_uniforms_lib_glsl,
                                    datatoc_effect_subsurface_frag_glsl);

  e_data.sss_sh[0] = DRW_shader_create_fullscreen(frag_str, "#define FIRST_PASS\n");
  e_data.sss_sh[1] = DRW_shader_create_fullscreen(frag_str, "#define SECOND_PASS\n");
  e_data.sss_sh[2] = DRW_shader_create_fullscreen(frag_str,
                                                  "#define SECOND_PASS\n"
                                                  "#define USE_SEP_ALBEDO\n");
  e_data.sss_sh[3] = DRW_shader_create_fullscreen(frag_str,
                                                  "#define SECOND_PASS\n"
                                                  "#define USE_SEP_ALBEDO\n"
                                                  "#define RESULT_ACCUM\n");

  MEM_freeN(frag_str);
}

int EEVEE_subsurface_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  const float *viewport_size = DRW_viewport_size_get();
  const int fs_size[2] = {(int)viewport_size[0], (int)viewport_size[1]};

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  if (scene_eval->eevee.flag & SCE_EEVEE_SSS_ENABLED) {
    effects->sss_sample_count = 1 + scene_eval->eevee.sss_samples * 2;
    effects->sss_separate_albedo = (scene_eval->eevee.flag & SCE_EEVEE_SSS_SEPARATE_ALBEDO) != 0;
    common_data->sss_jitter_threshold = scene_eval->eevee.sss_jitter_threshold;

    /* Shaders */
    if (!e_data.sss_sh[0]) {
      eevee_create_shader_subsurface();
    }

    /* NOTE : we need another stencil because the stencil buffer is on the same texture
     * as the depth buffer we are sampling from. This could be avoided if the stencil is
     * a separate texture but that needs OpenGL 4.4 or ARB_texture_stencil8.
     * OR OpenGL 4.3 / ARB_ES3_compatibility if using a renderbuffer instead */
    effects->sss_stencil = DRW_texture_pool_query_2d(
        fs_size[0], fs_size[1], GPU_DEPTH24_STENCIL8, &draw_engine_eevee_type);
    effects->sss_blur = DRW_texture_pool_query_2d(
        fs_size[0], fs_size[1], GPU_RGBA16F, &draw_engine_eevee_type);
    effects->sss_data = DRW_texture_pool_query_2d(
        fs_size[0], fs_size[1], GPU_RGBA16F, &draw_engine_eevee_type);

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
        &fbl->sss_clear_fb, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(effects->sss_data)});

    if (effects->sss_separate_albedo) {
      effects->sss_albedo = DRW_texture_pool_query_2d(
          fs_size[0], fs_size[1], GPU_R11F_G11F_B10F, &draw_engine_eevee_type);
    }
    else {
      effects->sss_albedo = NULL;
    }
    return EFFECT_SSS;
  }

  /* Cleanup to release memory */
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->sss_blur_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->sss_resolve_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->sss_clear_fb);
  effects->sss_stencil = NULL;
  effects->sss_blur = NULL;
  effects->sss_data = NULL;

  return 0;
}

static void set_shgrp_stencil(void *UNUSED(userData), DRWShadingGroup *shgrp)
{
  DRW_shgroup_stencil_mask(shgrp, 255);
}

void EEVEE_subsurface_output_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  if (scene_eval->eevee.flag & SCE_EEVEE_SSS_ENABLED) {
    DRW_texture_ensure_fullscreen_2d(&txl->sss_dir_accum, GPU_RGBA16F, 0);
    DRW_texture_ensure_fullscreen_2d(&txl->sss_col_accum, GPU_RGBA16F, 0);

    GPUTexture *stencil_tex = effects->sss_stencil;

    if (GPU_depth_blitting_workaround()) {
      /* Blitting stencil buffer does not work on macOS + Radeon Pro.
       * Blit depth instead and use sss_stencil's depth as depth texture,
       * and dtxl->depth as stencil mask. */
      stencil_tex = dtxl->depth;
    }

    GPU_framebuffer_ensure_config(&fbl->sss_accum_fb,
                                  {GPU_ATTACHMENT_TEXTURE(stencil_tex),
                                   GPU_ATTACHMENT_TEXTURE(txl->sss_dir_accum),
                                   GPU_ATTACHMENT_TEXTURE(txl->sss_col_accum)});

    /* Clear texture. */
    float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GPU_framebuffer_bind(fbl->sss_accum_fb);
    GPU_framebuffer_clear_color(fbl->sss_accum_fb, clear);

    /* Make the opaque refraction pass mask the sss. */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_CLIP_PLANES |
                     DRW_STATE_WIRE | DRW_STATE_WRITE_STENCIL;
    DRW_pass_state_set(vedata->psl->refract_pass, state);
    DRW_pass_foreach_shgroup(vedata->psl->refract_pass, &set_shgrp_stencil, NULL);
  }
  else {
    /* Cleanup to release memory */
    DRW_TEXTURE_FREE_SAFE(txl->sss_dir_accum);
    DRW_TEXTURE_FREE_SAFE(txl->sss_col_accum);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->sss_accum_fb);
  }
}

void EEVEE_subsurface_cache_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & EFFECT_SSS) != 0) {
    /** Screen Space SubSurface Scattering overview
     * TODO
     */
    psl->sss_blur_ps = DRW_pass_create("Blur Horiz",
                                       DRW_STATE_WRITE_COLOR | DRW_STATE_STENCIL_EQUAL);

    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_ADDITIVE | DRW_STATE_STENCIL_EQUAL;
    psl->sss_resolve_ps = DRW_pass_create("Blur Vert", state);
    psl->sss_accum_ps = DRW_pass_create("Resolve Accum", state);
  }
}

void EEVEE_subsurface_add_pass(EEVEE_ViewLayerData *sldata,
                               EEVEE_Data *vedata,
                               uint sss_id,
                               struct GPUUniformBuffer *sss_profile)
{
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();
  GPUTexture **depth_src = GPU_depth_blitting_workaround() ? &effects->sss_stencil : &dtxl->depth;

  DRWShadingGroup *grp = DRW_shgroup_create(e_data.sss_sh[0], psl->sss_blur_ps);
  DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
  DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", depth_src);
  DRW_shgroup_uniform_texture_ref(grp, "sssData", &effects->sss_data);
  DRW_shgroup_uniform_block(grp, "sssProfile", sss_profile);
  DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
  DRW_shgroup_stencil_mask(grp, sss_id);
  DRW_shgroup_call(grp, quad, NULL);

  struct GPUShader *sh = (effects->sss_separate_albedo) ? e_data.sss_sh[2] : e_data.sss_sh[1];
  grp = DRW_shgroup_create(sh, psl->sss_resolve_ps);
  DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
  DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", depth_src);
  DRW_shgroup_uniform_texture_ref(grp, "sssData", &effects->sss_blur);
  DRW_shgroup_uniform_block(grp, "sssProfile", sss_profile);
  DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
  DRW_shgroup_stencil_mask(grp, sss_id);
  DRW_shgroup_call(grp, quad, NULL);

  if (effects->sss_separate_albedo) {
    DRW_shgroup_uniform_texture_ref(grp, "sssAlbedo", &effects->sss_albedo);
  }

  if (DRW_state_is_image_render()) {
    grp = DRW_shgroup_create(e_data.sss_sh[3], psl->sss_accum_ps);
    DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", depth_src);
    DRW_shgroup_uniform_texture_ref(grp, "sssData", &effects->sss_blur);
    DRW_shgroup_uniform_block(grp, "sssProfile", sss_profile);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_stencil_mask(grp, sss_id);
    DRW_shgroup_call(grp, quad, NULL);

    if (effects->sss_separate_albedo) {
      DRW_shgroup_uniform_texture_ref(grp, "sssAlbedo", &effects->sss_albedo);
    }
  }
}

void EEVEE_subsurface_data_render(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & EFFECT_SSS) != 0) {
    float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    /* Clear sss_data texture only... can this be done in a more clever way? */
    GPU_framebuffer_bind(fbl->sss_clear_fb);
    GPU_framebuffer_clear_color(fbl->sss_clear_fb, clear);

    GPU_framebuffer_ensure_config(&fbl->main_fb,
                                  {GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_TEXTURE(effects->sss_data),
                                   GPU_ATTACHMENT_TEXTURE(effects->sss_albedo)});

    GPU_framebuffer_bind(fbl->main_fb);
    DRW_draw_pass(psl->sss_pass);
    DRW_draw_pass(psl->sss_pass_cull);

    /* Restore */
    GPU_framebuffer_ensure_config(&fbl->main_fb,
                                  {GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_LEAVE,
                                   GPU_ATTACHMENT_NONE,
                                   GPU_ATTACHMENT_NONE});
  }
}

void EEVEE_subsurface_compute(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & EFFECT_SSS) != 0) {
    float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    DRW_stats_group_start("SSS");

    if (GPU_depth_blitting_workaround()) {
      /* Copy depth channel */
      GPU_framebuffer_blit(fbl->main_fb, 0, fbl->sss_blit_fb, 0, GPU_DEPTH_BIT);
    }
    else {
      /* Copy stencil channel, could be avoided (see EEVEE_subsurface_init) */
      GPU_framebuffer_blit(fbl->main_fb, 0, fbl->sss_blur_fb, 0, GPU_STENCIL_BIT);
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

void EEVEE_subsurface_output_accumulate(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if (((effects->enabled_effects & EFFECT_SSS) != 0) && (fbl->sss_accum_fb != NULL)) {
    /* Copy stencil channel, could be avoided (see EEVEE_subsurface_init) */
    GPU_framebuffer_blit(fbl->main_fb, 0, fbl->sss_blur_fb, 0, GPU_STENCIL_BIT);

    /* Only do vertical pass + Resolve */
    GPU_framebuffer_bind(fbl->sss_accum_fb);
    DRW_draw_pass(psl->sss_accum_ps);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

void EEVEE_subsurface_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.sss_sh[0]);
  DRW_SHADER_FREE_SAFE(e_data.sss_sh[1]);
  DRW_SHADER_FREE_SAFE(e_data.sss_sh[2]);
  DRW_SHADER_FREE_SAFE(e_data.sss_sh[3]);
}

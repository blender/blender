/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Implementation of Blender Mist pass.
 * IMPORTANT: This is a "post process" of the Z depth so it will lack any transparent objects.
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_world_types.h"

#include "BLI_string_utils.h"

#include "eevee_private.h"

void EEVEE_mist_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  EEVEE_FramebufferList *fbl = vedata->fbl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_PrivateData *g_data = stl->g_data;
  Scene *scene = draw_ctx->scene;

  /* Create FrameBuffer. */
  /* Should be enough precision for many samples. */
  DRW_texture_ensure_fullscreen_2d(&txl->mist_accum, GPU_R32F, 0);

  GPU_framebuffer_ensure_config(&fbl->mist_accum_fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->mist_accum)});

  /* Mist settings. */
  if (scene && scene->world) {
    g_data->mist_start = scene->world->miststa;
    g_data->mist_inv_dist = (scene->world->mistdist > 0.0f) ? 1.0f / scene->world->mistdist : 0.0f;

    switch (scene->world->mistype) {
      case WO_MIST_QUADRATIC:
        g_data->mist_falloff = 2.0f;
        break;
      case WO_MIST_LINEAR:
        g_data->mist_falloff = 1.0f;
        break;
      case WO_MIST_INVERSE_QUADRATIC:
        g_data->mist_falloff = 0.5f;
        break;
    }
  }
  else {
    float near = DRW_view_near_distance_get(NULL);
    float far = DRW_view_far_distance_get(NULL);
    /* Fallback */
    g_data->mist_start = near;
    g_data->mist_inv_dist = 1.0f / fabsf(far - near);
    g_data->mist_falloff = 1.0f;
  }

  /* XXX ??!! WHY? If not it does not match cycles. */
  g_data->mist_falloff *= 0.5f;

  /* Create Pass and shgroup. */
  DRW_PASS_CREATE(psl->mist_accum_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD);
  DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_effect_mist_sh_get(),
                                            psl->mist_accum_ps);
  DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
  DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
  DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
  DRW_shgroup_uniform_vec3(grp, "mistSettings", &g_data->mist_start, 1);
  DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), NULL);
}

void EEVEE_mist_output_accumulate(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;

  if (fbl->mist_accum_fb != NULL) {
    GPU_framebuffer_bind(fbl->mist_accum_fb);

    /* Clear texture. */
    if (effects->taa_current_sample == 1) {
      const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      GPU_framebuffer_clear_color(fbl->mist_accum_fb, clear);
    }

    DRW_draw_pass(psl->mist_accum_ps);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

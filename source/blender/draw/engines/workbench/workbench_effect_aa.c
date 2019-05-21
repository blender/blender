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
 */

#include "ED_screen.h"

#include "workbench_private.h"

void workbench_aa_create_pass(WORKBENCH_Data *vedata, GPUTexture **tx)
{
  WORKBENCH_StorageList *stl = vedata->stl;
  WORKBENCH_PrivateData *wpd = stl->g_data;
  WORKBENCH_PassList *psl = vedata->psl;
  WORKBENCH_EffectInfo *effect_info = stl->effects;
  const DRWContextState *draw_ctx = DRW_context_state_get();

  if (draw_ctx->evil_C != NULL) {
    struct wmWindowManager *wm = CTX_wm_manager(draw_ctx->evil_C);
    wpd->is_playback = ED_screen_animation_playing(wm) != NULL;
  }
  else {
    wpd->is_playback = false;
  }

  if (workbench_is_taa_enabled(wpd)) {
    psl->effect_aa_pass = workbench_taa_create_pass(vedata, tx);
  }
  else if (workbench_is_fxaa_enabled(wpd)) {
    psl->effect_aa_pass = workbench_fxaa_create_pass(tx);
    effect_info->jitter_index = 0;
  }
  else {
    psl->effect_aa_pass = NULL;
    effect_info->jitter_index = 0;
  }
}

static void workspace_aa_draw_transform(GPUTexture *tx, WORKBENCH_PrivateData *wpd)
{
  if (DRW_state_is_image_render()) {
    /* Linear result for render. */
    DRW_transform_none(tx);
  }
  else {
    /* Display space result for viewport. */
    DRW_transform_to_display(tx, wpd->use_color_render_settings, wpd->use_color_render_settings);
  }
}

void workbench_aa_draw_pass(WORKBENCH_Data *vedata, GPUTexture *tx)
{
  WORKBENCH_StorageList *stl = vedata->stl;
  WORKBENCH_PrivateData *wpd = stl->g_data;
  WORKBENCH_FramebufferList *fbl = vedata->fbl;
  WORKBENCH_PassList *psl = vedata->psl;
  WORKBENCH_EffectInfo *effect_info = stl->effects;

  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  if (workbench_is_fxaa_enabled(wpd)) {
    GPU_framebuffer_bind(fbl->effect_fb);
    workspace_aa_draw_transform(tx, wpd);
    GPU_framebuffer_bind(dfbl->color_only_fb);
    DRW_draw_pass(psl->effect_aa_pass);
  }
  else if (workbench_is_taa_enabled(wpd)) {
    /*
     * when drawing the first TAA frame, we transform directly to the
     * color_only_fb as the TAA shader is just performing a direct copy.
     * the workbench_taa_draw_screen_end will fill the history buffer
     * for the other iterations.
     */
    if (effect_info->jitter_index == 1) {
      GPU_framebuffer_bind(dfbl->color_only_fb);
      workspace_aa_draw_transform(tx, wpd);
    }
    else {
      GPU_framebuffer_bind(fbl->effect_fb);
      workspace_aa_draw_transform(tx, wpd);
      GPU_framebuffer_bind(dfbl->color_only_fb);
      DRW_draw_pass(psl->effect_aa_pass);
    }
    workbench_taa_draw_scene_end(vedata);
  }
  else {
    GPU_framebuffer_bind(dfbl->color_only_fb);
    workspace_aa_draw_transform(tx, wpd);
  }
}

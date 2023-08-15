/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "BKE_paint.hh"
#include "BLI_math_color.h"
#include "DRW_render.h"

#include "ED_view3d.hh"

#include "PIL_time.h"
#include "UI_resources.hh"

#include "overlay_private.hh"

void OVERLAY_mode_transfer_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  pd->mode_transfer.time = PIL_check_seconds_timer();

  for (int i = 0; i < 2; i++) {
    /* Non Meshes Pass (Camera, empties, lights ...) */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND_ALPHA;
    DRW_PASS_CREATE(psl->mode_transfer_ps[i], state | pd->clipping_state);
  }
}

#define MODE_TRANSFER_FLASH_LENGTH 0.55f
/* TODO(pablodp606): Remove this option for 3.0 if fade in/out is not used. */
#define MODE_TRANSFER_FLASH_FADE 0.0f
#define MODE_TRANSFER_FLASH_MAX_ALPHA 0.25f

static bool mode_transfer_is_animation_running(const float anim_time)
{
  return anim_time >= 0.0f && anim_time <= MODE_TRANSFER_FLASH_LENGTH;
}

static float mode_transfer_alpha_for_animation_time_get(const float anim_time)
{
  if (anim_time > MODE_TRANSFER_FLASH_LENGTH) {
    return 0.0f;
  }

  if (anim_time < 0.0f) {
    return 0.0f;
  }

  if (MODE_TRANSFER_FLASH_FADE <= 0.0f) {
    return (1.0f - (anim_time / MODE_TRANSFER_FLASH_LENGTH)) * MODE_TRANSFER_FLASH_MAX_ALPHA;
  }

  const float flash_fade_in_time = MODE_TRANSFER_FLASH_LENGTH * MODE_TRANSFER_FLASH_FADE;
  const float flash_fade_out_time = MODE_TRANSFER_FLASH_LENGTH - flash_fade_in_time;

  float alpha = 0.0f;
  if (anim_time < flash_fade_in_time) {
    alpha = anim_time / flash_fade_in_time;
  }
  else {
    const float fade_out_anim_time = anim_time - flash_fade_in_time;
    alpha = 1.0f - (fade_out_anim_time / flash_fade_out_time);
  }

  return alpha * MODE_TRANSFER_FLASH_MAX_ALPHA;
}

void OVERLAY_mode_transfer_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  OVERLAY_PassList *psl = vedata->psl;

  if (pd->xray_enabled) {
    return;
  }

  const float animation_time = pd->mode_transfer.time -
                               ob->runtime.overlay_mode_transfer_start_time;

  if (!mode_transfer_is_animation_running(animation_time)) {
    return;
  }

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob, draw_ctx->rv3d) &&
                               !DRW_state_is_image_render();
  const bool is_xray = (ob->dtx & OB_DRAW_IN_FRONT) != 0;

  DRWShadingGroup *mode_transfer_grp[2];

  for (int i = 0; i < 2; i++) {
    GPUShader *sh = OVERLAY_shader_uniform_color();
    mode_transfer_grp[i] = DRW_shgroup_create(sh, psl->mode_transfer_ps[i]);
    DRW_shgroup_uniform_block(mode_transfer_grp[i], "globalsBlock", G_draw.block_ubo);

    float color[4];
    UI_GetThemeColor3fv(TH_VERTEX_SELECT, color);
    color[3] = mode_transfer_alpha_for_animation_time_get(animation_time);
    srgb_to_linearrgb_v4(color, color);
    DRW_shgroup_uniform_vec4_copy(mode_transfer_grp[i], "ucolor", color);
  }

  if (!pd->use_in_front) {
    mode_transfer_grp[IN_FRONT] = mode_transfer_grp[NOT_IN_FRONT];
  }

  pd->mode_transfer.any_animated = true;

  if (use_sculpt_pbvh) {
    DRW_shgroup_call_sculpt(mode_transfer_grp[is_xray], ob, false, false, false, false, false);
  }
  else {
    GPUBatch *geom = DRW_cache_object_surface_get(ob);
    if (geom) {
      DRW_shgroup_call(mode_transfer_grp[is_xray], geom, ob);
    }
  }
}

void OVERLAY_mode_transfer_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->mode_transfer_ps[NOT_IN_FRONT]);
}

void OVERLAY_mode_transfer_infront_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->mode_transfer_ps[IN_FRONT]);
}

void OVERLAY_mode_transfer_cache_finish(OVERLAY_Data *vedata)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  if (pd->mode_transfer.any_animated) {
    DRW_viewport_request_redraw();
  }
  pd->mode_transfer.any_animated = false;
}

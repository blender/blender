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
 * Copyright 2018, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#include "workbench_private.h"

#include "DNA_userdef_types.h"

#include "ED_view3d.h"

#include "UI_resources.h"

#include "GPU_batch.h"

/* -------------------------------------------------------------------- */
/** \name World Data
 * \{ */

static void workbench_world_data_free(DrawData *dd)
{
  WORKBENCH_WorldData *data = (WORKBENCH_WorldData *)dd;
  DRW_UBO_FREE_SAFE(data->world_ubo);
}

/* Ensure the availability of the world_ubo in the given WORKBENCH_PrivateData
 *
 * See T70167: Some platforms create threads to upload ubo's.
 *
 * Reuses the last previous created `world_ubo`. Due to limitations of
 * DrawData it will only be reused when there is a world attached to the Scene.
 * Future development: The best location would be to store it in the View3D.
 *
 * We don't cache the data itself as there was no indication that that lead to
 * an improvement.
 *
 * This functions also sets the `WORKBENCH_PrivateData.is_world_ubo_owner` that must
 * be respected.
 */
static void workbench_world_data_ubo_ensure(const Scene *scene, WORKBENCH_PrivateData *wpd)
{
  World *world = scene->world;
  if (world) {
    WORKBENCH_WorldData *engine_world_data = (WORKBENCH_WorldData *)DRW_drawdata_ensure(
        &world->id,
        &draw_engine_workbench_solid,
        sizeof(WORKBENCH_WorldData),
        NULL,
        &workbench_world_data_free);

    if (engine_world_data->world_ubo == NULL) {
      engine_world_data->world_ubo = DRW_uniformbuffer_create(sizeof(WORKBENCH_UBO_World),
                                                              &wpd->world_data);
    }
    else {
      DRW_uniformbuffer_update(engine_world_data->world_ubo, &wpd->world_data);
    }

    /* Borrow world data ubo */
    wpd->is_world_ubo_owner = false;
    wpd->world_ubo = engine_world_data->world_ubo;
  }
  else {
    /* there is no world so we cannot cache the UBO. */
    BLI_assert(!wpd->world_ubo || wpd->is_world_ubo_owner);
    if (!wpd->world_ubo) {
      wpd->is_world_ubo_owner = true;
      wpd->world_ubo = DRW_uniformbuffer_create(sizeof(WORKBENCH_UBO_World), &wpd->world_data);
    }
  }
}

static void workbench_world_data_update_shadow_direction_vs(WORKBENCH_PrivateData *wpd)
{
  WORKBENCH_UBO_World *wd = &wpd->world_data;
  float light_direction[3];
  float view_matrix[4][4];
  DRW_view_viewmat_get(NULL, view_matrix, false);

  workbench_private_data_get_light_direction(light_direction);

  /* Shadow direction. */
  mul_v3_mat3_m4v3(wd->shadow_direction_vs, view_matrix, light_direction);
}

/* \} */

void workbench_clear_color_get(float color[4])
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene = draw_ctx->scene;

  if (!DRW_state_is_scene_render() || !DRW_state_draw_background()) {
    zero_v4(color);
  }
  else if (scene->world) {
    copy_v3_v3(color, &scene->world->horr);
    color[3] = 1.0f;
  }
  else {
    zero_v3(color);
    color[3] = 1.0f;
  }
}

void workbench_effect_info_init(WORKBENCH_EffectInfo *effect_info)
{
  effect_info->jitter_index = 0;
  effect_info->view_updated = true;
}

void workbench_private_data_init(WORKBENCH_PrivateData *wpd)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene = draw_ctx->scene;
  wpd->material_hash = BLI_ghash_ptr_new(__func__);
  wpd->material_transp_hash = BLI_ghash_ptr_new(__func__);
  wpd->preferences = &U;

  View3D *v3d = draw_ctx->v3d;
  RegionView3D *rv3d = draw_ctx->rv3d;

  if (!v3d || (v3d->shading.type == OB_RENDER && BKE_scene_uses_blender_workbench(scene))) {
    wpd->shading = scene->display.shading;
    wpd->shading.xray_alpha = XRAY_ALPHA((&scene->display));
    wpd->use_color_render_settings = true;
  }
  else {
    wpd->shading = v3d->shading;
    wpd->shading.xray_alpha = XRAY_ALPHA(v3d);
    wpd->use_color_render_settings = false;
  }

  wpd->use_color_management = BKE_scene_check_color_management_enabled(scene);

  if (wpd->shading.light == V3D_LIGHTING_MATCAP) {
    wpd->studio_light = BKE_studiolight_find(wpd->shading.matcap, STUDIOLIGHT_TYPE_MATCAP);
  }
  else {
    wpd->studio_light = BKE_studiolight_find(wpd->shading.studio_light, STUDIOLIGHT_TYPE_STUDIO);
  }

  /* If matcaps are missing, use this as fallback. */
  if (UNLIKELY(wpd->studio_light == NULL)) {
    wpd->studio_light = BKE_studiolight_find(wpd->shading.studio_light, STUDIOLIGHT_TYPE_STUDIO);
  }

  float shadow_focus = scene->display.shadow_focus;
  /* Clamp to avoid overshadowing and shading errors. */
  CLAMP(shadow_focus, 0.0001f, 0.99999f);
  wpd->shadow_shift = scene->display.shadow_shift;
  wpd->shadow_focus = 1.0f - shadow_focus * (1.0f - wpd->shadow_shift);
  wpd->shadow_multiplier = 1.0 - wpd->shading.shadow_intensity;

  WORKBENCH_UBO_World *wd = &wpd->world_data;
  wd->matcap_orientation = (wpd->shading.flag & V3D_SHADING_MATCAP_FLIP_X) != 0;

  studiolight_update_world(wpd, wpd->studio_light, wd);

  copy_v3_v3(wd->object_outline_color, wpd->shading.object_outline_color);
  wd->object_outline_color[3] = 1.0f;

  wd->curvature_ridge = 0.5f / max_ff(square_f(wpd->shading.curvature_ridge_factor), 1e-4f);
  wd->curvature_valley = 0.7f / max_ff(square_f(wpd->shading.curvature_valley_factor), 1e-4f);

  /* Will be NULL when rendering. */
  if (RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
    wpd->world_clip_planes = rv3d->clip;
  }
  else {
    wpd->world_clip_planes = NULL;
  }

  workbench_world_data_update_shadow_direction_vs(wpd);
  workbench_world_data_ubo_ensure(scene, wpd);

  /* Cavity settings */
  {
    const int ssao_samples = scene->display.matcap_ssao_samples;

    float invproj[4][4];
    const bool is_persp = DRW_view_is_persp_get(NULL);
    /* view vectors for the corners of the view frustum.
     * Can be used to recreate the world space position easily */
    float viewvecs[3][4] = {
        {-1.0f, -1.0f, -1.0f, 1.0f},
        {1.0f, -1.0f, -1.0f, 1.0f},
        {-1.0f, 1.0f, -1.0f, 1.0f},
    };
    int i;
    const float *size = DRW_viewport_size_get();

    wpd->ssao_params[0] = ssao_samples;
    wpd->ssao_params[1] = size[0] / 64.0;
    wpd->ssao_params[2] = size[1] / 64.0;
    wpd->ssao_params[3] = 0;

    /* distance, factor, factor, attenuation */
    copy_v4_fl4(wpd->ssao_settings,
                scene->display.matcap_ssao_distance,
                wpd->shading.cavity_valley_factor,
                wpd->shading.cavity_ridge_factor,
                scene->display.matcap_ssao_attenuation);

    DRW_view_winmat_get(NULL, wpd->winmat, false);
    DRW_view_winmat_get(NULL, invproj, true);

    /* convert the view vectors to view space */
    for (i = 0; i < 3; i++) {
      mul_m4_v4(invproj, viewvecs[i]);
      /* normalized trick see:
       * http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
      mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][3]);
      if (is_persp) {
        mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][2]);
      }
      viewvecs[i][3] = 1.0;

      copy_v4_v4(wpd->viewvecs[i], viewvecs[i]);
    }

    /* we need to store the differences */
    wpd->viewvecs[1][0] -= wpd->viewvecs[0][0];
    wpd->viewvecs[1][1] = wpd->viewvecs[2][1] - wpd->viewvecs[0][1];

    /* calculate a depth offset as well */
    if (!is_persp) {
      float vec_far[] = {-1.0f, -1.0f, 1.0f, 1.0f};
      mul_m4_v4(invproj, vec_far);
      mul_v3_fl(vec_far, 1.0f / vec_far[3]);
      wpd->viewvecs[1][2] = vec_far[2] - wpd->viewvecs[0][2];
    }
  }

  wpd->volumes_do = false;
  BLI_listbase_clear(&wpd->smoke_domains);
}

void workbench_private_data_get_light_direction(float r_light_direction[3])
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;

  copy_v3_v3(r_light_direction, scene->display.light_direction);
  SWAP(float, r_light_direction[2], r_light_direction[1]);
  r_light_direction[2] = -r_light_direction[2];
  r_light_direction[0] = -r_light_direction[0];
}

void workbench_private_data_free(WORKBENCH_PrivateData *wpd)
{
  BLI_ghash_free(wpd->material_hash, NULL, MEM_freeN);
  BLI_ghash_free(wpd->material_transp_hash, NULL, MEM_freeN);

  if (wpd->is_world_ubo_owner) {
    DRW_UBO_FREE_SAFE(wpd->world_ubo);
  }
  else {
    wpd->world_ubo = NULL;
  }

  DRW_UBO_FREE_SAFE(wpd->dof_ubo);
}

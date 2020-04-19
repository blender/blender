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
#include "DRW_render.h"

#include "workbench_private.h"

#include "BLI_memblock.h"

#include "DNA_userdef_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "GPU_uniformbuffer.h"

/* -------------------------------------------------------------------- */
/** \name World Data
 * \{ */

GPUUniformBuffer *workbench_material_ubo_alloc(WORKBENCH_PrivateData *wpd)
{
  struct GPUUniformBuffer **ubo = BLI_memblock_alloc(wpd->material_ubo);
  if (*ubo == NULL) {
    *ubo = GPU_uniformbuffer_create(sizeof(WORKBENCH_UBO_Material) * MAX_MATERIAL, NULL, NULL);
  }
  return *ubo;
}

static void workbench_ubo_free(void *elem)
{
  GPUUniformBuffer **ubo = elem;
  DRW_UBO_FREE_SAFE(*ubo);
}

static void workbench_view_layer_data_free(void *storage)
{
  WORKBENCH_ViewLayerData *vldata = (WORKBENCH_ViewLayerData *)storage;

  DRW_UBO_FREE_SAFE(vldata->dof_sample_ubo);
  DRW_UBO_FREE_SAFE(vldata->world_ubo);
  DRW_UBO_FREE_SAFE(vldata->cavity_sample_ubo);
  DRW_TEXTURE_FREE_SAFE(vldata->cavity_jitter_tx);

  BLI_memblock_destroy(vldata->material_ubo_data, NULL);
  BLI_memblock_destroy(vldata->material_ubo, workbench_ubo_free);
}

static WORKBENCH_ViewLayerData *workbench_view_layer_data_ensure_ex(struct ViewLayer *view_layer)
{
  WORKBENCH_ViewLayerData **vldata = (WORKBENCH_ViewLayerData **)
      DRW_view_layer_engine_data_ensure_ex(view_layer,
                                           (DrawEngineType *)&workbench_view_layer_data_ensure_ex,
                                           &workbench_view_layer_data_free);

  if (*vldata == NULL) {
    *vldata = MEM_callocN(sizeof(**vldata), "WORKBENCH_ViewLayerData");
    size_t matbuf_size = sizeof(WORKBENCH_UBO_Material) * MAX_MATERIAL;
    (*vldata)->material_ubo_data = BLI_memblock_create_ex(matbuf_size, matbuf_size * 2);
    (*vldata)->material_ubo = BLI_memblock_create_ex(sizeof(void *), sizeof(void *) * 8);
    (*vldata)->world_ubo = DRW_uniformbuffer_create(sizeof(WORKBENCH_UBO_World), NULL);
  }

  return *vldata;
}

/* \} */

static void workbench_viewvecs_update(float r_viewvecs[3][4])
{
  float invproj[4][4];
  const bool is_persp = DRW_view_is_persp_get(NULL);
  DRW_view_winmat_get(NULL, invproj, true);

  /* view vectors for the corners of the view frustum.
   * Can be used to recreate the world space position easily */
  copy_v4_fl4(r_viewvecs[0], -1.0f, -1.0f, -1.0f, 1.0f);
  copy_v4_fl4(r_viewvecs[1], 1.0f, -1.0f, -1.0f, 1.0f);
  copy_v4_fl4(r_viewvecs[2], -1.0f, 1.0f, -1.0f, 1.0f);

  /* convert the view vectors to view space */
  for (int i = 0; i < 3; i++) {
    mul_m4_v4(invproj, r_viewvecs[i]);
    /* normalized trick see:
     * http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
    mul_v3_fl(r_viewvecs[i], 1.0f / r_viewvecs[i][3]);
    if (is_persp) {
      mul_v3_fl(r_viewvecs[i], 1.0f / r_viewvecs[i][2]);
    }
    r_viewvecs[i][3] = 1.0;
  }

  /* we need to store the differences */
  r_viewvecs[1][0] -= r_viewvecs[0][0];
  r_viewvecs[1][1] = r_viewvecs[2][1] - r_viewvecs[0][1];

  /* calculate a depth offset as well */
  if (!is_persp) {
    float vec_far[] = {-1.0f, -1.0f, 1.0f, 1.0f};
    mul_m4_v4(invproj, vec_far);
    mul_v3_fl(vec_far, 1.0f / vec_far[3]);
    r_viewvecs[1][2] = vec_far[2] - r_viewvecs[0][2];
  }
}

static void workbench_studiolight_data_update(WORKBENCH_PrivateData *wpd, WORKBENCH_UBO_World *wd)
{
  StudioLight *studiolight = wpd->studio_light;
  float view_matrix[4][4], rot_matrix[4][4];
  DRW_view_viewmat_get(NULL, view_matrix, false);

  if (USE_WORLD_ORIENTATION(wpd)) {
    axis_angle_to_mat4_single(rot_matrix, 'Z', -wpd->shading.studiolight_rot_z);
    mul_m4_m4m4(rot_matrix, view_matrix, rot_matrix);
    swap_v3_v3(rot_matrix[2], rot_matrix[1]);
    negate_v3(rot_matrix[2]);
  }
  else {
    unit_m4(rot_matrix);
  }

  if (U.edit_studio_light) {
    studiolight = BKE_studiolight_studio_edit_get();
  }

  /* Studio Lights. */
  for (int i = 0; i < 4; i++) {
    WORKBENCH_UBO_Light *light = &wd->lights[i];

    SolidLight *sl = (studiolight) ? &studiolight->light[i] : NULL;
    if (sl && sl->flag) {
      copy_v3_v3(light->light_direction, sl->vec);
      mul_mat3_m4_v3(rot_matrix, light->light_direction);
      /* We should predivide the power by PI but that makes the lights really dim. */
      copy_v3_v3(light->specular_color, sl->spec);
      copy_v3_v3(light->diffuse_color, sl->col);
      light->wrapped = sl->smooth;
    }
    else {
      copy_v3_fl3(light->light_direction, 1.0f, 0.0f, 0.0f);
      copy_v3_fl(light->specular_color, 0.0f);
      copy_v3_fl(light->diffuse_color, 0.0f);
    }
  }

  if (studiolight) {
    copy_v3_v3(wd->ambient_color, studiolight->light_ambient);
  }
  else {
    copy_v3_fl(wd->ambient_color, 1.0f);
  }

  wd->use_specular = workbench_is_specular_highlight_enabled(wpd);
}

void workbench_private_data_init(WORKBENCH_PrivateData *wpd)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  RegionView3D *rv3d = draw_ctx->rv3d;
  View3D *v3d = draw_ctx->v3d;
  Scene *scene = draw_ctx->scene;
  WORKBENCH_ViewLayerData *vldata = workbench_view_layer_data_ensure_ex(draw_ctx->view_layer);

  wpd->is_playback = DRW_state_is_playback();
  wpd->is_navigating = DRW_state_is_navigating();

  wpd->ctx_mode = CTX_data_mode_enum_ex(
      draw_ctx->object_edit, draw_ctx->obact, draw_ctx->object_mode);

  wpd->preferences = &U;
  wpd->scene = scene;
  wpd->sh_cfg = draw_ctx->sh_cfg;

  /* FIXME: This reproduce old behavior when workbench was separated in 2 engines.
   * But this is a workaround for a missing update tagging. */
  DRWState clip_state = RV3D_CLIPPING_ENABLED(v3d, rv3d) ? DRW_STATE_CLIP_PLANES : 0;
  if (clip_state != wpd->clip_state) {
    wpd->view_updated = true;
  }
  wpd->clip_state = clip_state;

  wpd->vldata = vldata;
  wpd->world_ubo = vldata->world_ubo;

  wpd->taa_sample_len = workbench_antialiasing_sample_count_get(wpd);

  wpd->volumes_do = false;
  BLI_listbase_clear(&wpd->smoke_domains);

  /* FIXME: This reproduce old behavior when workbench was separated in 2 engines.
   * But this is a workaround for a missing update tagging. */
  if ((rv3d != NULL) && (rv3d->rflag & RV3D_GPULIGHT_UPDATE)) {
    wpd->view_updated = true;
    rv3d->rflag &= ~RV3D_GPULIGHT_UPDATE;
  }

  if (!v3d || (v3d->shading.type == OB_RENDER && BKE_scene_uses_blender_workbench(scene))) {
    /* FIXME: This reproduce old behavior when workbench was separated in 2 engines.
     * But this is a workaround for a missing update tagging from operators. */
    if (scene->display.shading.type != wpd->shading.type ||
        (v3d && (XRAY_ENABLED(v3d) != XRAY_ENABLED(&scene->display))) ||
        (scene->display.shading.flag != wpd->shading.flag)) {
      wpd->view_updated = true;
    }

    wpd->shading = scene->display.shading;
    if (XRAY_FLAG_ENABLED((&scene->display))) {
      wpd->shading.xray_alpha = XRAY_ALPHA((&scene->display));
    }
    else {
      wpd->shading.xray_alpha = 1.0f;
    }

    if (scene->r.alphamode == R_ALPHAPREMUL) {
      copy_v4_fl(wpd->background_color, 0.0f);
    }
    else if (scene->world) {
      World *wo = scene->world;
      copy_v4_fl4(wpd->background_color, wo->horr, wo->horg, wo->horb, 1.0f);
    }
    else {
      copy_v4_fl4(wpd->background_color, 0.0f, 0.0f, 0.0f, 1.0f);
    }
  }
  else {
    /* FIXME: This reproduce old behavior when workbench was separated in 2 engines.
     * But this is a workaround for a missing update tagging from operators. */
    if (v3d->shading.type != wpd->shading.type || XRAY_ENABLED(v3d) != XRAY_ENABLED(wpd) ||
        v3d->shading.flag != wpd->shading.flag) {
      wpd->view_updated = true;
    }

    wpd->shading = v3d->shading;
    if (wpd->shading.type < OB_SOLID) {
      wpd->shading.xray_alpha = 0.0f;
    }
    else if (XRAY_ENABLED(v3d)) {
      wpd->shading.xray_alpha = XRAY_ALPHA(v3d);
    }
    else {
      wpd->shading.xray_alpha = 1.0f;
    }

    /* No background. The overlays will draw the correct one. */
    copy_v4_fl(wpd->background_color, 0.0f);
  }

  wpd->cull_state = CULL_BACKFACE_ENABLED(wpd) ? DRW_STATE_CULL_BACK : 0;

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

  {
    /* Material UBOs. */
    wpd->material_ubo_data = vldata->material_ubo_data;
    wpd->material_ubo = vldata->material_ubo;
    wpd->material_chunk_count = 1;
    wpd->material_chunk_curr = 0;
    wpd->material_index = 1;
    /* Create default material ubo. */
    wpd->material_ubo_data_curr = BLI_memblock_alloc(wpd->material_ubo_data);
    wpd->material_ubo_curr = workbench_material_ubo_alloc(wpd);
    /* Init default material used by vertex color & texture. */
    workbench_material_ubo_data(
        wpd, NULL, NULL, &wpd->material_ubo_data_curr[0], V3D_SHADING_MATERIAL_COLOR);
  }
}

void workbench_update_world_ubo(WORKBENCH_PrivateData *wpd)
{
  WORKBENCH_UBO_World wd;

  copy_v2_v2(wd.viewport_size, DRW_viewport_size_get());
  copy_v2_v2(wd.viewport_size_inv, DRW_viewport_invert_size_get());
  copy_v3_v3(wd.object_outline_color, wpd->shading.object_outline_color);
  wd.object_outline_color[3] = 1.0f;
  wd.ui_scale = G_draw.block.sizePixel;
  wd.matcap_orientation = (wpd->shading.flag & V3D_SHADING_MATCAP_FLIP_X) != 0;

  workbench_studiolight_data_update(wpd, &wd);
  workbench_shadow_data_update(wpd, &wd);
  workbench_cavity_data_update(wpd, &wd);
  workbench_viewvecs_update(wd.viewvecs);

  DRW_uniformbuffer_update(wpd->world_ubo, &wd);
}

void workbench_update_material_ubos(WORKBENCH_PrivateData *UNUSED(wpd))
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  WORKBENCH_ViewLayerData *vldata = workbench_view_layer_data_ensure_ex(draw_ctx->view_layer);

  BLI_memblock_iter iter, iter_data;
  BLI_memblock_iternew(vldata->material_ubo, &iter);
  BLI_memblock_iternew(vldata->material_ubo_data, &iter_data);
  WORKBENCH_UBO_Material *matchunk;
  while ((matchunk = BLI_memblock_iterstep(&iter_data))) {
    GPUUniformBuffer **ubo = BLI_memblock_iterstep(&iter);
    BLI_assert(*ubo != NULL);
    GPU_uniformbuffer_update(*ubo, matchunk);
  }

  BLI_memblock_clear(vldata->material_ubo, workbench_ubo_free);
  BLI_memblock_clear(vldata->material_ubo_data, NULL);
}

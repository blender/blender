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
#include "DRW_render.h"

#include "BKE_camera.h"
#include "BKE_studiolight.h"

#include "BLI_rect.h"
#include "BLI_rand.h"

#include "DNA_screen_types.h"
#include "DNA_world_types.h"

#include "DEG_depsgraph_query.h"

#include "ED_screen.h"

#include "UI_resources.h"

#include "eevee_private.h"
#include "eevee_lightcache.h"

#include "draw_common.h"

static void eevee_lookdev_lightcache_delete(EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PrivateData *g_data = stl->g_data;
  EEVEE_TextureList *txl = vedata->txl;

  MEM_SAFE_FREE(stl->lookdev_lightcache);
  MEM_SAFE_FREE(stl->lookdev_grid_data);
  MEM_SAFE_FREE(stl->lookdev_cube_data);
  DRW_TEXTURE_FREE_SAFE(txl->lookdev_grid_tx);
  DRW_TEXTURE_FREE_SAFE(txl->lookdev_cube_tx);
  g_data->studiolight_index = -1;
  g_data->studiolight_rot_z = 0.0f;
}

void EEVEE_lookdev_cache_init(EEVEE_Data *vedata,
                              DRWShadingGroup **grp,
                              DRWPass *pass,
                              float background_alpha,
                              World *UNUSED(world),
                              EEVEE_LightProbesInfo *pinfo)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_PrivateData *g_data = stl->g_data;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;
  Scene *scene = draw_ctx->scene;

  if (LOOK_DEV_OVERLAY_ENABLED(v3d)) {
    /* Viewport / Spheres size. */
    rcti rect;
    ED_region_visible_rect(draw_ctx->ar, &rect);

    /* Make the viewport width scale the lookdev spheres a bit.
     * Scale between 1000px and 2000px. */
    const float viewport_scale = clamp_f(
        BLI_rcti_size_x(&rect) / (2000.0f * U.dpi_fac), 0.5f, 1.0f);
    const int sphere_size = U.lookdev_sphere_size * U.dpi_fac * viewport_scale;

    if (sphere_size != effects->sphere_size || rect.xmax != effects->anchor[0] ||
        rect.ymin != effects->anchor[1]) {
      /* If sphere size or anchor point moves, reset TAA to avoid ghosting issue.
       * This needs to happen early because we are changing taa_current_sample. */
      effects->sphere_size = sphere_size;
      effects->anchor[0] = rect.xmax;
      effects->anchor[1] = rect.ymin;
      EEVEE_temporal_sampling_reset(vedata);
    }
  }

  if (LOOK_DEV_STUDIO_LIGHT_ENABLED(v3d)) {
    StudioLight *sl = BKE_studiolight_find(v3d->shading.lookdev_light,
                                           STUDIOLIGHT_ORIENTATIONS_MATERIAL_MODE);
    if (sl && (sl->flag & STUDIOLIGHT_TYPE_WORLD)) {
      GPUShader *shader = EEVEE_shaders_default_studiolight_sh_get();
      struct GPUBatch *geom = DRW_cache_fullscreen_quad_get();
      GPUTexture *tex = NULL;

      /* If one of the component is missing we start from scratch. */
      if ((stl->lookdev_grid_data == NULL) || (stl->lookdev_cube_data == NULL) ||
          (txl->lookdev_grid_tx == NULL) || (txl->lookdev_cube_tx == NULL)) {
        eevee_lookdev_lightcache_delete(vedata);
      }

      if (stl->lookdev_lightcache == NULL) {
        const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);
#if defined(IRRADIANCE_SH_L2)
        int grid_res = 4;
#elif defined(IRRADIANCE_CUBEMAP)
        int grid_res = 8;
#elif defined(IRRADIANCE_HL2)
        int grid_res = 4;
#endif
        int cube_res = OCTAHEDRAL_SIZE_FROM_CUBESIZE(scene_eval->eevee.gi_cubemap_resolution);
        int vis_res = scene_eval->eevee.gi_visibility_resolution;

        stl->lookdev_lightcache = EEVEE_lightcache_create(
            1, 1, cube_res, vis_res, (int[3]){grid_res, grid_res, 1});

        /* XXX: Fix memleak. TODO find out why. */
        MEM_SAFE_FREE(stl->lookdev_cube_mips);

        /* We do this to use a special light cache for lookdev.
         * This lightcache needs to be per viewport. But we need to
         * have correct freeing when the viewport is closed. So we
         * need to reference all textures to the txl and the memblocks
         * to the stl. */
        stl->lookdev_grid_data = stl->lookdev_lightcache->grid_data;
        stl->lookdev_cube_data = stl->lookdev_lightcache->cube_data;
        stl->lookdev_cube_mips = stl->lookdev_lightcache->cube_mips;
        txl->lookdev_grid_tx = stl->lookdev_lightcache->grid_tx.tex;
        txl->lookdev_cube_tx = stl->lookdev_lightcache->cube_tx.tex;
      }

      stl->g_data->light_cache = stl->lookdev_lightcache;

      static float background_color[4];
      UI_GetThemeColor4fv(TH_BACK, background_color);
      /* XXX: Really quick conversion to avoid washed out background.
       * Needs to be addressed properly (color managed using ocio). */
      srgb_to_linearrgb_v4(background_color, background_color);

      *grp = DRW_shgroup_create(shader, pass);
      axis_angle_to_mat3_single(
          stl->g_data->studiolight_matrix, 'Z', v3d->shading.studiolight_rot_z);
      DRW_shgroup_uniform_mat3(*grp, "StudioLightMatrix", stl->g_data->studiolight_matrix);
      DRW_shgroup_uniform_float_copy(*grp, "backgroundAlpha", background_alpha);
      DRW_shgroup_uniform_vec3(*grp, "color", background_color, 1);
      DRW_shgroup_call(*grp, geom, NULL);
      if (!pinfo) {
        /* Do not fadeout when doing probe rendering, only when drawing the background */
        DRW_shgroup_uniform_float(
            *grp, "studioLightBackground", &v3d->shading.studiolight_background, 1);
        BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EQUIRECT_IRRADIANCE_GPUTEXTURE);
        tex = sl->equirect_irradiance_gputexture;
      }
      else {
        DRW_shgroup_uniform_float_copy(*grp, "studioLightBackground", 1.0f);
        BKE_studiolight_ensure_flag(sl, STUDIOLIGHT_EQUIRECT_RADIANCE_GPUTEXTURE);
        tex = sl->equirect_radiance_gputexture;
      }
      DRW_shgroup_uniform_texture(*grp, "image", tex);

      /* Do we need to recalc the lightprobes? */
      if (g_data->studiolight_index != sl->index ||
          g_data->studiolight_rot_z != v3d->shading.studiolight_rot_z ||
          g_data->studiolight_cubemap_res != scene->eevee.gi_cubemap_resolution ||
          g_data->studiolight_glossy_clamp != scene->eevee.gi_glossy_clamp ||
          g_data->studiolight_filter_quality != scene->eevee.gi_filter_quality) {
        stl->lookdev_lightcache->flag |= LIGHTCACHE_UPDATE_WORLD;
        g_data->studiolight_index = sl->index;
        g_data->studiolight_rot_z = v3d->shading.studiolight_rot_z;
        g_data->studiolight_cubemap_res = scene->eevee.gi_cubemap_resolution;
        g_data->studiolight_glossy_clamp = scene->eevee.gi_glossy_clamp;
        g_data->studiolight_filter_quality = scene->eevee.gi_filter_quality;
      }
    }
  }
}

static void eevee_lookdev_apply_taa(const EEVEE_EffectsInfo *effects,
                                    int sphere_size,
                                    float winmat[4][4])
{
  if (DRW_state_is_image_render() || ((effects->enabled_effects & EFFECT_TAA) != 0)) {
    double ht_point[2];
    double ht_offset[2] = {0.0, 0.0};
    uint ht_primes[2] = {2, 3};
    float ofs[2];

    BLI_halton_2d(ht_primes, ht_offset, effects->taa_current_sample, ht_point);
    EEVEE_temporal_sampling_offset_calc(ht_point, 1.5f, ofs);
    winmat[3][0] += ofs[0] / sphere_size;
    winmat[3][1] += ofs[1] / sphere_size;
  }
}

void EEVEE_lookdev_draw(EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();

  const DRWContextState *draw_ctx = DRW_context_state_get();

  if (psl->lookdev_diffuse_pass && LOOK_DEV_OVERLAY_ENABLED(draw_ctx->v3d)) {
    /* Config renderer. */
    EEVEE_CommonUniformBuffer *common = &sldata->common_data;
    common->la_num_light = 0;
    common->prb_num_planar = 0;
    common->prb_num_render_cube = 1;
    common->prb_num_render_grid = 1;
    common->ao_dist = 0.0f;
    common->ao_factor = 0.0f;
    common->ao_settings = 0.0f;
    DRW_uniformbuffer_update(sldata->common_ubo, common);

    /* override matrices */
    DRWMatrixState matstate;
    unit_m4(matstate.winmat);

    eevee_lookdev_apply_taa(effects, effects->sphere_size, matstate.winmat);

    /* "Remove" view matrix location. Leaving only rotation. */
    DRW_viewport_matrix_get(matstate.viewmat, DRW_MAT_VIEW);
    zero_v3(matstate.viewmat[3]);
    mul_m4_m4m4(matstate.persmat, matstate.winmat, matstate.viewmat);
    invert_m4_m4(matstate.wininv, matstate.winmat);
    invert_m4_m4(matstate.viewinv, matstate.viewmat);
    invert_m4_m4(matstate.persinv, matstate.persmat);

    DRW_viewport_matrix_override_set_all(&matstate);

    /* Find the right framebuffers to render to. */
    GPUFrameBuffer *fb = (effects->target_buffer == fbl->effect_color_fb) ? fbl->main_fb :
                                                                            fbl->effect_fb;

    DRW_stats_group_start("Look Dev");

    GPU_framebuffer_bind(fb);

    const int sphere_margin = effects->sphere_size / 6.0f;
    float offset[2] = {0.0f, sphere_margin};

    offset[0] = effects->sphere_size + sphere_margin;
    GPU_framebuffer_viewport_set(fb,
                                 effects->anchor[0] - offset[0],
                                 effects->anchor[1] + offset[1],
                                 effects->sphere_size,
                                 effects->sphere_size);

    DRW_draw_pass(psl->lookdev_diffuse_pass);

    offset[0] = (effects->sphere_size + sphere_margin) +
                (sphere_margin + effects->sphere_size + sphere_margin);
    GPU_framebuffer_viewport_set(fb,
                                 effects->anchor[0] - offset[0],
                                 effects->anchor[1] + offset[1],
                                 effects->sphere_size,
                                 effects->sphere_size);

    DRW_draw_pass(psl->lookdev_glossy_pass);

    DRW_stats_group_end();

    DRW_viewport_matrix_override_unset_all();
  }
}

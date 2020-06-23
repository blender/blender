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

#include "draw_color_management.h" /* TODO remove dependency */

#include "BLI_rand.h"

#include "BKE_object.h"

#include "DEG_depsgraph_query.h"

#include "DNA_world_types.h"

#include "eevee_private.h"

#include "eevee_engine.h" /* own include */

#define EEVEE_ENGINE "BLENDER_EEVEE"

/* *********** FUNCTIONS *********** */

static void eevee_engine_init(void *ved)
{
  EEVEE_Data *vedata = (EEVEE_Data *)ved;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  const DRWContextState *draw_ctx = DRW_context_state_get();
  View3D *v3d = draw_ctx->v3d;
  RegionView3D *rv3d = draw_ctx->rv3d;
  Object *camera = (rv3d->persp == RV3D_CAMOB) ? v3d->camera : NULL;

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = MEM_callocN(sizeof(*stl->g_data), __func__);
  }
  stl->g_data->use_color_render_settings = USE_SCENE_LIGHT(v3d) ||
                                           !LOOK_DEV_STUDIO_LIGHT_ENABLED(v3d);
  stl->g_data->background_alpha = DRW_state_draw_background() ? 1.0f : 0.0f;
  stl->g_data->valid_double_buffer = (txl->color_double_buffer != NULL);
  stl->g_data->valid_taa_history = (txl->taa_history != NULL);
  stl->g_data->queued_shaders_count = 0;

  /* Main Buffer */
  DRW_texture_ensure_fullscreen_2d(&txl->color, GPU_RGBA16F, DRW_TEX_FILTER | DRW_TEX_MIPMAP);

  GPU_framebuffer_ensure_config(&fbl->main_fb,
                                {GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                 GPU_ATTACHMENT_TEXTURE(txl->color),
                                 GPU_ATTACHMENT_LEAVE,
                                 GPU_ATTACHMENT_LEAVE,
                                 GPU_ATTACHMENT_LEAVE,
                                 GPU_ATTACHMENT_LEAVE});

  GPU_framebuffer_ensure_config(&fbl->main_color_fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->color)});

  if (sldata->common_ubo == NULL) {
    sldata->common_ubo = DRW_uniformbuffer_create(sizeof(sldata->common_data),
                                                  &sldata->common_data);
  }

  /* `EEVEE_renderpasses_init` will set the active render passes used by `EEVEE_effects_init`.
   * `EEVEE_effects_init` needs to go second for TAA. */
  EEVEE_renderpasses_init(vedata);
  EEVEE_effects_init(sldata, vedata, camera, false);
  EEVEE_materials_init(sldata, vedata, stl, fbl);
  EEVEE_shadows_init(sldata);
  EEVEE_lightprobes_init(sldata, vedata);
}

static void eevee_cache_init(void *vedata)
{
  EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();

  EEVEE_bloom_cache_init(sldata, vedata);
  EEVEE_depth_of_field_cache_init(sldata, vedata);
  EEVEE_effects_cache_init(sldata, vedata);
  EEVEE_lightprobes_cache_init(sldata, vedata);
  EEVEE_lights_cache_init(sldata, vedata);
  EEVEE_materials_cache_init(sldata, vedata);
  EEVEE_motion_blur_cache_init(sldata, vedata);
  EEVEE_occlusion_cache_init(sldata, vedata);
  EEVEE_screen_raytrace_cache_init(sldata, vedata);
  EEVEE_subsurface_cache_init(sldata, vedata);
  EEVEE_temporal_sampling_cache_init(sldata, vedata);
  EEVEE_volumes_cache_init(sldata, vedata);
}

void EEVEE_cache_populate(void *vedata, Object *ob)
{
  EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const int ob_visibility = DRW_object_visibility_in_active_context(ob);
  bool cast_shadow = false;

  if (ob_visibility & OB_VISIBLE_PARTICLES) {
    EEVEE_particle_hair_cache_populate(vedata, sldata, ob, &cast_shadow);
  }

  if (DRW_object_is_renderable(ob) && (ob_visibility & OB_VISIBLE_SELF)) {
    if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL)) {
      EEVEE_materials_cache_populate(vedata, sldata, ob, &cast_shadow);
    }
    else if (ob->type == OB_HAIR) {
      EEVEE_object_hair_cache_populate(vedata, sldata, ob, &cast_shadow);
    }
    else if (ob->type == OB_VOLUME) {
      EEVEE_volumes_cache_object_add(sldata, vedata, draw_ctx->scene, ob);
    }
    else if (!USE_SCENE_LIGHT(draw_ctx->v3d)) {
      /* do not add any scene light sources to the cache */
    }
    else if (ob->type == OB_LIGHTPROBE) {
      if ((ob->base_flag & BASE_FROM_DUPLI) != 0) {
        /* TODO: Special case for dupli objects because we cannot save the object pointer. */
      }
      else {
        EEVEE_lightprobes_cache_add(sldata, vedata, ob);
      }
    }
    else if (ob->type == OB_LAMP) {
      EEVEE_lights_cache_add(sldata, ob);
    }
  }

  if (cast_shadow) {
    EEVEE_shadows_caster_register(sldata, ob);
  }
}

static void eevee_cache_finish(void *vedata)
{
  EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  EEVEE_PrivateData *g_data = stl->g_data;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  EEVEE_volumes_cache_finish(sldata, vedata);
  EEVEE_materials_cache_finish(sldata, vedata);
  EEVEE_lights_cache_finish(sldata, vedata);
  EEVEE_lightprobes_cache_finish(sldata, vedata);

  EEVEE_subsurface_draw_init(sldata, vedata);
  EEVEE_effects_draw_init(sldata, vedata);
  EEVEE_volumes_draw_init(sldata, vedata);

  uint tot_samples = scene_eval->eevee.taa_render_samples;
  if (tot_samples == 0) {
    /* Use a high number of samples so the outputs accumulation buffers
     * will have the highest possible precision. */
    tot_samples = 1024;
  }
  EEVEE_renderpasses_output_init(sldata, vedata, tot_samples);

  /* Restart taa if a shader has finish compiling. */
  /* HACK We should use notification of some sort from the compilation job instead. */
  if (g_data->queued_shaders_count != g_data->queued_shaders_count_prev) {
    g_data->queued_shaders_count_prev = g_data->queued_shaders_count;
    EEVEE_temporal_sampling_reset(vedata);
  }
}

/* As renders in an HDR offscreen buffer, we need draw everything once
 * during the background pass. This way the other drawing callback between
 * the background and the scene pass are visible.
 * Note: we could break it up in two passes using some depth test
 * to reduce the fillrate */
static void eevee_draw_scene(void *vedata)
{
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  EEVEE_FramebufferList *fbl = ((EEVEE_Data *)vedata)->fbl;
  EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();

  /* Default framebuffer and texture */
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  /* Sort transparents before the loop. */
  DRW_pass_sort_shgroup_z(psl->transparent_pass);

  /* Number of iteration: Use viewport taa_samples when using viewport rendering */
  int loop_len = 1;
  if (DRW_state_is_image_render()) {
    const DRWContextState *draw_ctx = DRW_context_state_get();
    const Scene *scene = draw_ctx->scene;
    loop_len = MAX2(1, scene->eevee.taa_samples);
  }

  while (loop_len--) {
    float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float clear_depth = 1.0f;
    uint clear_stencil = 0x0;
    uint primes[3] = {2, 3, 7};
    double offset[3] = {0.0, 0.0, 0.0};
    double r[3];

    bool taa_use_reprojection = (stl->effects->enabled_effects & EFFECT_TAA_REPROJECT) != 0;

    if (DRW_state_is_image_render() || taa_use_reprojection ||
        ((stl->effects->enabled_effects & EFFECT_TAA) != 0)) {
      int samp = taa_use_reprojection ? stl->effects->taa_reproject_sample + 1 :
                                        stl->effects->taa_current_sample;
      BLI_halton_3d(primes, offset, samp, r);
      EEVEE_update_noise(psl, fbl, r);
      EEVEE_volumes_set_jitter(sldata, samp - 1);
      EEVEE_materials_init(sldata, vedata, stl, fbl);
    }
    /* Copy previous persmat to UBO data */
    copy_m4_m4(sldata->common_data.prev_persmat, stl->effects->prev_persmat);

    /* Refresh Probes
     * Shadows needs to be updated for correct probes */
    DRW_stats_group_start("Probes Refresh");
    EEVEE_shadows_update(sldata, vedata);
    EEVEE_lightprobes_refresh(sldata, vedata);
    EEVEE_lightprobes_refresh_planar(sldata, vedata);
    DRW_stats_group_end();

    /* Refresh shadows */
    DRW_stats_group_start("Shadows");
    EEVEE_shadows_draw(sldata, vedata, stl->effects->taa_view);
    DRW_stats_group_end();

    if (((stl->effects->enabled_effects & EFFECT_TAA) != 0) &&
        (stl->effects->taa_current_sample > 1) && !DRW_state_is_image_render() &&
        !taa_use_reprojection) {
      DRW_view_set_active(stl->effects->taa_view);
    }
    /* when doing viewport rendering the overrides needs to be recalculated for
     * every loop as this normally happens once inside
     * `EEVEE_temporal_sampling_init` */
    else if (((stl->effects->enabled_effects & EFFECT_TAA) != 0) &&
             (stl->effects->taa_current_sample > 1) && DRW_state_is_image_render()) {
      EEVEE_temporal_sampling_update_matrices(vedata);
    }

    /* Set ray type. */
    sldata->common_data.ray_type = EEVEE_RAY_CAMERA;
    sldata->common_data.ray_depth = 0.0f;
    DRW_uniformbuffer_update(sldata->common_ubo, &sldata->common_data);

    GPU_framebuffer_bind(fbl->main_fb);
    eGPUFrameBufferBits clear_bits = GPU_DEPTH_BIT;
    SET_FLAG_FROM_TEST(clear_bits, !DRW_state_draw_background(), GPU_COLOR_BIT);
    SET_FLAG_FROM_TEST(clear_bits, (stl->effects->enabled_effects & EFFECT_SSS), GPU_STENCIL_BIT);
    GPU_framebuffer_clear(fbl->main_fb, clear_bits, clear_col, clear_depth, clear_stencil);

    /* Depth prepass */
    DRW_stats_group_start("Prepass");
    DRW_draw_pass(psl->depth_ps);
    DRW_stats_group_end();

    /* Create minmax texture */
    DRW_stats_group_start("Main MinMax buffer");
    EEVEE_create_minmax_buffer(vedata, dtxl->depth, -1);
    DRW_stats_group_end();

    EEVEE_occlusion_compute(sldata, vedata, dtxl->depth, -1);
    EEVEE_volumes_compute(sldata, vedata);

    /* Shading pass */
    DRW_stats_group_start("Shading");
    if (DRW_state_draw_background()) {
      DRW_draw_pass(psl->background_ps);
    }
    DRW_draw_pass(psl->material_ps);
    EEVEE_subsurface_data_render(sldata, vedata);
    DRW_stats_group_end();

    /* Effects pre-transparency */
    EEVEE_subsurface_compute(sldata, vedata);
    EEVEE_reflection_compute(sldata, vedata);
    EEVEE_occlusion_draw_debug(sldata, vedata);
    if (psl->probe_display) {
      DRW_draw_pass(psl->probe_display);
    }
    EEVEE_refraction_compute(sldata, vedata);

    /* Opaque refraction */
    DRW_stats_group_start("Opaque Refraction");
    DRW_draw_pass(psl->depth_refract_ps);
    DRW_draw_pass(psl->material_refract_ps);
    DRW_stats_group_end();

    /* Volumetrics Resolve Opaque */
    EEVEE_volumes_resolve(sldata, vedata);

    /* Renderpasses */
    EEVEE_renderpasses_output_accumulate(sldata, vedata, false);

    /* Transparent */
    /* TODO(fclem): should be its own Framebuffer.
     * This is needed because dualsource blending only works with 1 color buffer. */
    GPU_framebuffer_texture_attach(fbl->main_color_fb, dtxl->depth, 0, 0);
    GPU_framebuffer_bind(fbl->main_color_fb);
    DRW_draw_pass(psl->transparent_pass);
    GPU_framebuffer_bind(fbl->main_fb);
    GPU_framebuffer_texture_detach(fbl->main_color_fb, dtxl->depth);

    /* Post Process */
    DRW_stats_group_start("Post FX");
    EEVEE_draw_effects(sldata, vedata);
    DRW_stats_group_end();

    DRW_view_set_active(NULL);

    if (DRW_state_is_image_render() && (stl->effects->enabled_effects & EFFECT_SSR) &&
        !stl->effects->ssr_was_valid_double_buffer) {
      /* SSR needs one iteration to start properly. */
      loop_len++;
      /* Reset sampling (and accumulation) after the first sample to avoid
       * washed out first bounce for SSR. */
      EEVEE_temporal_sampling_reset(vedata);
      stl->effects->ssr_was_valid_double_buffer = stl->g_data->valid_double_buffer;
    }
  }

  if ((stl->g_data->render_passes & EEVEE_RENDER_PASS_COMBINED) != 0) {
    /* Transfer result to default framebuffer. */
    GPU_framebuffer_bind(dfbl->default_fb);
    DRW_transform_none(stl->effects->final_tx);
  }
  else {
    EEVEE_renderpasses_draw(sldata, vedata);
  }

  EEVEE_renderpasses_draw_debug(vedata);

  EEVEE_volumes_free_smoke_textures();

  stl->g_data->view_updated = false;
}

static void eevee_view_update(void *vedata)
{
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  if (stl->g_data) {
    stl->g_data->view_updated = true;
  }
}

static void eevee_id_object_update(void *UNUSED(vedata), Object *object)
{
  EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_get(object);
  if (ped != NULL && ped->dd.recalc != 0) {
    ped->need_update = (ped->dd.recalc & ID_RECALC_TRANSFORM) != 0;
    ped->dd.recalc = 0;
  }
  EEVEE_LightEngineData *led = EEVEE_light_data_get(object);
  if (led != NULL && led->dd.recalc != 0) {
    led->need_update = true;
    led->dd.recalc = 0;
  }
  EEVEE_ObjectEngineData *oedata = EEVEE_object_data_get(object);
  if (oedata != NULL && oedata->dd.recalc != 0) {
    oedata->need_update = true;
    oedata->geom_update = (oedata->dd.recalc & (ID_RECALC_GEOMETRY)) != 0;
    oedata->dd.recalc = 0;
  }
}

static void eevee_id_world_update(void *vedata, World *wo)
{
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  LightCache *lcache = stl->g_data->light_cache;

  EEVEE_WorldEngineData *wedata = EEVEE_world_data_ensure(wo);

  if (wedata != NULL && wedata->dd.recalc != 0) {
    if ((lcache->flag & LIGHTCACHE_BAKING) == 0) {
      lcache->flag |= LIGHTCACHE_UPDATE_WORLD;
    }
    wedata->dd.recalc = 0;
  }
}

void eevee_id_update(void *vedata, ID *id)
{
  /* Handle updates based on ID type. */
  switch (GS(id->name)) {
    case ID_WO:
      eevee_id_world_update(vedata, (World *)id);
      break;
    case ID_OB:
      eevee_id_object_update(vedata, (Object *)id);
      break;
    default:
      /* pass */
      break;
  }
}

static void eevee_render_reset_passes(EEVEE_Data *vedata)
{
  /* Reset passlist. This is safe as they are stored into managed memory chunks. */
  memset(vedata->psl, 0, sizeof(*vedata->psl));
}

static void eevee_render_to_image(void *vedata,
                                  RenderEngine *engine,
                                  struct RenderLayer *render_layer,
                                  const rcti *rect)
{
  EEVEE_Data *ved = (EEVEE_Data *)vedata;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Depsgraph *depsgraph = draw_ctx->depsgraph;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();
  const bool do_motion_blur = (scene->eevee.flag & SCE_EEVEE_MOTION_BLUR_ENABLED) != 0;
  const bool do_motion_blur_fx = do_motion_blur && (scene->eevee.motion_blur_max > 0);

  if (!EEVEE_render_init(vedata, engine, depsgraph)) {
    return;
  }
  EEVEE_PrivateData *g_data = ved->stl->g_data;

  int steps = max_ii(1, scene->eevee.motion_blur_steps);
  int time_steps_tot = (do_motion_blur) ? steps : 1;
  g_data->render_tot_samples = divide_ceil_u(scene->eevee.taa_render_samples, time_steps_tot);
  /* Centered on frame for now. */
  float time = CFRA - scene->eevee.motion_blur_shutter / 2.0f;
  float time_step = scene->eevee.motion_blur_shutter / time_steps_tot;
  for (int i = 0; i < time_steps_tot && !RE_engine_test_break(engine); i++) {
    float time_prev = time;
    float time_curr = time + time_step * 0.5f;
    float time_next = time + time_step;
    time += time_step;

    /* Previous motion step. */
    if (do_motion_blur_fx) {
      if (i > 0) {
        /* The previous step of this iteration N is exactly the next step of iteration N - 1.
         * So we just swap the resources to avoid too much re-evaluation. */
        EEVEE_motion_blur_swap_data(vedata);
      }
      else {
        EEVEE_motion_blur_step_set(ved, MB_PREV);
        RE_engine_frame_set(engine, floorf(time_prev), fractf(time_prev));

        EEVEE_render_view_sync(vedata, engine, depsgraph);
        EEVEE_render_cache_init(sldata, vedata);

        DRW_render_object_iter(vedata, engine, depsgraph, EEVEE_render_cache);

        EEVEE_motion_blur_cache_finish(vedata);
        EEVEE_materials_cache_finish(sldata, vedata);
        eevee_render_reset_passes(vedata);
      }
    }

    /* Next motion step. */
    if (do_motion_blur_fx) {
      EEVEE_motion_blur_step_set(ved, MB_NEXT);
      RE_engine_frame_set(engine, floorf(time_next), fractf(time_next));

      EEVEE_render_view_sync(vedata, engine, depsgraph);
      EEVEE_render_cache_init(sldata, vedata);

      DRW_render_object_iter(vedata, engine, depsgraph, EEVEE_render_cache);

      EEVEE_motion_blur_cache_finish(vedata);
      EEVEE_materials_cache_finish(sldata, vedata);
      eevee_render_reset_passes(vedata);
    }

    /* Current motion step. */
    {
      if (do_motion_blur) {
        EEVEE_motion_blur_step_set(ved, MB_CURR);
        RE_engine_frame_set(engine, floorf(time_curr), fractf(time_curr));
      }

      EEVEE_render_view_sync(vedata, engine, depsgraph);
      EEVEE_render_cache_init(sldata, vedata);

      DRW_render_object_iter(vedata, engine, depsgraph, EEVEE_render_cache);

      EEVEE_motion_blur_cache_finish(vedata);
      EEVEE_volumes_cache_finish(sldata, vedata);
      EEVEE_materials_cache_finish(sldata, vedata);
      EEVEE_lights_cache_finish(sldata, vedata);
      EEVEE_lightprobes_cache_finish(sldata, vedata);

      EEVEE_subsurface_draw_init(sldata, vedata);
      EEVEE_effects_draw_init(sldata, vedata);
      EEVEE_volumes_draw_init(sldata, vedata);
    }

    /* Actual drawing. */
    {
      if (i == 0) {
        EEVEE_renderpasses_output_init(
            sldata, vedata, g_data->render_tot_samples * time_steps_tot);
      }

      EEVEE_temporal_sampling_create_view(vedata);
      EEVEE_render_draw(vedata, engine, render_layer, rect);

      DRW_cache_restart();
    }
  }

  EEVEE_volumes_free_smoke_textures();
  EEVEE_motion_blur_data_free(&ved->stl->effects->motion_blur);

  if (RE_engine_test_break(engine)) {
    return;
  }

  EEVEE_render_read_result(vedata, engine, render_layer, rect);

  /* Restore original viewport size. */
  DRW_render_viewport_size_set((int[2]){g_data->size_orig[0], g_data->size_orig[1]});
}

static void eevee_engine_free(void)
{
  EEVEE_shaders_free();
  EEVEE_bloom_free();
  EEVEE_depth_of_field_free();
  EEVEE_effects_free();
  EEVEE_lightprobes_free();
  EEVEE_shadows_free();
  EEVEE_materials_free();
  EEVEE_mist_free();
  EEVEE_motion_blur_free();
  EEVEE_occlusion_free();
  EEVEE_screen_raytrace_free();
  EEVEE_subsurface_free();
  EEVEE_volumes_free();
  EEVEE_renderpasses_free();
}

static const DrawEngineDataSize eevee_data_size = DRW_VIEWPORT_DATA_SIZE(EEVEE_Data);

DrawEngineType draw_engine_eevee_type = {
    NULL,
    NULL,
    N_("Eevee"),
    &eevee_data_size,
    &eevee_engine_init,
    &eevee_engine_free,
    &eevee_cache_init,
    &EEVEE_cache_populate,
    &eevee_cache_finish,
    &eevee_draw_scene,
    &eevee_view_update,
    &eevee_id_update,
    &eevee_render_to_image,
};

RenderEngineType DRW_engine_viewport_eevee_type = {
    NULL,
    NULL,
    EEVEE_ENGINE,
    N_("Eevee"),
    RE_INTERNAL | RE_USE_PREVIEW | RE_USE_STEREO_VIEWPORT,
    NULL,
    &DRW_render_to_image,
    NULL,
    NULL,
    NULL,
    NULL,
    &EEVEE_render_update_passes,
    &draw_engine_eevee_type,
    {NULL, NULL, NULL},
};

#undef EEVEE_ENGINE

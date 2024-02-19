/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.hh"

#include "draw_color_management.h" /* TODO: remove dependency. */

#include "BLI_rand.h"

#include "BLT_translation.hh"

#include "BKE_object.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_world_types.h"

#include "GPU_context.h"

#include "IMB_imbuf.hh"

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
  Object *camera = (rv3d->persp == RV3D_CAMOB) ? v3d->camera : nullptr;

  if (!stl->g_data) {
    /* Alloc transient pointers */
    stl->g_data = static_cast<EEVEE_PrivateData *>(MEM_callocN(sizeof(*stl->g_data), __func__));
  }
  stl->g_data->use_color_render_settings = USE_SCENE_LIGHT(v3d) ||
                                           !LOOK_DEV_STUDIO_LIGHT_ENABLED(v3d);
  stl->g_data->background_alpha = DRW_state_draw_background() ? 1.0f : 0.0f;
  stl->g_data->valid_double_buffer = (txl->color_double_buffer != nullptr);
  stl->g_data->valid_taa_history = (txl->taa_history != nullptr);
  stl->g_data->queued_shaders_count = 0;
  stl->g_data->queued_optimise_shaders_count = 0;
  stl->g_data->render_timesteps = 1;
  stl->g_data->disable_ligthprobes = v3d &&
                                     (v3d->object_type_exclude_viewport & (1 << OB_LIGHTPROBE));

  /* Main Buffer */
  DRW_texture_ensure_fullscreen_2d(&txl->color, GPU_RGBA16F, DRW_TEX_FILTER);

  GPU_framebuffer_ensure_config(&fbl->main_fb,
                                {GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                 GPU_ATTACHMENT_TEXTURE(txl->color),
                                 GPU_ATTACHMENT_LEAVE,
                                 GPU_ATTACHMENT_LEAVE,
                                 GPU_ATTACHMENT_LEAVE,
                                 GPU_ATTACHMENT_LEAVE});

  GPU_framebuffer_ensure_config(&fbl->main_color_fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->color)});

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

  EEVEE_bloom_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_depth_of_field_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_effects_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_lightprobes_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_lights_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_materials_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_motion_blur_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_occlusion_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_screen_raytrace_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_subsurface_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_temporal_sampling_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_volumes_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));
}

void EEVEE_cache_populate(void *vedata, Object *ob)
{
  EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const int ob_visibility = DRW_object_visibility_in_active_context(ob);
  bool cast_shadow = false;

  if (ob_visibility & OB_VISIBLE_PARTICLES) {
    EEVEE_particle_hair_cache_populate(
        static_cast<EEVEE_Data *>(vedata), sldata, ob, &cast_shadow);
  }

  if (DRW_object_is_renderable(ob) && (ob_visibility & OB_VISIBLE_SELF)) {
    if (ob->type == OB_MESH) {
      EEVEE_materials_cache_populate(static_cast<EEVEE_Data *>(vedata), sldata, ob, &cast_shadow);
    }
    else if (ob->type == OB_CURVES) {
      EEVEE_object_curves_cache_populate(
          static_cast<EEVEE_Data *>(vedata), sldata, ob, &cast_shadow);
    }
    else if (ob->type == OB_VOLUME) {
      EEVEE_volumes_cache_object_add(
          sldata, static_cast<EEVEE_Data *>(vedata), draw_ctx->scene, ob);
    }
    else if (!USE_SCENE_LIGHT(draw_ctx->v3d)) {
      /* do not add any scene light sources to the cache */
    }
    else if (ob->type == OB_LIGHTPROBE) {
      if ((ob->base_flag & BASE_FROM_DUPLI) != 0) {
        /* TODO: Special case for dupli objects because we cannot save the object pointer. */
      }
      else {
        EEVEE_lightprobes_cache_add(sldata, static_cast<EEVEE_Data *>(vedata), ob);
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
  EEVEE_Data *ved = (EEVEE_Data *)vedata;
  EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();
  EEVEE_StorageList *stl = ved->stl;
  EEVEE_PrivateData *g_data = stl->g_data;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  EEVEE_volumes_cache_finish(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_materials_cache_finish(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_lights_cache_finish(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_lightprobes_cache_finish(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_renderpasses_cache_finish(sldata, static_cast<EEVEE_Data *>(vedata));

  EEVEE_subsurface_draw_init(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_effects_draw_init(sldata, static_cast<EEVEE_Data *>(vedata));
  EEVEE_volumes_draw_init(sldata, static_cast<EEVEE_Data *>(vedata));

  uint tot_samples = scene_eval->eevee.taa_render_samples;
  if (tot_samples == 0) {
    /* Use a high number of samples so the outputs accumulation buffers
     * will have the highest possible precision. */
    tot_samples = 1024;
  }
  EEVEE_renderpasses_output_init(sldata, static_cast<EEVEE_Data *>(vedata), tot_samples);

  /* Restart TAA if a shader has finish compiling. */
  /* HACK: We should use notification of some sort from the compilation job instead. */
  if (g_data->queued_shaders_count != g_data->queued_shaders_count_prev) {
    g_data->queued_shaders_count_prev = g_data->queued_shaders_count;
    EEVEE_temporal_sampling_reset(static_cast<EEVEE_Data *>(vedata));
  }

  if (g_data->queued_shaders_count > 0) {
    SNPRINTF(ved->info, RPT_("Compiling Shaders (%d remaining)"), g_data->queued_shaders_count);
  }
  else if (g_data->queued_optimise_shaders_count > 0) {
    SNPRINTF(ved->info,
             RPT_("Optimizing Shaders (%d remaining)"),
             g_data->queued_optimise_shaders_count);
  }
}

/* As renders in an HDR off-screen buffer, we need draw everything once
 * during the background pass. This way the other drawing callback between
 * the background and the scene pass are visible.
 * NOTE: we could break it up in two passes using some depth test
 * to reduce the fill-rate. */
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
    loop_len = std::max(1, scene->eevee.taa_samples);
  }

  if (stl->effects->bypass_drawing) {
    loop_len = 0;
  }

  while (loop_len--) {
    const float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float clear_depth = 1.0f;
    uint clear_stencil = 0x0;
    const uint primes[3] = {2, 3, 7};
    double offset[3] = {0.0, 0.0, 0.0};
    double r[3];

    bool taa_use_reprojection = (stl->effects->enabled_effects & EFFECT_TAA_REPROJECT) != 0;

    if (DRW_state_is_image_render() || taa_use_reprojection ||
        ((stl->effects->enabled_effects & EFFECT_TAA) != 0))
    {
      int samp = taa_use_reprojection ? stl->effects->taa_reproject_sample + 1 :
                                        stl->effects->taa_current_sample;
      BLI_halton_3d(primes, offset, samp, r);
      EEVEE_update_noise(psl, fbl, r);
      EEVEE_volumes_set_jitter(sldata, samp - 1);
      EEVEE_materials_init(sldata, static_cast<EEVEE_Data *>(vedata), stl, fbl);
    }
    /* Copy previous persmat to UBO data */
    copy_m4_m4(sldata->common_data.prev_persmat, stl->effects->prev_persmat);

    /* Refresh Probes
     * Shadows needs to be updated for correct probes */
    DRW_stats_group_start("Probes Refresh");
    EEVEE_shadows_update(sldata, static_cast<EEVEE_Data *>(vedata));
    EEVEE_lightprobes_refresh(sldata, static_cast<EEVEE_Data *>(vedata));
    EEVEE_lightprobes_refresh_planar(sldata, static_cast<EEVEE_Data *>(vedata));
    DRW_stats_group_end();

    /* Refresh shadows */
    DRW_stats_group_start("Shadows");
    EEVEE_shadows_draw(sldata, static_cast<EEVEE_Data *>(vedata), stl->effects->taa_view);
    DRW_stats_group_end();

    if (((stl->effects->enabled_effects & EFFECT_TAA) != 0) &&
        (stl->effects->taa_current_sample > 1) && !DRW_state_is_image_render() &&
        !taa_use_reprojection)
    {
      DRW_view_set_active(stl->effects->taa_view);
    }
    /* when doing viewport rendering the overrides needs to be recalculated for
     * every loop as this normally happens once inside
     * `EEVEE_temporal_sampling_init` */
    else if (((stl->effects->enabled_effects & EFFECT_TAA) != 0) &&
             (stl->effects->taa_current_sample > 1) && DRW_state_is_image_render())
    {
      EEVEE_temporal_sampling_update_matrices(static_cast<EEVEE_Data *>(vedata));
    }

    /* Set ray type. */
    sldata->common_data.ray_type = EEVEE_RAY_CAMERA;
    sldata->common_data.ray_depth = 0.0f;
    if (stl->g_data->disable_ligthprobes) {
      sldata->common_data.prb_num_render_cube = 1;
      sldata->common_data.prb_num_render_grid = 1;
    }
    GPU_uniformbuf_update(sldata->common_ubo, &sldata->common_data);

    GPU_framebuffer_bind(fbl->main_fb);
    eGPUFrameBufferBits clear_bits = GPU_DEPTH_BIT;
    SET_FLAG_FROM_TEST(clear_bits, !DRW_state_draw_background(), GPU_COLOR_BIT);
    SET_FLAG_FROM_TEST(clear_bits, (stl->effects->enabled_effects & EFFECT_SSS), GPU_STENCIL_BIT);
    GPU_framebuffer_clear(fbl->main_fb, clear_bits, clear_col, clear_depth, clear_stencil);

    /* Depth pre-pass. */
    DRW_stats_group_start("Prepass");
    DRW_draw_pass(psl->depth_ps);
    DRW_stats_group_end();

    /* Create minmax texture */
    DRW_stats_group_start("Main MinMax buffer");
    EEVEE_create_minmax_buffer(static_cast<EEVEE_Data *>(vedata), dtxl->depth, -1);
    DRW_stats_group_end();

    EEVEE_occlusion_compute(sldata, static_cast<EEVEE_Data *>(vedata));
    EEVEE_volumes_compute(sldata, static_cast<EEVEE_Data *>(vedata));

    /* Shading pass */
    DRW_stats_group_start("Shading");
    if (DRW_state_draw_background()) {
      DRW_draw_pass(psl->background_ps);
    }
    DRW_draw_pass(psl->material_ps);
    EEVEE_subsurface_data_render(sldata, static_cast<EEVEE_Data *>(vedata));
    DRW_stats_group_end();

    /* Effects pre-transparency */
    EEVEE_subsurface_compute(sldata, static_cast<EEVEE_Data *>(vedata));
    EEVEE_reflection_compute(sldata, static_cast<EEVEE_Data *>(vedata));
    EEVEE_occlusion_draw_debug(sldata, static_cast<EEVEE_Data *>(vedata));
    if (psl->probe_display) {
      DRW_draw_pass(psl->probe_display);
    }
    EEVEE_refraction_compute(sldata, static_cast<EEVEE_Data *>(vedata));

    /* Opaque refraction */
    DRW_stats_group_start("Opaque Refraction");
    DRW_draw_pass(psl->depth_refract_ps);
    DRW_draw_pass(psl->material_refract_ps);
    DRW_stats_group_end();

    /* Volumetrics Resolve Opaque */
    EEVEE_volumes_resolve(sldata, static_cast<EEVEE_Data *>(vedata));

    /* Render-passes. */
    EEVEE_renderpasses_output_accumulate(sldata, static_cast<EEVEE_Data *>(vedata), false);

    /* Transparent */
    EEVEE_material_transparent_output_accumulate(static_cast<EEVEE_Data *>(vedata));
    /* TODO(@fclem): should be its own Frame-buffer.
     * This is needed because dual-source blending only works with 1 color buffer. */
    GPU_framebuffer_texture_attach(fbl->main_color_fb, dtxl->depth, 0, 0);
    GPU_framebuffer_bind(fbl->main_color_fb);
    DRW_draw_pass(psl->transparent_pass);
    GPU_framebuffer_bind(fbl->main_fb);
    GPU_framebuffer_texture_detach(fbl->main_color_fb, dtxl->depth);

    /* Post Process */
    DRW_stats_group_start("Post FX");
    EEVEE_draw_effects(sldata, static_cast<EEVEE_Data *>(vedata));
    DRW_stats_group_end();

    DRW_view_set_active(nullptr);

    if (DRW_state_is_image_render() && (stl->effects->enabled_effects & EFFECT_SSR) &&
        !stl->effects->ssr_was_valid_double_buffer)
    {
      /* SSR needs one iteration to start properly. */
      loop_len++;
      /* Reset sampling (and accumulation) after the first sample to avoid
       * washed out first bounce for SSR. */
      EEVEE_temporal_sampling_reset(static_cast<EEVEE_Data *>(vedata));
      stl->effects->ssr_was_valid_double_buffer = stl->g_data->valid_double_buffer;
    }

    /* Perform render step between samples to allow flushing of freed temporary GPUBackend
     * resources. This prevents the GPU backend accumulating a high amount of in-flight memory when
     * performing renders using eevee_draw_scene. e.g. During file thumbnail generation. */
    if (loop_len > 2) {
      if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
        GPU_flush();
        GPU_render_step();
      }
    }
  }

  if ((stl->g_data->render_passes & EEVEE_RENDER_PASS_COMBINED) != 0) {
    /* Transfer result to default framebuffer. */
    GPU_framebuffer_bind(dfbl->default_fb);
    DRW_transform_none(stl->effects->final_tx);
  }
  else {
    EEVEE_renderpasses_draw(sldata, static_cast<EEVEE_Data *>(vedata));
  }

  if (stl->effects->bypass_drawing) {
    /* Restore the depth from sample 1. */
    GPU_framebuffer_blit(fbl->double_buffer_depth_fb, 0, dfbl->default_fb, 0, GPU_DEPTH_BIT);
  }

  EEVEE_renderpasses_draw_debug(static_cast<EEVEE_Data *>(vedata));

  stl->g_data->view_updated = false;

  DRW_view_set_active(nullptr);
}

static void eevee_view_update(void *vedata)
{
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  if (stl && stl->g_data) {
    stl->g_data->view_updated = true;
  }
}

static void eevee_id_object_update(void * /*vedata*/, Object *object)
{
  EEVEE_LightProbeEngineData *ped = EEVEE_lightprobe_data_get(object);
  if (ped != nullptr && ped->dd.recalc != 0) {
    ped->need_update = (ped->dd.recalc & ID_RECALC_TRANSFORM) != 0;
    ped->dd.recalc = 0;
  }
  EEVEE_LightEngineData *led = EEVEE_light_data_get(object);
  if (led != nullptr && led->dd.recalc != 0) {
    led->need_update = true;
    led->dd.recalc = 0;
  }
  EEVEE_ObjectEngineData *oedata = EEVEE_object_data_get(object);
  if (oedata != nullptr && oedata->dd.recalc != 0) {
    oedata->need_update = true;
    oedata->geom_update = (oedata->dd.recalc & (ID_RECALC_GEOMETRY)) != 0;
    oedata->dd.recalc = 0;
  }
}

static void eevee_id_world_update(void *vedata, World *wo)
{
  EEVEE_StorageList *stl = ((EEVEE_Data *)vedata)->stl;
  LightCache *lcache = stl->g_data->light_cache;

  if (ELEM(lcache, nullptr, stl->lookdev_lightcache)) {
    /* Avoid Lookdev viewport clearing the update flag (see #67741). */
    return;
  }

  EEVEE_WorldEngineData *wedata = EEVEE_world_data_ensure(wo);

  if (wedata != nullptr && wedata->dd.recalc != 0) {
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
  /* Reset pass-list. This is safe as they are stored into managed memory chunks. */
  memset(vedata->psl, 0, sizeof(*vedata->psl));
}

static void eevee_render_to_image(void *vedata,
                                  RenderEngine *engine,
                                  RenderLayer *render_layer,
                                  const rcti *rect)
{
  EEVEE_Data *ved = (EEVEE_Data *)vedata;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Depsgraph *depsgraph = draw_ctx->depsgraph;
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();
  const bool do_motion_blur = (scene->r.mode & R_MBLUR) != 0;
  const bool do_motion_blur_fx = do_motion_blur && (scene->eevee.motion_blur_max > 0);

  if (!EEVEE_render_init(static_cast<EEVEE_Data *>(vedata), engine, depsgraph)) {
    return;
  }
  EEVEE_PrivateData *g_data = ved->stl->g_data;

  int initial_frame = scene->r.cfra;
  float initial_subframe = scene->r.subframe;
  float shuttertime = (do_motion_blur) ? scene->r.motion_blur_shutter : 0.0f;
  int time_steps_tot = (do_motion_blur) ? max_ii(1, scene->eevee.motion_blur_steps) : 1;
  g_data->render_timesteps = time_steps_tot;

  EEVEE_render_modules_init(static_cast<EEVEE_Data *>(vedata), engine, depsgraph);

  g_data->render_sample_count_per_timestep = EEVEE_temporal_sampling_sample_count_get(scene,
                                                                                      ved->stl);

  /* Reset in case the same engine is used on multiple views. */
  EEVEE_temporal_sampling_reset(static_cast<EEVEE_Data *>(vedata));

  /* Compute start time. The motion blur will cover `[time ...time + shuttertime]`. */
  float time = initial_frame + initial_subframe;
  switch (scene->r.motion_blur_position) {
    case SCE_MB_START:
      /* No offset. */
      break;
    case SCE_MB_CENTER:
      time -= shuttertime * 0.5f;
      break;
    case SCE_MB_END:
      time -= shuttertime;
      break;
    default:
      BLI_assert_msg(0, "Invalid motion blur position enum!");
      break;
  }

  float time_step = shuttertime / time_steps_tot;
  for (int i = 0; i < time_steps_tot && !RE_engine_test_break(engine); i++) {
    float time_prev = time;
    float time_curr = time + time_step * 0.5f;
    float time_next = time + time_step;
    time += time_step;

    /* Previous motion step. */
    if (do_motion_blur_fx) {
      if (i == 0) {
        EEVEE_motion_blur_step_set(ved, MB_PREV);
        DRW_render_set_time(engine, depsgraph, floorf(time_prev), fractf(time_prev));
        EEVEE_render_modules_init(static_cast<EEVEE_Data *>(vedata), engine, depsgraph);
        sldata = EEVEE_view_layer_data_ensure();

        EEVEE_render_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));

        DRW_render_object_iter(vedata, engine, depsgraph, EEVEE_render_cache);

        EEVEE_motion_blur_cache_finish(static_cast<EEVEE_Data *>(vedata));
        EEVEE_materials_cache_finish(sldata, static_cast<EEVEE_Data *>(vedata));
        eevee_render_reset_passes(static_cast<EEVEE_Data *>(vedata));
      }
    }

    /* Next motion step. */
    if (do_motion_blur_fx) {
      EEVEE_motion_blur_step_set(ved, MB_NEXT);
      DRW_render_set_time(engine, depsgraph, floorf(time_next), fractf(time_next));
      EEVEE_render_modules_init(static_cast<EEVEE_Data *>(vedata), engine, depsgraph);
      sldata = EEVEE_view_layer_data_ensure();

      EEVEE_render_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));

      DRW_render_object_iter(vedata, engine, depsgraph, EEVEE_render_cache);

      EEVEE_motion_blur_cache_finish(static_cast<EEVEE_Data *>(vedata));
      EEVEE_materials_cache_finish(sldata, static_cast<EEVEE_Data *>(vedata));
      eevee_render_reset_passes(static_cast<EEVEE_Data *>(vedata));
    }

    /* Current motion step. */
    {
      if (do_motion_blur) {
        EEVEE_motion_blur_step_set(ved, MB_CURR);
        DRW_render_set_time(engine, depsgraph, floorf(time_curr), fractf(time_curr));
        EEVEE_render_modules_init(static_cast<EEVEE_Data *>(vedata), engine, depsgraph);
        sldata = EEVEE_view_layer_data_ensure();
      }

      EEVEE_render_cache_init(sldata, static_cast<EEVEE_Data *>(vedata));

      DRW_render_object_iter(vedata, engine, depsgraph, EEVEE_render_cache);

      EEVEE_motion_blur_cache_finish(static_cast<EEVEE_Data *>(vedata));
      EEVEE_volumes_cache_finish(sldata, static_cast<EEVEE_Data *>(vedata));
      EEVEE_materials_cache_finish(sldata, static_cast<EEVEE_Data *>(vedata));
      EEVEE_lights_cache_finish(sldata, static_cast<EEVEE_Data *>(vedata));
      EEVEE_lightprobes_cache_finish(sldata, static_cast<EEVEE_Data *>(vedata));
      EEVEE_renderpasses_cache_finish(sldata, static_cast<EEVEE_Data *>(vedata));

      EEVEE_subsurface_draw_init(sldata, static_cast<EEVEE_Data *>(vedata));
      EEVEE_effects_draw_init(sldata, static_cast<EEVEE_Data *>(vedata));
      EEVEE_volumes_draw_init(sldata, static_cast<EEVEE_Data *>(vedata));
    }

    /* Actual drawing. */
    {
      EEVEE_renderpasses_output_init(sldata,
                                     static_cast<EEVEE_Data *>(vedata),
                                     g_data->render_sample_count_per_timestep * time_steps_tot);

      if (scene->world) {
        /* Update world in case of animated world material. */
        eevee_id_world_update(vedata, scene->world);
      }

      EEVEE_temporal_sampling_create_view(static_cast<EEVEE_Data *>(vedata));
      EEVEE_render_draw(static_cast<EEVEE_Data *>(vedata), engine, render_layer, rect);

      if (i < time_steps_tot - 1) {
        /* Don't reset after the last loop. Since EEVEE_render_read_result
         * might need some DRWPasses. */
        DRW_cache_restart();
      }
    }

    if (do_motion_blur_fx) {
      /* The previous step of next iteration N is exactly the next step of this iteration N - 1.
       * So we just swap the resources to avoid too much re-evaluation.
       * Note that this also clears the VBO references from the GPUBatches of deformed
       * geometries. */
      EEVEE_motion_blur_swap_data(static_cast<EEVEE_Data *>(vedata));
    }
  }

  EEVEE_motion_blur_data_free(&ved->stl->effects->motion_blur);

  if (RE_engine_test_break(engine)) {
    return;
  }

  EEVEE_render_read_result(static_cast<EEVEE_Data *>(vedata), engine, render_layer, rect);

  /* Restore original viewport size. */
  int viewport_size[2] = {int(g_data->size_orig[0]), int(g_data->size_orig[1])};
  DRW_render_viewport_size_set(viewport_size);

  if (scene->r.cfra != initial_frame || scene->r.subframe != initial_subframe) {
    /* Restore original frame number. This is because the render pipeline expects it. */
    RE_engine_frame_set(engine, initial_frame, initial_subframe);
  }
}

static void eevee_store_metadata(void *vedata, RenderResult *render_result)
{
  EEVEE_Data *ved = (EEVEE_Data *)vedata;
  EEVEE_PrivateData *g_data = ved->stl->g_data;
  if (g_data->render_passes & EEVEE_RENDER_PASS_CRYPTOMATTE) {
    EEVEE_cryptomatte_store_metadata(ved, render_result);
    EEVEE_cryptomatte_free(ved);
  }
}

static void eevee_engine_free()
{
  EEVEE_shaders_free();
  EEVEE_lightprobes_free();
  EEVEE_materials_free();
  EEVEE_occlusion_free();
  EEVEE_volumes_free();
}

static const DrawEngineDataSize eevee_data_size = DRW_VIEWPORT_DATA_SIZE(EEVEE_Data);

DrawEngineType draw_engine_eevee_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("EEVEE"),
    /*vedata_size*/ &eevee_data_size,
    /*engine_init*/ &eevee_engine_init,
    /*engine_free*/ &eevee_engine_free,
    /*instance_free*/ nullptr,
    /*cache_init*/ &eevee_cache_init,
    /*cache_populate*/ &EEVEE_cache_populate,
    /*cache_finish*/ &eevee_cache_finish,
    /*draw_scene*/ &eevee_draw_scene,
    /*view_update*/ &eevee_view_update,
    /*id_update*/ &eevee_id_update,
    /*render_to_image*/ &eevee_render_to_image,
    /*store_metadata*/ &eevee_store_metadata,
};

RenderEngineType DRW_engine_viewport_eevee_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ EEVEE_ENGINE,
    /*name*/ N_("EEVEE"),
    /*flag*/ RE_INTERNAL | RE_USE_PREVIEW | RE_USE_STEREO_VIEWPORT | RE_USE_GPU_CONTEXT,
    /*update*/ nullptr,
    /*render*/ &DRW_render_to_image,
    /*render_frame_finish*/ nullptr,
    /*draw*/ nullptr,
    /*bake*/ nullptr,
    /*view_update*/ nullptr,
    /*view_draw*/ nullptr,
    /*update_script_node*/ nullptr,
    /*update_render_passes*/ &EEVEE_render_update_passes,
    /*draw_engine*/ &draw_engine_eevee_type,
    /*rna_ext*/
    {
        /*data*/ nullptr,
        /*srna*/ nullptr,
        /*call*/ nullptr,
    },
};

#undef EEVEE_ENGINE

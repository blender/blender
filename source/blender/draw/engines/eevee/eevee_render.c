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

/**
 * Render functions for final render outputs.
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_node_types.h"
#include "DNA_object_types.h"

#include "BKE_object.h"

#include "BLI_rand.h"
#include "BLI_rect.h"

#include "DEG_depsgraph_query.h"

#include "GPU_framebuffer.h"
#include "GPU_state.h"

#include "RE_pipeline.h"

#include "eevee_private.h"

void EEVEE_render_init(EEVEE_Data *ved, RenderEngine *engine, struct Depsgraph *depsgraph)
{
  EEVEE_Data *vedata = (EEVEE_Data *)ved;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();
  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  const float *size_orig = DRW_viewport_size_get();
  float size_final[2];
  float camtexcofac[4];

  /* Init default FB and render targets:
   * In render mode the default framebuffer is not generated
   * because there is no viewport. So we need to manually create it or
   * not use it. For code clarity we just allocate it make use of it. */
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  /* Alloc transient data. */
  if (!stl->g_data) {
    stl->g_data = MEM_callocN(sizeof(*stl->g_data), __func__);
  }
  EEVEE_PrivateData *g_data = stl->g_data;
  g_data->background_alpha = DRW_state_draw_background() ? 1.0f : 0.0f;
  g_data->valid_double_buffer = 0;
  copy_v2_v2(g_data->size_orig, size_orig);

  if (scene->eevee.flag & SCE_EEVEE_OVERSCAN) {
    g_data->overscan = scene->eevee.overscan / 100.0f;
    g_data->overscan_pixels = roundf(max_ff(size_orig[0], size_orig[1]) * g_data->overscan);

    madd_v2_v2v2fl(size_final, size_orig, (float[2]){2.0f, 2.0f}, g_data->overscan_pixels);

    camtexcofac[0] = size_final[0] / size_orig[0];
    camtexcofac[1] = size_final[1] / size_orig[1];

    camtexcofac[2] = -camtexcofac[0] * g_data->overscan_pixels / size_final[0];
    camtexcofac[3] = -camtexcofac[1] * g_data->overscan_pixels / size_final[1];
  }
  else {
    copy_v2_v2(size_final, size_orig);
    g_data->overscan = 0.0f;
    g_data->overscan_pixels = 0.0f;
    copy_v4_fl4(camtexcofac, 1.0f, 1.0f, 0.0f, 0.0f);
  }

  /* XXX overriding viewport size. Simplify things but is not really 100% safe. */
  DRW_render_viewport_size_set((int[2]){size_orig[0] + g_data->overscan_pixels * 2.0f,
                                        size_orig[1] + g_data->overscan_pixels * 2.0f});

  /* TODO 32 bit depth */
  DRW_texture_ensure_fullscreen_2d(&dtxl->depth, GPU_DEPTH24_STENCIL8, 0);
  DRW_texture_ensure_fullscreen_2d(&txl->color, GPU_RGBA32F, DRW_TEX_FILTER | DRW_TEX_MIPMAP);

  GPU_framebuffer_ensure_config(
      &dfbl->default_fb,
      {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_TEXTURE(txl->color)});
  GPU_framebuffer_ensure_config(
      &fbl->main_fb, {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_TEXTURE(txl->color)});
  GPU_framebuffer_ensure_config(&fbl->main_color_fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->color)});

  /* Alloc common ubo data. */
  if (sldata->common_ubo == NULL) {
    sldata->common_ubo = DRW_uniformbuffer_create(sizeof(sldata->common_data),
                                                  &sldata->common_data);
  }

  /* Set the pers & view matrix. */
  float winmat[4][4], viewmat[4][4], viewinv[4][4];
  /* TODO(sergey): Shall render hold pointer to an evaluated camera instead? */
  struct Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, RE_GetCamera(engine->re));
  float frame = BKE_scene_frame_get(scene);

  RE_GetCameraWindow(engine->re, ob_camera_eval, frame, winmat);
  RE_GetCameraWindowWithOverscan(engine->re, winmat, g_data->overscan);
  RE_GetCameraModelMatrix(engine->re, ob_camera_eval, viewinv);

  invert_m4_m4(viewmat, viewinv);

  DRWView *view = DRW_view_create(viewmat, winmat, NULL, NULL, NULL);
  DRW_view_camtexco_set(view, camtexcofac);
  DRW_view_default_set(view);
  DRW_view_set_active(view);

  /* `EEVEE_renderpasses_init` will set the active render passes used by `EEVEE_effects_init`.
   * `EEVEE_effects_init` needs to go second for TAA. */
  EEVEE_renderpasses_init(vedata);
  EEVEE_effects_init(sldata, vedata, ob_camera_eval, false);
  EEVEE_materials_init(sldata, stl, fbl);
  EEVEE_shadows_init(sldata);
  EEVEE_lightprobes_init(sldata, vedata);

  /* INIT CACHE */
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

/* Used by light cache. in this case engine is NULL. */
void EEVEE_render_cache(void *vedata,
                        struct Object *ob,
                        struct RenderEngine *engine,
                        struct Depsgraph *UNUSED(depsgraph))
{
  EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();
  EEVEE_LightProbesInfo *pinfo = sldata->probes;
  bool cast_shadow = false;

  if (pinfo->vis_data.collection) {
    /* Used for rendering probe with visibility groups. */
    bool ob_vis = BKE_collection_has_object_recursive(pinfo->vis_data.collection, ob);
    ob_vis = (pinfo->vis_data.invert) ? !ob_vis : ob_vis;

    if (!ob_vis) {
      return;
    }
  }

  /* Don't print dupli objects as this can be very verbose and
   * increase the render time on Windows because of slow windows term.
   * (see T59649) */
  if (engine && (ob->base_flag & BASE_FROM_DUPLI) == 0) {
    char info[42];
    BLI_snprintf(info, sizeof(info), "Syncing %s", ob->id.name + 2);
    RE_engine_update_stats(engine, NULL, info);
  }

  const int ob_visibility = DRW_object_visibility_in_active_context(ob);
  if (ob_visibility & OB_VISIBLE_PARTICLES) {
    EEVEE_hair_cache_populate(vedata, sldata, ob, &cast_shadow);
  }

  if (ob_visibility & OB_VISIBLE_SELF) {
    if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_MBALL)) {
      EEVEE_materials_cache_populate(vedata, sldata, ob, &cast_shadow);
    }
    else if (ob->type == OB_LIGHTPROBE) {
      EEVEE_lightprobes_cache_add(sldata, vedata, ob);
    }
    else if (ob->type == OB_LAMP) {
      EEVEE_lights_cache_add(sldata, ob);
    }
  }

  if (cast_shadow) {
    EEVEE_shadows_caster_register(sldata, ob);
  }
}

static void eevee_render_color_result(RenderLayer *rl,
                                      const char *viewname,
                                      const rcti *rect,
                                      const char *render_pass_name,
                                      int num_channels,
                                      GPUFrameBuffer *framebuffer,
                                      EEVEE_Data *vedata)
{
  RenderPass *rp = RE_pass_find_by_name(rl, render_pass_name, viewname);
  GPU_framebuffer_bind(framebuffer);
  GPU_framebuffer_read_color(framebuffer,
                             vedata->stl->g_data->overscan_pixels + rect->xmin,
                             vedata->stl->g_data->overscan_pixels + rect->ymin,
                             BLI_rcti_size_x(rect),
                             BLI_rcti_size_y(rect),
                             num_channels,
                             0,
                             rp->rect);
}

static void eevee_render_result_combined(RenderLayer *rl,
                                         const char *viewname,
                                         const rcti *rect,
                                         EEVEE_Data *vedata,
                                         EEVEE_ViewLayerData *UNUSED(sldata))
{
  eevee_render_color_result(
      rl, viewname, rect, RE_PASSNAME_COMBINED, 4, vedata->stl->effects->final_fb, vedata);
}

static void eevee_render_result_subsurface(RenderLayer *rl,
                                           const char *viewname,
                                           const rcti *rect,
                                           EEVEE_Data *vedata,
                                           EEVEE_ViewLayerData *sldata)
{
  if (vedata->fbl->sss_accum_fb == NULL) {
    /* SSS is not enabled. */
    return;
  }

  if ((vedata->stl->g_data->render_passes & SCE_PASS_SUBSURFACE_COLOR) != 0) {
    EEVEE_renderpasses_postprocess(sldata, vedata, SCE_PASS_SUBSURFACE_COLOR);
    eevee_render_color_result(
        rl, viewname, rect, RE_PASSNAME_SUBSURFACE_COLOR, 3, vedata->fbl->renderpass_fb, vedata);
  }

  if ((vedata->stl->g_data->render_passes & SCE_PASS_SUBSURFACE_DIRECT) != 0) {
    EEVEE_renderpasses_postprocess(sldata, vedata, SCE_PASS_SUBSURFACE_DIRECT);
    eevee_render_color_result(
        rl, viewname, rect, RE_PASSNAME_SUBSURFACE_DIRECT, 3, vedata->fbl->renderpass_fb, vedata);
  }

  if ((vedata->stl->g_data->render_passes & SCE_PASS_SUBSURFACE_INDIRECT) != 0) {
    /* Do nothing as all the lighting is in the direct pass.
     * TODO : Separate Direct from indirect lighting. */
  }
}

static void eevee_render_result_normal(RenderLayer *rl,
                                       const char *viewname,
                                       const rcti *rect,
                                       EEVEE_Data *vedata,
                                       EEVEE_ViewLayerData *sldata)
{
  const int current_sample = vedata->stl->effects->taa_current_sample;

  /* Only read the center texel. */
  if (current_sample > 1) {
    return;
  }

  if ((vedata->stl->g_data->render_passes & SCE_PASS_NORMAL) != 0) {
    EEVEE_renderpasses_postprocess(sldata, vedata, SCE_PASS_NORMAL);
    eevee_render_color_result(
        rl, viewname, rect, RE_PASSNAME_NORMAL, 3, vedata->fbl->renderpass_fb, vedata);
  }
}

static void eevee_render_result_z(RenderLayer *rl,
                                  const char *viewname,
                                  const rcti *rect,
                                  EEVEE_Data *vedata,
                                  EEVEE_ViewLayerData *sldata)
{
  const int current_sample = vedata->stl->effects->taa_current_sample;

  /* Only read the center texel. */
  if (current_sample > 1) {
    return;
  }

  if ((vedata->stl->g_data->render_passes & SCE_PASS_Z) != 0) {
    EEVEE_renderpasses_postprocess(sldata, vedata, SCE_PASS_Z);
    eevee_render_color_result(
        rl, viewname, rect, RE_PASSNAME_Z, 1, vedata->fbl->renderpass_fb, vedata);
  }
}

static void eevee_render_result_mist(RenderLayer *rl,
                                     const char *viewname,
                                     const rcti *rect,
                                     EEVEE_Data *vedata,
                                     EEVEE_ViewLayerData *sldata)
{
  if ((vedata->stl->g_data->render_passes & SCE_PASS_MIST) != 0) {
    EEVEE_renderpasses_postprocess(sldata, vedata, SCE_PASS_MIST);
    eevee_render_color_result(
        rl, viewname, rect, RE_PASSNAME_MIST, 1, vedata->fbl->renderpass_fb, vedata);
  }
}

static void eevee_render_result_occlusion(RenderLayer *rl,
                                          const char *viewname,
                                          const rcti *rect,
                                          EEVEE_Data *vedata,
                                          EEVEE_ViewLayerData *sldata)
{
  if (vedata->fbl->ao_accum_fb == NULL) {
    /* AO is not enabled. */
    return;
  }

  if ((vedata->stl->g_data->render_passes & SCE_PASS_AO) != 0) {
    EEVEE_renderpasses_postprocess(sldata, vedata, SCE_PASS_AO);
    eevee_render_color_result(
        rl, viewname, rect, RE_PASSNAME_AO, 3, vedata->fbl->renderpass_fb, vedata);
  }
}

static void eevee_render_draw_background(EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PassList *psl = vedata->psl;

  /* Prevent background to write to data buffers.
   * NOTE : This also make sure the textures are bound
   *        to the right double buffer. */
  GPU_framebuffer_ensure_config(&fbl->main_fb,
                                {GPU_ATTACHMENT_LEAVE,
                                 GPU_ATTACHMENT_LEAVE,
                                 GPU_ATTACHMENT_NONE,
                                 GPU_ATTACHMENT_NONE,
                                 GPU_ATTACHMENT_NONE,
                                 GPU_ATTACHMENT_NONE});
  GPU_framebuffer_bind(fbl->main_fb);

  DRW_draw_pass(psl->background_pass);

  GPU_framebuffer_ensure_config(&fbl->main_fb,
                                {GPU_ATTACHMENT_LEAVE,
                                 GPU_ATTACHMENT_LEAVE,
                                 GPU_ATTACHMENT_TEXTURE(stl->effects->ssr_normal_input),
                                 GPU_ATTACHMENT_TEXTURE(stl->effects->ssr_specrough_input),
                                 GPU_ATTACHMENT_TEXTURE(stl->effects->sss_irradiance),
                                 GPU_ATTACHMENT_TEXTURE(stl->effects->sss_radius),
                                 GPU_ATTACHMENT_TEXTURE(stl->effects->sss_albedo)});
  GPU_framebuffer_bind(fbl->main_fb);
}

void EEVEE_render_draw(EEVEE_Data *vedata, RenderEngine *engine, RenderLayer *rl, const rcti *rect)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);
  const char *viewname = RE_GetActiveRenderView(engine->re);
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  EEVEE_ViewLayerData *sldata = EEVEE_view_layer_data_ensure();
  EEVEE_PrivateData *g_data = stl->g_data;

  /* FINISH CACHE */
  EEVEE_volumes_cache_finish(sldata, vedata);
  EEVEE_materials_cache_finish(sldata, vedata);
  EEVEE_lights_cache_finish(sldata, vedata);
  EEVEE_lightprobes_cache_finish(sldata, vedata);

  EEVEE_effects_draw_init(sldata, vedata);
  EEVEE_volumes_draw_init(sldata, vedata);

  /* Sort transparents before the loop. */
  DRW_pass_sort_shgroup_z(psl->transparent_pass);

  /* Push instances attributes to the GPU. */
  DRW_render_instance_buffer_finish();

  /* Need to be called after DRW_render_instance_buffer_finish() */
  /* Also we weed to have a correct fbo bound for DRW_hair_update */
  GPU_framebuffer_bind(fbl->main_fb);
  DRW_hair_update();

  uint tot_sample = scene_eval->eevee.taa_render_samples;
  uint render_samples = 0;

  /* SSR needs one iteration to start properly. */
  if (stl->effects->enabled_effects & EFFECT_SSR) {
    tot_sample += 1;
  }

  EEVEE_renderpasses_output_init(sldata, vedata, tot_sample);

  if (RE_engine_test_break(engine)) {
    return;
  }

  while (render_samples < tot_sample && !RE_engine_test_break(engine)) {
    float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float clear_depth = 1.0f;
    uint clear_stencil = 0x00;
    uint primes[3] = {2, 3, 7};
    double offset[3] = {0.0, 0.0, 0.0};
    double r[3];

    if ((stl->effects->enabled_effects & EFFECT_SSR) && (render_samples == 1) &&
        !stl->effects->ssr_was_valid_double_buffer) {
      /* SSR needs one iteration to start properly.
       * This iteration was done, reset to the original target sample count. */
      render_samples--;
      tot_sample--;
      /* Reset sampling (and accumulation) after the first sample to avoid
       * washed out first bounce for SSR. */
      EEVEE_temporal_sampling_reset(vedata);
      stl->effects->ssr_was_valid_double_buffer = stl->g_data->valid_double_buffer;
    }
    /* Don't print every samples as it can lead to bad performance. (see T59649) */
    else if ((render_samples % 25) == 0 || (render_samples + 1) == tot_sample) {
      char info[42];
      BLI_snprintf(
          info, sizeof(info), "Rendering %u / %u samples", render_samples + 1, tot_sample);
      RE_engine_update_stats(engine, NULL, info);
    }

    /* Copy previous persmat to UBO data */
    copy_m4_m4(sldata->common_data.prev_persmat, stl->effects->prev_persmat);

    BLI_halton_3d(primes, offset, stl->effects->taa_current_sample, r);
    EEVEE_update_noise(psl, fbl, r);
    EEVEE_temporal_sampling_matrices_calc(stl->effects, r);
    EEVEE_volumes_set_jitter(sldata, stl->effects->taa_current_sample - 1);
    EEVEE_materials_init(sldata, stl, fbl);

    /* Refresh Probes
     * Shadows needs to be updated for correct probes */
    EEVEE_shadows_update(sldata, vedata);
    EEVEE_lightprobes_refresh(sldata, vedata);
    EEVEE_lightprobes_refresh_planar(sldata, vedata);

    /* Refresh Shadows */
    EEVEE_shadows_draw(sldata, vedata, stl->effects->taa_view);

    /* Set matrices. */
    DRW_view_set_active(stl->effects->taa_view);

    /* Set ray type. */
    sldata->common_data.ray_type = EEVEE_RAY_CAMERA;
    sldata->common_data.ray_depth = 0.0f;
    DRW_uniformbuffer_update(sldata->common_ubo, &sldata->common_data);

    GPU_framebuffer_bind(fbl->main_fb);
    GPU_framebuffer_clear_color_depth_stencil(fbl->main_fb, clear_col, clear_depth, clear_stencil);
    /* Depth prepass */
    DRW_draw_pass(psl->depth_pass);
    DRW_draw_pass(psl->depth_pass_cull);
    /* Create minmax texture */
    EEVEE_create_minmax_buffer(vedata, dtxl->depth, -1);
    EEVEE_occlusion_compute(sldata, vedata, dtxl->depth, -1);
    EEVEE_volumes_compute(sldata, vedata);
    /* Shading pass */
    eevee_render_draw_background(vedata);
    GPU_framebuffer_bind(fbl->main_fb);
    EEVEE_materials_draw_opaque(sldata, psl);
    EEVEE_subsurface_data_render(sldata, vedata);
    /* Effects pre-transparency */
    EEVEE_subsurface_compute(sldata, vedata);
    EEVEE_reflection_compute(sldata, vedata);
    EEVEE_refraction_compute(sldata, vedata);
    /* Opaque refraction */
    DRW_draw_pass(psl->refract_depth_pass);
    DRW_draw_pass(psl->refract_depth_pass_cull);
    DRW_draw_pass(psl->refract_pass);
    /* Result NORMAL */
    eevee_render_result_normal(rl, viewname, rect, vedata, sldata);
    /* Volumetrics Resolve Opaque */
    EEVEE_volumes_resolve(sldata, vedata);
    /* Subsurface output, Occlusion output, Mist output */
    EEVEE_renderpasses_output_accumulate(sldata, vedata);
    /* Transparent */
    GPU_framebuffer_texture_attach(fbl->main_color_fb, dtxl->depth, 0, 0);
    GPU_framebuffer_bind(fbl->main_color_fb);
    DRW_draw_pass(psl->transparent_pass);
    GPU_framebuffer_bind(fbl->main_fb);
    GPU_framebuffer_texture_detach(fbl->main_color_fb, dtxl->depth);
    /* Result Z */
    eevee_render_result_z(rl, viewname, rect, vedata, sldata);
    /* Post Process */
    EEVEE_draw_effects(sldata, vedata);

    /* XXX Seems to fix TDR issue with NVidia drivers on linux. */
    GPU_finish();

    RE_engine_update_progress(engine, (float)(render_samples++) / (float)tot_sample);
  }

  eevee_render_result_combined(rl, viewname, rect, vedata, sldata);
  eevee_render_result_subsurface(rl, viewname, rect, vedata, sldata);
  eevee_render_result_mist(rl, viewname, rect, vedata, sldata);
  eevee_render_result_occlusion(rl, viewname, rect, vedata, sldata);

  /* Restore original viewport size. */
  DRW_render_viewport_size_set((int[2]){g_data->size_orig[0], g_data->size_orig[1]});
}

void EEVEE_render_update_passes(RenderEngine *engine, Scene *scene, ViewLayer *view_layer)
{
  int type;

  RE_engine_register_pass(engine, scene, view_layer, RE_PASSNAME_COMBINED, 4, "RGBA", SOCK_RGBA);

#define CHECK_PASS(name, channels, chanid) \
  if (view_layer->passflag & (SCE_PASS_##name)) { \
    if (channels == 4) \
      type = SOCK_RGBA; \
    else if (channels == 3) \
      type = SOCK_VECTOR; \
    else \
      type = SOCK_FLOAT; \
    RE_engine_register_pass( \
        engine, scene, view_layer, RE_PASSNAME_##name, channels, chanid, type); \
  } \
  ((void)0)

  CHECK_PASS(Z, 1, "Z");
  CHECK_PASS(MIST, 1, "Z");
  CHECK_PASS(NORMAL, 3, "XYZ");
  CHECK_PASS(AO, 3, "RGB");
  CHECK_PASS(SUBSURFACE_COLOR, 3, "RGB");
  CHECK_PASS(SUBSURFACE_DIRECT, 3, "RGB");

#undef CHECK_PASS
}

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
 * Gather all screen space effects technique such as Bloom, Motion Blur, DoF, SSAO, SSR, ...
 */

#include "DRW_render.h"

#include "BKE_global.h" /* for G.debug_value */

#include "GPU_capabilities.h"
#include "GPU_platform.h"
#include "GPU_state.h"
#include "GPU_texture.h"
#include "eevee_private.h"

static struct {
  /* These are just references, not actually allocated */
  struct GPUTexture *depth_src;
  struct GPUTexture *color_src;

  int depth_src_layer;
  float cube_texel_size;
} e_data = {NULL}; /* Engine data */

#define SETUP_BUFFER(tex, fb, fb_color) \
  { \
    eGPUTextureFormat format = (DRW_state_is_scene_render()) ? GPU_RGBA32F : GPU_RGBA16F; \
    DRW_texture_ensure_fullscreen_2d(&tex, format, DRW_TEX_FILTER); \
    GPU_framebuffer_ensure_config(&fb, \
                                  { \
                                      GPU_ATTACHMENT_TEXTURE(dtxl->depth), \
                                      GPU_ATTACHMENT_TEXTURE(tex), \
                                  }); \
    GPU_framebuffer_ensure_config(&fb_color, \
                                  { \
                                      GPU_ATTACHMENT_NONE, \
                                      GPU_ATTACHMENT_TEXTURE(tex), \
                                  }); \
  } \
  ((void)0)

#define CLEANUP_BUFFER(tex, fb, fb_color) \
  { \
    /* Cleanup to release memory */ \
    DRW_TEXTURE_FREE_SAFE(tex); \
    GPU_FRAMEBUFFER_FREE_SAFE(fb); \
    GPU_FRAMEBUFFER_FREE_SAFE(fb_color); \
  } \
  ((void)0)

void EEVEE_effects_init(EEVEE_ViewLayerData *sldata,
                        EEVEE_Data *vedata,
                        Object *camera,
                        const bool minimal)
{
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  const float *viewport_size = DRW_viewport_size_get();
  const int size_fs[2] = {(int)viewport_size[0], (int)viewport_size[1]};

  if (!stl->effects) {
    stl->effects = MEM_callocN(sizeof(EEVEE_EffectsInfo), "EEVEE_EffectsInfo");
    stl->effects->taa_render_sample = 1;
  }

  /* WORKAROUND: EEVEE_lookdev_init can reset TAA and needs a stl->effect.
   * So putting this before EEVEE_temporal_sampling_init for now. */
  EEVEE_lookdev_init(vedata);

  effects = stl->effects;

  effects->enabled_effects = 0;
  effects->enabled_effects |= (G.debug_value == 9) ? EFFECT_VELOCITY_BUFFER : 0;
  effects->enabled_effects |= EEVEE_motion_blur_init(sldata, vedata);
  effects->enabled_effects |= EEVEE_bloom_init(sldata, vedata);
  effects->enabled_effects |= EEVEE_depth_of_field_init(sldata, vedata, camera);
  effects->enabled_effects |= EEVEE_temporal_sampling_init(sldata, vedata);
  effects->enabled_effects |= EEVEE_occlusion_init(sldata, vedata);
  effects->enabled_effects |= EEVEE_screen_raytrace_init(sldata, vedata);

  /* Update matrices here because EEVEE_screen_raytrace_init can have reset the
   * taa_current_sample. (See T66811) */
  EEVEE_temporal_sampling_update_matrices(vedata);

  EEVEE_volumes_init(sldata, vedata);
  EEVEE_subsurface_init(sldata, vedata);

  /* Force normal buffer creation. */
  if (!minimal && (stl->g_data->render_passes & EEVEE_RENDER_PASS_NORMAL) != 0) {
    effects->enabled_effects |= EFFECT_NORMAL_BUFFER;
  }

  /**
   * MinMax Pyramid
   */
  int div = 1 << MAX_SCREEN_BUFFERS_LOD_LEVEL;
  effects->hiz_size[0] = divide_ceil_u(size_fs[0], div) * div;
  effects->hiz_size[1] = divide_ceil_u(size_fs[1], div) * div;

  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    /* Intel gpu seems to have problem rendering to only depth hiz_format */
    DRW_texture_ensure_2d(&txl->maxzbuffer, UNPACK2(effects->hiz_size), GPU_R32F, DRW_TEX_MIPMAP);
    GPU_framebuffer_ensure_config(&fbl->maxzbuffer_fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(txl->maxzbuffer),
                                  });
  }
  else {
    DRW_texture_ensure_2d(
        &txl->maxzbuffer, UNPACK2(effects->hiz_size), GPU_DEPTH_COMPONENT24, DRW_TEX_MIPMAP);
    GPU_framebuffer_ensure_config(&fbl->maxzbuffer_fb,
                                  {
                                      GPU_ATTACHMENT_TEXTURE(txl->maxzbuffer),
                                      GPU_ATTACHMENT_NONE,
                                  });
  }

  if (fbl->downsample_fb == NULL) {
    fbl->downsample_fb = GPU_framebuffer_create("downsample_fb");
  }

  /**
   * Compute hiZ texel alignment.
   */
  common_data->hiz_uv_scale[0] = viewport_size[0] / effects->hiz_size[0];
  common_data->hiz_uv_scale[1] = viewport_size[1] / effects->hiz_size[1];

  /* Compute pixel size. Size is multiplied by 2 because it is applied in NDC [-1..1] range. */
  sldata->common_data.ssr_pixelsize[0] = 2.0f / size_fs[0];
  sldata->common_data.ssr_pixelsize[1] = 2.0f / size_fs[1];

  /**
   * Color buffer with correct down-sampling alignment.
   * Used for SSReflections & SSRefractions.
   */
  if ((effects->enabled_effects & EFFECT_RADIANCE_BUFFER) != 0) {
    DRW_texture_ensure_2d(&txl->filtered_radiance,
                          UNPACK2(effects->hiz_size),
                          GPU_R11F_G11F_B10F,
                          DRW_TEX_FILTER | DRW_TEX_MIPMAP);

    GPU_framebuffer_ensure_config(&fbl->radiance_filtered_fb,
                                  {
                                      GPU_ATTACHMENT_NONE,
                                      GPU_ATTACHMENT_TEXTURE(txl->filtered_radiance),
                                  });
  }
  else {
    txl->filtered_radiance = NULL;
  }

  /**
   * Normal buffer for deferred passes.
   */
  if ((effects->enabled_effects & EFFECT_NORMAL_BUFFER) != 0) {
    effects->ssr_normal_input = DRW_texture_pool_query_2d(
        size_fs[0], size_fs[1], GPU_RG16, &draw_engine_eevee_type);

    GPU_framebuffer_texture_attach(fbl->main_fb, effects->ssr_normal_input, 1, 0);
  }
  else {
    effects->ssr_normal_input = NULL;
  }

  /**
   * Motion vector buffer for correct TAA / motion blur.
   */
  if ((effects->enabled_effects & EFFECT_VELOCITY_BUFFER) != 0) {
    effects->velocity_tx = DRW_texture_pool_query_2d(
        size_fs[0], size_fs[1], GPU_RGBA16, &draw_engine_eevee_type);

    GPU_framebuffer_ensure_config(&fbl->velocity_fb,
                                  {
                                      GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                      GPU_ATTACHMENT_TEXTURE(effects->velocity_tx),
                                  });

    GPU_framebuffer_ensure_config(
        &fbl->velocity_resolve_fb,
        {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(effects->velocity_tx)});
  }
  else {
    effects->velocity_tx = NULL;
  }

  /**
   * Setup depth double buffer.
   */
  if ((effects->enabled_effects & EFFECT_DEPTH_DOUBLE_BUFFER) != 0) {
    DRW_texture_ensure_fullscreen_2d(&txl->depth_double_buffer, GPU_DEPTH24_STENCIL8, 0);

    GPU_framebuffer_ensure_config(&fbl->double_buffer_depth_fb,
                                  {GPU_ATTACHMENT_TEXTURE(txl->depth_double_buffer)});
  }
  else {
    /* Cleanup to release memory */
    DRW_TEXTURE_FREE_SAFE(txl->depth_double_buffer);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->double_buffer_depth_fb);
  }

  if ((effects->enabled_effects & (EFFECT_TAA | EFFECT_TAA_REPROJECT)) != 0) {
    SETUP_BUFFER(txl->taa_history, fbl->taa_history_fb, fbl->taa_history_color_fb);
  }
  else {
    CLEANUP_BUFFER(txl->taa_history, fbl->taa_history_fb, fbl->taa_history_color_fb);
  }
}

void EEVEE_effects_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  DRWState downsample_write = DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS;
  DRWShadingGroup *grp;

  /* Intel gpu seems to have problem rendering to only depth format.
   * Use color texture instead. */
  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    downsample_write = DRW_STATE_WRITE_COLOR;
  }

  struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();

  if (effects->enabled_effects & EFFECT_RADIANCE_BUFFER) {
    DRW_PASS_CREATE(psl->color_copy_ps, DRW_STATE_WRITE_COLOR);
    grp = DRW_shgroup_create(EEVEE_shaders_effect_color_copy_sh_get(), psl->color_copy_ps);
    DRW_shgroup_uniform_texture_ref_ex(grp, "source", &e_data.color_src, GPU_SAMPLER_DEFAULT);
    DRW_shgroup_uniform_float(grp, "fireflyFactor", &sldata->common_data.ssr_firefly_fac, 1);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);

    DRW_PASS_CREATE(psl->color_downsample_ps, DRW_STATE_WRITE_COLOR);
    grp = DRW_shgroup_create(EEVEE_shaders_effect_downsample_sh_get(), psl->color_downsample_ps);
    DRW_shgroup_uniform_texture_ex(grp, "source", txl->filtered_radiance, GPU_SAMPLER_FILTER);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }

  {
    DRW_PASS_CREATE(psl->color_downsample_cube_ps, DRW_STATE_WRITE_COLOR);
    grp = DRW_shgroup_create(EEVEE_shaders_effect_downsample_cube_sh_get(),
                             psl->color_downsample_cube_ps);
    DRW_shgroup_uniform_texture_ref(grp, "source", &e_data.color_src);
    DRW_shgroup_uniform_float(grp, "texelSize", &e_data.cube_texel_size, 1);
    DRW_shgroup_uniform_int_copy(grp, "Layer", 0);
    DRW_shgroup_call_instances(grp, NULL, quad, 6);
  }

  {
    /* Perform min/max down-sample. */
    DRW_PASS_CREATE(psl->maxz_downlevel_ps, downsample_write);
    grp = DRW_shgroup_create(EEVEE_shaders_effect_maxz_downlevel_sh_get(), psl->maxz_downlevel_ps);
    DRW_shgroup_uniform_texture_ref_ex(grp, "depthBuffer", &txl->maxzbuffer, GPU_SAMPLER_DEFAULT);
    DRW_shgroup_call(grp, quad, NULL);

    /* Copy depth buffer to top level of HiZ */
    DRW_PASS_CREATE(psl->maxz_copydepth_ps, downsample_write);
    grp = DRW_shgroup_create(EEVEE_shaders_effect_maxz_copydepth_sh_get(), psl->maxz_copydepth_ps);
    DRW_shgroup_uniform_texture_ref_ex(grp, "depthBuffer", &e_data.depth_src, GPU_SAMPLER_DEFAULT);
    DRW_shgroup_call(grp, quad, NULL);

    DRW_PASS_CREATE(psl->maxz_copydepth_layer_ps, downsample_write);
    grp = DRW_shgroup_create(EEVEE_shaders_effect_maxz_copydepth_layer_sh_get(),
                             psl->maxz_copydepth_layer_ps);
    DRW_shgroup_uniform_texture_ref_ex(grp, "depthBuffer", &e_data.depth_src, GPU_SAMPLER_DEFAULT);
    DRW_shgroup_uniform_int(grp, "depthLayer", &e_data.depth_src_layer, 1);
    DRW_shgroup_call(grp, quad, NULL);
  }

  if ((effects->enabled_effects & EFFECT_VELOCITY_BUFFER) != 0) {
    EEVEE_MotionBlurData *mb_data = &effects->motion_blur;

    /* This pass compute camera motions to the non moving objects. */
    DRW_PASS_CREATE(psl->velocity_resolve, DRW_STATE_WRITE_COLOR);
    grp = DRW_shgroup_create(EEVEE_shaders_velocity_resolve_sh_get(), psl->velocity_resolve);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &e_data.depth_src);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);

    DRW_shgroup_uniform_mat4(grp, "prevViewProjMatrix", mb_data->camera[MB_PREV].persmat);
    DRW_shgroup_uniform_mat4(grp, "currViewProjMatrixInv", mb_data->camera[MB_CURR].persinv);
    DRW_shgroup_uniform_mat4(grp, "nextViewProjMatrix", mb_data->camera[MB_NEXT].persmat);
    DRW_shgroup_call(grp, quad, NULL);
  }
}

void EEVEE_effects_draw_init(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects = vedata->stl->effects;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  /**
   * Setup double buffer so we can access last frame as it was before post processes.
   */
  if ((effects->enabled_effects & EFFECT_DOUBLE_BUFFER) != 0) {
    SETUP_BUFFER(txl->color_double_buffer, fbl->double_buffer_fb, fbl->double_buffer_color_fb);
  }
  else {
    CLEANUP_BUFFER(txl->color_double_buffer, fbl->double_buffer_fb, fbl->double_buffer_color_fb);
  }

  /**
   * Ping Pong buffer
   */
  if ((effects->enabled_effects & EFFECT_POST_BUFFER) != 0) {
    SETUP_BUFFER(txl->color_post, fbl->effect_fb, fbl->effect_color_fb);
  }
  else {
    CLEANUP_BUFFER(txl->color_post, fbl->effect_fb, fbl->effect_color_fb);
  }
}

#if 0 /* Not required for now */
static void min_downsample_cb(void *vedata, int UNUSED(level))
{
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
  DRW_draw_pass(psl->minz_downlevel_ps);
}
#endif

static void max_downsample_cb(void *vedata, int UNUSED(level))
{
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
  DRW_draw_pass(psl->maxz_downlevel_ps);
}

static void simple_downsample_cube_cb(void *vedata, int level)
{
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
  e_data.cube_texel_size = (float)(1 << level) / (float)GPU_texture_width(e_data.color_src);
  DRW_draw_pass(psl->color_downsample_cube_ps);
}

void EEVEE_create_minmax_buffer(EEVEE_Data *vedata, GPUTexture *depth_src, int layer)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;

  e_data.depth_src = depth_src;
  e_data.depth_src_layer = layer;

  DRW_stats_group_start("Max buffer");
  /* Copy depth buffer to max texture top level */
  GPU_framebuffer_bind(fbl->maxzbuffer_fb);
  if (layer >= 0) {
    DRW_draw_pass(psl->maxz_copydepth_layer_ps);
  }
  else {
    DRW_draw_pass(psl->maxz_copydepth_ps);
  }
  /* Create lower levels */
  GPU_framebuffer_recursive_downsample(
      fbl->maxzbuffer_fb, MAX_SCREEN_BUFFERS_LOD_LEVEL, &max_downsample_cb, vedata);
  DRW_stats_group_end();

  /* Restore */
  GPU_framebuffer_bind(fbl->main_fb);

  if (GPU_mip_render_workaround() ||
      GPU_type_matches(GPU_DEVICE_INTEL_UHD, GPU_OS_WIN, GPU_DRIVER_ANY)) {
    /* Fix dot corruption on intel HD5XX/HD6XX series. */
    GPU_flush();
  }
}

static void downsample_radiance_cb(void *vedata, int UNUSED(level))
{
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
  DRW_draw_pass(psl->color_downsample_ps);
}

/**
 * Simple down-sampling algorithm. Reconstruct mip chain up to mip level.
 */
void EEVEE_effects_downsample_radiance_buffer(EEVEE_Data *vedata, GPUTexture *texture_src)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;

  e_data.color_src = texture_src;
  DRW_stats_group_start("Downsample Radiance");

  GPU_framebuffer_bind(fbl->radiance_filtered_fb);
  DRW_draw_pass(psl->color_copy_ps);

  GPU_framebuffer_recursive_downsample(
      fbl->radiance_filtered_fb, MAX_SCREEN_BUFFERS_LOD_LEVEL, &downsample_radiance_cb, vedata);
  DRW_stats_group_end();
}

/**
 * Simple down-sampling algorithm for cube-map. Reconstruct mip chain up to mip level.
 */
void EEVEE_downsample_cube_buffer(EEVEE_Data *vedata, GPUTexture *texture_src, int level)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  e_data.color_src = texture_src;

  /* Create lower levels */
  DRW_stats_group_start("Downsample Cube buffer");
  GPU_framebuffer_texture_attach(fbl->downsample_fb, texture_src, 0, 0);
  GPU_framebuffer_recursive_downsample(
      fbl->downsample_fb, level, &simple_downsample_cube_cb, vedata);
  GPU_framebuffer_texture_detach(fbl->downsample_fb, texture_src);
  DRW_stats_group_end();
}

static void EEVEE_velocity_resolve(EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & EFFECT_VELOCITY_BUFFER) != 0) {
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
    e_data.depth_src = dtxl->depth;

    GPU_framebuffer_bind(fbl->velocity_resolve_fb);
    DRW_draw_pass(psl->velocity_resolve);

    if (psl->velocity_object) {
      GPU_framebuffer_bind(fbl->velocity_fb);
      DRW_draw_pass(psl->velocity_object);
    }
  }
}

void EEVEE_draw_effects(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  /* only once per frame after the first post process */
  effects->swap_double_buffer = ((effects->enabled_effects & EFFECT_DOUBLE_BUFFER) != 0);

  /* Init pointers */
  effects->source_buffer = txl->color;           /* latest updated texture */
  effects->target_buffer = fbl->effect_color_fb; /* next target to render to */

  /* Post process stack (order matters) */
  EEVEE_velocity_resolve(vedata);
  EEVEE_motion_blur_draw(vedata);
  EEVEE_depth_of_field_draw(vedata);

  /* NOTE: Lookdev drawing happens before TAA but after
   * motion blur and dof to avoid distortions.
   * Velocity resolve use a hack to exclude lookdev
   * spheres from creating shimmering re-projection vectors. */
  EEVEE_lookdev_draw(vedata);

  EEVEE_temporal_sampling_draw(vedata);
  EEVEE_bloom_draw(vedata);

  /* Post effect render passes are done here just after the drawing of the effects and just before
   * the swapping of the buffers. */
  EEVEE_renderpasses_output_accumulate(sldata, vedata, true);

  /* Save the final texture and framebuffer for final transformation or read. */
  effects->final_tx = effects->source_buffer;
  effects->final_fb = (effects->target_buffer != fbl->main_color_fb) ? fbl->main_fb :
                                                                       fbl->effect_fb;
  if ((effects->enabled_effects & EFFECT_TAA) && (effects->source_buffer == txl->taa_history)) {
    effects->final_fb = fbl->taa_history_fb;
  }

  /* If no post processes is enabled, buffers are still not swapped, do it now. */
  SWAP_DOUBLE_BUFFERS();

  if (!stl->g_data->valid_double_buffer &&
      ((effects->enabled_effects & EFFECT_DOUBLE_BUFFER) != 0) &&
      (DRW_state_is_image_render() == false)) {
    /* If history buffer is not valid request another frame.
     * This fix black reflections on area resize. */
    DRW_viewport_request_redraw();
  }

  /* Record perspective matrix for the next frame. */
  DRW_view_persmat_get(effects->taa_view, effects->prev_persmat, false);

  /* Update double buffer status if render mode. */
  if (DRW_state_is_image_render()) {
    stl->g_data->valid_double_buffer = (txl->color_double_buffer != NULL);
    stl->g_data->valid_taa_history = (txl->taa_history != NULL);
  }
}

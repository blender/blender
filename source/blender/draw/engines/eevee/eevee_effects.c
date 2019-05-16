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

#include "eevee_private.h"
#include "GPU_texture.h"
#include "GPU_extensions.h"
#include "GPU_state.h"

static struct {
  /* Downsample Depth */
  struct GPUShader *minz_downlevel_sh;
  struct GPUShader *maxz_downlevel_sh;
  struct GPUShader *minz_downdepth_sh;
  struct GPUShader *maxz_downdepth_sh;
  struct GPUShader *minz_downdepth_layer_sh;
  struct GPUShader *maxz_downdepth_layer_sh;
  struct GPUShader *maxz_copydepth_layer_sh;
  struct GPUShader *minz_copydepth_sh;
  struct GPUShader *maxz_copydepth_sh;

  /* Simple Downsample */
  struct GPUShader *downsample_sh;
  struct GPUShader *downsample_cube_sh;

  /* Theses are just references, not actually allocated */
  struct GPUTexture *depth_src;
  struct GPUTexture *color_src;

  int depth_src_layer;
  float cube_texel_size;
} e_data = {NULL}; /* Engine data */

extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_effect_minmaxz_frag_glsl[];
extern char datatoc_effect_downsample_frag_glsl[];
extern char datatoc_effect_downsample_cube_frag_glsl[];
extern char datatoc_lightprobe_vert_glsl[];
extern char datatoc_lightprobe_geom_glsl[];

static void eevee_create_shader_downsample(void)
{
  e_data.downsample_sh = DRW_shader_create_fullscreen(datatoc_effect_downsample_frag_glsl, NULL);
  e_data.downsample_cube_sh = DRW_shader_create(datatoc_lightprobe_vert_glsl,
                                                datatoc_lightprobe_geom_glsl,
                                                datatoc_effect_downsample_cube_frag_glsl,
                                                NULL);

  e_data.minz_downlevel_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl,
                                                          "#define MIN_PASS\n");
  e_data.maxz_downlevel_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl,
                                                          "#define MAX_PASS\n");
  e_data.minz_downdepth_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl,
                                                          "#define MIN_PASS\n");
  e_data.maxz_downdepth_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl,
                                                          "#define MAX_PASS\n");
  e_data.minz_downdepth_layer_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl,
                                                                "#define MIN_PASS\n"
                                                                "#define LAYERED\n");
  e_data.maxz_downdepth_layer_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl,
                                                                "#define MAX_PASS\n"
                                                                "#define LAYERED\n");
  e_data.maxz_copydepth_layer_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl,
                                                                "#define MAX_PASS\n"
                                                                "#define COPY_DEPTH\n"
                                                                "#define LAYERED\n");
  e_data.minz_copydepth_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl,
                                                          "#define MIN_PASS\n"
                                                          "#define COPY_DEPTH\n");
  e_data.maxz_copydepth_sh = DRW_shader_create_fullscreen(datatoc_effect_minmaxz_frag_glsl,
                                                          "#define MAX_PASS\n"
                                                          "#define COPY_DEPTH\n");
}

#define SETUP_BUFFER(tex, fb, fb_color) \
  { \
    eGPUTextureFormat format = (DRW_state_is_scene_render()) ? GPU_RGBA32F : GPU_RGBA16F; \
    DRW_texture_ensure_fullscreen_2d(&tex, format, DRW_TEX_FILTER | DRW_TEX_MIPMAP); \
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
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ViewLayer *view_layer = draw_ctx->view_layer;

  const float *viewport_size = DRW_viewport_size_get();
  int size_fs[2] = {(int)viewport_size[0], (int)viewport_size[1]};

  /* Shaders */
  if (!e_data.downsample_sh) {
    eevee_create_shader_downsample();
  }

  if (!stl->effects) {
    stl->effects = MEM_callocN(sizeof(EEVEE_EffectsInfo), "EEVEE_EffectsInfo");
  }

  effects = stl->effects;

  effects->enabled_effects = 0;
  effects->enabled_effects |= (G.debug_value == 9) ? EFFECT_VELOCITY_BUFFER : 0;
  effects->enabled_effects |= EEVEE_motion_blur_init(sldata, vedata, camera);
  effects->enabled_effects |= EEVEE_bloom_init(sldata, vedata);
  effects->enabled_effects |= EEVEE_depth_of_field_init(sldata, vedata, camera);
  effects->enabled_effects |= EEVEE_temporal_sampling_init(sldata, vedata);
  effects->enabled_effects |= EEVEE_occlusion_init(sldata, vedata);
  effects->enabled_effects |= EEVEE_screen_raytrace_init(sldata, vedata);

  EEVEE_volumes_init(sldata, vedata);
  EEVEE_subsurface_init(sldata, vedata);

  /* Force normal buffer creation. */
  if (DRW_state_is_image_render() && !minimal && (view_layer->passflag & SCE_PASS_NORMAL) != 0) {
    effects->enabled_effects |= EFFECT_NORMAL_BUFFER;
  }

  /* Alpha checker if background is not drawn in viewport. */
  if (!DRW_state_is_image_render() && !DRW_state_draw_background()) {
    effects->enabled_effects |= EFFECT_ALPHA_CHECKER;
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

  /**
   * MinMax Pyramid
   */
  const bool half_res_hiz = true;
  int size[2], div;
  common_data->hiz_mip_offset = (half_res_hiz) ? 1 : 0;
  div = (half_res_hiz) ? 2 : 1;
  size[0] = max_ii(size_fs[0] / div, 1);
  size[1] = max_ii(size_fs[1] / div, 1);

  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    /* Intel gpu seems to have problem rendering to only depth format */
    DRW_texture_ensure_2d(&txl->maxzbuffer, size[0], size[1], GPU_R32F, DRW_TEX_MIPMAP);
  }
  else {
    DRW_texture_ensure_2d(
        &txl->maxzbuffer, size[0], size[1], GPU_DEPTH_COMPONENT24, DRW_TEX_MIPMAP);
  }

  if (fbl->downsample_fb == NULL) {
    fbl->downsample_fb = GPU_framebuffer_create();
  }

  /**
   * Compute Mipmap texel alignment.
   */
  for (int i = 0; i < 10; ++i) {
    int mip_size[2];
    GPU_texture_get_mipmap_size(txl->color, i, mip_size);
    common_data->mip_ratio[i][0] = viewport_size[0] / (mip_size[0] * powf(2.0f, i));
    common_data->mip_ratio[i][1] = viewport_size[1] / (mip_size[1] * powf(2.0f, i));
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
        size_fs[0], size_fs[1], GPU_RG16, &draw_engine_eevee_type);

    /* TODO output objects velocity during the mainpass. */
    // GPU_framebuffer_texture_attach(fbl->main_fb, effects->velocity_tx, 1, 0);

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
  int downsample_write = DRW_STATE_WRITE_DEPTH;

  /* Intel gpu seems to have problem rendering to only depth format.
   * Use color texture instead. */
  if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    downsample_write = DRW_STATE_WRITE_COLOR;
  }

  struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();

  {
    psl->color_downsample_ps = DRW_pass_create("Downsample", DRW_STATE_WRITE_COLOR);
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.downsample_sh, psl->color_downsample_ps);
    DRW_shgroup_uniform_texture_ref(grp, "source", &e_data.color_src);
    DRW_shgroup_uniform_float(grp, "fireflyFactor", &sldata->common_data.ssr_firefly_fac, 1);
    DRW_shgroup_call(grp, quad, NULL);
  }

  {
    psl->color_downsample_cube_ps = DRW_pass_create("Downsample Cube", DRW_STATE_WRITE_COLOR);
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.downsample_cube_sh,
                                              psl->color_downsample_cube_ps);
    DRW_shgroup_uniform_texture_ref(grp, "source", &e_data.color_src);
    DRW_shgroup_uniform_float(grp, "texelSize", &e_data.cube_texel_size, 1);
    DRW_shgroup_uniform_int_copy(grp, "Layer", 0);
    DRW_shgroup_call_instances(grp, quad, NULL, 6);
  }

  {
    /* Perform min/max downsample */
    DRWShadingGroup *grp;

    psl->maxz_downlevel_ps = DRW_pass_create("HiZ Max Down Level",
                                             downsample_write | DRW_STATE_DEPTH_ALWAYS);
    grp = DRW_shgroup_create(e_data.maxz_downlevel_sh, psl->maxz_downlevel_ps);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &txl->maxzbuffer);
    DRW_shgroup_call(grp, quad, NULL);

    /* Copy depth buffer to halfres top level of HiZ */

    psl->maxz_downdepth_ps = DRW_pass_create("HiZ Max Copy Depth Halfres",
                                             downsample_write | DRW_STATE_DEPTH_ALWAYS);
    grp = DRW_shgroup_create(e_data.maxz_downdepth_sh, psl->maxz_downdepth_ps);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &e_data.depth_src);
    DRW_shgroup_call(grp, quad, NULL);

    psl->maxz_downdepth_layer_ps = DRW_pass_create("HiZ Max Copy DepthLayer Halfres",
                                                   downsample_write | DRW_STATE_DEPTH_ALWAYS);
    grp = DRW_shgroup_create(e_data.maxz_downdepth_layer_sh, psl->maxz_downdepth_layer_ps);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &e_data.depth_src);
    DRW_shgroup_uniform_int(grp, "depthLayer", &e_data.depth_src_layer, 1);
    DRW_shgroup_call(grp, quad, NULL);

    psl->maxz_copydepth_ps = DRW_pass_create("HiZ Max Copy Depth Fullres",
                                             downsample_write | DRW_STATE_DEPTH_ALWAYS);
    grp = DRW_shgroup_create(e_data.maxz_copydepth_sh, psl->maxz_copydepth_ps);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &e_data.depth_src);
    DRW_shgroup_call(grp, quad, NULL);

    psl->maxz_copydepth_layer_ps = DRW_pass_create("HiZ Max Copy DepthLayer Halfres",
                                                   downsample_write | DRW_STATE_DEPTH_ALWAYS);
    grp = DRW_shgroup_create(e_data.maxz_copydepth_layer_sh, psl->maxz_copydepth_layer_ps);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &e_data.depth_src);
    DRW_shgroup_uniform_int(grp, "depthLayer", &e_data.depth_src_layer, 1);
    DRW_shgroup_call(grp, quad, NULL);
  }

  if ((effects->enabled_effects & EFFECT_VELOCITY_BUFFER) != 0) {
    /* This pass compute camera motions to the non moving objects. */
    psl->velocity_resolve = DRW_pass_create("Velocity Resolve", DRW_STATE_WRITE_COLOR);
    DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_velocity_resolve_sh_get(),
                                              psl->velocity_resolve);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &e_data.depth_src);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_mat4(grp, "currPersinv", effects->velocity_curr_persinv);
    DRW_shgroup_uniform_mat4(grp, "pastPersmat", effects->velocity_past_persmat);
    DRW_shgroup_call(grp, quad, NULL);
  }

  if ((effects->enabled_effects & EFFECT_ALPHA_CHECKER) != 0) {
    psl->alpha_checker = DRW_pass_create("Alpha Checker",
                                         DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_PREMUL_UNDER);

    GPUShader *checker_sh = GPU_shader_get_builtin_shader(GPU_SHADER_2D_CHECKER);

    DRWShadingGroup *grp = DRW_shgroup_create(checker_sh, psl->alpha_checker);

    copy_v4_fl4(effects->color_checker_dark, 0.15f, 0.15f, 0.15f, 1.0f);
    copy_v4_fl4(effects->color_checker_light, 0.2f, 0.2f, 0.2f, 1.0f);

    DRW_shgroup_uniform_vec4(grp, "color1", effects->color_checker_dark, 1);
    DRW_shgroup_uniform_vec4(grp, "color2", effects->color_checker_light, 1);
    DRW_shgroup_uniform_int_copy(grp, "size", 8);
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

static void simple_downsample_cb(void *vedata, int UNUSED(level))
{
  EEVEE_PassList *psl = ((EEVEE_Data *)vedata)->psl;
  DRW_draw_pass(psl->color_downsample_ps);
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
  EEVEE_TextureList *txl = vedata->txl;

  e_data.depth_src = depth_src;
  e_data.depth_src_layer = layer;

#if 0 /* Not required for now */
  DRW_stats_group_start("Min buffer");
  /* Copy depth buffer to min texture top level */
  GPU_framebuffer_texture_attach(fbl->downsample_fb, stl->g_data->minzbuffer, 0, 0);
  GPU_framebuffer_bind(fbl->downsample_fb);
  if (layer >= 0) {
    DRW_draw_pass(psl->minz_downdepth_layer_ps);
  }
  else {
    DRW_draw_pass(psl->minz_downdepth_ps);
  }
  GPU_framebuffer_texture_detach(stl->g_data->minzbuffer);

  /* Create lower levels */
  GPU_framebuffer_recursive_downsample(
      fbl->downsample_fb, stl->g_data->minzbuffer, 8, &min_downsample_cb, vedata);
  DRW_stats_group_end();
#endif
  int minmax_size[3], depth_size[3];
  GPU_texture_get_mipmap_size(depth_src, 0, depth_size);
  GPU_texture_get_mipmap_size(txl->maxzbuffer, 0, minmax_size);
  bool is_full_res_minmaxz = (minmax_size[0] == depth_size[0] && minmax_size[1] == depth_size[1]);

  DRW_stats_group_start("Max buffer");
  /* Copy depth buffer to max texture top level */
  GPU_framebuffer_texture_attach(fbl->downsample_fb, txl->maxzbuffer, 0, 0);
  GPU_framebuffer_bind(fbl->downsample_fb);
  if (layer >= 0) {
    if (is_full_res_minmaxz) {
      DRW_draw_pass(psl->maxz_copydepth_layer_ps);
    }
    else {
      DRW_draw_pass(psl->maxz_downdepth_layer_ps);
    }
  }
  else {
    if (is_full_res_minmaxz) {
      DRW_draw_pass(psl->maxz_copydepth_ps);
    }
    else {
      DRW_draw_pass(psl->maxz_downdepth_ps);
    }
  }

  /* Create lower levels */
  GPU_framebuffer_recursive_downsample(fbl->downsample_fb, 8, &max_downsample_cb, vedata);
  GPU_framebuffer_texture_detach(fbl->downsample_fb, txl->maxzbuffer);
  DRW_stats_group_end();

  /* Restore */
  GPU_framebuffer_bind(fbl->main_fb);

  if (GPU_mip_render_workaround() ||
      GPU_type_matches(GPU_DEVICE_INTEL_UHD, GPU_OS_WIN, GPU_DRIVER_ANY)) {
    /* Fix dot corruption on intel HD5XX/HD6XX series. */
    GPU_flush();
  }
}

/**
 * Simple downsampling algorithm. Reconstruct mip chain up to mip level.
 */
void EEVEE_downsample_buffer(EEVEE_Data *vedata, GPUTexture *texture_src, int level)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  e_data.color_src = texture_src;

  /* Create lower levels */
  DRW_stats_group_start("Downsample buffer");
  GPU_framebuffer_texture_attach(fbl->downsample_fb, texture_src, 0, 0);
  GPU_framebuffer_recursive_downsample(fbl->downsample_fb, level, &simple_downsample_cb, vedata);
  GPU_framebuffer_texture_detach(fbl->downsample_fb, texture_src);
  DRW_stats_group_end();
}

/**
 * Simple downsampling algorithm for cubemap. Reconstruct mip chain up to mip level.
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

void EEVEE_draw_alpha_checker(EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & EFFECT_ALPHA_CHECKER) != 0) {
    float mat[4][4];
    unit_m4(mat);

    /* Fragile, rely on the fact that GPU_SHADER_2D_CHECKER
     * only use the persmat. */
    DRW_viewport_matrix_override_set(mat, DRW_MAT_PERS);

    DRW_draw_pass(psl->alpha_checker);

    DRW_viewport_matrix_override_unset(DRW_MAT_PERS);
  }
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
    DRW_viewport_matrix_get(effects->velocity_curr_persinv, DRW_MAT_PERSINV);

    GPU_framebuffer_bind(fbl->velocity_resolve_fb);
    DRW_draw_pass(psl->velocity_resolve);
  }
  DRW_viewport_matrix_get(effects->velocity_past_persmat, DRW_MAT_PERS);
}

void EEVEE_draw_effects(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
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
  EEVEE_motion_blur_draw(vedata);
  EEVEE_depth_of_field_draw(vedata);

  /* NOTE: Lookdev drawing happens before TAA but after
   * motion blur and dof to avoid distortions.
   * Velocity resolve use a hack to exclude lookdev
   * spheres from creating shimering reprojection vectors. */
  EEVEE_lookdev_draw(vedata);
  EEVEE_velocity_resolve(vedata);

  EEVEE_temporal_sampling_draw(vedata);
  EEVEE_bloom_draw(vedata);

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

  /* Record pers matrix for the next frame. */
  DRW_viewport_matrix_get(stl->effects->prev_persmat, DRW_MAT_PERS);

  /* Update double buffer status if render mode. */
  if (DRW_state_is_image_render()) {
    stl->g_data->valid_double_buffer = (txl->color_double_buffer != NULL);
    stl->g_data->valid_taa_history = (txl->taa_history != NULL);
  }
}

void EEVEE_effects_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.downsample_sh);
  DRW_SHADER_FREE_SAFE(e_data.downsample_cube_sh);

  DRW_SHADER_FREE_SAFE(e_data.minz_downlevel_sh);
  DRW_SHADER_FREE_SAFE(e_data.maxz_downlevel_sh);
  DRW_SHADER_FREE_SAFE(e_data.minz_downdepth_sh);
  DRW_SHADER_FREE_SAFE(e_data.maxz_downdepth_sh);
  DRW_SHADER_FREE_SAFE(e_data.minz_downdepth_layer_sh);
  DRW_SHADER_FREE_SAFE(e_data.maxz_downdepth_layer_sh);
  DRW_SHADER_FREE_SAFE(e_data.maxz_copydepth_layer_sh);
  DRW_SHADER_FREE_SAFE(e_data.minz_copydepth_sh);
  DRW_SHADER_FREE_SAFE(e_data.maxz_copydepth_sh);
}

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
 * Implementation of the screen space Ground Truth Ambient Occlusion.
 */

#include "DRW_render.h"

#include "BLI_string_utils.h"

#include "DEG_depsgraph_query.h"

#include "BKE_global.h" /* for G.debug_value */

#include "eevee_private.h"

#include "GPU_extensions.h"
#include "GPU_state.h"

static struct {
  /* Ground Truth Ambient Occlusion */
  struct GPUShader *gtao_sh;
  struct GPUShader *gtao_layer_sh;
  struct GPUShader *gtao_debug_sh;
  struct GPUTexture *src_depth;
} e_data = {NULL}; /* Engine data */

extern char datatoc_ambient_occlusion_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_effect_gtao_frag_glsl[];

static void eevee_create_shader_occlusion(void)
{
  char *frag_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                    datatoc_common_uniforms_lib_glsl,
                                    datatoc_bsdf_common_lib_glsl,
                                    datatoc_ambient_occlusion_lib_glsl,
                                    datatoc_effect_gtao_frag_glsl);

  e_data.gtao_sh = DRW_shader_create_fullscreen(frag_str, NULL);
  e_data.gtao_layer_sh = DRW_shader_create_fullscreen(frag_str, "#define LAYERED_DEPTH\n");
  e_data.gtao_debug_sh = DRW_shader_create_fullscreen(frag_str, "#define DEBUG_AO\n");

  MEM_freeN(frag_str);
}

int EEVEE_occlusion_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  if (scene_eval->eevee.flag & SCE_EEVEE_GTAO_ENABLED) {
    const float *viewport_size = DRW_viewport_size_get();
    const int fs_size[2] = {(int)viewport_size[0], (int)viewport_size[1]};

    /* Shaders */
    if (!e_data.gtao_sh) {
      eevee_create_shader_occlusion();
    }

    common_data->ao_dist = scene_eval->eevee.gtao_distance;
    common_data->ao_factor = scene_eval->eevee.gtao_factor;
    common_data->ao_quality = 1.0f - scene_eval->eevee.gtao_quality;

    common_data->ao_settings = 1.0f; /* USE_AO */
    if (scene_eval->eevee.flag & SCE_EEVEE_GTAO_BENT_NORMALS) {
      common_data->ao_settings += 2.0f; /* USE_BENT_NORMAL */
    }
    if (scene_eval->eevee.flag & SCE_EEVEE_GTAO_BOUNCE) {
      common_data->ao_settings += 4.0f; /* USE_DENOISE */
    }

    common_data->ao_bounce_fac = (scene_eval->eevee.flag & SCE_EEVEE_GTAO_BOUNCE) ? 1.0f : 0.0f;

    effects->gtao_horizons = DRW_texture_pool_query_2d(
        fs_size[0], fs_size[1], GPU_RGBA8, &draw_engine_eevee_type);
    GPU_framebuffer_ensure_config(
        &fbl->gtao_fb, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(effects->gtao_horizons)});

    if (G.debug_value == 6) {
      effects->gtao_horizons_debug = DRW_texture_pool_query_2d(
          fs_size[0], fs_size[1], GPU_RGBA8, &draw_engine_eevee_type);
      GPU_framebuffer_ensure_config(
          &fbl->gtao_debug_fb,
          {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(effects->gtao_horizons_debug)});
    }
    else {
      effects->gtao_horizons_debug = NULL;
    }

    return EFFECT_GTAO | EFFECT_NORMAL_BUFFER;
  }

  /* Cleanup */
  effects->gtao_horizons = NULL;
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->gtao_fb);
  common_data->ao_settings = 0.0f;

  return 0;
}

void EEVEE_occlusion_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_EffectsInfo *effects = stl->effects;

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  if (scene_eval->eevee.flag & SCE_EEVEE_GTAO_ENABLED) {
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
    float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    DRW_texture_ensure_fullscreen_2d(
        &txl->ao_accum, GPU_R32F, 0); /* Should be enough precision for many samples. */

    GPU_framebuffer_ensure_config(&fbl->ao_accum_fb,
                                  {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->ao_accum)});

    /* Clear texture. */
    GPU_framebuffer_bind(fbl->ao_accum_fb);
    GPU_framebuffer_clear_color(fbl->ao_accum_fb, clear);

    /* Accumulation pass */
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_ADDITIVE;
    DRW_PASS_CREATE(psl->ao_accum_ps, state);
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.gtao_debug_sh, psl->ao_accum_ps);
    DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
    DRW_shgroup_uniform_texture_ref(grp, "maxzBuffer", &txl->maxzbuffer);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
    DRW_shgroup_uniform_texture_ref(grp, "normalBuffer", &effects->ssr_normal_input);
    DRW_shgroup_uniform_texture_ref(grp, "horizonBuffer", &effects->gtao_horizons);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), NULL);
  }
  else {
    /* Cleanup to release memory */
    DRW_TEXTURE_FREE_SAFE(txl->ao_accum);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->ao_accum_fb);
  }
}

void EEVEE_occlusion_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects = stl->effects;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();

  if ((effects->enabled_effects & EFFECT_GTAO) != 0) {
    /**  Occlusion algorithm overview
     *
     *  We separate the computation into 2 steps.
     *
     * - First we scan the neighborhood pixels to find the maximum horizon angle.
     *   We save this angle in a RG8 array texture.
     *
     * - Then we use this angle to compute occlusion with the shading normal at
     *   the shading stage. This let us do correct shadowing for each diffuse / specular
     *   lobe present in the shader using the correct normal.
     */
    DRW_PASS_CREATE(psl->ao_horizon_search, DRW_STATE_WRITE_COLOR);
    DRWShadingGroup *grp = DRW_shgroup_create(e_data.gtao_sh, psl->ao_horizon_search);
    DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
    DRW_shgroup_uniform_texture_ref(grp, "maxzBuffer", &txl->maxzbuffer);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &effects->ao_src_depth);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_call(grp, quad, NULL);

    DRW_PASS_CREATE(psl->ao_horizon_search_layer, DRW_STATE_WRITE_COLOR);
    grp = DRW_shgroup_create(e_data.gtao_layer_sh, psl->ao_horizon_search_layer);
    DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
    DRW_shgroup_uniform_texture_ref(grp, "maxzBuffer", &txl->maxzbuffer);
    DRW_shgroup_uniform_texture_ref(grp, "depthBufferLayered", &effects->ao_src_depth);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_int(grp, "layer", &stl->effects->ao_depth_layer, 1);
    DRW_shgroup_call(grp, quad, NULL);

    if (G.debug_value == 6) {
      DRW_PASS_CREATE(psl->ao_horizon_debug, DRW_STATE_WRITE_COLOR);
      grp = DRW_shgroup_create(e_data.gtao_debug_sh, psl->ao_horizon_debug);
      DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
      DRW_shgroup_uniform_texture_ref(grp, "maxzBuffer", &txl->maxzbuffer);
      DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
      DRW_shgroup_uniform_texture_ref(grp, "normalBuffer", &effects->ssr_normal_input);
      DRW_shgroup_uniform_texture_ref(grp, "horizonBuffer", &effects->gtao_horizons);
      DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
      DRW_shgroup_call(grp, quad, NULL);
    }
  }
}

void EEVEE_occlusion_compute(EEVEE_ViewLayerData *UNUSED(sldata),
                             EEVEE_Data *vedata,
                             struct GPUTexture *depth_src,
                             int layer)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & EFFECT_GTAO) != 0) {
    DRW_stats_group_start("GTAO Horizon Scan");
    effects->ao_src_depth = depth_src;
    effects->ao_depth_layer = layer;

    GPU_framebuffer_bind(fbl->gtao_fb);

    if (layer >= 0) {
      DRW_draw_pass(psl->ao_horizon_search_layer);
    }
    else {
      DRW_draw_pass(psl->ao_horizon_search);
    }

    if (GPU_mip_render_workaround() ||
        GPU_type_matches(GPU_DEVICE_INTEL_UHD, GPU_OS_WIN, GPU_DRIVER_ANY)) {
      /* Fix dot corruption on intel HD5XX/HD6XX series. */
      GPU_flush();
    }

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);

    DRW_stats_group_end();
  }
}

void EEVEE_occlusion_draw_debug(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if (((effects->enabled_effects & EFFECT_GTAO) != 0) && (G.debug_value == 6)) {
    DRW_stats_group_start("GTAO Debug");

    GPU_framebuffer_bind(fbl->gtao_debug_fb);
    DRW_draw_pass(psl->ao_horizon_debug);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);

    DRW_stats_group_end();
  }
}

void EEVEE_occlusion_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_PassList *psl = vedata->psl;

  if (fbl->ao_accum_fb != NULL) {
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

    /* Update the min_max/horizon buffers so the refracion materials appear in it. */
    EEVEE_create_minmax_buffer(vedata, dtxl->depth, -1);
    EEVEE_occlusion_compute(sldata, vedata, dtxl->depth, -1);

    GPU_framebuffer_bind(fbl->ao_accum_fb);
    DRW_draw_pass(psl->ao_accum_ps);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

void EEVEE_occlusion_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.gtao_sh);
  DRW_SHADER_FREE_SAFE(e_data.gtao_layer_sh);
  DRW_SHADER_FREE_SAFE(e_data.gtao_debug_sh);
}

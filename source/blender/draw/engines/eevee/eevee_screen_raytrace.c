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
 * Screen space reflections and refractions techniques.
 */

#include "DRW_render.h"

#include "BLI_dynstr.h"
#include "BLI_string_utils.h"

#include "DEG_depsgraph_query.h"

#include "eevee_private.h"
#include "GPU_texture.h"

/* SSR shader variations */
enum {
  SSR_RESOLVE = (1 << 0),
  SSR_FULL_TRACE = (1 << 1),
  SSR_AO = (1 << 3),
  SSR_MAX_SHADER = (1 << 4),
};

static struct {
  /* Screen Space Reflection */
  struct GPUShader *ssr_sh[SSR_MAX_SHADER];

  /* Theses are just references, not actually allocated */
  struct GPUTexture *depth_src;
  struct GPUTexture *color_src;
} e_data = {{NULL}}; /* Engine data */

extern char datatoc_ambient_occlusion_lib_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_bsdf_sampling_lib_glsl[];
extern char datatoc_octahedron_lib_glsl[];
extern char datatoc_effect_ssr_frag_glsl[];
extern char datatoc_lightprobe_lib_glsl[];
extern char datatoc_raytrace_lib_glsl[];

static struct GPUShader *eevee_effects_screen_raytrace_shader_get(int options)
{
  if (e_data.ssr_sh[options] == NULL) {
    char *ssr_shader_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                            datatoc_common_uniforms_lib_glsl,
                                            datatoc_bsdf_common_lib_glsl,
                                            datatoc_bsdf_sampling_lib_glsl,
                                            datatoc_ambient_occlusion_lib_glsl,
                                            datatoc_octahedron_lib_glsl,
                                            datatoc_lightprobe_lib_glsl,
                                            datatoc_raytrace_lib_glsl,
                                            datatoc_effect_ssr_frag_glsl);

    DynStr *ds_defines = BLI_dynstr_new();
    BLI_dynstr_append(ds_defines, SHADER_DEFINES);
    if (options & SSR_RESOLVE) {
      BLI_dynstr_append(ds_defines, "#define STEP_RESOLVE\n");
    }
    else {
      BLI_dynstr_append(ds_defines, "#define STEP_RAYTRACE\n");
      BLI_dynstr_append(ds_defines, "#define PLANAR_PROBE_RAYTRACE\n");
    }
    if (options & SSR_FULL_TRACE) {
      BLI_dynstr_append(ds_defines, "#define FULLRES\n");
    }
    if (options & SSR_AO) {
      BLI_dynstr_append(ds_defines, "#define SSR_AO\n");
    }
    char *ssr_define_str = BLI_dynstr_get_cstring(ds_defines);
    BLI_dynstr_free(ds_defines);

    e_data.ssr_sh[options] = DRW_shader_create_fullscreen(ssr_shader_str, ssr_define_str);

    MEM_freeN(ssr_shader_str);
    MEM_freeN(ssr_define_str);
  }

  return e_data.ssr_sh[options];
}

int EEVEE_screen_raytrace_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_CommonUniformBuffer *common_data = &sldata->common_data;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects = stl->effects;
  const float *viewport_size = DRW_viewport_size_get();

  const DRWContextState *draw_ctx = DRW_context_state_get();
  const Scene *scene_eval = DEG_get_evaluated_scene(draw_ctx->depsgraph);

  /* Compute pixel size, (shared with contact shadows) */
  copy_v2_v2(common_data->ssr_pixelsize, viewport_size);
  invert_v2(common_data->ssr_pixelsize);

  if (scene_eval->eevee.flag & SCE_EEVEE_SSR_ENABLED) {
    const bool use_refraction = (scene_eval->eevee.flag & SCE_EEVEE_SSR_REFRACTION) != 0;

    if (use_refraction) {
      /* TODO: Opti: Could be shared. */
      DRW_texture_ensure_fullscreen_2d(
          &txl->refract_color, GPU_R11F_G11F_B10F, DRW_TEX_FILTER | DRW_TEX_MIPMAP);

      GPU_framebuffer_ensure_config(
          &fbl->refract_fb, {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->refract_color)});
    }

    const bool is_persp = DRW_view_is_persp_get(NULL);
    if (effects->ssr_was_persp != is_persp) {
      effects->ssr_was_persp = is_persp;
      DRW_viewport_request_redraw();
      EEVEE_temporal_sampling_reset(vedata);
      stl->g_data->valid_double_buffer = false;
    }

    effects->reflection_trace_full = (scene_eval->eevee.flag & SCE_EEVEE_SSR_HALF_RESOLUTION) == 0;
    common_data->ssr_thickness = scene_eval->eevee.ssr_thickness;
    common_data->ssr_border_fac = scene_eval->eevee.ssr_border_fade;
    common_data->ssr_firefly_fac = scene_eval->eevee.ssr_firefly_fac;
    common_data->ssr_max_roughness = scene_eval->eevee.ssr_max_roughness;
    common_data->ssr_quality = 1.0f - 0.95f * scene_eval->eevee.ssr_quality;
    common_data->ssr_brdf_bias = 0.1f + common_data->ssr_quality * 0.6f; /* Range [0.1, 0.7]. */

    if (common_data->ssr_firefly_fac < 1e-8f) {
      common_data->ssr_firefly_fac = FLT_MAX;
    }

    const int divisor = (effects->reflection_trace_full) ? 1 : 2;
    int tracing_res[2] = {(int)viewport_size[0] / divisor, (int)viewport_size[1] / divisor};
    int size_fs[2] = {(int)viewport_size[0], (int)viewport_size[1]};
    const bool high_qual_input = true; /* TODO dither low quality input */
    const eGPUTextureFormat format = (high_qual_input) ? GPU_RGBA16F : GPU_RGBA8;

    /* MRT for the shading pass in order to output needed data for the SSR pass. */
    effects->ssr_specrough_input = DRW_texture_pool_query_2d(
        size_fs[0], size_fs[1], format, &draw_engine_eevee_type);

    GPU_framebuffer_texture_attach(fbl->main_fb, effects->ssr_specrough_input, 2, 0);

    /* Raytracing output */
    effects->ssr_hit_output = DRW_texture_pool_query_2d(
        tracing_res[0], tracing_res[1], GPU_RG16I, &draw_engine_eevee_type);
    effects->ssr_pdf_output = DRW_texture_pool_query_2d(
        tracing_res[0], tracing_res[1], GPU_R16F, &draw_engine_eevee_type);

    GPU_framebuffer_ensure_config(&fbl->screen_tracing_fb,
                                  {GPU_ATTACHMENT_NONE,
                                   GPU_ATTACHMENT_TEXTURE(effects->ssr_hit_output),
                                   GPU_ATTACHMENT_TEXTURE(effects->ssr_pdf_output)});

    /* Enable double buffering to be able to read previous frame color */
    return EFFECT_SSR | EFFECT_NORMAL_BUFFER | EFFECT_DOUBLE_BUFFER |
           ((use_refraction) ? EFFECT_REFRACT : 0);
  }

  /* Cleanup to release memory */
  GPU_FRAMEBUFFER_FREE_SAFE(fbl->screen_tracing_fb);
  effects->ssr_specrough_input = NULL;
  effects->ssr_hit_output = NULL;
  effects->ssr_pdf_output = NULL;

  return 0;
}

void EEVEE_screen_raytrace_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects = stl->effects;
  LightCache *lcache = stl->g_data->light_cache;

  struct GPUBatch *quad = DRW_cache_fullscreen_quad_get();

  if ((effects->enabled_effects & EFFECT_SSR) != 0) {
    int options = (effects->reflection_trace_full) ? SSR_FULL_TRACE : 0;
    options |= ((effects->enabled_effects & EFFECT_GTAO) != 0) ? SSR_AO : 0;

    struct GPUShader *trace_shader = eevee_effects_screen_raytrace_shader_get(options);
    struct GPUShader *resolve_shader = eevee_effects_screen_raytrace_shader_get(SSR_RESOLVE |
                                                                                options);

    /** Screen space raytracing overview
     *
     * Following Frostbite stochastic SSR.
     *
     * - First pass Trace rays across the depth buffer. The hit position and pdf are
     *   recorded in a RGBA16F render target for each ray (sample).
     *
     * - We downsample the previous frame color buffer.
     *
     * - For each final pixel, we gather neighbors rays and choose a color buffer
     *   mipmap for each ray using its pdf. (filtered importance sampling)
     *   We then evaluate the lighting from the probes and mix the results together.
     */
    DRW_PASS_CREATE(psl->ssr_raytrace, DRW_STATE_WRITE_COLOR);
    DRWShadingGroup *grp = DRW_shgroup_create(trace_shader, psl->ssr_raytrace);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &e_data.depth_src);
    DRW_shgroup_uniform_texture_ref(grp, "normalBuffer", &effects->ssr_normal_input);
    DRW_shgroup_uniform_texture_ref(grp, "specroughBuffer", &effects->ssr_specrough_input);
    DRW_shgroup_uniform_texture_ref(grp, "maxzBuffer", &txl->maxzbuffer);
    DRW_shgroup_uniform_texture_ref(grp, "planarDepth", &vedata->txl->planar_depth);
    DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
    DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);
    DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
    DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    if (!effects->reflection_trace_full) {
      DRW_shgroup_uniform_ivec2(grp, "halfresOffset", effects->ssr_halfres_ofs, 1);
    }
    DRW_shgroup_call(grp, quad, NULL);

    DRW_PASS_CREATE(psl->ssr_resolve, DRW_STATE_WRITE_COLOR | DRW_STATE_ADDITIVE);
    grp = DRW_shgroup_create(resolve_shader, psl->ssr_resolve);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &e_data.depth_src);
    DRW_shgroup_uniform_texture_ref(grp, "normalBuffer", &effects->ssr_normal_input);
    DRW_shgroup_uniform_texture_ref(grp, "specroughBuffer", &effects->ssr_specrough_input);
    DRW_shgroup_uniform_texture_ref(grp, "probeCubes", &lcache->cube_tx.tex);
    DRW_shgroup_uniform_texture_ref(grp, "probePlanars", &vedata->txl->planar_pool);
    DRW_shgroup_uniform_texture_ref(grp, "planarDepth", &vedata->txl->planar_depth);
    DRW_shgroup_uniform_texture_ref(grp, "hitBuffer", &effects->ssr_hit_output);
    DRW_shgroup_uniform_texture_ref(grp, "pdfBuffer", &effects->ssr_pdf_output);
    DRW_shgroup_uniform_texture_ref(grp, "prevColorBuffer", &txl->color_double_buffer);
    DRW_shgroup_uniform_block(grp, "grid_block", sldata->grid_ubo);
    DRW_shgroup_uniform_block(grp, "probe_block", sldata->probe_ubo);
    DRW_shgroup_uniform_block(grp, "planar_block", sldata->planar_ubo);
    DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
    DRW_shgroup_uniform_int(grp, "neighborOffset", &effects->ssr_neighbor_ofs, 1);
    if ((effects->enabled_effects & EFFECT_GTAO) != 0) {
      DRW_shgroup_uniform_texture(grp, "utilTex", EEVEE_materials_get_util_tex());
      DRW_shgroup_uniform_texture_ref(grp, "horizonBuffer", &effects->gtao_horizons);
    }

    DRW_shgroup_call(grp, quad, NULL);
  }
}

void EEVEE_refraction_compute(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if ((effects->enabled_effects & EFFECT_REFRACT) != 0) {
    GPU_framebuffer_blit(fbl->main_fb, 0, fbl->refract_fb, 0, GPU_COLOR_BIT);
    EEVEE_downsample_buffer(vedata, txl->refract_color, 9);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

void EEVEE_reflection_compute(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_EffectsInfo *effects = stl->effects;

  if (((effects->enabled_effects & EFFECT_SSR) != 0) && stl->g_data->valid_double_buffer) {
    DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
    e_data.depth_src = dtxl->depth;

    DRW_stats_group_start("SSR");

    /* Raytrace. */
    GPU_framebuffer_bind(fbl->screen_tracing_fb);
    DRW_draw_pass(psl->ssr_raytrace);

    EEVEE_downsample_buffer(vedata, txl->color_double_buffer, 9);

    /* Resolve at fullres */
    int sample = (DRW_state_is_image_render()) ? effects->taa_render_sample :
                                                 effects->taa_current_sample;
    /* Doing a neighbor shift only after a few iteration.
     * We wait for a prime number of cycles to avoid noise correlation.
     * This reduces variance faster. */
    effects->ssr_neighbor_ofs = ((sample / 5) % 8) * 4;
    switch ((sample / 11) % 4) {
      case 0:
        effects->ssr_halfres_ofs[0] = 0;
        effects->ssr_halfres_ofs[1] = 0;
        break;
      case 1:
        effects->ssr_halfres_ofs[0] = 0;
        effects->ssr_halfres_ofs[1] = 1;
        break;
      case 2:
        effects->ssr_halfres_ofs[0] = 1;
        effects->ssr_halfres_ofs[1] = 0;
        break;
      case 4:
        effects->ssr_halfres_ofs[0] = 1;
        effects->ssr_halfres_ofs[1] = 1;
        break;
    }
    GPU_framebuffer_bind(fbl->main_color_fb);
    DRW_draw_pass(psl->ssr_resolve);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);
    DRW_stats_group_end();
  }
}

void EEVEE_screen_raytrace_free(void)
{
  for (int i = 0; i < SSR_MAX_SHADER; ++i) {
    DRW_SHADER_FREE_SAFE(e_data.ssr_sh[i]);
  }
}

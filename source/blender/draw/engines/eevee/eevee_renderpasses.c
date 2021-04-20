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
 * Copyright 2019, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "draw_color_management.h" /* TODO remove dependency. */

#include "BKE_global.h" /* for G.debug_value */

#include "BLI_hash.h"
#include "BLI_string_utils.h"

#include "DEG_depsgraph_query.h"

#include "eevee_private.h"

typedef enum eRenderPassPostProcessType {
  PASS_POST_UNDEFINED = 0,
  PASS_POST_ACCUMULATED_COLOR = 1,
  PASS_POST_ACCUMULATED_COLOR_ALPHA = 2,
  PASS_POST_ACCUMULATED_LIGHT = 3,
  PASS_POST_ACCUMULATED_VALUE = 4,
  PASS_POST_DEPTH = 5,
  PASS_POST_AO = 6,
  PASS_POST_NORMAL = 7,
  PASS_POST_TWO_LIGHT_BUFFERS = 8,
  PASS_POST_ACCUMULATED_TRANSMITTANCE_COLOR = 9,
} eRenderPassPostProcessType;

/* bitmask containing all renderpasses that need post-processing */
#define EEVEE_RENDERPASSES_WITH_POST_PROCESSING \
  (EEVEE_RENDER_PASS_Z | EEVEE_RENDER_PASS_MIST | EEVEE_RENDER_PASS_NORMAL | \
   EEVEE_RENDER_PASS_AO | EEVEE_RENDER_PASS_BLOOM | EEVEE_RENDER_PASS_VOLUME_LIGHT | \
   EEVEE_RENDER_PASS_SHADOW | EEVEE_RENDERPASSES_MATERIAL)

#define EEVEE_RENDERPASSES_ALL \
  (EEVEE_RENDERPASSES_WITH_POST_PROCESSING | EEVEE_RENDER_PASS_COMBINED)

#define EEVEE_RENDERPASSES_POST_PROCESS_ON_FIRST_SAMPLE \
  (EEVEE_RENDER_PASS_Z | EEVEE_RENDER_PASS_NORMAL)

#define EEVEE_RENDERPASSES_COLOR_PASS \
  (EEVEE_RENDER_PASS_DIFFUSE_COLOR | EEVEE_RENDER_PASS_SPECULAR_COLOR | EEVEE_RENDER_PASS_EMIT | \
   EEVEE_RENDER_PASS_BLOOM)
#define EEVEE_RENDERPASSES_LIGHT_PASS \
  (EEVEE_RENDER_PASS_DIFFUSE_LIGHT | EEVEE_RENDER_PASS_SPECULAR_LIGHT)
/* Render passes that uses volume transmittance when available */
#define EEVEE_RENDERPASSES_USES_TRANSMITTANCE \
  (EEVEE_RENDER_PASS_DIFFUSE_COLOR | EEVEE_RENDER_PASS_SPECULAR_COLOR | EEVEE_RENDER_PASS_EMIT | \
   EEVEE_RENDER_PASS_ENVIRONMENT)
bool EEVEE_renderpasses_only_first_sample_pass_active(EEVEE_Data *vedata)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PrivateData *g_data = stl->g_data;
  return (g_data->render_passes & ~EEVEE_RENDERPASSES_POST_PROCESS_ON_FIRST_SAMPLE) == 0;
}

/* Calculate the hash for an AOV. The least significant bit is used to store the AOV
 * type the rest of the bits are used for the name hash. */
int EEVEE_renderpasses_aov_hash(const ViewLayerAOV *aov)
{
  int hash = BLI_hash_string(aov->name);
  SET_FLAG_FROM_TEST(hash, aov->type == AOV_TYPE_COLOR, EEVEE_AOV_HASH_COLOR_TYPE_MASK);
  return hash;
}

void EEVEE_renderpasses_init(EEVEE_Data *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PrivateData *g_data = stl->g_data;
  ViewLayer *view_layer = draw_ctx->view_layer;
  View3D *v3d = draw_ctx->v3d;

  if (v3d) {
    const Scene *scene = draw_ctx->scene;
    eViewLayerEEVEEPassType render_pass = v3d->shading.render_pass;
    g_data->aov_hash = 0;

    if (render_pass == EEVEE_RENDER_PASS_BLOOM &&
        ((scene->eevee.flag & SCE_EEVEE_BLOOM_ENABLED) == 0)) {
      render_pass = EEVEE_RENDER_PASS_COMBINED;
    }
    if (render_pass == EEVEE_RENDER_PASS_AOV) {
      ViewLayerAOV *aov = BLI_findstring(
          &view_layer->aovs, v3d->shading.aov_name, offsetof(ViewLayerAOV, name));
      if (aov != NULL) {
        g_data->aov_hash = EEVEE_renderpasses_aov_hash(aov);
      }
      else {
        /* AOV not found in view layer. */
        render_pass = EEVEE_RENDER_PASS_COMBINED;
      }
    }

    g_data->render_passes = render_pass;
  }
  else {
    eViewLayerEEVEEPassType enabled_render_passes = view_layer->eevee.render_passes;

#define ENABLE_FROM_LEGACY(name_legacy, name_eevee) \
  SET_FLAG_FROM_TEST(enabled_render_passes, \
                     (view_layer->passflag & SCE_PASS_##name_legacy) != 0, \
                     EEVEE_RENDER_PASS_##name_eevee);

    ENABLE_FROM_LEGACY(Z, Z)
    ENABLE_FROM_LEGACY(MIST, MIST)
    ENABLE_FROM_LEGACY(NORMAL, NORMAL)
    ENABLE_FROM_LEGACY(SHADOW, SHADOW)
    ENABLE_FROM_LEGACY(AO, AO)
    ENABLE_FROM_LEGACY(EMIT, EMIT)
    ENABLE_FROM_LEGACY(ENVIRONMENT, ENVIRONMENT)
    ENABLE_FROM_LEGACY(DIFFUSE_COLOR, DIFFUSE_COLOR)
    ENABLE_FROM_LEGACY(GLOSSY_COLOR, SPECULAR_COLOR)
    ENABLE_FROM_LEGACY(DIFFUSE_DIRECT, DIFFUSE_LIGHT)
    ENABLE_FROM_LEGACY(GLOSSY_DIRECT, SPECULAR_LIGHT)

    ENABLE_FROM_LEGACY(ENVIRONMENT, ENVIRONMENT)

#undef ENABLE_FROM_LEGACY
    if (DRW_state_is_image_render() && !BLI_listbase_is_empty(&view_layer->aovs)) {
      enabled_render_passes |= EEVEE_RENDER_PASS_AOV;
      g_data->aov_hash = EEVEE_AOV_HASH_ALL;
    }

    g_data->render_passes = (enabled_render_passes & EEVEE_RENDERPASSES_ALL) |
                            EEVEE_RENDER_PASS_COMBINED;
  }
  EEVEE_material_renderpasses_init(vedata);
  EEVEE_cryptomatte_renderpasses_init(vedata);
}

BLI_INLINE bool eevee_renderpasses_volumetric_active(const EEVEE_EffectsInfo *effects,
                                                     const EEVEE_PrivateData *g_data)
{
  if (effects->enabled_effects & EFFECT_VOLUMETRIC) {
    if (g_data->render_passes &
        (EEVEE_RENDER_PASS_VOLUME_LIGHT | EEVEE_RENDERPASSES_USES_TRANSMITTANCE)) {
      return true;
    }
  }
  return false;
}

void EEVEE_renderpasses_output_init(EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata,
                                    uint tot_samples)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_PrivateData *g_data = stl->g_data;

  const bool needs_post_processing = (g_data->render_passes &
                                      EEVEE_RENDERPASSES_WITH_POST_PROCESSING) > 0;
  if (needs_post_processing) {
    /* Create FrameBuffer. */

    /* Should be enough to store the data needs for a single pass.
     * Some passes will use less, but it is only relevant for final renderings and
     * when renderpasses other than `EEVEE_RENDER_PASS_COMBINED` are requested */
    DRW_texture_ensure_fullscreen_2d(&txl->renderpass, GPU_RGBA16F, 0);
    GPU_framebuffer_ensure_config(&fbl->renderpass_fb,
                                  {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->renderpass)});

    if ((g_data->render_passes & EEVEE_RENDERPASSES_MATERIAL) != 0) {
      EEVEE_material_output_init(sldata, vedata, tot_samples);
    }

    if ((g_data->render_passes & EEVEE_RENDER_PASS_MIST) != 0) {
      EEVEE_mist_output_init(sldata, vedata);
    }
    if ((g_data->render_passes & EEVEE_RENDER_PASS_SHADOW) != 0) {
      EEVEE_shadow_output_init(sldata, vedata, tot_samples);
    }

    if ((g_data->render_passes & EEVEE_RENDER_PASS_AO) != 0) {
      EEVEE_occlusion_output_init(sldata, vedata, tot_samples);
    }

    if ((g_data->render_passes & EEVEE_RENDER_PASS_BLOOM) != 0 &&
        (effects->enabled_effects & EFFECT_BLOOM) != 0) {
      EEVEE_bloom_output_init(sldata, vedata, tot_samples);
    }

    if (eevee_renderpasses_volumetric_active(effects, g_data)) {
      EEVEE_volumes_output_init(sldata, vedata, tot_samples);
    }

    /* We set a default texture as not all post processes uses the inputBuffer. */
    g_data->renderpass_input = txl->color;
    g_data->renderpass_col_input = txl->color;
    g_data->renderpass_light_input = txl->color;
    g_data->renderpass_transmittance_input = txl->color;
  }
  else {
    /* Free unneeded memory */
    DRW_TEXTURE_FREE_SAFE(txl->renderpass);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->renderpass_fb);
  }

  /* Cryptomatte doesn't use the GPU shader for post processing */
  if ((g_data->render_passes & (EEVEE_RENDER_PASS_CRYPTOMATTE)) != 0) {
    EEVEE_cryptomatte_output_init(sldata, vedata, tot_samples);
  }
}

void EEVEE_renderpasses_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_PrivateData *g_data = vedata->stl->g_data;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  const bool needs_post_processing = (g_data->render_passes &
                                      EEVEE_RENDERPASSES_WITH_POST_PROCESSING) > 0;
  if (needs_post_processing) {
    DRW_PASS_CREATE(psl->renderpass_pass, DRW_STATE_WRITE_COLOR);
    DRWShadingGroup *grp = DRW_shgroup_create(EEVEE_shaders_renderpasses_post_process_sh_get(),
                                              psl->renderpass_pass);
    DRW_shgroup_uniform_texture_ref(grp, "inputBuffer", &g_data->renderpass_input);
    DRW_shgroup_uniform_texture_ref(grp, "inputColorBuffer", &g_data->renderpass_col_input);
    DRW_shgroup_uniform_texture_ref(
        grp, "inputSecondLightBuffer", &g_data->renderpass_light_input);
    DRW_shgroup_uniform_texture_ref(
        grp, "inputTransmittanceBuffer", &g_data->renderpass_transmittance_input);
    DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
    DRW_shgroup_uniform_block_ref(grp, "common_block", &sldata->common_ubo);
    DRW_shgroup_uniform_block_ref(grp, "renderpass_block", &sldata->renderpass_ubo.combined);
    DRW_shgroup_uniform_int(grp, "currentSample", &g_data->renderpass_current_sample, 1);
    DRW_shgroup_uniform_int(grp, "renderpassType", &g_data->renderpass_type, 1);
    DRW_shgroup_uniform_int(grp, "postProcessType", &g_data->renderpass_postprocess, 1);
    DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), NULL);
  }
  else {
    psl->renderpass_pass = NULL;
  }
}

/* Post-process data to construct a specific render-pass
 *
 * This method will create a shading group to perform the post-processing for the given
 * `renderpass_type`. The post-processing will be done and the result will be stored in the
 * `vedata->txl->renderpass` texture.
 *
 * Only invoke this function for passes that need post-processing.
 *
 * After invoking this function the active frame-buffer is set to `vedata->fbl->renderpass_fb`. */
void EEVEE_renderpasses_postprocess(EEVEE_ViewLayerData *UNUSED(sldata),
                                    EEVEE_Data *vedata,
                                    eViewLayerEEVEEPassType renderpass_type,
                                    int aov_index)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PrivateData *g_data = stl->g_data;
  EEVEE_EffectsInfo *effects = stl->effects;

  /* Compensate for taa_current_sample being incremented after last drawing in
   * EEVEE_temporal_sampling_draw when DRW_state_is_image_render(). */
  const int current_sample = DRW_state_is_image_render() ? effects->taa_current_sample - 1 :
                                                           effects->taa_current_sample;
  g_data->renderpass_current_sample = current_sample;
  g_data->renderpass_type = renderpass_type;
  g_data->renderpass_postprocess = PASS_POST_UNDEFINED;
  const bool volumetric_active = eevee_renderpasses_volumetric_active(effects, g_data);
  eRenderPassPostProcessType default_color_pass_type =
      volumetric_active ? PASS_POST_ACCUMULATED_TRANSMITTANCE_COLOR : PASS_POST_ACCUMULATED_COLOR;
  g_data->renderpass_transmittance_input = volumetric_active ? txl->volume_transmittance_accum :
                                                               txl->color;

  if (!volumetric_active && renderpass_type == EEVEE_RENDER_PASS_VOLUME_LIGHT) {
    /* Early exit: Volumetric effect is off, but the volume light pass was requested. */
    static float clear_col[4] = {0.0f};
    GPU_framebuffer_bind(fbl->renderpass_fb);
    GPU_framebuffer_clear_color(fbl->renderpass_fb, clear_col);
    return;
  }

  switch (renderpass_type) {
    case EEVEE_RENDER_PASS_Z: {
      g_data->renderpass_postprocess = PASS_POST_DEPTH;
      break;
    }
    case EEVEE_RENDER_PASS_AO: {
      g_data->renderpass_postprocess = PASS_POST_AO;
      g_data->renderpass_input = txl->ao_accum;
      break;
    }
    case EEVEE_RENDER_PASS_NORMAL: {
      g_data->renderpass_postprocess = PASS_POST_NORMAL;
      g_data->renderpass_input = effects->ssr_normal_input;
      break;
    }
    case EEVEE_RENDER_PASS_MIST: {
      g_data->renderpass_postprocess = PASS_POST_ACCUMULATED_VALUE;
      g_data->renderpass_input = txl->mist_accum;
      break;
    }
    case EEVEE_RENDER_PASS_VOLUME_LIGHT: {
      g_data->renderpass_postprocess = PASS_POST_ACCUMULATED_COLOR;
      g_data->renderpass_input = txl->volume_scatter_accum;
      break;
    }
    case EEVEE_RENDER_PASS_SHADOW: {
      g_data->renderpass_postprocess = PASS_POST_ACCUMULATED_VALUE;
      g_data->renderpass_input = txl->shadow_accum;
      break;
    }
    case EEVEE_RENDER_PASS_DIFFUSE_COLOR: {
      g_data->renderpass_postprocess = default_color_pass_type;
      g_data->renderpass_input = txl->diff_color_accum;
      break;
    }
    case EEVEE_RENDER_PASS_SPECULAR_COLOR: {
      g_data->renderpass_postprocess = default_color_pass_type;
      g_data->renderpass_input = txl->spec_color_accum;
      break;
    }
    case EEVEE_RENDER_PASS_ENVIRONMENT: {
      g_data->renderpass_postprocess = default_color_pass_type;
      g_data->renderpass_input = txl->env_accum;
      break;
    }
    case EEVEE_RENDER_PASS_EMIT: {
      g_data->renderpass_postprocess = default_color_pass_type;
      g_data->renderpass_input = txl->emit_accum;
      break;
    }
    case EEVEE_RENDER_PASS_SPECULAR_LIGHT: {
      g_data->renderpass_postprocess = PASS_POST_ACCUMULATED_LIGHT;
      g_data->renderpass_input = txl->spec_light_accum;
      g_data->renderpass_col_input = txl->spec_color_accum;
      if ((stl->effects->enabled_effects & EFFECT_SSR) != 0) {
        g_data->renderpass_postprocess = PASS_POST_TWO_LIGHT_BUFFERS;
        g_data->renderpass_light_input = txl->ssr_accum;
      }
      else {
        g_data->renderpass_postprocess = PASS_POST_ACCUMULATED_LIGHT;
      }
      break;
    }
    case EEVEE_RENDER_PASS_DIFFUSE_LIGHT: {
      g_data->renderpass_postprocess = PASS_POST_ACCUMULATED_LIGHT;
      g_data->renderpass_input = txl->diff_light_accum;
      g_data->renderpass_col_input = txl->diff_color_accum;
      if ((stl->effects->enabled_effects & EFFECT_SSS) != 0) {
        g_data->renderpass_postprocess = PASS_POST_TWO_LIGHT_BUFFERS;
        g_data->renderpass_light_input = txl->sss_accum;
      }
      else {
        g_data->renderpass_postprocess = PASS_POST_ACCUMULATED_LIGHT;
      }
      break;
    }
    case EEVEE_RENDER_PASS_AOV: {
      g_data->renderpass_postprocess = PASS_POST_ACCUMULATED_COLOR_ALPHA;
      g_data->renderpass_input = txl->aov_surface_accum[aov_index];
      break;
    }
    case EEVEE_RENDER_PASS_BLOOM: {
      g_data->renderpass_postprocess = PASS_POST_ACCUMULATED_COLOR;
      g_data->renderpass_input = txl->bloom_accum;
      g_data->renderpass_current_sample = 1;
      break;
    }
    default: {
      break;
    }
  }
  GPU_framebuffer_bind(fbl->renderpass_fb);
  DRW_draw_pass(psl->renderpass_pass);
}

void EEVEE_renderpasses_output_accumulate(EEVEE_ViewLayerData *sldata,
                                          EEVEE_Data *vedata,
                                          bool post_effect)
{
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_PrivateData *g_data = stl->g_data;
  eViewLayerEEVEEPassType render_pass = g_data->render_passes;

  if (!post_effect) {
    if ((render_pass & EEVEE_RENDER_PASS_MIST) != 0) {
      EEVEE_mist_output_accumulate(sldata, vedata);
    }
    if ((render_pass & EEVEE_RENDER_PASS_AO) != 0) {
      EEVEE_occlusion_output_accumulate(sldata, vedata);
    }
    if ((render_pass & EEVEE_RENDER_PASS_SHADOW) != 0) {
      EEVEE_shadow_output_accumulate(sldata, vedata);
    }
    if ((render_pass & EEVEE_RENDERPASSES_MATERIAL) != 0) {
      EEVEE_material_output_accumulate(sldata, vedata);
    }
    if (eevee_renderpasses_volumetric_active(effects, g_data)) {
      EEVEE_volumes_output_accumulate(sldata, vedata);
    }
    if ((render_pass & EEVEE_RENDER_PASS_CRYPTOMATTE) != 0) {
      EEVEE_cryptomatte_output_accumulate(sldata, vedata);
    }
  }
  else {
    if ((render_pass & EEVEE_RENDER_PASS_BLOOM) != 0 &&
        (effects->enabled_effects & EFFECT_BLOOM) != 0) {
      EEVEE_bloom_output_accumulate(sldata, vedata);
    }
  }
}

void EEVEE_renderpasses_draw(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  /* We can only draw a single render-pass. Light-passes also select their color pass
   * (a second pass). We mask the light pass when a light pass is selected. */
  const eViewLayerEEVEEPassType render_pass =
      ((stl->g_data->render_passes & EEVEE_RENDERPASSES_LIGHT_PASS) != 0) ?
          (stl->g_data->render_passes & EEVEE_RENDERPASSES_LIGHT_PASS) :
          stl->g_data->render_passes;

  bool is_valid = (render_pass & EEVEE_RENDERPASSES_ALL) != 0;
  bool needs_color_transfer = (render_pass & EEVEE_RENDERPASSES_COLOR_PASS) != 0 &&
                              DRW_state_is_opengl_render();
  UNUSED_VARS(needs_color_transfer);

  if ((render_pass & EEVEE_RENDER_PASS_BLOOM) != 0 &&
      (effects->enabled_effects & EFFECT_BLOOM) == 0) {
    is_valid = false;
  }

  const int current_sample = stl->effects->taa_current_sample;
  const int total_samples = stl->effects->taa_total_sample;
  if ((render_pass & EEVEE_RENDERPASSES_POST_PROCESS_ON_FIRST_SAMPLE) &&
      (current_sample > 1 && total_samples != 1)) {
    return;
  }

  if (is_valid) {
    EEVEE_renderpasses_postprocess(sldata, vedata, render_pass, 0);
    GPU_framebuffer_bind(dfbl->default_fb);
    DRW_transform_none(txl->renderpass);
  }
  else {
    /* Draw state is not valid for this pass, clear the buffer */
    static float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    GPU_framebuffer_bind(dfbl->default_fb);
    GPU_framebuffer_clear_color(dfbl->default_fb, clear_color);
  }
  GPU_framebuffer_bind(fbl->main_fb);
}

void EEVEE_renderpasses_draw_debug(EEVEE_Data *vedata)
{
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_EffectsInfo *effects = stl->effects;

  GPUTexture *tx = NULL;
  /* Debug : Output buffer to view. */
  switch (G.debug_value) {
    case 1:
      tx = txl->maxzbuffer;
      break;
    case 2:
      /* UNUSED */
      break;
    case 3:
      tx = effects->ssr_normal_input;
      break;
    case 4:
      tx = effects->ssr_specrough_input;
      break;
    case 5:
      tx = txl->color_double_buffer;
      break;
    case 6:
      tx = effects->gtao_horizons_renderpass;
      break;
    case 7:
      tx = effects->gtao_horizons_renderpass;
      break;
    case 8:
      tx = effects->sss_irradiance;
      break;
    case 9:
      tx = effects->sss_radius;
      break;
    case 10:
      tx = effects->sss_albedo;
      break;
    case 11:
      tx = effects->velocity_tx;
      break;
    default:
      break;
  }

  if (tx) {
    DRW_transform_none(tx);
  }
}

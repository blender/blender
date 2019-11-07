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

#include "BLI_string_utils.h"

#include "eevee_private.h"

extern char datatoc_common_view_lib_glsl[];
extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_renderpass_postprocess_frag_glsl[];

static struct {
  struct GPUShader *postprocess_sh;
} e_data = {NULL}; /* Engine data */

/* bitmask containing all renderpasses that need post-processing */
#define EEVEE_RENDERPASSES_WITH_POST_PROCESSING \
  (SCE_PASS_Z | SCE_PASS_MIST | SCE_PASS_NORMAL | SCE_PASS_AO | SCE_PASS_SUBSURFACE_COLOR | \
   SCE_PASS_SUBSURFACE_DIRECT)

#define EEVEE_RENDERPASSES_SUBSURFACE \
  (SCE_PASS_SUBSURFACE_COLOR | SCE_PASS_SUBSURFACE_DIRECT | SCE_PASS_SUBSURFACE_INDIRECT)

#define EEVEE_RENDERPASSES_ALL (EEVEE_RENDERPASSES_WITH_POST_PROCESSING | SCE_PASS_COMBINED)

void EEVEE_renderpasses_init(EEVEE_Data *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PrivateData *g_data = stl->g_data;
  ViewLayer *view_layer = draw_ctx->view_layer;

  g_data->render_passes = (view_layer->passflag & EEVEE_RENDERPASSES_ALL) | SCE_PASS_COMBINED;
}

void EEVEE_renderpasses_output_init(EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata,
                                    uint tot_samples)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PrivateData *g_data = stl->g_data;

  const bool needs_post_processing = (g_data->render_passes &
                                      EEVEE_RENDERPASSES_WITH_POST_PROCESSING) > 0;
  if (needs_post_processing) {
    if (e_data.postprocess_sh == NULL) {
      char *frag_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                        datatoc_common_uniforms_lib_glsl,
                                        datatoc_bsdf_common_lib_glsl,
                                        datatoc_renderpass_postprocess_frag_glsl);
      e_data.postprocess_sh = DRW_shader_create_fullscreen(frag_str, NULL);
      MEM_freeN(frag_str);
    }

    /* Create FrameBuffer. */

    /* Should be enough to store the data needs for a single pass.
     * Some passes will use less, but it is only relevant for final renderings and
     * when renderpasses other than `SCE_PASS_COMBINED` are requested */
    DRW_texture_ensure_fullscreen_2d(&txl->renderpass, GPU_RGBA16F, 0);
    GPU_framebuffer_ensure_config(&fbl->renderpass_fb,
                                  {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->renderpass)});

    if ((g_data->render_passes & EEVEE_RENDERPASSES_SUBSURFACE) != 0) {
      EEVEE_subsurface_output_init(sldata, vedata, tot_samples);
    }

    if ((g_data->render_passes & SCE_PASS_MIST) != 0) {
      EEVEE_mist_output_init(sldata, vedata);
    }

    if ((g_data->render_passes & SCE_PASS_AO) != 0) {
      EEVEE_occlusion_output_init(sldata, vedata, tot_samples);
    }

    /* Create Pass. */
    DRW_PASS_CREATE(psl->renderpass_pass, DRW_STATE_WRITE_COLOR);
  }
  else {
    /* Free unneeded memory */
    DRW_TEXTURE_FREE_SAFE(txl->renderpass);
    GPU_FRAMEBUFFER_FREE_SAFE(fbl->renderpass_fb);
    psl->renderpass_pass = NULL;
  }
}

/* Postprocess data to construct a specific renderpass
 *
 * This method will create a shading group to perform the post-processing for the given
 * `renderpass_type`. The post-processing will be done and the result will be stored in the
 * `vedata->txl->renderpass` texture.
 *
 * Only invoke this function for passes that need post-processing.
 *
 * After invoking this function the active framebuffer is set to `vedata->fbl->renderpass_fb`. */
void EEVEE_renderpasses_postprocess(EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata,
                                    eScenePassType renderpass_type)
{
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_EffectsInfo *effects = stl->effects;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  const int current_sample = effects->taa_current_sample;

  DRWShadingGroup *shgrp = DRW_shgroup_create(e_data.postprocess_sh, psl->renderpass_pass);
  DRW_shgroup_uniform_int_copy(shgrp, "renderpassType", renderpass_type);

  switch (renderpass_type) {
    case SCE_PASS_Z: {
      DRW_shgroup_uniform_block(shgrp, "common_block", sldata->common_ubo);
      DRW_shgroup_uniform_texture_ref(shgrp, "depthBuffer", &dtxl->depth);
      break;
    }

    case SCE_PASS_AO: {
      DRW_shgroup_uniform_texture_ref(shgrp, "inputBuffer", &txl->ao_accum);
      DRW_shgroup_uniform_int_copy(shgrp, "currentSample", current_sample);
      break;
    }

    case SCE_PASS_NORMAL: {
      DRW_shgroup_uniform_texture_ref(shgrp, "inputBuffer", &effects->ssr_normal_input);
      break;
    }

    case SCE_PASS_MIST: {
      DRW_shgroup_uniform_texture_ref(shgrp, "inputBuffer", &txl->mist_accum);
      DRW_shgroup_uniform_int_copy(shgrp, "currentSample", current_sample);
      break;
    }

    case SCE_PASS_SUBSURFACE_DIRECT: {
      DRW_shgroup_uniform_texture_ref(shgrp, "inputBuffer", &txl->sss_dir_accum);
      DRW_shgroup_uniform_int_copy(shgrp, "currentSample", current_sample);
      break;
    }

    case SCE_PASS_SUBSURFACE_COLOR: {
      DRW_shgroup_uniform_texture_ref(shgrp, "inputBuffer", &txl->sss_col_accum);
      DRW_shgroup_uniform_int_copy(shgrp, "currentSample", current_sample);
      break;
    }

    default: {
      break;
    }
  }

  DRW_shgroup_call(shgrp, DRW_cache_fullscreen_quad_get(), NULL);

  /* only draw the shading group that has been added. This function can be called multiple times
   * and the pass still hold the previous shading groups.*/
  GPU_framebuffer_bind(fbl->renderpass_fb);
  DRW_draw_pass_subset(psl->renderpass_pass, shgrp, shgrp);
}

void EEVEE_renderpasses_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.postprocess_sh);
}

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
 * Implementation of Blender Mist pass.
 * IMPORTANT: This is a "post process" of the Z depth so it will lack any transparent objects.
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_world_types.h"

#include "BLI_string_utils.h"

#include "eevee_private.h"

extern char datatoc_common_view_lib_glsl[];
extern char datatoc_common_uniforms_lib_glsl[];
extern char datatoc_bsdf_common_lib_glsl[];
extern char datatoc_effect_mist_frag_glsl[];

static struct {
  struct GPUShader *mist_sh;
} e_data = {NULL}; /* Engine data */

void EEVEE_mist_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  EEVEE_FramebufferList *fbl = vedata->fbl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  EEVEE_TextureList *txl = vedata->txl;
  EEVEE_StorageList *stl = vedata->stl;
  EEVEE_PassList *psl = vedata->psl;
  EEVEE_EffectsInfo *effects = stl->effects;
  EEVEE_PrivateData *g_data = stl->g_data;
  Scene *scene = draw_ctx->scene;

  float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  if (e_data.mist_sh == NULL) {
    char *frag_str = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                      datatoc_common_uniforms_lib_glsl,
                                      datatoc_bsdf_common_lib_glsl,
                                      datatoc_effect_mist_frag_glsl);

    e_data.mist_sh = DRW_shader_create_fullscreen(frag_str, "#define FIRST_PASS\n");

    MEM_freeN(frag_str);
  }

  /* Create FrameBuffer. */

  /* Should be enough precision for many samples. */
  DRW_texture_ensure_fullscreen_2d(&txl->mist_accum, GPU_R32F, 0);

  GPU_framebuffer_ensure_config(&fbl->mist_accum_fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->mist_accum)});

  /* Clear texture. */
  if (DRW_state_is_image_render() || effects->taa_current_sample == 1) {
    GPU_framebuffer_bind(fbl->mist_accum_fb);
    GPU_framebuffer_clear_color(fbl->mist_accum_fb, clear);
  }

  /* Mist settings. */
  if (scene && scene->world) {
    g_data->mist_start = scene->world->miststa;
    g_data->mist_inv_dist = (scene->world->mistdist > 0.0f) ? 1.0f / scene->world->mistdist : 0.0f;

    switch (scene->world->mistype) {
      case WO_MIST_QUADRATIC:
        g_data->mist_falloff = 2.0f;
        break;
      case WO_MIST_LINEAR:
        g_data->mist_falloff = 1.0f;
        break;
      case WO_MIST_INVERSE_QUADRATIC:
        g_data->mist_falloff = 0.5f;
        break;
    }
  }
  else {
    float near = -sldata->common_data.view_vecs[0][2];
    float range = sldata->common_data.view_vecs[1][2];
    /* Fallback */
    g_data->mist_start = near;
    g_data->mist_inv_dist = 1.0f / fabsf(range);
    g_data->mist_falloff = 1.0f;
  }

  /* XXX ??!! WHY? If not it does not match cycles. */
  g_data->mist_falloff *= 0.5f;

  /* Create Pass and shgroup. */
  DRW_PASS_CREATE(psl->mist_accum_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ADD);
  DRWShadingGroup *grp = DRW_shgroup_create(e_data.mist_sh, psl->mist_accum_ps);
  DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
  DRW_shgroup_uniform_block(grp, "common_block", sldata->common_ubo);
  DRW_shgroup_uniform_block(grp, "renderpass_block", sldata->renderpass_ubo.combined);
  DRW_shgroup_uniform_vec3(grp, "mistSettings", &g_data->mist_start, 1);
  DRW_shgroup_call(grp, DRW_cache_fullscreen_quad_get(), NULL);
}

void EEVEE_mist_output_accumulate(EEVEE_ViewLayerData *UNUSED(sldata), EEVEE_Data *vedata)
{
  EEVEE_FramebufferList *fbl = vedata->fbl;
  EEVEE_PassList *psl = vedata->psl;

  if (fbl->mist_accum_fb != NULL) {
    GPU_framebuffer_bind(fbl->mist_accum_fb);
    DRW_draw_pass(psl->mist_accum_ps);

    /* Restore */
    GPU_framebuffer_bind(fbl->main_fb);
  }
}

void EEVEE_mist_free(void)
{
  DRW_SHADER_FREE_SAFE(e_data.mist_sh);
}

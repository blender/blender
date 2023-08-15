/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Transparent Pipeline:
 *
 * Use Weight Blended Order Independent Transparency to render transparent surfaces.
 *
 * The rendering is broken down in two passes:
 * - the accumulation pass where we render all the surfaces and accumulate all the weights.
 * - the resolve pass where we divide the accumulated information by the weights.
 *
 * An additional re-render of the transparent surfaces is sometime done in order to have their
 * correct depth and object ids correctly written.
 */

#include "DRW_render.h"

#include "ED_view3d.hh"

#include "workbench_engine.h"
#include "workbench_private.h"

void workbench_transparent_engine_init(WORKBENCH_Data *data)
{
  WORKBENCH_FramebufferList *fbl = data->fbl;
  WORKBENCH_PrivateData *wpd = data->stl->wpd;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  DrawEngineType *owner = (DrawEngineType *)&workbench_transparent_engine_init;

  /* Reuse same format as opaque pipeline to reuse the textures. */
  /* NOTE: Floating point texture is required for the reveal_tex as it is used for
   * the alpha accumulation component (see accumulation shader for more explanation). */
  const eGPUTextureFormat accum_tex_format = GPU_RGBA16F;
  const eGPUTextureFormat reveal_tex_format = NORMAL_ENCODING_ENABLED() ? GPU_RG16F : GPU_RGBA32F;

  eGPUTextureUsage usage = GPU_TEXTURE_USAGE_ATTACHMENT | GPU_TEXTURE_USAGE_SHADER_READ;
  wpd->accum_buffer_tx = DRW_texture_pool_query_fullscreen_ex(accum_tex_format, usage, owner);
  wpd->reveal_buffer_tx = DRW_texture_pool_query_fullscreen_ex(reveal_tex_format, usage, owner);

  GPU_framebuffer_ensure_config(&fbl->transp_accum_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                    GPU_ATTACHMENT_TEXTURE(wpd->accum_buffer_tx),
                                    GPU_ATTACHMENT_TEXTURE(wpd->reveal_buffer_tx),
                                });
}

static void workbench_transparent_lighting_uniforms(WORKBENCH_PrivateData *wpd,
                                                    DRWShadingGroup *grp)
{
  DRW_shgroup_uniform_block(grp, "world_data", wpd->world_ubo);
  DRW_shgroup_uniform_bool_copy(grp, "forceShadowing", false);

  if (STUDIOLIGHT_TYPE_MATCAP_ENABLED(wpd)) {
    BKE_studiolight_ensure_flag(wpd->studio_light,
                                STUDIOLIGHT_MATCAP_DIFFUSE_GPUTEXTURE |
                                    STUDIOLIGHT_MATCAP_SPECULAR_GPUTEXTURE);
    GPUTexture *diff_tx = wpd->studio_light->matcap_diffuse.gputexture;
    GPUTexture *spec_tx = wpd->studio_light->matcap_specular.gputexture;
    const bool use_spec = workbench_is_specular_highlight_enabled(wpd);
    spec_tx = (use_spec && spec_tx) ? spec_tx : diff_tx;
    DRW_shgroup_uniform_texture(grp, "matcap_diffuse_tx", diff_tx);
    DRW_shgroup_uniform_texture(grp, "matcap_specular_tx", spec_tx);
  }
}

void workbench_transparent_cache_init(WORKBENCH_Data *vedata)
{
  WORKBENCH_PassList *psl = vedata->psl;
  WORKBENCH_PrivateData *wpd = vedata->stl->wpd;
  GPUShader *sh;
  DRWShadingGroup *grp;

  {
    int transp = 1;
    for (int infront = 0; infront < 2; infront++) {
      DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_OIT |
                       wpd->cull_state | wpd->clip_state;

      DRWPass *pass;
      if (infront) {
        psl->transp_accum_infront_ps = pass = DRW_pass_create("transp_accum_infront", state);
        DRW_PASS_INSTANCE_CREATE(
            psl->transp_depth_infront_ps, pass, state | DRW_STATE_WRITE_DEPTH);
      }
      else {
        psl->transp_accum_ps = pass = DRW_pass_create("transp_accum", state);
        DRW_PASS_INSTANCE_CREATE(psl->transp_depth_ps, pass, state | DRW_STATE_WRITE_DEPTH);
      }

      for (int data_i = 0; data_i < WORKBENCH_DATATYPE_MAX; data_i++) {
        eWORKBENCH_DataType data = eWORKBENCH_DataType(data_i);
        wpd->prepass[transp][infront][data].material_hash = BLI_ghash_ptr_new(__func__);

        sh = workbench_shader_transparent_get(wpd, data);

        wpd->prepass[transp][infront][data].common_shgrp = grp = DRW_shgroup_create(sh, pass);
        DRW_shgroup_uniform_block(grp, "materials_data", wpd->material_ubo_curr);
        DRW_shgroup_uniform_int_copy(grp, "materialIndex", -1);
        workbench_transparent_lighting_uniforms(wpd, grp);

        wpd->prepass[transp][infront][data].vcol_shgrp = grp = DRW_shgroup_create(sh, pass);
        DRW_shgroup_uniform_block(grp, "materials_data", wpd->material_ubo_curr);
        DRW_shgroup_uniform_int_copy(grp, "materialIndex", 0); /* Default material. (uses vcol) */

        sh = workbench_shader_transparent_image_get(wpd, data, false);

        wpd->prepass[transp][infront][data].image_shgrp = grp = DRW_shgroup_create(sh, pass);
        DRW_shgroup_uniform_block(grp, "materials_data", wpd->material_ubo_curr);
        DRW_shgroup_uniform_int_copy(grp, "materialIndex", 0); /* Default material. */
        workbench_transparent_lighting_uniforms(wpd, grp);

        sh = workbench_shader_transparent_image_get(wpd, data, true);

        wpd->prepass[transp][infront][data].image_tiled_shgrp = grp = DRW_shgroup_create(sh, pass);
        DRW_shgroup_uniform_block(grp, "materials_data", wpd->material_ubo_curr);
        DRW_shgroup_uniform_int_copy(grp, "materialIndex", 0); /* Default material. */
        workbench_transparent_lighting_uniforms(wpd, grp);
      }
    }
  }
  {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA;

    DRW_PASS_CREATE(psl->transp_resolve_ps, state);

    sh = workbench_shader_transparent_resolve_get(wpd);

    grp = DRW_shgroup_create(sh, psl->transp_resolve_ps);
    DRW_shgroup_uniform_texture(grp, "transparentAccum", wpd->accum_buffer_tx);
    DRW_shgroup_uniform_texture(grp, "transparentRevealage", wpd->reveal_buffer_tx);
    DRW_shgroup_call_procedural_triangles(grp, nullptr, 1);
  }
}

void workbench_transparent_draw_depth_pass(WORKBENCH_Data *data)
{
  WORKBENCH_PrivateData *wpd = data->stl->wpd;
  WORKBENCH_FramebufferList *fbl = data->fbl;
  WORKBENCH_PassList *psl = data->psl;

  const bool do_xray_depth_pass = !XRAY_FLAG_ENABLED(wpd) || XRAY_ALPHA(wpd) > 0.0f;
  const bool do_transparent_depth_pass = psl->outline_ps || wpd->dof_enabled || do_xray_depth_pass;

  if (do_transparent_depth_pass) {

    if (!DRW_pass_is_empty(psl->transp_depth_ps)) {
      GPU_framebuffer_bind(fbl->opaque_fb);
      /* TODO(fclem): Disable writing to first two buffers. Unnecessary waste of bandwidth. */
      DRW_draw_pass(psl->transp_depth_ps);
    }

    if (!DRW_pass_is_empty(psl->transp_depth_infront_ps)) {
      GPU_framebuffer_bind(fbl->opaque_infront_fb);
      /* TODO(fclem): Disable writing to first two buffers. Unnecessary waste of bandwidth. */
      DRW_draw_pass(psl->transp_depth_infront_ps);
    }
  }
}

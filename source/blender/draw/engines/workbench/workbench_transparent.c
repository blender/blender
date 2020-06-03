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
 * Copyright 2020, Blender Foundation.
 */

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

#include "ED_view3d.h"

#include "GPU_extensions.h"

#include "workbench_engine.h"
#include "workbench_private.h"

void workbench_transparent_engine_init(WORKBENCH_Data *data)
{
  WORKBENCH_FramebufferList *fbl = data->fbl;
  WORKBENCH_PrivateData *wpd = data->stl->wpd;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  DrawEngineType *owner = (DrawEngineType *)&workbench_transparent_engine_init;

  /* Reuse same format as opaque pipeline to reuse the textures. */
  /* Note: Floating point texture is required for the reveal_tex as it is used for
   * the alpha accumulation component (see accumulation shader for more explanation). */
  const eGPUTextureFormat accum_tex_format = GPU_RGBA16F;
  const eGPUTextureFormat reveal_tex_format = NORMAL_ENCODING_ENABLED() ? GPU_RG16F : GPU_RGBA32F;

  wpd->accum_buffer_tx = DRW_texture_pool_query_fullscreen(accum_tex_format, owner);
  wpd->reveal_buffer_tx = DRW_texture_pool_query_fullscreen(reveal_tex_format, owner);

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
  DRW_shgroup_uniform_block(grp, "world_block", wpd->world_ubo);
  DRW_shgroup_uniform_bool_copy(grp, "forceShadowing", false);

  if (STUDIOLIGHT_TYPE_MATCAP_ENABLED(wpd)) {
    BKE_studiolight_ensure_flag(wpd->studio_light,
                                STUDIOLIGHT_MATCAP_DIFFUSE_GPUTEXTURE |
                                    STUDIOLIGHT_MATCAP_SPECULAR_GPUTEXTURE);
    struct GPUTexture *diff_tx = wpd->studio_light->matcap_diffuse.gputexture;
    struct GPUTexture *spec_tx = wpd->studio_light->matcap_specular.gputexture;
    const bool use_spec = workbench_is_specular_highlight_enabled(wpd);
    spec_tx = (use_spec && spec_tx) ? spec_tx : diff_tx;
    DRW_shgroup_uniform_texture(grp, "matcapDiffuseImage", diff_tx);
    DRW_shgroup_uniform_texture(grp, "matcapSpecularImage", spec_tx);
  }
}

void workbench_transparent_cache_init(WORKBENCH_Data *data)
{
  WORKBENCH_PassList *psl = data->psl;
  WORKBENCH_PrivateData *wpd = data->stl->wpd;
  struct GPUShader *sh;
  DRWShadingGroup *grp;

  {
    int transp = 1;
    for (int infront = 0; infront < 2; infront++) {
      DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_OIT;

      DRWPass *pass;
      if (infront) {
        DRW_PASS_CREATE(psl->transp_accum_infront_ps, state | wpd->cull_state | wpd->clip_state);
        pass = psl->transp_accum_infront_ps;
      }
      else {
        DRW_PASS_CREATE(psl->transp_accum_ps, state | wpd->cull_state | wpd->clip_state);
        pass = psl->transp_accum_ps;
      }

      for (int hair = 0; hair < 2; hair++) {
        wpd->prepass[transp][infront][hair].material_hash = BLI_ghash_ptr_new(__func__);

        sh = workbench_shader_transparent_get(wpd, hair);

        wpd->prepass[transp][infront][hair].common_shgrp = grp = DRW_shgroup_create(sh, pass);
        DRW_shgroup_uniform_block(grp, "material_block", wpd->material_ubo_curr);
        DRW_shgroup_uniform_int_copy(grp, "materialIndex", -1);
        workbench_transparent_lighting_uniforms(wpd, grp);

        wpd->prepass[transp][infront][hair].vcol_shgrp = grp = DRW_shgroup_create(sh, pass);
        DRW_shgroup_uniform_block(grp, "material_block", wpd->material_ubo_curr);
        DRW_shgroup_uniform_int_copy(grp, "materialIndex", 0); /* Default material. (uses vcol) */

        sh = workbench_shader_transparent_image_get(wpd, hair, false);

        wpd->prepass[transp][infront][hair].image_shgrp = grp = DRW_shgroup_create(sh, pass);
        DRW_shgroup_uniform_block(grp, "material_block", wpd->material_ubo_curr);
        DRW_shgroup_uniform_int_copy(grp, "materialIndex", 0); /* Default material. */
        workbench_transparent_lighting_uniforms(wpd, grp);

        sh = workbench_shader_transparent_image_get(wpd, hair, true);

        wpd->prepass[transp][infront][hair].image_tiled_shgrp = grp = DRW_shgroup_create(sh, pass);
        DRW_shgroup_uniform_block(grp, "material_block", wpd->material_ubo_curr);
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
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }
}

/* Redraw the transparent passes but with depth test
 * to output correct outline IDs and depth. */
void workbench_transparent_draw_depth_pass(WORKBENCH_Data *data)
{
  WORKBENCH_PrivateData *wpd = data->stl->wpd;
  WORKBENCH_FramebufferList *fbl = data->fbl;
  WORKBENCH_PassList *psl = data->psl;

  const bool do_xray_depth_pass = XRAY_ALPHA(wpd) > 0.0f;
  const bool do_transparent_depth_pass = psl->outline_ps || wpd->dof_enabled || do_xray_depth_pass;

  if (do_transparent_depth_pass) {
    DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;

    if (!DRW_pass_is_empty(psl->transp_accum_ps)) {
      GPU_framebuffer_bind(fbl->opaque_fb);
      /* TODO(fclem) Disable writting to first two buffers. Unecessary waste of bandwidth. */
      DRW_pass_state_set(psl->transp_accum_ps, state | wpd->cull_state | wpd->clip_state);
      DRW_draw_pass(psl->transp_accum_ps);
    }

    if (!DRW_pass_is_empty(psl->transp_accum_infront_ps)) {
      GPU_framebuffer_bind(fbl->opaque_infront_fb);
      /* TODO(fclem) Disable writting to first two buffers. Unecessary waste of bandwidth. */
      DRW_pass_state_set(psl->transp_accum_infront_ps, state | wpd->cull_state | wpd->clip_state);
      DRW_draw_pass(psl->transp_accum_infront_ps);
    }
  }
}

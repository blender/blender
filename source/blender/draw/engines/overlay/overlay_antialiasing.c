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
 *
 * Overlay antialiasing:
 *
 * Most of the overlays are wires which causes a lot of flickering in motions
 * due to aliasing problems.
 *
 * Our goal is to have a technique that works with single sample per pixel
 * to avoid extra cost of managing MSAA or additional texture buffers and jitters.
 *
 * To solve this we use a simple and effective post-process AA. The technique
 * goes like this:
 *
 * - During wireframe rendering, we output the line color, the line direction
 *   and the distance from the line for the pixel center.
 *
 * - Then, in a post process pass, for each pixels we gather all lines in a search area
 *   that could cover (even partially) the center pixel.
 *   We compute the coverage of each line and do a sorted alpha compositing of them.
 *
 * This technique has one major shortcoming compared to MSAA:
 * - It handles (initial) partial visibility poorly (because of single sample). This makes
 *   overlaping / crossing wires a bit too thin at their intersection.
 *   Wireframe meshes overlaid over solid meshes can have half of the edge missing due to
 *   z-fighting (this has workaround).
 *   Another manifestation of this, is fickering of really dense wireframe if using small
 *   line thickness (also has workaround).
 *
 * The pros of this approach are many:
 *  - Works without geometry shader.
 *  - Can inflate line thickness.
 *  - Coverage is very close to perfect and can even be filtered (Blackman-Harris, gaussian).
 *  - Wires can "bleed" / overlap non-line objects since the filter is in screenspace.
 *  - Only uses one additional lightweight fullscreen buffer (compared to MSAA/SMAA).
 *  - No convergence time (compared to TAA).
 */

#include "DRW_render.h"

#include "ED_screen.h"

#include "overlay_private.h"

void OVERLAY_antialiasing_reset(OVERLAY_Data *vedata)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  pd->antialiasing.sample = 0;
}

void OVERLAY_antialiasing_init(OVERLAY_Data *vedata)
{
  OVERLAY_FramebufferList *fbl = vedata->fbl;
  OVERLAY_TextureList *txl = vedata->txl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  if (!DRW_state_is_fbo()) {
    /* Use default view */
    pd->view_default = (DRWView *)DRW_view_default_get();
    pd->antialiasing.enabled = false;
    return;
  }

  bool need_wire_expansion = (G_draw.block.sizePixel > 1.0f);
  /* TODO Get real userpref option and remove MSAA buffer. */
  pd->antialiasing.enabled = (dtxl->multisample_color != NULL) || need_wire_expansion;

  /* Use default view */
  pd->view_default = (DRWView *)DRW_view_default_get();

  if (pd->antialiasing.enabled) {
    DRW_texture_ensure_fullscreen_2d(&txl->overlay_color_tx, GPU_RGBA8, DRW_TEX_FILTER);
    DRW_texture_ensure_fullscreen_2d(&txl->overlay_line_tx, GPU_RGBA8, 0);

    GPU_framebuffer_ensure_config(
        &fbl->overlay_color_only_fb,
        {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(txl->overlay_color_tx)});
    GPU_framebuffer_ensure_config(
        &fbl->overlay_default_fb,
        {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_TEXTURE(txl->overlay_color_tx)});
    GPU_framebuffer_ensure_config(&fbl->overlay_line_fb,
                                  {GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                   GPU_ATTACHMENT_TEXTURE(txl->overlay_color_tx),
                                   GPU_ATTACHMENT_TEXTURE(txl->overlay_line_tx)});
  }
  else {
    /* Just a copy of the defaults framebuffers. */
    GPU_framebuffer_ensure_config(&fbl->overlay_color_only_fb,
                                  {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(dtxl->color)});
    GPU_framebuffer_ensure_config(
        &fbl->overlay_default_fb,
        {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_TEXTURE(dtxl->color)});
    GPU_framebuffer_ensure_config(
        &fbl->overlay_line_fb,
        {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_TEXTURE(dtxl->color)});
  }
}

void OVERLAY_antialiasing_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_TextureList *txl = vedata->txl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  OVERLAY_PassList *psl = vedata->psl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  struct GPUShader *sh;
  DRWShadingGroup *grp;

  if (pd->antialiasing.enabled) {
    /* TODO Get real userpref option and remove MSAA buffer. */
    const bool do_smooth_lines = (dtxl->multisample_color != NULL);

    DRW_PASS_CREATE(psl->antialiasing_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL);

    sh = OVERLAY_shader_antialiasing();
    grp = DRW_shgroup_create(sh, psl->antialiasing_ps);
    DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
    DRW_shgroup_uniform_bool_copy(grp, "doSmoothLines", do_smooth_lines);
    DRW_shgroup_uniform_texture_ref(grp, "depthTex", &dtxl->depth);
    DRW_shgroup_uniform_texture_ref(grp, "colorTex", &txl->overlay_color_tx);
    DRW_shgroup_uniform_texture_ref(grp, "lineTex", &txl->overlay_line_tx);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }
}

void OVERLAY_antialiasing_cache_finish(OVERLAY_Data *vedata)
{
  OVERLAY_FramebufferList *fbl = vedata->fbl;
  OVERLAY_TextureList *txl = vedata->txl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  if (pd->antialiasing.enabled) {
    GPU_framebuffer_ensure_config(&fbl->overlay_in_front_fb,
                                  {GPU_ATTACHMENT_TEXTURE(dtxl->depth_in_front),
                                   GPU_ATTACHMENT_TEXTURE(txl->overlay_color_tx)});
  }
  else {
    GPU_framebuffer_ensure_config(
        &fbl->overlay_in_front_fb,
        {GPU_ATTACHMENT_TEXTURE(dtxl->depth_in_front), GPU_ATTACHMENT_TEXTURE(dtxl->color)});
  }
}

void OVERLAY_antialiasing_start(OVERLAY_Data *vedata)
{
  OVERLAY_FramebufferList *fbl = vedata->fbl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  if (pd->antialiasing.enabled) {
    float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    GPU_framebuffer_bind(fbl->overlay_line_fb);
    GPU_framebuffer_clear_color(fbl->overlay_line_fb, clear_col);

    GPU_framebuffer_bind(fbl->overlay_default_fb);
  }
}

void OVERLAY_antialiasing_end(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  if (pd->antialiasing.enabled) {
    GPU_framebuffer_bind(dfbl->color_only_fb);
    DRW_draw_pass(psl->antialiasing_ps);

    GPU_framebuffer_bind(dfbl->default_fb);
  }
}

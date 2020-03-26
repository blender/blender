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
 *   overlapping / crossing wires a bit too thin at their intersection.
 *   Wireframe meshes overlaid over solid meshes can have half of the edge missing due to
 *   z-fighting (this has workaround).
 *   Another manifestation of this, is flickering of really dense wireframe if using small
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

void OVERLAY_antialiasing_init(OVERLAY_Data *vedata)
{
  OVERLAY_FramebufferList *fbl = vedata->fbl;
  OVERLAY_TextureList *txl = vedata->txl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  /* Small texture which will have very small impact on rendertime. */
  if (txl->dummy_depth_tx == NULL) {
    float pixel[1] = {1.0f};
    txl->dummy_depth_tx = DRW_texture_create_2d(1, 1, GPU_DEPTH_COMPONENT24, 0, pixel);
  }

  if (!DRW_state_is_fbo()) {
    pd->antialiasing.enabled = false;
    return;
  }

  bool need_wire_expansion = (G_draw.block.sizePixel > 1.0f);
  pd->antialiasing.enabled = need_wire_expansion ||
                             ((U.gpu_flag & USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE) > 0);

  GPUTexture *color_tex = NULL;
  GPUTexture *line_tex = NULL;

  if (pd->antialiasing.enabled) {
    DRW_texture_ensure_fullscreen_2d(&txl->overlay_color_tx, GPU_SRGB8_A8, DRW_TEX_FILTER);
    DRW_texture_ensure_fullscreen_2d(&txl->overlay_line_tx, GPU_RGBA8, 0);

    color_tex = txl->overlay_color_tx;
    line_tex = txl->overlay_line_tx;
  }
  else {
    /* Just a copy of the defaults framebuffers. */
    color_tex = dtxl->color_overlay;
  }

  GPU_framebuffer_ensure_config(&fbl->overlay_color_only_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(color_tex),
                                });
  GPU_framebuffer_ensure_config(&fbl->overlay_default_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                    GPU_ATTACHMENT_TEXTURE(color_tex),
                                });
  GPU_framebuffer_ensure_config(&fbl->overlay_line_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                    GPU_ATTACHMENT_TEXTURE(color_tex),
                                    GPU_ATTACHMENT_TEXTURE(line_tex),
                                });
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
    /* `antialiasing.enabled` is also enabled for wire expansion. Check here if
     * anti aliasing is needed. */
    const bool do_smooth_lines = (U.gpu_flag & USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE) > 0;

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

  /* A bit out of place... not related to antialiasing. */
  if (pd->xray_enabled) {
    DRW_PASS_CREATE(psl->xray_fade_ps, DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_MUL);

    sh = OVERLAY_shader_xray_fade();
    grp = DRW_shgroup_create(sh, psl->xray_fade_ps);
    DRW_shgroup_uniform_texture_ref(grp, "depthTex", &dtxl->depth);
    DRW_shgroup_uniform_texture_ref(grp, "xrayDepthTex", &txl->temp_depth_tx);
    DRW_shgroup_uniform_float_copy(grp, "opacity", 1.0f - pd->xray_opacity);
    DRW_shgroup_call_procedural_triangles(grp, NULL, 1);
  }
}

void OVERLAY_antialiasing_cache_finish(OVERLAY_Data *vedata)
{
  OVERLAY_FramebufferList *fbl = vedata->fbl;
  OVERLAY_TextureList *txl = vedata->txl;
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  if (pd->antialiasing.enabled) {
    GPU_framebuffer_ensure_config(&fbl->overlay_in_front_fb,
                                  {GPU_ATTACHMENT_TEXTURE(dtxl->depth_in_front),
                                   GPU_ATTACHMENT_TEXTURE(txl->overlay_color_tx)});

    GPU_framebuffer_ensure_config(&fbl->overlay_line_in_front_fb,
                                  {GPU_ATTACHMENT_TEXTURE(dtxl->depth_in_front),
                                   GPU_ATTACHMENT_TEXTURE(txl->overlay_color_tx),
                                   GPU_ATTACHMENT_TEXTURE(txl->overlay_line_tx)});
  }
  else {
    GPU_framebuffer_ensure_config(&fbl->overlay_in_front_fb,
                                  {GPU_ATTACHMENT_TEXTURE(dtxl->depth_in_front),
                                   GPU_ATTACHMENT_TEXTURE(dtxl->color_overlay)});

    GPU_framebuffer_ensure_config(&fbl->overlay_line_in_front_fb,
                                  {GPU_ATTACHMENT_TEXTURE(dtxl->depth_in_front),
                                   GPU_ATTACHMENT_TEXTURE(dtxl->color_overlay),
                                   GPU_ATTACHMENT_TEXTURE(txl->overlay_line_tx)});
  }

  pd->antialiasing.do_depth_copy = !(psl->wireframe_ps == NULL ||
                                     DRW_pass_is_empty(psl->wireframe_ps)) ||
                                   (pd->xray_enabled && pd->xray_opacity > 0.0f);
  pd->antialiasing.do_depth_infront_copy = !(psl->wireframe_xray_ps == NULL ||
                                             DRW_pass_is_empty(psl->wireframe_xray_ps));

  const bool do_wireframe = pd->antialiasing.do_depth_copy ||
                            pd->antialiasing.do_depth_infront_copy;
  if (pd->xray_enabled || do_wireframe) {
    DRW_texture_ensure_fullscreen_2d(&txl->temp_depth_tx, GPU_DEPTH24_STENCIL8, 0);

    GPU_framebuffer_ensure_config(&fbl->overlay_xray_depth_copy_fb,
                                  {
                                      GPU_ATTACHMENT_TEXTURE(txl->temp_depth_tx),
                                      GPU_ATTACHMENT_NONE,
                                  });
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
  }

  /* If we are not in solid shading mode, we clear the depth. */
  if (DRW_state_is_fbo() && pd->clear_in_front) {
    /* TODO(fclem) This clear should be done in a global place. */
    GPU_framebuffer_bind(fbl->overlay_in_front_fb);
    GPU_framebuffer_clear_depth(fbl->overlay_in_front_fb, 1.0f);
  }
}

void OVERLAY_xray_depth_copy(OVERLAY_Data *vedata)
{
  OVERLAY_FramebufferList *fbl = vedata->fbl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  if (DRW_state_is_fbo() && pd->antialiasing.do_depth_copy) {
    /* We copy the depth of the rendered geometry to be able to compare to the overlays depth. */
    GPU_framebuffer_blit(
        fbl->overlay_default_fb, 0, fbl->overlay_xray_depth_copy_fb, 0, GPU_DEPTH_BIT);
  }

  if (DRW_state_is_fbo() && pd->xray_enabled) {
    /* We then clear to not occlude the overlays directly. */
    GPU_framebuffer_bind(fbl->overlay_default_fb);
    GPU_framebuffer_clear_depth(fbl->overlay_default_fb, 1.0f);
  }
}

void OVERLAY_xray_depth_infront_copy(OVERLAY_Data *vedata)
{
  OVERLAY_FramebufferList *fbl = vedata->fbl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  if (DRW_state_is_fbo() && pd->antialiasing.do_depth_infront_copy) {
    /* We copy the depth of the rendered geometry to be able to compare to the overlays depth. */
    GPU_framebuffer_blit(
        fbl->overlay_in_front_fb, 0, fbl->overlay_xray_depth_copy_fb, 0, GPU_DEPTH_BIT);
  }
}

void OVERLAY_xray_fade_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;

  if (DRW_state_is_fbo() && pd->xray_enabled && pd->xray_opacity > 0.0f) {
    /* Partially occlude overlays using the geometry depth pass. */
    DRW_draw_pass(psl->xray_fade_ps);
  }
}

void OVERLAY_antialiasing_end(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  if (pd->antialiasing.enabled) {
    GPU_framebuffer_bind(dfbl->overlay_only_fb);
    DRW_draw_pass(psl->antialiasing_ps);
  }
}

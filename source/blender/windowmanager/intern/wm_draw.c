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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup wm
 *
 * Handle OpenGL buffers for windowing, also paint cursor.
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_camera_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BIF_gl.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_scene.h"
#include "BKE_workspace.h"

#include "GHOST_C-api.h"

#include "ED_node.h"
#include "ED_view3d.h"
#include "ED_screen.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"
#include "GPU_texture.h"
#include "GPU_viewport.h"

#include "RE_engine.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_toolsystem.h"
#include "wm.h"
#include "wm_draw.h"
#include "wm_window.h"
#include "wm_event_system.h"

#ifdef WITH_OPENSUBDIV
#  include "BKE_subsurf.h"
#endif

/* ******************* paint cursor *************** */

static void wm_paintcursor_draw(bContext *C, ScrArea *sa, ARegion *ar)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = WM_window_get_active_screen(win);
  wmPaintCursor *pc;

  if (ar->visible && ar == screen->active_region) {
    for (pc = wm->paintcursors.first; pc; pc = pc->next) {

      if ((pc->space_type != SPACE_TYPE_ANY) && (sa->spacetype != pc->space_type)) {
        continue;
      }

      if ((pc->region_type != RGN_TYPE_ANY) && (ar->regiontype != pc->region_type)) {
        continue;
      }

      if (pc->poll == NULL || pc->poll(C)) {
        /* Prevent drawing outside region. */
        glEnable(GL_SCISSOR_TEST);
        glScissor(ar->winrct.xmin,
                  ar->winrct.ymin,
                  BLI_rcti_size_x(&ar->winrct) + 1,
                  BLI_rcti_size_y(&ar->winrct) + 1);

        if (ELEM(win->grabcursor, GHOST_kGrabWrap, GHOST_kGrabHide)) {
          int x = 0, y = 0;
          wm_get_cursor_position(win, &x, &y);
          pc->draw(C, x, y, pc->customdata);
        }
        else {
          pc->draw(C, win->eventstate->x, win->eventstate->y, pc->customdata);
        }

        glDisable(GL_SCISSOR_TEST);
      }
    }
  }
}

static bool wm_draw_region_stereo_set(Main *bmain, ScrArea *sa, ARegion *ar, eStereoViews sview)
{
  /* We could detect better when stereo is actually needed, by inspecting the
   * image in the image editor and sequencer. */
  if (!ELEM(ar->regiontype, RGN_TYPE_WINDOW, RGN_TYPE_PREVIEW)) {
    return false;
  }

  switch (sa->spacetype) {
    case SPACE_IMAGE: {
      if (ar->regiontype == RGN_TYPE_WINDOW) {
        SpaceImage *sima = sa->spacedata.first;
        sima->iuser.multiview_eye = sview;
        return true;
      }
      break;
    }
    case SPACE_VIEW3D: {
      if (ar->regiontype == RGN_TYPE_WINDOW) {
        View3D *v3d = sa->spacedata.first;
        if (v3d->camera && v3d->camera->type == OB_CAMERA) {
          Camera *cam = v3d->camera->data;
          CameraBGImage *bgpic = cam->bg_images.first;
          v3d->multiview_eye = sview;
          if (bgpic) {
            bgpic->iuser.multiview_eye = sview;
          }
          return true;
        }
      }
      break;
    }
    case SPACE_NODE: {
      if (ar->regiontype == RGN_TYPE_WINDOW) {
        SpaceNode *snode = sa->spacedata.first;
        if ((snode->flag & SNODE_BACKDRAW) && ED_node_is_compositor(snode)) {
          Image *ima = BKE_image_verify_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
          ima->eye = sview;
          return true;
        }
      }
      break;
    }
    case SPACE_SEQ: {
      SpaceSeq *sseq = sa->spacedata.first;
      sseq->multiview_eye = sview;

      if (ar->regiontype == RGN_TYPE_PREVIEW) {
        return true;
      }
      else if (ar->regiontype == RGN_TYPE_WINDOW) {
        return (sseq->draw_flag & SEQ_DRAW_BACKDROP) != 0;
      }
    }
  }

  return false;
}

/* ********************* drawing ****************** */

static void wm_area_mark_invalid_backbuf(ScrArea *sa)
{
  if (sa->spacetype == SPACE_VIEW3D) {
    ((View3D *)sa->spacedata.first)->flag |= V3D_INVALID_BACKBUF;
  }
}

static void wm_region_test_gizmo_do_draw(ARegion *ar, bool tag_redraw)
{
  if (ar->gizmo_map == NULL) {
    return;
  }

  wmGizmoMap *gzmap = ar->gizmo_map;
  for (wmGizmoGroup *gzgroup = WM_gizmomap_group_list(gzmap)->first; gzgroup;
       gzgroup = gzgroup->next) {
    for (wmGizmo *gz = gzgroup->gizmos.first; gz; gz = gz->next) {
      if (gz->do_draw) {
        if (tag_redraw) {
          ED_region_tag_redraw_no_rebuild(ar);
        }
        gz->do_draw = false;
      }
    }
  }
}

static void wm_region_test_render_do_draw(const Scene *scene,
                                          struct Depsgraph *depsgraph,
                                          ScrArea *sa,
                                          ARegion *ar)
{
  /* tag region for redraw from render engine preview running inside of it */
  if (sa->spacetype == SPACE_VIEW3D && ar->regiontype == RGN_TYPE_WINDOW) {
    RegionView3D *rv3d = ar->regiondata;
    RenderEngine *engine = rv3d->render_engine;
    GPUViewport *viewport = WM_draw_region_get_viewport(ar, 0);

    if (engine && (engine->flag & RE_ENGINE_DO_DRAW)) {
      View3D *v3d = sa->spacedata.first;
      rcti border_rect;

      /* do partial redraw when possible */
      if (ED_view3d_calc_render_border(scene, depsgraph, v3d, ar, &border_rect)) {
        ED_region_tag_redraw_partial(ar, &border_rect, false);
      }
      else {
        ED_region_tag_redraw_no_rebuild(ar);
      }

      engine->flag &= ~RE_ENGINE_DO_DRAW;
    }
    else if (viewport && GPU_viewport_do_update(viewport)) {
      ED_region_tag_redraw_no_rebuild(ar);
    }
  }
}

static bool wm_region_use_viewport_by_type(short space_type, short region_type)
{
  return (ELEM(space_type, SPACE_VIEW3D, SPACE_IMAGE) && region_type == RGN_TYPE_WINDOW);
}

static bool wm_region_use_viewport(ScrArea *sa, ARegion *ar)
{
  return wm_region_use_viewport_by_type(sa->spacetype, ar->regiontype);
}

/********************** draw all **************************/
/* - reference method, draw all each time                 */

typedef struct WindowDrawCB {
  struct WindowDrawCB *next, *prev;

  void (*draw)(const struct wmWindow *, void *);
  void *customdata;

} WindowDrawCB;

void *WM_draw_cb_activate(wmWindow *win,
                          void (*draw)(const struct wmWindow *, void *),
                          void *customdata)
{
  WindowDrawCB *wdc = MEM_callocN(sizeof(*wdc), "WindowDrawCB");

  BLI_addtail(&win->drawcalls, wdc);
  wdc->draw = draw;
  wdc->customdata = customdata;

  return wdc;
}

void WM_draw_cb_exit(wmWindow *win, void *handle)
{
  for (WindowDrawCB *wdc = win->drawcalls.first; wdc; wdc = wdc->next) {
    if (wdc == (WindowDrawCB *)handle) {
      BLI_remlink(&win->drawcalls, wdc);
      MEM_freeN(wdc);
      return;
    }
  }
}

static void wm_draw_callbacks(wmWindow *win)
{
  for (WindowDrawCB *wdc = win->drawcalls.first; wdc; wdc = wdc->next) {
    wdc->draw(win, wdc->customdata);
  }
}

/************************* Region drawing. ********************************
 *
 * Each region draws into its own framebuffer, which is then blit on the
 * window draw buffer. This helps with fast redrawing if only some regions
 * change. It also means we can share a single context for multiple windows,
 * so that for example VAOs can be shared between windows. */

static void wm_draw_region_buffer_free(ARegion *ar)
{
  if (ar->draw_buffer) {
    for (int view = 0; view < 2; view++) {
      if (ar->draw_buffer->offscreen[view]) {
        GPU_offscreen_free(ar->draw_buffer->offscreen[view]);
      }
      if (ar->draw_buffer->viewport[view]) {
        GPU_viewport_free(ar->draw_buffer->viewport[view]);
      }
    }

    MEM_freeN(ar->draw_buffer);
    ar->draw_buffer = NULL;
  }
}

static void wm_draw_offscreen_texture_parameters(GPUOffScreen *offscreen)
{
  /* Setup offscreen color texture for drawing. */
  GPUTexture *texture = GPU_offscreen_color_texture(offscreen);

  /* We don't support multisample textures here. */
  BLI_assert(GPU_texture_target(texture) == GL_TEXTURE_2D);

  glBindTexture(GL_TEXTURE_2D, GPU_texture_opengl_bindcode(texture));

  /* No mipmaps or filtering. */
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  /* GL_TEXTURE_BASE_LEVEL = 0 by default */
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glBindTexture(GL_TEXTURE_2D, 0);
}

static void wm_draw_region_buffer_create(ARegion *ar, bool stereo, bool use_viewport)
{
  if (ar->draw_buffer) {
    if (ar->draw_buffer->stereo != stereo) {
      /* Free draw buffer on stereo changes. */
      wm_draw_region_buffer_free(ar);
    }
    else {
      /* Free offscreen buffer on size changes. Viewport auto resizes. */
      GPUOffScreen *offscreen = ar->draw_buffer->offscreen[0];
      if (offscreen && (GPU_offscreen_width(offscreen) != ar->winx ||
                        GPU_offscreen_height(offscreen) != ar->winy)) {
        wm_draw_region_buffer_free(ar);
      }
    }
  }

  if (!ar->draw_buffer) {
    if (use_viewport) {
      /* Allocate viewport which includes an offscreen buffer with depth
       * multisample, etc. */
      ar->draw_buffer = MEM_callocN(sizeof(wmDrawBuffer), "wmDrawBuffer");
      ar->draw_buffer->viewport[0] = GPU_viewport_create();
      ar->draw_buffer->viewport[1] = (stereo) ? GPU_viewport_create() : NULL;
    }
    else {
      /* Allocate offscreen buffer if it does not exist. This one has no
       * depth or multisample buffers. 3D view creates own buffers with
       * the data it needs. */
      GPUOffScreen *offscreen = GPU_offscreen_create(ar->winx, ar->winy, 0, false, false, NULL);
      if (!offscreen) {
        return;
      }

      wm_draw_offscreen_texture_parameters(offscreen);

      GPUOffScreen *offscreen_right = NULL;
      if (stereo) {
        offscreen_right = GPU_offscreen_create(ar->winx, ar->winy, 0, false, false, NULL);

        if (!offscreen_right) {
          GPU_offscreen_free(offscreen);
          return;
        }

        wm_draw_offscreen_texture_parameters(offscreen_right);
      }

      ar->draw_buffer = MEM_callocN(sizeof(wmDrawBuffer), "wmDrawBuffer");
      ar->draw_buffer->offscreen[0] = offscreen;
      ar->draw_buffer->offscreen[1] = offscreen_right;
    }

    ar->draw_buffer->bound_view = -1;
    ar->draw_buffer->stereo = stereo;
  }
}

static void wm_draw_region_bind(ARegion *ar, int view)
{
  if (!ar->draw_buffer) {
    return;
  }

  if (ar->draw_buffer->viewport[view]) {
    GPU_viewport_bind(ar->draw_buffer->viewport[view], &ar->winrct);
  }
  else {
    GPU_offscreen_bind(ar->draw_buffer->offscreen[view], false);

    /* For now scissor is expected by region drawing, we could disable it
     * and do the enable/disable in the specific cases that setup scissor. */
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, ar->winx, ar->winy);
  }

  ar->draw_buffer->bound_view = view;
}

static void wm_draw_region_unbind(ARegion *ar, int view)
{
  if (!ar->draw_buffer) {
    return;
  }

  ar->draw_buffer->bound_view = -1;

  if (ar->draw_buffer->viewport[view]) {
    GPU_viewport_unbind(ar->draw_buffer->viewport[view]);
  }
  else {
    glDisable(GL_SCISSOR_TEST);
    GPU_offscreen_unbind(ar->draw_buffer->offscreen[view], false);
  }
}

static void wm_draw_region_blit(ARegion *ar, int view)
{
  if (!ar->draw_buffer) {
    return;
  }

  if (ar->draw_buffer->viewport[view]) {
    GPU_viewport_draw_to_screen(ar->draw_buffer->viewport[view], &ar->winrct);
  }
  else {
    GPU_offscreen_draw_to_screen(
        ar->draw_buffer->offscreen[view], ar->winrct.xmin, ar->winrct.ymin);
  }
}

GPUTexture *wm_draw_region_texture(ARegion *ar, int view)
{
  if (!ar->draw_buffer) {
    return NULL;
  }

  if (ar->draw_buffer->viewport[view]) {
    return GPU_viewport_color_texture(ar->draw_buffer->viewport[view]);
  }
  else {
    return GPU_offscreen_color_texture(ar->draw_buffer->offscreen[view]);
  }
}

void wm_draw_region_blend(ARegion *ar, int view, bool blend)
{
  if (!ar->draw_buffer) {
    return;
  }

  /* Alpha is always 1, except when blend timer is running. */
  float alpha = ED_region_blend_alpha(ar);
  if (alpha <= 0.0f) {
    return;
  }

  if (!blend) {
    alpha = 1.0f;
  }

  /* setup actual texture */
  GPUTexture *texture = wm_draw_region_texture(ar, view);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, GPU_texture_opengl_bindcode(texture));

  /* wmOrtho for the screen has this same offset */
  const float halfx = GLA_PIXEL_OFS / (BLI_rcti_size_x(&ar->winrct) + 1);
  const float halfy = GLA_PIXEL_OFS / (BLI_rcti_size_y(&ar->winrct) + 1);

  if (blend) {
    /* GL_ONE because regions drawn offscreen have premultiplied alpha. */
    GPU_blend(true);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  }

  GPUShader *shader = GPU_shader_get_builtin_shader(GPU_SHADER_2D_IMAGE_RECT_COLOR);
  GPU_shader_bind(shader);

  rcti rect_geo = ar->winrct;
  rect_geo.xmax += 1;
  rect_geo.ymax += 1;

  rctf rect_tex;
  rect_tex.xmin = halfx;
  rect_tex.ymin = halfy;
  rect_tex.xmax = 1.0f + halfx;
  rect_tex.ymax = 1.0f + halfy;

  float alpha_easing = 1.0f - alpha;
  alpha_easing = 1.0f - alpha_easing * alpha_easing;

  /* Slide vertical panels */
  float ofs_x = BLI_rcti_size_x(&ar->winrct) * (1.0f - alpha_easing);
  if (ar->alignment == RGN_ALIGN_RIGHT) {
    rect_geo.xmin += ofs_x;
    rect_tex.xmax *= alpha_easing;
    alpha = 1.0f;
  }
  else if (ar->alignment == RGN_ALIGN_LEFT) {
    rect_geo.xmax -= ofs_x;
    rect_tex.xmin += 1.0f - alpha_easing;
    alpha = 1.0f;
  }

  glUniform1i(GPU_shader_get_uniform_ensure(shader, "image"), 0);
  glUniform4f(GPU_shader_get_uniform_ensure(shader, "rect_icon"),
              rect_tex.xmin,
              rect_tex.ymin,
              rect_tex.xmax,
              rect_tex.ymax);
  glUniform4f(GPU_shader_get_uniform_ensure(shader, "rect_geom"),
              rect_geo.xmin,
              rect_geo.ymin,
              rect_geo.xmax,
              rect_geo.ymax);
  glUniform4f(
      GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_COLOR), alpha, alpha, alpha, alpha);

  GPU_draw_primitive(GPU_PRIM_TRI_STRIP, 4);

  glBindTexture(GL_TEXTURE_2D, 0);

  if (blend) {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GPU_blend(false);
  }
}

GPUViewport *WM_draw_region_get_viewport(ARegion *ar, int view)
{
  if (!ar->draw_buffer) {
    return NULL;
  }

  return ar->draw_buffer->viewport[view];
}

GPUViewport *WM_draw_region_get_bound_viewport(ARegion *ar)
{
  if (!ar->draw_buffer || ar->draw_buffer->bound_view == -1) {
    return NULL;
  }

  int view = ar->draw_buffer->bound_view;
  return ar->draw_buffer->viewport[view];
}

static void wm_draw_window_offscreen(bContext *C, wmWindow *win, bool stereo)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  bScreen *screen = WM_window_get_active_screen(win);

  /* Draw screen areas into own frame buffer. */
  ED_screen_areas_iter(win, screen, sa)
  {
    CTX_wm_area_set(C, sa);

    /* Compute UI layouts for dynamically size regions. */
    for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
      if (ar->visible && ar->do_draw && ar->type && ar->type->layout) {
        CTX_wm_region_set(C, ar);
        ED_region_do_layout(C, ar);
        CTX_wm_region_set(C, NULL);
      }
    }

    ED_area_update_region_sizes(wm, win, sa);

    if (sa->flag & AREA_FLAG_ACTIVE_TOOL_UPDATE) {
      if ((1 << sa->spacetype) & WM_TOOLSYSTEM_SPACE_MASK) {
        WM_toolsystem_update_from_context(C, CTX_wm_workspace(C), CTX_data_view_layer(C), sa);
      }
      sa->flag &= ~AREA_FLAG_ACTIVE_TOOL_UPDATE;
    }

    /* Then do actual drawing of regions. */
    for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
      if (ar->visible && ar->do_draw) {
        CTX_wm_region_set(C, ar);
        bool use_viewport = wm_region_use_viewport(sa, ar);

        if (stereo && wm_draw_region_stereo_set(bmain, sa, ar, STEREO_LEFT_ID)) {
          wm_draw_region_buffer_create(ar, true, use_viewport);

          for (int view = 0; view < 2; view++) {
            eStereoViews sview;
            if (view == 0) {
              sview = STEREO_LEFT_ID;
            }
            else {
              sview = STEREO_RIGHT_ID;
              wm_draw_region_stereo_set(bmain, sa, ar, sview);
            }

            wm_draw_region_bind(ar, view);
            ED_region_do_draw(C, ar);
            wm_draw_region_unbind(ar, view);
          }
        }
        else {
          wm_draw_region_buffer_create(ar, false, use_viewport);
          wm_draw_region_bind(ar, 0);
          ED_region_do_draw(C, ar);
          wm_draw_region_unbind(ar, 0);
        }

        ar->do_draw = false;
        CTX_wm_region_set(C, NULL);
      }
    }

    wm_area_mark_invalid_backbuf(sa);
    CTX_wm_area_set(C, NULL);
  }

  /* Draw menus into their own framebuffer. */
  for (ARegion *ar = screen->regionbase.first; ar; ar = ar->next) {
    if (ar->visible) {
      CTX_wm_menu_set(C, ar);

      if (ar->type && ar->type->layout) {
        /* UI code reads the OpenGL state, but we have to refresh
         * the UI layout beforehand in case the menu size changes. */
        wmViewport(&ar->winrct);
        ar->type->layout(C, ar);
      }

      wm_draw_region_buffer_create(ar, false, false);
      wm_draw_region_bind(ar, 0);
      glClearColor(0, 0, 0, 0);
      glClear(GL_COLOR_BUFFER_BIT);
      ED_region_do_draw(C, ar);
      wm_draw_region_unbind(ar, 0);

      ar->do_draw = false;
      CTX_wm_menu_set(C, NULL);
    }
  }
}

static void wm_draw_window_onscreen(bContext *C, wmWindow *win, int view)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  bScreen *screen = WM_window_get_active_screen(win);

  /* Draw into the window framebuffer, in full window coordinates. */
  wmWindowViewport(win);

  /* We draw on all pixels of the windows so we don't need to clear them before.
   * Actually this is only a problem when resizing the window.
   * If it becomes a problem we should clear only when window size changes. */
#if 0
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
#endif

  /* Blit non-overlapping area regions. */
  ED_screen_areas_iter(win, screen, sa)
  {
    for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
      if (ar->visible && ar->overlap == false) {
        if (view == -1 && ar->draw_buffer && ar->draw_buffer->stereo) {
          /* Stereo drawing from textures. */
          if (win->stereo3d_format->display_mode == S3D_DISPLAY_ANAGLYPH) {
            wm_stereo3d_draw_anaglyph(win, ar);
          }
          else {
            wm_stereo3d_draw_interlace(win, ar);
          }
        }
        else {
          /* Blit from offscreen buffer. */
          wm_draw_region_blit(ar, 0);
        }
      }
    }
  }

  /* Draw paint cursors. */
  if (wm->paintcursors.first) {
    ED_screen_areas_iter(win, screen, sa)
    {
      for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
        if (ar->visible && ar == screen->active_region) {
          CTX_wm_area_set(C, sa);
          CTX_wm_region_set(C, ar);

          /* make region ready for draw, scissor, pixelspace */
          wm_paintcursor_draw(C, sa, ar);

          CTX_wm_region_set(C, NULL);
          CTX_wm_area_set(C, NULL);
        }
      }
    }

    wmWindowViewport(win);
  }

  /* Blend in overlapping area regions */
  ED_screen_areas_iter(win, screen, sa)
  {
    for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
      if (ar->visible && ar->overlap) {
        wm_draw_region_blend(ar, 0, true);
      }
    }
  }

  /* After area regions so we can do area 'overlay' drawing. */
  ED_screen_draw_edges(win);
  wm_draw_callbacks(win);

  /* Blend in floating regions (menus). */
  for (ARegion *ar = screen->regionbase.first; ar; ar = ar->next) {
    if (ar->visible) {
      wm_draw_region_blend(ar, 0, true);
    }
  }

  /* always draw, not only when screen tagged */
  if (win->gesture.first) {
    wm_gesture_draw(win);
  }

  /* needs pixel coords in screen */
  if (wm->drags.first) {
    wm_drags_draw(C, win, NULL);
  }
}

static void wm_draw_window(bContext *C, wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);
  bool stereo = WM_stereo3d_enabled(win, false);

  /* Draw area regions into their own framebuffer. This way we can redraw
   * the areas that need it, and blit the rest from existing framebuffers. */
  wm_draw_window_offscreen(C, win, stereo);

  /* Now we draw into the window framebuffer, in full window coordinates. */
  if (!stereo) {
    /* Regular mono drawing. */
    wm_draw_window_onscreen(C, win, -1);
  }
  else if (win->stereo3d_format->display_mode == S3D_DISPLAY_PAGEFLIP) {
    /* For pageflip we simply draw to both back buffers. */
    glDrawBuffer(GL_BACK_LEFT);
    wm_draw_window_onscreen(C, win, 0);
    glDrawBuffer(GL_BACK_RIGHT);
    wm_draw_window_onscreen(C, win, 1);
    glDrawBuffer(GL_BACK);
  }
  else if (ELEM(win->stereo3d_format->display_mode, S3D_DISPLAY_ANAGLYPH, S3D_DISPLAY_INTERLACE)) {
    /* For anaglyph and interlace, we draw individual regions with
     * stereo framebuffers using different shaders. */
    wm_draw_window_onscreen(C, win, -1);
  }
  else {
    /* For side-by-side and top-bottom, we need to render each view to an
     * an offscreen texture and then draw it. This used to happen for all
     * stereo methods, but it's less efficient than drawing directly. */
    const int width = WM_window_pixels_x(win);
    const int height = WM_window_pixels_y(win);
    GPUOffScreen *offscreen = GPU_offscreen_create(width, height, 0, false, false, NULL);

    if (offscreen) {
      GPUTexture *texture = GPU_offscreen_color_texture(offscreen);
      wm_draw_offscreen_texture_parameters(offscreen);

      for (int view = 0; view < 2; view++) {
        /* Draw view into offscreen buffer. */
        GPU_offscreen_bind(offscreen, false);
        wm_draw_window_onscreen(C, win, view);
        GPU_offscreen_unbind(offscreen, false);

        /* Draw offscreen buffer to screen. */
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, GPU_texture_opengl_bindcode(texture));

        if (win->stereo3d_format->display_mode == S3D_DISPLAY_SIDEBYSIDE) {
          wm_stereo3d_draw_sidebyside(win, view);
        }
        else {
          wm_stereo3d_draw_topbottom(win, view);
        }

        glBindTexture(GL_TEXTURE_2D, 0);
      }

      GPU_offscreen_free(offscreen);
    }
    else {
      /* Still draw something in case of allocation failure. */
      wm_draw_window_onscreen(C, win, 0);
    }
  }

  screen->do_draw = false;
}

/****************** main update call **********************/

/* quick test to prevent changing window drawable */
static bool wm_draw_update_test_window(wmWindow *win)
{
  Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  struct Depsgraph *depsgraph = BKE_scene_get_depsgraph(scene, view_layer, true);
  bScreen *screen = WM_window_get_active_screen(win);
  ARegion *ar;
  bool do_draw = false;

  for (ar = screen->regionbase.first; ar; ar = ar->next) {
    if (ar->do_draw_overlay) {
      screen->do_draw_paintcursor = true;
      ar->do_draw_overlay = false;
    }
    if (ar->visible && ar->do_draw) {
      do_draw = true;
    }
  }

  ED_screen_areas_iter(win, screen, sa)
  {
    for (ar = sa->regionbase.first; ar; ar = ar->next) {
      wm_region_test_gizmo_do_draw(ar, true);
      wm_region_test_render_do_draw(scene, depsgraph, sa, ar);

      if (ar->visible && ar->do_draw) {
        do_draw = true;
      }
    }
  }

  if (do_draw) {
    return true;
  }

  if (screen->do_refresh) {
    return true;
  }
  if (screen->do_draw) {
    return true;
  }
  if (screen->do_draw_gesture) {
    return true;
  }
  if (screen->do_draw_paintcursor) {
    return true;
  }
  if (screen->do_draw_drag) {
    return true;
  }

  return false;
}

/* Clear drawing flags, after drawing is complete so any draw flags set during
 * drawing don't cause any additional redraws. */
static void wm_draw_update_clear_window(wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);

  ED_screen_areas_iter(win, screen, sa)
  {
    for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
      wm_region_test_gizmo_do_draw(ar, false);
    }
  }

  screen->do_draw_gesture = false;
  screen->do_draw_paintcursor = false;
  screen->do_draw_drag = false;
}

void WM_paint_cursor_tag_redraw(wmWindow *win, ARegion *UNUSED(ar))
{
  if (win) {
    bScreen *screen = WM_window_get_active_screen(win);
    screen->do_draw_paintcursor = true;
  }
}

void wm_draw_update(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win;

#ifdef WITH_OPENSUBDIV
  BKE_subsurf_free_unused_buffers();
#endif

  GPU_free_unused_buffers(bmain);

  for (win = wm->windows.first; win; win = win->next) {
#ifdef WIN32
    GHOST_TWindowState state = GHOST_GetWindowState(win->ghostwin);

    if (state == GHOST_kWindowStateMinimized) {
      /* do not update minimized windows, gives issues on Intel (see T33223)
       * and AMD (see T50856). it seems logical to skip update for invisible
       * window anyway.
       */
      continue;
    }
#endif

    if (wm_draw_update_test_window(win)) {
      bScreen *screen = WM_window_get_active_screen(win);

      CTX_wm_window_set(C, win);

      /* sets context window+screen */
      wm_window_make_drawable(wm, win);

      /* notifiers for screen redraw */
      ED_screen_ensure_updated(wm, win, screen);

      wm_draw_window(C, win);
      wm_draw_update_clear_window(win);

      wm_window_swap_buffers(win);

      CTX_wm_window_set(C, NULL);
    }
  }
}

void wm_draw_region_clear(wmWindow *win, ARegion *UNUSED(ar))
{
  bScreen *screen = WM_window_get_active_screen(win);
  screen->do_draw = true;
}

void WM_draw_region_free(ARegion *ar)
{
  wm_draw_region_buffer_free(ar);
  ar->visible = 0;
}

void wm_draw_region_test(bContext *C, ScrArea *sa, ARegion *ar)
{
  /* Function for redraw timer benchmark. */
  bool use_viewport = wm_region_use_viewport(sa, ar);
  wm_draw_region_buffer_create(ar, false, use_viewport);
  wm_draw_region_bind(ar, 0);
  ED_region_do_draw(C, ar);
  wm_draw_region_unbind(ar, 0);
  ar->do_draw = false;
}

void WM_redraw_windows(bContext *C)
{
  wmWindow *win_prev = CTX_wm_window(C);
  ScrArea *area_prev = CTX_wm_area(C);
  ARegion *ar_prev = CTX_wm_region(C);

  wm_draw_update(C);

  CTX_wm_window_set(C, win_prev);
  CTX_wm_area_set(C, area_prev);
  CTX_wm_region_set(C, ar_prev);
}

/* -------------------------------------------------------------------- */
/** \name Region Viewport Drawing
 *
 * This is needed for viewport drawing for operator use
 * (where the viewport may not have drawn yet).
 *
 * Otherwise avoid using these sine they're exposing low level logic externally.
 *
 * \{ */

void WM_draw_region_viewport_ensure(ARegion *ar, short space_type)
{
  bool use_viewport = wm_region_use_viewport_by_type(space_type, ar->regiontype);
  wm_draw_region_buffer_create(ar, false, use_viewport);
}

void WM_draw_region_viewport_bind(ARegion *ar)
{
  wm_draw_region_bind(ar, 0);
}

void WM_draw_region_viewport_unbind(ARegion *ar)
{
  wm_draw_region_unbind(ar, 0);
}

/** \} */

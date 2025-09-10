/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edrend
 */

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "BLI_listbase.h"

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"

#include "wm_window.hh"

#include "render_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Utilities for Finding Areas
 * \{ */

/**
 * Returns biggest area that is not uv/image editor. Note that it uses buttons
 * window as the last possible alternative.
 * would use #BKE_screen_find_big_area(...) but this is too specific.
 */
static ScrArea *biggest_non_image_area(bContext *C)
{
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *big = nullptr;
  int size, maxsize = 0, bwmaxsize = 0;
  short foundwin = 0;

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    if (area->winx > 30 && area->winy > 30) {
      size = area->winx * area->winy;
      if (!area->full && area->spacetype == SPACE_PROPERTIES) {
        if (foundwin == 0 && size > bwmaxsize) {
          bwmaxsize = size;
          big = area;
        }
      }
      else if (area->spacetype != SPACE_IMAGE && size > maxsize) {
        maxsize = size;
        big = area;
        foundwin = 1;
      }
    }
  }

  return big;
}

static ScrArea *find_area_showing_render_result(bContext *C, Scene *scene, wmWindow **r_win)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  ScrArea *area_render = nullptr;
  wmWindow *win_render = nullptr;

  /* find an image-window showing render result */
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (WM_window_get_active_scene(win) != scene) {
      continue;
    }

    const bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->spacetype == SPACE_IMAGE) {
        SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
        if (sima->image && sima->image->type == IMA_TYPE_R_RESULT) {
          area_render = area;
          win_render = win;
          break;
        }
      }
    }
    if (area_render) {
      break;
    }
  }

  *r_win = win_render;
  return area_render;
}

static ScrArea *find_area_image_empty(bContext *C)
{
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area;
  SpaceImage *sima;

  /* find an image-window showing render result */
  for (area = static_cast<ScrArea *>(screen->areabase.first); area; area = area->next) {
    if (area->spacetype == SPACE_IMAGE) {
      sima = static_cast<SpaceImage *>(area->spacedata.first);
      if ((sima->mode == SI_MODE_VIEW) && !sima->image) {
        break;
      }
    }
  }

  return area;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Open Image Editor for Render
 * \{ */

ScrArea *render_view_open(bContext *C, int mx, int my, ReportList *reports)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ScrArea *area = nullptr;
  SpaceImage *sima;
  bool area_was_image = false;

  if (U.render_display_type == USER_RENDER_DISPLAY_NONE) {
    return nullptr;
  }

  if (U.render_display_type == USER_RENDER_DISPLAY_WINDOW) {
    int sizex, sizey;
    BKE_render_resolution(&scene->r, false, &sizex, &sizey);

    sizex += 30 * UI_SCALE_FAC;
    sizey += 60 * UI_SCALE_FAC;

    /* arbitrary... miniature image window views don't make much sense */
    sizex = std::max(sizex, 320);
    sizey = std::max(sizey, 256);

    WM_window_dpi_set_userdef(CTX_wm_window(C));
    rctf *stored_bounds = &U.stored_bounds.image;
    const bool bounds_valid = (stored_bounds && (BLI_rctf_size_x(stored_bounds) > 150.0f) &&
                               (BLI_rctf_size_y(stored_bounds) > 100.0f));
    const bool mm_placement = WM_capabilities_flag() & WM_CAPABILITY_MULTIMONITOR_PLACEMENT;
    if (bounds_valid && mm_placement) {
      mx = int(stored_bounds->xmin * UI_SCALE_FAC);
      my = int(stored_bounds->ymin * UI_SCALE_FAC);
    }

    const rcti window_rect = {
        /*xmin*/ mx,
        /*xmax*/ mx + sizex,
        /*ymin*/ my,
        /*ymax*/ my + sizey,
    };

    /* changes context! */
    if (WM_window_open(C,
                       IFACE_("Blender Render"),
                       &window_rect,
                       SPACE_IMAGE,
                       true,
                       false,
                       true,
                       WIN_ALIGN_ABSOLUTE,
                       nullptr,
                       nullptr) == nullptr)
    {
      BKE_report(reports, RPT_ERROR, "Failed to open window!");
      return nullptr;
    }

    area = CTX_wm_area(C);
    if (BLI_listbase_is_single(&area->spacedata) == false) {
      sima = static_cast<SpaceImage *>(area->spacedata.first);
      sima->flag |= SI_PREVSPACE;
    }
  }
  else if (U.render_display_type == USER_RENDER_DISPLAY_SCREEN) {
    area = CTX_wm_area(C);

    /* If the active screen is already in full-screen mode, skip this and
     * unset the area, so that the full-screen area is just changed later. */
    if (area && area->full) {
      area = nullptr;
    }
    else {
      if (area && area->spacetype == SPACE_IMAGE) {
        area_was_image = true;
      }

      /* this function returns with changed context */
      area = ED_screen_full_newspace(C, area, SPACE_IMAGE);
    }
  }

  if (!area) {
    wmWindow *win_show = nullptr;
    area = find_area_showing_render_result(C, scene, &win_show);
    if (area == nullptr) {
      /* No need to set `win_show` as the area selected will be from the active window. */
      area = find_area_image_empty(C);
    }

    /* if area found in other window, we make that one show in front */
    if (win_show && win_show != CTX_wm_window(C)) {
      wm_window_raise(win_show);
    }

    if (area == nullptr) {
      /* find largest open non-image area */
      area = biggest_non_image_area(C);
      if (area) {
        ED_area_newspace(C, area, SPACE_IMAGE, true);
        sima = static_cast<SpaceImage *>(area->spacedata.first);

        /* Makes "Escape" go back to previous space. */
        sima->flag |= SI_PREVSPACE;

        /* We already had a full-screen here -> mark new space as a stacked full-screen. */
        if (area->full) {
          area->flag |= AREA_FLAG_STACKED_FULLSCREEN;
        }
      }
      else {
        /* use any area of decent size */
        area = BKE_screen_find_big_area(CTX_wm_screen(C), SPACE_TYPE_ANY, 0);
        if (area->spacetype != SPACE_IMAGE) {
          // XXX newspace(area, SPACE_IMAGE);
          sima = static_cast<SpaceImage *>(area->spacedata.first);

          /* Makes "Escape" go back to previous space. */
          sima->flag |= SI_PREVSPACE;
        }
      }
    }
  }
  sima = static_cast<SpaceImage *>(area->spacedata.first);
  sima->link_flag |= SPACE_FLAG_TYPE_TEMPORARY;

  /* get the correct image, and scale it */
  sima->image = BKE_image_ensure_viewer(bmain, IMA_TYPE_R_RESULT, "Render Result");

  /* If we're rendering to full screen, set appropriate hints on image editor
   * so it can restore properly on pressing escape. */
  if (area->full) {
    sima->flag |= SI_FULLWINDOW;

    /* Tell the image editor to revert to previous space in space list on close
     * _only_ if it wasn't already an image editor when the render was invoked */
    if (area_was_image == 0) {
      sima->flag |= SI_PREVSPACE;
    }
    else {
      /* Leave it alone so the image editor will just go back from
       * full screen to the original tiled setup */
    }
  }

  if ((sima->flag & SI_PREVSPACE) && sima->next) {
    SpaceLink *old_sl = sima->next;
    old_sl->link_flag |= SPACE_FLAG_TYPE_WAS_ACTIVE;
  }

  return area;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cancel Render Viewer Operator
 * \{ */

static wmOperatorStatus render_view_cancel_exec(bContext *C, wmOperator * /*op*/)
{
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);

  /* ensure image editor full-screen and area full-screen states are in sync */
  if ((sima->flag & SI_FULLWINDOW) && !area->full) {
    sima->flag &= ~SI_FULLWINDOW;
  }

  /* determine if render already shows */
  if (sima->flag & SI_PREVSPACE) {
    sima->flag &= ~SI_PREVSPACE;

    if (sima->flag & SI_FULLWINDOW) {
      sima->flag &= ~SI_FULLWINDOW;
      ED_screen_full_prevspace(C, area);
    }
    else {
      ED_area_prevspace(C, area);
    }

    return OPERATOR_FINISHED;
  }
  if (sima->flag & SI_FULLWINDOW) {
    sima->flag &= ~SI_FULLWINDOW;
    ED_screen_state_toggle(C, win, area, SCREENMAXIMIZED);
    return OPERATOR_FINISHED;
  }
  if (WM_window_is_temp_screen(win)) {
    wm_window_close(C, CTX_wm_manager(C), win);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_PASS_THROUGH;
}

void RENDER_OT_view_cancel(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cancel Render View";
  ot->description = "Cancel show render view";
  ot->idname = "RENDER_OT_view_cancel";

  /* API callbacks. */
  ot->exec = render_view_cancel_exec;
  ot->poll = ED_operator_image_active;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Show Render Viewer Operator
 * \{ */

static wmOperatorStatus render_view_show_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  wmWindow *wincur = CTX_wm_window(C);

  /* test if we have currently a temp screen active */
  if (WM_window_is_temp_screen(wincur)) {
    wm_window_lower(wincur);
  }
  else {
    wmWindow *win_show = nullptr;
    ScrArea *area = find_area_showing_render_result(C, CTX_data_scene(C), &win_show);

    /* is there another window on current scene showing result? */
    LISTBASE_FOREACH (wmWindow *, win, &CTX_wm_manager(C)->windows) {
      const bScreen *screen = WM_window_get_active_screen(win);

      if ((WM_window_is_temp_screen(win) &&
           ((ScrArea *)screen->areabase.first)->spacetype == SPACE_IMAGE) ||
          (win == win_show && win_show != wincur))
      {
        wm_window_raise(win);
        return OPERATOR_FINISHED;
      }
    }

    /* determine if render already shows */
    if (area) {
      /* but don't close it when rendering */
      if (G.is_rendering == false) {
        SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);

        if (sima->flag & SI_PREVSPACE) {
          sima->flag &= ~SI_PREVSPACE;

          if (sima->flag & SI_FULLWINDOW) {
            sima->flag &= ~SI_FULLWINDOW;
            ED_screen_full_prevspace(C, area);
          }
          else {
            ED_area_prevspace(C, area);
          }
        }
      }
    }
    else {
      render_view_open(C, event->xy[0], event->xy[1], op->reports);
    }
  }

  return OPERATOR_FINISHED;
}

void RENDER_OT_view_show(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Show/Hide Render View";
  ot->description = "Toggle show render view";
  ot->idname = "RENDER_OT_view_show";

  /* API callbacks. */
  ot->invoke = render_view_show_invoke;
  ot->poll = ED_operator_screenactive;
}

/** \} */

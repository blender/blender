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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edscr
 */

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_workspace_types.h"
#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_screen.h"
#include "BKE_scene.h"
#include "BKE_workspace.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_screen_types.h"
#include "ED_clip.h"
#include "ED_node.h"
#include "ED_render.h"

#include "UI_interface.h"

#include "WM_message.h"

#include "DEG_depsgraph_query.h"

#include "screen_intern.h" /* own module include */

/* adds no space data */
static ScrArea *screen_addarea_ex(ScrAreaMap *area_map,
                                  ScrVert *bottom_left,
                                  ScrVert *top_left,
                                  ScrVert *top_right,
                                  ScrVert *bottom_right,
                                  short spacetype)
{
  ScrArea *sa = MEM_callocN(sizeof(ScrArea), "addscrarea");

  sa->v1 = bottom_left;
  sa->v2 = top_left;
  sa->v3 = top_right;
  sa->v4 = bottom_right;
  sa->spacetype = spacetype;

  BLI_addtail(&area_map->areabase, sa);

  return sa;
}
static ScrArea *screen_addarea(bScreen *sc,
                               ScrVert *left_bottom,
                               ScrVert *left_top,
                               ScrVert *right_top,
                               ScrVert *right_bottom,
                               short spacetype)
{
  return screen_addarea_ex(
      AREAMAP_FROM_SCREEN(sc), left_bottom, left_top, right_top, right_bottom, spacetype);
}

static void screen_delarea(bContext *C, bScreen *sc, ScrArea *sa)
{

  ED_area_exit(C, sa);

  BKE_screen_area_free(sa);

  BLI_remlink(&sc->areabase, sa);
  MEM_freeN(sa);
}

ScrArea *area_split(const wmWindow *win, bScreen *sc, ScrArea *sa, char dir, float fac, int merge)
{
  ScrArea *newa = NULL;
  ScrVert *sv1, *sv2;
  short split;
  rcti window_rect;

  if (sa == NULL) {
    return NULL;
  }

  WM_window_rect_calc(win, &window_rect);

  split = screen_geom_find_area_split_point(sa, &window_rect, dir, fac);
  if (split == 0) {
    return NULL;
  }

  /* note regarding (fac > 0.5f) checks below.
   * normally it shouldn't matter which is used since the copy should match the original
   * however with viewport rendering and python console this isn't the case. - campbell */

  if (dir == 'h') {
    /* new vertices */
    sv1 = screen_geom_vertex_add(sc, sa->v1->vec.x, split);
    sv2 = screen_geom_vertex_add(sc, sa->v4->vec.x, split);

    /* new edges */
    screen_geom_edge_add(sc, sa->v1, sv1);
    screen_geom_edge_add(sc, sv1, sa->v2);
    screen_geom_edge_add(sc, sa->v3, sv2);
    screen_geom_edge_add(sc, sv2, sa->v4);
    screen_geom_edge_add(sc, sv1, sv2);

    if (fac > 0.5f) {
      /* new areas: top */
      newa = screen_addarea(sc, sv1, sa->v2, sa->v3, sv2, sa->spacetype);

      /* area below */
      sa->v2 = sv1;
      sa->v3 = sv2;
    }
    else {
      /* new areas: bottom */
      newa = screen_addarea(sc, sa->v1, sv1, sv2, sa->v4, sa->spacetype);

      /* area above */
      sa->v1 = sv1;
      sa->v4 = sv2;
    }

    ED_area_data_copy(newa, sa, true);
  }
  else {
    /* new vertices */
    sv1 = screen_geom_vertex_add(sc, split, sa->v1->vec.y);
    sv2 = screen_geom_vertex_add(sc, split, sa->v2->vec.y);

    /* new edges */
    screen_geom_edge_add(sc, sa->v1, sv1);
    screen_geom_edge_add(sc, sv1, sa->v4);
    screen_geom_edge_add(sc, sa->v2, sv2);
    screen_geom_edge_add(sc, sv2, sa->v3);
    screen_geom_edge_add(sc, sv1, sv2);

    if (fac > 0.5f) {
      /* new areas: right */
      newa = screen_addarea(sc, sv1, sv2, sa->v3, sa->v4, sa->spacetype);

      /* area left */
      sa->v3 = sv2;
      sa->v4 = sv1;
    }
    else {
      /* new areas: left */
      newa = screen_addarea(sc, sa->v1, sa->v2, sv2, sv1, sa->spacetype);

      /* area right */
      sa->v1 = sv1;
      sa->v2 = sv2;
    }

    ED_area_data_copy(newa, sa, true);
  }

  /* remove double vertices en edges */
  if (merge) {
    BKE_screen_remove_double_scrverts(sc);
  }
  BKE_screen_remove_double_scredges(sc);
  BKE_screen_remove_unused_scredges(sc);

  return newa;
}

/**
 * Empty screen, with 1 dummy area without spacedata. Uses window size.
 */
bScreen *screen_add(Main *bmain, const char *name, const rcti *rect)
{
  bScreen *sc;
  ScrVert *sv1, *sv2, *sv3, *sv4;

  sc = BKE_libblock_alloc(bmain, ID_SCR, name, 0);
  sc->do_refresh = true;
  sc->redraws_flag = TIME_ALL_3D_WIN | TIME_ALL_ANIM_WIN;

  sv1 = screen_geom_vertex_add(sc, rect->xmin, rect->ymin);
  sv2 = screen_geom_vertex_add(sc, rect->xmin, rect->ymax - 1);
  sv3 = screen_geom_vertex_add(sc, rect->xmax - 1, rect->ymax - 1);
  sv4 = screen_geom_vertex_add(sc, rect->xmax - 1, rect->ymin);

  screen_geom_edge_add(sc, sv1, sv2);
  screen_geom_edge_add(sc, sv2, sv3);
  screen_geom_edge_add(sc, sv3, sv4);
  screen_geom_edge_add(sc, sv4, sv1);

  /* dummy type, no spacedata */
  screen_addarea(sc, sv1, sv2, sv3, sv4, SPACE_EMPTY);

  return sc;
}

void screen_data_copy(bScreen *to, bScreen *from)
{
  ScrVert *s1, *s2;
  ScrEdge *se;
  ScrArea *sa, *saf;

  /* free contents of 'to', is from blenkernel screen.c */
  BKE_screen_free(to);

  to->flag = from->flag;

  BLI_duplicatelist(&to->vertbase, &from->vertbase);
  BLI_duplicatelist(&to->edgebase, &from->edgebase);
  BLI_duplicatelist(&to->areabase, &from->areabase);
  BLI_listbase_clear(&to->regionbase);

  s2 = to->vertbase.first;
  for (s1 = from->vertbase.first; s1; s1 = s1->next, s2 = s2->next) {
    s1->newv = s2;
  }

  for (se = to->edgebase.first; se; se = se->next) {
    se->v1 = se->v1->newv;
    se->v2 = se->v2->newv;
    BKE_screen_sort_scrvert(&(se->v1), &(se->v2));
  }

  saf = from->areabase.first;
  for (sa = to->areabase.first; sa; sa = sa->next, saf = saf->next) {
    sa->v1 = sa->v1->newv;
    sa->v2 = sa->v2->newv;
    sa->v3 = sa->v3->newv;
    sa->v4 = sa->v4->newv;

    BLI_listbase_clear(&sa->spacedata);
    BLI_listbase_clear(&sa->regionbase);
    BLI_listbase_clear(&sa->actionzones);
    BLI_listbase_clear(&sa->handlers);

    ED_area_data_copy(sa, saf, true);
  }

  /* put at zero (needed?) */
  for (s1 = from->vertbase.first; s1; s1 = s1->next) {
    s1->newv = NULL;
  }
}

/**
 * Prepare a newly created screen for initializing it as active screen.
 */
void screen_new_activate_prepare(const wmWindow *win, bScreen *screen_new)
{
  screen_new->winid = win->winid;
  screen_new->do_refresh = true;
  screen_new->do_draw = true;
}

/* with sa as center, sb is located at: 0=W, 1=N, 2=E, 3=S */
/* -1 = not valid check */
/* used with join operator */
int area_getorientation(ScrArea *sa, ScrArea *sb)
{
  ScrVert *sav1, *sav2, *sav3, *sav4;
  ScrVert *sbv1, *sbv2, *sbv3, *sbv4;

  if (sa == NULL || sb == NULL) {
    return -1;
  }

  sav1 = sa->v1;
  sav2 = sa->v2;
  sav3 = sa->v3;
  sav4 = sa->v4;
  sbv1 = sb->v1;
  sbv2 = sb->v2;
  sbv3 = sb->v3;
  sbv4 = sb->v4;

  if (sav1 == sbv4 && sav2 == sbv3) { /* sa to right of sb = W */
    return 0;
  }
  else if (sav2 == sbv1 && sav3 == sbv4) { /* sa to bottom of sb = N */
    return 1;
  }
  else if (sav3 == sbv2 && sav4 == sbv1) { /* sa to left of sb = E */
    return 2;
  }
  else if (sav1 == sbv2 && sav4 == sbv3) { /* sa on top of sb = S*/
    return 3;
  }

  return -1;
}

/* Helper function to join 2 areas, it has a return value, 0=failed 1=success
 * used by the split, join operators
 */
int screen_area_join(bContext *C, bScreen *scr, ScrArea *sa1, ScrArea *sa2)
{
  int dir;

  dir = area_getorientation(sa1, sa2);
  /*printf("dir is : %i\n", dir);*/

  if (dir == -1) {
    return 0;
  }

  if (dir == 0) {
    sa1->v1 = sa2->v1;
    sa1->v2 = sa2->v2;
    screen_geom_edge_add(scr, sa1->v2, sa1->v3);
    screen_geom_edge_add(scr, sa1->v1, sa1->v4);
  }
  else if (dir == 1) {
    sa1->v2 = sa2->v2;
    sa1->v3 = sa2->v3;
    screen_geom_edge_add(scr, sa1->v1, sa1->v2);
    screen_geom_edge_add(scr, sa1->v3, sa1->v4);
  }
  else if (dir == 2) {
    sa1->v3 = sa2->v3;
    sa1->v4 = sa2->v4;
    screen_geom_edge_add(scr, sa1->v2, sa1->v3);
    screen_geom_edge_add(scr, sa1->v1, sa1->v4);
  }
  else if (dir == 3) {
    sa1->v1 = sa2->v1;
    sa1->v4 = sa2->v4;
    screen_geom_edge_add(scr, sa1->v1, sa1->v2);
    screen_geom_edge_add(scr, sa1->v3, sa1->v4);
  }

  screen_delarea(C, scr, sa2);
  BKE_screen_remove_double_scrverts(scr);
  /* Update preview thumbnail */
  BKE_icon_changed(scr->id.icon_id);

  return 1;
}

/* ****************** EXPORTED API TO OTHER MODULES *************************** */

/* screen sets cursor based on active region */
static void region_cursor_set(wmWindow *win, bool swin_changed)
{
  bScreen *screen = WM_window_get_active_screen(win);

  ED_screen_areas_iter(win, screen, sa)
  {
    for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
      if (ar == screen->active_region) {
        if (swin_changed || (ar->type && ar->type->event_cursor)) {
          if (ar->gizmo_map != NULL) {
            if (WM_gizmomap_cursor_set(ar->gizmo_map, win)) {
              return;
            }
          }
          ED_region_cursor_set(win, sa, ar);
        }
        return;
      }
    }
  }
}

void ED_screen_do_listen(bContext *C, wmNotifier *note)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);

  /* generic notes */
  switch (note->category) {
    case NC_WM:
      if (note->data == ND_FILEREAD) {
        screen->do_draw = true;
      }
      break;
    case NC_WINDOW:
      screen->do_draw = true;
      break;
    case NC_SCREEN:
      if (note->action == NA_EDITED) {
        screen->do_draw = screen->do_refresh = true;
      }
      break;
    case NC_SCENE:
      if (note->data == ND_MODE) {
        region_cursor_set(win, true);
      }
      break;
  }
}

/* make this screen usable */
/* for file read and first use, for scaling window, area moves */
void ED_screen_refresh(wmWindowManager *wm, wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);

  /* exception for bg mode, we only need the screen context */
  if (!G.background) {
    /* header size depends on DPI, let's verify */
    WM_window_set_dpi(win);

    ED_screen_global_areas_refresh(win);

    screen_geom_vertices_scale(win, screen);

    ED_screen_areas_iter(win, screen, area)
    {
      /* set spacetype and region callbacks, calls init() */
      /* sets subwindows for regions, adds handlers */
      ED_area_initialize(wm, win, area);
    }

    /* wake up animtimer */
    if (screen->animtimer) {
      WM_event_timer_sleep(wm, win, screen->animtimer, false);
    }
  }

  if (G.debug & G_DEBUG_EVENTS) {
    printf("%s: set screen\n", __func__);
  }
  screen->do_refresh = false;
  /* prevent multiwin errors */
  screen->winid = win->winid;

  screen->context = ed_screen_context;
}

/* file read, set all screens, ... */
void ED_screens_initialize(Main *bmain, wmWindowManager *wm)
{
  wmWindow *win;

  for (win = wm->windows.first; win; win = win->next) {
    if (BKE_workspace_active_get(win->workspace_hook) == NULL) {
      BKE_workspace_active_set(win->workspace_hook, bmain->workspaces.first);
    }

    ED_screen_refresh(wm, win);
    if (win->eventstate) {
      ED_screen_set_active_region(NULL, win, &win->eventstate->x);
    }
  }

  if (U.uiflag & USER_HEADER_FROM_PREF) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      BKE_screen_header_alignment_reset(screen);
    }
  }
}

void ED_screen_ensure_updated(wmWindowManager *wm, wmWindow *win, bScreen *screen)
{
  if (screen->do_refresh) {
    ED_screen_refresh(wm, win);
  }
}

/* *********** exit calls are for closing running stuff ******** */

void ED_region_exit(bContext *C, ARegion *ar)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  ARegion *prevar = CTX_wm_region(C);

  if (ar->type && ar->type->exit) {
    ar->type->exit(wm, ar);
  }

  CTX_wm_region_set(C, ar);

  WM_event_remove_handlers(C, &ar->handlers);
  WM_event_modal_handler_region_replace(win, ar, NULL);
  WM_draw_region_free(ar);

  if (ar->headerstr) {
    MEM_freeN(ar->headerstr);
    ar->headerstr = NULL;
  }

  if (ar->regiontimer) {
    WM_event_remove_timer(wm, win, ar->regiontimer);
    ar->regiontimer = NULL;
  }

  WM_msgbus_clear_by_owner(wm->message_bus, ar);

  CTX_wm_region_set(C, prevar);
}

void ED_area_exit(bContext *C, ScrArea *sa)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  ScrArea *prevsa = CTX_wm_area(C);
  ARegion *ar;

  if (sa->type && sa->type->exit) {
    sa->type->exit(wm, sa);
  }

  CTX_wm_area_set(C, sa);

  for (ar = sa->regionbase.first; ar; ar = ar->next) {
    ED_region_exit(C, ar);
  }

  WM_event_remove_handlers(C, &sa->handlers);
  WM_event_modal_handler_area_replace(win, sa, NULL);

  CTX_wm_area_set(C, prevsa);
}

void ED_screen_exit(bContext *C, wmWindow *window, bScreen *screen)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *prevwin = CTX_wm_window(C);

  CTX_wm_window_set(C, window);

  if (screen->animtimer) {
    WM_event_remove_timer(wm, window, screen->animtimer);
  }
  screen->animtimer = NULL;
  screen->scrubbing = false;

  screen->active_region = NULL;

  for (ARegion *ar = screen->regionbase.first; ar; ar = ar->next) {
    ED_region_exit(C, ar);
  }
  for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
    ED_area_exit(C, sa);
  }
  /* Don't use ED_screen_areas_iter here, it skips hidden areas. */
  for (ScrArea *sa = window->global_areas.areabase.first; sa; sa = sa->next) {
    ED_area_exit(C, sa);
  }

  /* mark it available for use for other windows */
  screen->winid = 0;

  if (!WM_window_is_temp_screen(prevwin)) {
    /* use previous window if possible */
    CTX_wm_window_set(C, prevwin);
  }
  else {
    /* none otherwise */
    CTX_wm_window_set(C, NULL);
  }
}

/* *********************************** */

/* case when on area-edge or in azones, or outside window */
static void screen_cursor_set(wmWindow *win, const int xy[2])
{
  const bScreen *screen = WM_window_get_active_screen(win);
  AZone *az = NULL;
  ScrArea *sa;

  for (sa = screen->areabase.first; sa; sa = sa->next) {
    if ((az = ED_area_actionzone_find_xy(sa, xy))) {
      break;
    }
  }

  if (sa) {
    if (az->type == AZONE_AREA) {
      WM_cursor_set(win, CURSOR_EDIT);
    }
    else if (az->type == AZONE_REGION) {
      if (az->edge == AE_LEFT_TO_TOPRIGHT || az->edge == AE_RIGHT_TO_TOPLEFT) {
        WM_cursor_set(win, CURSOR_X_MOVE);
      }
      else {
        WM_cursor_set(win, CURSOR_Y_MOVE);
      }
    }
  }
  else {
    ScrEdge *actedge = screen_geom_find_active_scredge(win, screen, xy[0], xy[1]);

    if (actedge) {
      if (screen_geom_edge_is_horizontal(actedge)) {
        WM_cursor_set(win, CURSOR_Y_MOVE);
      }
      else {
        WM_cursor_set(win, CURSOR_X_MOVE);
      }
    }
    else {
      WM_cursor_set(win, CURSOR_STD);
    }
  }
}

/* called in wm_event_system.c. sets state vars in screen, cursors */
/* event type is mouse move */
void ED_screen_set_active_region(bContext *C, wmWindow *win, const int xy[2])
{
  bScreen *scr = WM_window_get_active_screen(win);

  if (scr) {
    ScrArea *sa = NULL;
    ARegion *ar;
    ARegion *old_ar = scr->active_region;

    ED_screen_areas_iter(win, scr, area_iter)
    {
      if (xy[0] > area_iter->totrct.xmin && xy[0] < area_iter->totrct.xmax) {
        if (xy[1] > area_iter->totrct.ymin && xy[1] < area_iter->totrct.ymax) {
          if (ED_area_azones_update(area_iter, xy) == NULL) {
            sa = area_iter;
            break;
          }
        }
      }
    }
    if (sa) {
      /* make overlap active when mouse over */
      for (ar = sa->regionbase.first; ar; ar = ar->next) {
        if (ED_region_contains_xy(ar, xy)) {
          scr->active_region = ar;
          break;
        }
      }
    }
    else {
      scr->active_region = NULL;
    }

    /* check for redraw headers */
    if (old_ar != scr->active_region) {

      ED_screen_areas_iter(win, scr, area_iter)
      {
        bool do_draw = false;

        for (ar = area_iter->regionbase.first; ar; ar = ar->next) {

          /* call old area's deactivate if assigned */
          if (ar == old_ar && area_iter->type->deactivate) {
            area_iter->type->deactivate(area_iter);
          }

          if (ar == old_ar && ar != scr->active_region) {
            wmGizmoMap *gzmap = old_ar->gizmo_map;
            if (gzmap) {
              if (WM_gizmo_highlight_set(gzmap, NULL)) {
                ED_region_tag_redraw_no_rebuild(old_ar);
              }
            }
          }

          if (ar == old_ar || ar == scr->active_region) {
            do_draw = true;
          }
        }

        if (do_draw) {
          for (ar = area_iter->regionbase.first; ar; ar = ar->next) {
            if (ELEM(ar->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
              ED_region_tag_redraw_no_rebuild(ar);
            }
          }
        }
      }
    }

    /* cursors, for time being set always on edges, otherwise aregion doesn't switch */
    if (scr->active_region == NULL) {
      screen_cursor_set(win, xy);
    }
    else {
      /* notifier invokes freeing the buttons... causing a bit too much redraws */
      if (old_ar != scr->active_region) {
        region_cursor_set(win, true);

        /* this used to be a notifier, but needs to be done immediate
         * because it can undo setting the right button as active due
         * to delayed notifier handling */
        if (C) {
          UI_screen_free_active_but(C, scr);
        }
      }
      else {
        region_cursor_set(win, false);
      }
    }
  }
}

int ED_screen_area_active(const bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *sc = CTX_wm_screen(C);
  ScrArea *sa = CTX_wm_area(C);

  if (win && sc && sa) {
    AZone *az = ED_area_actionzone_find_xy(sa, &win->eventstate->x);
    ARegion *ar;

    if (az && az->type == AZONE_REGION) {
      return 1;
    }

    for (ar = sa->regionbase.first; ar; ar = ar->next) {
      if (ar == sc->active_region) {
        return 1;
      }
    }
  }
  return 0;
}

/**
 * Add an area and geometry (screen-edges and -vertices) for it to \a area_map,
 * with coordinates/dimensions matching \a rect.
 */
static ScrArea *screen_area_create_with_geometry(ScrAreaMap *area_map,
                                                 const rcti *rect,
                                                 short spacetype)
{
  ScrVert *bottom_left = screen_geom_vertex_add_ex(area_map, rect->xmin, rect->ymin);
  ScrVert *top_left = screen_geom_vertex_add_ex(area_map, rect->xmin, rect->ymax);
  ScrVert *top_right = screen_geom_vertex_add_ex(area_map, rect->xmax, rect->ymax);
  ScrVert *bottom_right = screen_geom_vertex_add_ex(area_map, rect->xmax, rect->ymin);

  screen_geom_edge_add_ex(area_map, bottom_left, top_left);
  screen_geom_edge_add_ex(area_map, top_left, top_right);
  screen_geom_edge_add_ex(area_map, top_right, bottom_right);
  screen_geom_edge_add_ex(area_map, bottom_right, bottom_left);

  return screen_addarea_ex(area_map, bottom_left, top_left, top_right, bottom_right, spacetype);
}

static void screen_area_set_geometry_rect(ScrArea *sa, const rcti *rect)
{
  sa->v1->vec.x = rect->xmin;
  sa->v1->vec.y = rect->ymin;
  sa->v2->vec.x = rect->xmin;
  sa->v2->vec.y = rect->ymax;
  sa->v3->vec.x = rect->xmax;
  sa->v3->vec.y = rect->ymax;
  sa->v4->vec.x = rect->xmax;
  sa->v4->vec.y = rect->ymin;
}

static void screen_global_area_refresh(wmWindow *win,
                                       bScreen *screen,
                                       eSpace_Type space_type,
                                       GlobalAreaAlign align,
                                       const rcti *rect,
                                       const short height_cur,
                                       const short height_min,
                                       const short height_max)
{
  ScrArea *area;

  for (area = win->global_areas.areabase.first; area; area = area->next) {
    if (area->spacetype == space_type) {
      break;
    }
  }

  if (area) {
    screen_area_set_geometry_rect(area, rect);
  }
  else {
    area = screen_area_create_with_geometry(&win->global_areas, rect, space_type);
    SpaceType *stype = BKE_spacetype_from_id(space_type);
    SpaceLink *slink = stype->new (area, WM_window_get_active_scene(win));

    area->regionbase = slink->regionbase;

    BLI_addhead(&area->spacedata, slink);
    BLI_listbase_clear(&slink->regionbase);

    /* Data specific to global areas. */
    area->global = MEM_callocN(sizeof(*area->global), __func__);
    area->global->size_max = height_max;
    area->global->size_min = height_min;
    area->global->align = align;
  }

  if (area->global->cur_fixed_height != height_cur) {
    /* Refresh layout if size changes. */
    area->global->cur_fixed_height = height_cur;
    screen->do_refresh = true;
  }
}

static int screen_global_header_size(void)
{
  return (int)ceilf(ED_area_headersize() / UI_DPI_FAC);
}

static void screen_global_topbar_area_refresh(wmWindow *win, bScreen *screen)
{
  const short size = screen_global_header_size();
  rcti rect;

  BLI_rcti_init(&rect, 0, WM_window_pixels_x(win) - 1, 0, WM_window_pixels_y(win) - 1);
  rect.ymin = rect.ymax - size;

  screen_global_area_refresh(
      win, screen, SPACE_TOPBAR, GLOBAL_AREA_ALIGN_TOP, &rect, size, size, size);
}

static void screen_global_statusbar_area_refresh(wmWindow *win, bScreen *screen)
{
  const short size_min = 1;
  const short size_max = 0.8f * screen_global_header_size();
  const short size = (screen->flag & SCREEN_COLLAPSE_STATUSBAR) ? size_min : size_max;
  rcti rect;

  BLI_rcti_init(&rect, 0, WM_window_pixels_x(win) - 1, 0, WM_window_pixels_y(win) - 1);
  rect.ymax = rect.ymin + size_max;

  screen_global_area_refresh(
      win, screen, SPACE_STATUSBAR, GLOBAL_AREA_ALIGN_BOTTOM, &rect, size, size_min, size_max);
}

void ED_screen_global_areas_sync(wmWindow *win)
{
  /* Update screen flags from height in window, this is weak and perhaps
   * global areas should just become part of the screen instead. */
  bScreen *screen = BKE_workspace_active_screen_get(win->workspace_hook);

  screen->flag &= ~SCREEN_COLLAPSE_STATUSBAR;

  for (ScrArea *area = win->global_areas.areabase.first; area; area = area->next) {
    if (area->global->cur_fixed_height == area->global->size_min) {
      if (area->spacetype == SPACE_STATUSBAR) {
        screen->flag |= SCREEN_COLLAPSE_STATUSBAR;
      }
    }
  }
}

void ED_screen_global_areas_refresh(wmWindow *win)
{
  /* Don't create global area for child and temporary windows. */
  bScreen *screen = BKE_workspace_active_screen_get(win->workspace_hook);
  if ((win->parent != NULL) || screen->temp) {
    if (win->global_areas.areabase.first) {
      screen->do_refresh = true;
      BKE_screen_area_map_free(&win->global_areas);
    }
    return;
  }

  screen_global_topbar_area_refresh(win, screen);
  screen_global_statusbar_area_refresh(win, screen);
}

/* -------------------------------------------------------------------- */
/* Screen changing */

static bScreen *screen_fullscreen_find_associated_normal_screen(const Main *bmain, bScreen *screen)
{
  for (bScreen *screen_iter = bmain->screens.first; screen_iter;
       screen_iter = screen_iter->id.next) {
    if ((screen_iter != screen) && ELEM(screen_iter->state, SCREENMAXIMIZED, SCREENFULL)) {
      ScrArea *sa = screen_iter->areabase.first;
      if (sa && sa->full == screen) {
        return screen_iter;
      }
    }
  }

  return screen;
}

/**
 * \return the screen to activate.
 * \warning The returned screen may not always equal \a screen_new!
 */
bScreen *screen_change_prepare(
    bScreen *screen_old, bScreen *screen_new, Main *bmain, bContext *C, wmWindow *win)
{
  /* validate screen, it's called with notifier reference */
  if (BLI_findindex(&bmain->screens, screen_new) == -1) {
    return NULL;
  }

  screen_new = screen_fullscreen_find_associated_normal_screen(bmain, screen_new);

  /* check for valid winid */
  if (!(screen_new->winid == 0 || screen_new->winid == win->winid)) {
    return NULL;
  }

  if (screen_old != screen_new) {
    wmTimer *wt = screen_old->animtimer;

    /* remove handlers referencing areas in old screen */
    for (ScrArea *sa = screen_old->areabase.first; sa; sa = sa->next) {
      WM_event_remove_area_handler(&win->modalhandlers, sa);
    }

    /* we put timer to sleep, so screen_exit has to think there's no timer */
    screen_old->animtimer = NULL;
    if (wt) {
      WM_event_timer_sleep(CTX_wm_manager(C), win, wt, true);
    }
    ED_screen_exit(C, win, screen_old);

    /* Same scene, "transfer" playback to new screen. */
    if (wt) {
      screen_new->animtimer = wt;
    }

    return screen_new;
  }

  return NULL;
}

void screen_change_update(bContext *C, wmWindow *win, bScreen *sc)
{
  Scene *scene = WM_window_get_active_scene(win);
  WorkSpace *workspace = BKE_workspace_active_get(win->workspace_hook);
  WorkSpaceLayout *layout = BKE_workspace_layout_find(workspace, sc);

  CTX_wm_window_set(C, win); /* stores C->wm.screen... hrmf */

  ED_screen_refresh(CTX_wm_manager(C), win);

  BKE_screen_view3d_scene_sync(sc, scene); /* sync new screen with scene data */
  WM_event_add_notifier(C, NC_WINDOW, NULL);
  WM_event_add_notifier(C, NC_SCREEN | ND_LAYOUTSET, layout);

  /* makes button hilites work */
  WM_event_add_mousemove(C);
}

/**
 * \brief Change the active screen.
 *
 * Operator call, WM + Window + screen already existed before
 *
 * \warning Do NOT call in area/region queues!
 * \returns if screen changing was successful.
 */
bool ED_screen_change(bContext *C, bScreen *sc)
{
  Main *bmain = CTX_data_main(C);
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen_old = CTX_wm_screen(C);
  bScreen *screen_new = screen_change_prepare(screen_old, sc, bmain, C, win);

  if (screen_new) {
    WorkSpace *workspace = BKE_workspace_active_get(win->workspace_hook);
    WM_window_set_active_screen(win, workspace, sc);
    screen_change_update(C, win, screen_new);

    return true;
  }

  return false;
}

static void screen_set_3dview_camera(Scene *scene, ViewLayer *view_layer, ScrArea *sa, View3D *v3d)
{
  /* fix any cameras that are used in the 3d view but not in the scene */
  BKE_screen_view3d_sync(v3d, scene);

  if (!v3d->camera || !BKE_view_layer_base_find(view_layer, v3d->camera)) {
    v3d->camera = BKE_view_layer_camera_find(view_layer);
    // XXX if (sc == curscreen) handle_view3d_lock();
    if (!v3d->camera) {
      ARegion *ar;
      ListBase *regionbase;

      /* regionbase is in different place depending if space is active */
      if (v3d == sa->spacedata.first) {
        regionbase = &sa->regionbase;
      }
      else {
        regionbase = &v3d->regionbase;
      }

      for (ar = regionbase->first; ar; ar = ar->next) {
        if (ar->regiontype == RGN_TYPE_WINDOW) {
          RegionView3D *rv3d = ar->regiondata;
          if (rv3d->persp == RV3D_CAMOB) {
            rv3d->persp = RV3D_PERSP;
          }
        }
      }
    }
  }
}

void ED_screen_scene_change(bContext *C, wmWindow *win, Scene *scene)
{
#if 0
  ViewLayer *view_layer_old = WM_window_get_active_view_layer(win);
#endif

  /* Switch scene. */
  win->scene = scene;
  if (CTX_wm_window(C) == win) {
    CTX_data_scene_set(C, scene);
  }

  /* Ensure the view layer name is updated. */
  WM_window_ensure_active_view_layer(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);

#if 0
  /* Mode Syncing. */
  if (view_layer_old) {
    WorkSpace *workspace = CTX_wm_workspace(C);
    Object *obact_new = OBACT(view_layer);
    UNUSED_VARS(obact_new);
    eObjectMode object_mode_old = workspace->object_mode;
    Object *obact_old = OBACT(view_layer_old);
    UNUSED_VARS(obact_old, object_mode_old);
  }
#endif

  /* Update 3D view cameras. */
  const bScreen *screen = WM_window_get_active_screen(win);
  for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
    for (SpaceLink *sl = sa->spacedata.first; sl; sl = sl->next) {
      if (sl->spacetype == SPACE_VIEW3D) {
        View3D *v3d = (View3D *)sl;
        screen_set_3dview_camera(scene, view_layer, sa, v3d);
      }
    }
  }
}

ScrArea *ED_screen_full_newspace(bContext *C, ScrArea *sa, int type)
{
  wmWindow *win = CTX_wm_window(C);
  ScrArea *newsa = NULL;

  if (!sa || sa->full == NULL) {
    newsa = ED_screen_state_toggle(C, win, sa, SCREENMAXIMIZED);
  }

  if (!newsa) {
    newsa = sa;
  }

  BLI_assert(newsa);

  if (sa && (sa->spacetype != type)) {
    newsa->flag |= AREA_FLAG_TEMP_TYPE;
  }
  else {
    newsa->flag &= ~AREA_FLAG_TEMP_TYPE;
  }

  ED_area_newspace(C, newsa, type, (newsa->flag & AREA_FLAG_TEMP_TYPE));

  return newsa;
}

/**
 * \a was_prev_temp for the case previous space was a temporary fullscreen as well
 */
void ED_screen_full_prevspace(bContext *C, ScrArea *sa)
{
  BLI_assert(sa->full);

  if (sa->flag & AREA_FLAG_STACKED_FULLSCREEN) {
    /* stacked fullscreen -> only go back to previous screen and don't toggle out of fullscreen */
    ED_area_prevspace(C, sa);
  }
  else {
    ED_screen_restore_temp_type(C, sa);
  }
}

void ED_screen_restore_temp_type(bContext *C, ScrArea *sa)
{
  /* incase nether functions below run */
  ED_area_tag_redraw(sa);

  if (sa->flag & AREA_FLAG_TEMP_TYPE) {
    ED_area_prevspace(C, sa);
    /* Flag should be cleared now. */
    BLI_assert((sa->flag & AREA_FLAG_TEMP_TYPE) == 0);
  }

  if (sa->full) {
    ED_screen_state_toggle(C, CTX_wm_window(C), sa, SCREENMAXIMIZED);
  }
}

/* restore a screen / area back to default operation, after temp fullscreen modes */
void ED_screen_full_restore(bContext *C, ScrArea *sa)
{
  wmWindow *win = CTX_wm_window(C);
  SpaceLink *sl = sa->spacedata.first;
  bScreen *screen = CTX_wm_screen(C);
  short state = (screen ? screen->state : SCREENMAXIMIZED);

  /* if fullscreen area has a temporary space (such as a file browser or fullscreen render
   * overlaid on top of an existing setup) then return to the previous space */

  if (sl->next) {
    if (sa->flag & AREA_FLAG_TEMP_TYPE) {
      ED_screen_full_prevspace(C, sa);
    }
    else {
      ED_screen_state_toggle(C, win, sa, state);
    }
    /* warning: 'sa' may be freed */
  }
  /* otherwise just tile the area again */
  else {
    ED_screen_state_toggle(C, win, sa, state);
  }
}

/**
 * this function toggles: if area is maximized/full then the parent will be restored
 *
 * \warning \a sa may be freed.
 */
ScrArea *ED_screen_state_toggle(bContext *C, wmWindow *win, ScrArea *sa, const short state)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  WorkSpace *workspace = WM_window_get_active_workspace(win);
  bScreen *sc, *oldscreen;
  ARegion *ar;

  if (sa) {
    /* ensure we don't have a button active anymore, can crash when
     * switching screens with tooltip open because region and tooltip
     * are no longer in the same screen */
    for (ar = sa->regionbase.first; ar; ar = ar->next) {
      UI_blocklist_free(C, &ar->uiblocks);

      if (ar->regiontimer) {
        WM_event_remove_timer(wm, NULL, ar->regiontimer);
        ar->regiontimer = NULL;
      }
    }

    /* prevent hanging status prints */
    ED_area_status_text(sa, NULL);
    ED_workspace_status_text(C, NULL);
  }

  if (sa && sa->full) {
    WorkSpaceLayout *layout_old = WM_window_get_active_layout(win);
    /* restoring back to SCREENNORMAL */
    sc = sa->full;                                /* the old screen to restore */
    oldscreen = WM_window_get_active_screen(win); /* the one disappearing */

    BLI_assert(BKE_workspace_layout_screen_get(layout_old) != sc);
    BLI_assert(BKE_workspace_layout_screen_get(layout_old)->state != SCREENNORMAL);

    sc->state = SCREENNORMAL;
    sc->flag = oldscreen->flag;

    /* find old area to restore from */
    ScrArea *fullsa = NULL;
    for (ScrArea *old = sc->areabase.first; old; old = old->next) {
      /* area to restore from is always first */
      if (old->full && !fullsa) {
        fullsa = old;
      }

      /* clear full screen state */
      old->full = NULL;
    }

    sa->full = NULL;

    if (fullsa == NULL) {
      if (G.debug & G_DEBUG) {
        printf("%s: something wrong in areafullscreen\n", __func__);
      }
      return NULL;
    }

    if (state == SCREENFULL) {
      /* unhide global areas */
      for (ScrArea *glob_area = win->global_areas.areabase.first; glob_area;
           glob_area = glob_area->next) {
        glob_area->global->flag &= ~GLOBAL_AREA_IS_HIDDEN;
      }
      /* restore the old side panels/header visibility */
      for (ar = sa->regionbase.first; ar; ar = ar->next) {
        ar->flag = ar->flagfullscreen;
      }
    }

    ED_area_data_swap(fullsa, sa);

    /* animtimer back */
    sc->animtimer = oldscreen->animtimer;
    oldscreen->animtimer = NULL;

    ED_screen_change(C, sc);

    BKE_workspace_layout_remove(CTX_data_main(C), workspace, layout_old);

    /* After we've restored back to SCREENNORMAL, we have to wait with
     * screen handling as it uses the area coords which aren't updated yet.
     * Without doing so, the screen handling gets wrong area coords,
     * which in worst case can lead to crashes (see T43139) */
    sc->skip_handling = true;
  }
  else {
    /* change from SCREENNORMAL to new state */
    WorkSpaceLayout *layout_new;
    ScrArea *newa;
    char newname[MAX_ID_NAME - 2];

    BLI_assert(ELEM(state, SCREENMAXIMIZED, SCREENFULL));

    oldscreen = WM_window_get_active_screen(win);

    oldscreen->state = state;
    BLI_snprintf(newname, sizeof(newname), "%s-%s", oldscreen->id.name + 2, "nonnormal");

    layout_new = ED_workspace_layout_add(bmain, workspace, win, newname);

    sc = BKE_workspace_layout_screen_get(layout_new);
    sc->state = state;
    sc->redraws_flag = oldscreen->redraws_flag;
    sc->temp = oldscreen->temp;
    sc->flag = oldscreen->flag;

    /* timer */
    sc->animtimer = oldscreen->animtimer;
    oldscreen->animtimer = NULL;

    /* use random area when we have no active one, e.g. when the
     * mouse is outside of the window and we open a file browser */
    if (!sa || sa->global) {
      sa = oldscreen->areabase.first;
    }

    newa = (ScrArea *)sc->areabase.first;

    /* copy area */
    ED_area_data_swap(newa, sa);
    newa->flag = sa->flag; /* mostly for AREA_FLAG_WASFULLSCREEN */

    if (state == SCREENFULL) {
      /* temporarily hide global areas */
      for (ScrArea *glob_area = win->global_areas.areabase.first; glob_area;
           glob_area = glob_area->next) {
        glob_area->global->flag |= GLOBAL_AREA_IS_HIDDEN;
      }
      /* temporarily hide the side panels/header */
      for (ar = newa->regionbase.first; ar; ar = ar->next) {
        ar->flagfullscreen = ar->flag;

        if (ELEM(ar->regiontype,
                 RGN_TYPE_UI,
                 RGN_TYPE_HEADER,
                 RGN_TYPE_TOOL_HEADER,
                 RGN_TYPE_FOOTER,
                 RGN_TYPE_TOOLS,
                 RGN_TYPE_NAV_BAR,
                 RGN_TYPE_EXECUTE)) {
          ar->flag |= RGN_FLAG_HIDDEN;
        }
      }
    }

    sa->full = oldscreen;
    newa->full = oldscreen;

    ED_screen_change(C, sc);
  }

  /* XXX bad code: setscreen() ends with first area active. fullscreen render assumes this too */
  CTX_wm_area_set(C, sc->areabase.first);

  return sc->areabase.first;
}

/* update frame rate info for viewport drawing */
void ED_refresh_viewport_fps(bContext *C)
{
  wmTimer *animtimer = CTX_wm_screen(C)->animtimer;
  Scene *scene = CTX_data_scene(C);

  /* is anim playback running? */
  if (animtimer && (U.uiflag & USER_SHOW_FPS)) {
    ScreenFrameRateInfo *fpsi = scene->fps_info;

    /* if there isn't any info, init it first */
    if (fpsi == NULL) {
      fpsi = scene->fps_info = MEM_callocN(sizeof(ScreenFrameRateInfo),
                                           "refresh_viewport_fps fps_info");
    }

    /* update the values */
    fpsi->redrawtime = fpsi->lredrawtime;
    fpsi->lredrawtime = animtimer->ltime;
  }
  else {
    /* playback stopped or shouldn't be running */
    if (scene->fps_info) {
      MEM_freeN(scene->fps_info);
    }
    scene->fps_info = NULL;
  }
}

/* redraws: uses defines from stime->redraws
 * enable: 1 - forward on, -1 - backwards on, 0 - off
 */
void ED_screen_animation_timer(bContext *C, int redraws, int refresh, int sync, int enable)
{
  bScreen *screen = CTX_wm_screen(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  Scene *scene = CTX_data_scene(C);
  bScreen *stopscreen = ED_screen_animation_playing(wm);

  if (stopscreen) {
    WM_event_remove_timer(wm, win, stopscreen->animtimer);
    stopscreen->animtimer = NULL;
  }

  if (enable) {
    ScreenAnimData *sad = MEM_callocN(sizeof(ScreenAnimData), "ScreenAnimData");

    screen->animtimer = WM_event_add_timer(wm, win, TIMER0, (1.0 / FPS));

    sad->ar = CTX_wm_region(C);
    /* if startframe is larger than current frame, we put currentframe on startframe.
     * note: first frame then is not drawn! (ton) */
    if (PRVRANGEON) {
      if (scene->r.psfra > scene->r.cfra) {
        sad->sfra = scene->r.cfra;
        scene->r.cfra = scene->r.psfra;
      }
      else {
        sad->sfra = scene->r.cfra;
      }
    }
    else {
      if (scene->r.sfra > scene->r.cfra) {
        sad->sfra = scene->r.cfra;
        scene->r.cfra = scene->r.sfra;
      }
      else {
        sad->sfra = scene->r.cfra;
      }
    }
    sad->redraws = redraws;
    sad->refresh = refresh;
    sad->flag |= (enable < 0) ? ANIMPLAY_FLAG_REVERSE : 0;
    sad->flag |= (sync == 0) ? ANIMPLAY_FLAG_NO_SYNC : (sync == 1) ? ANIMPLAY_FLAG_SYNC : 0;

    ScrArea *sa = CTX_wm_area(C);

    char spacetype = -1;

    if (sa) {
      spacetype = sa->spacetype;
    }

    sad->from_anim_edit = (ELEM(spacetype, SPACE_GRAPH, SPACE_ACTION, SPACE_NLA));

    screen->animtimer->customdata = sad;
  }

  /* notifier catched by top header, for button */
  WM_event_add_notifier(C, NC_SCREEN | ND_ANIMPLAY, NULL);
}

/* helper for screen_animation_play() - only to be used for TimeLine */
static ARegion *time_top_left_3dwindow(bScreen *screen)
{
  ARegion *aret = NULL;
  ScrArea *sa;
  int min = 10000;

  for (sa = screen->areabase.first; sa; sa = sa->next) {
    if (sa->spacetype == SPACE_VIEW3D) {
      ARegion *ar;
      for (ar = sa->regionbase.first; ar; ar = ar->next) {
        if (ar->regiontype == RGN_TYPE_WINDOW) {
          if (ar->winrct.xmin - ar->winrct.ymin < min) {
            aret = ar;
            min = ar->winrct.xmin - ar->winrct.ymin;
          }
        }
      }
    }
  }

  return aret;
}

void ED_screen_animation_timer_update(bScreen *screen, int redraws, int refresh)
{
  if (screen && screen->animtimer) {
    wmTimer *wt = screen->animtimer;
    ScreenAnimData *sad = wt->customdata;

    sad->redraws = redraws;
    sad->refresh = refresh;
    sad->ar = NULL;
    if (redraws & TIME_REGION) {
      sad->ar = time_top_left_3dwindow(screen);
    }
  }
}

/* results in fully updated anim system */
void ED_update_for_newframe(Main *bmain, Depsgraph *depsgraph)
{
  Scene *scene = DEG_get_input_scene(depsgraph);

  DEG_id_tag_update_ex(bmain, &scene->id, ID_RECALC_TIME);

#ifdef DURIAN_CAMERA_SWITCH
  void *camera = BKE_scene_camera_switch_find(scene);
  if (camera && scene->camera != camera) {
    bScreen *sc;
    scene->camera = camera;
    /* are there cameras in the views that are not in the scene? */
    for (sc = bmain->screens.first; sc; sc = sc->id.next) {
      BKE_screen_view3d_scene_sync(sc, scene);
    }
    DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE);
  }
#endif

  ED_clip_update_frame(bmain, scene->r.cfra);

  /* this function applies the changes too */
  BKE_scene_graph_update_for_newframe(depsgraph, bmain);
}

/*
 * return true if any active area requires to see in 3D
 */
bool ED_screen_stereo3d_required(const bScreen *screen, const Scene *scene)
{
  ScrArea *sa;
  const bool is_multiview = (scene->r.scemode & R_MULTIVIEW) != 0;

  for (sa = screen->areabase.first; sa; sa = sa->next) {
    switch (sa->spacetype) {
      case SPACE_VIEW3D: {
        View3D *v3d;

        if (!is_multiview) {
          continue;
        }

        v3d = sa->spacedata.first;
        if (v3d->camera && v3d->stereo3d_camera == STEREO_3D_ID) {
          ARegion *ar;
          for (ar = sa->regionbase.first; ar; ar = ar->next) {
            if (ar->regiondata && ar->regiontype == RGN_TYPE_WINDOW) {
              RegionView3D *rv3d = ar->regiondata;
              if (rv3d->persp == RV3D_CAMOB) {
                return true;
              }
            }
          }
        }
        break;
      }
      case SPACE_IMAGE: {
        SpaceImage *sima;

        /* images should always show in stereo, even if
         * the file doesn't have views enabled */
        sima = sa->spacedata.first;
        if (sima->image && BKE_image_is_stereo(sima->image) &&
            (sima->iuser.flag & IMA_SHOW_STEREO)) {
          return true;
        }
        break;
      }
      case SPACE_NODE: {
        SpaceNode *snode;

        if (!is_multiview) {
          continue;
        }

        snode = sa->spacedata.first;
        if ((snode->flag & SNODE_BACKDRAW) && ED_node_is_compositor(snode)) {
          return true;
        }
        break;
      }
      case SPACE_SEQ: {
        SpaceSeq *sseq;

        if (!is_multiview) {
          continue;
        }

        sseq = sa->spacedata.first;
        if (ELEM(sseq->view, SEQ_VIEW_PREVIEW, SEQ_VIEW_SEQUENCE_PREVIEW)) {
          return true;
        }

        if (sseq->draw_flag & SEQ_DRAW_BACKDROP) {
          return true;
        }

        break;
      }
    }
  }

  return false;
}

/**
 * Find the scene displayed in \a screen.
 * \note Assumes \a screen to be visible/active!
 */

Scene *ED_screen_scene_find_with_window(const bScreen *screen,
                                        const wmWindowManager *wm,
                                        struct wmWindow **r_window)
{
  for (wmWindow *win = wm->windows.first; win; win = win->next) {
    if (WM_window_get_active_screen(win) == screen) {
      if (r_window) {
        *r_window = win;
      }
      return WM_window_get_active_scene(win);
    }
  }

  BLI_assert(0);
  return NULL;
}

ScrArea *ED_screen_area_find_with_spacedata(const bScreen *screen,
                                            const SpaceLink *sl,
                                            const bool only_visible)
{
  if (only_visible) {
    for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
      if (sa->spacedata.first == sl) {
        return sa;
      }
    }
  }
  else {
    for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
      if (BLI_findindex(&sa->spacedata, sl) != -1) {
        return sa;
      }
    }
  }
  return NULL;
}

Scene *ED_screen_scene_find(const bScreen *screen, const wmWindowManager *wm)
{
  return ED_screen_scene_find_with_window(screen, wm, NULL);
}

wmWindow *ED_screen_window_find(const bScreen *screen, const wmWindowManager *wm)
{
  for (wmWindow *win = wm->windows.first; win; win = win->next) {
    if (WM_window_get_active_screen(win) == screen) {
      return win;
    }
  }
  return NULL;
}

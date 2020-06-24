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

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_workspace_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_sound.h"
#include "BKE_workspace.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_clip.h"
#include "ED_node.h"
#include "ED_screen.h"
#include "ED_screen_types.h"

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
  ScrArea *area = MEM_callocN(sizeof(ScrArea), "addscrarea");

  area->v1 = bottom_left;
  area->v2 = top_left;
  area->v3 = top_right;
  area->v4 = bottom_right;
  area->spacetype = spacetype;

  BLI_addtail(&area_map->areabase, area);

  return area;
}
static ScrArea *screen_addarea(bScreen *screen,
                               ScrVert *left_bottom,
                               ScrVert *left_top,
                               ScrVert *right_top,
                               ScrVert *right_bottom,
                               short spacetype)
{
  return screen_addarea_ex(
      AREAMAP_FROM_SCREEN(screen), left_bottom, left_top, right_top, right_bottom, spacetype);
}

static void screen_delarea(bContext *C, bScreen *screen, ScrArea *area)
{

  ED_area_exit(C, area);

  BKE_screen_area_free(area);

  BLI_remlink(&screen->areabase, area);
  MEM_freeN(area);
}

ScrArea *area_split(
    const wmWindow *win, bScreen *screen, ScrArea *area, char dir, float fac, int merge)
{
  ScrArea *newa = NULL;
  ScrVert *sv1, *sv2;
  short split;
  rcti window_rect;

  if (area == NULL) {
    return NULL;
  }

  WM_window_rect_calc(win, &window_rect);

  split = screen_geom_find_area_split_point(area, &window_rect, dir, fac);
  if (split == 0) {
    return NULL;
  }

  /* note regarding (fac > 0.5f) checks below.
   * normally it shouldn't matter which is used since the copy should match the original
   * however with viewport rendering and python console this isn't the case. - campbell */

  if (dir == 'h') {
    /* new vertices */
    sv1 = screen_geom_vertex_add(screen, area->v1->vec.x, split);
    sv2 = screen_geom_vertex_add(screen, area->v4->vec.x, split);

    /* new edges */
    screen_geom_edge_add(screen, area->v1, sv1);
    screen_geom_edge_add(screen, sv1, area->v2);
    screen_geom_edge_add(screen, area->v3, sv2);
    screen_geom_edge_add(screen, sv2, area->v4);
    screen_geom_edge_add(screen, sv1, sv2);

    if (fac > 0.5f) {
      /* new areas: top */
      newa = screen_addarea(screen, sv1, area->v2, area->v3, sv2, area->spacetype);

      /* area below */
      area->v2 = sv1;
      area->v3 = sv2;
    }
    else {
      /* new areas: bottom */
      newa = screen_addarea(screen, area->v1, sv1, sv2, area->v4, area->spacetype);

      /* area above */
      area->v1 = sv1;
      area->v4 = sv2;
    }

    ED_area_data_copy(newa, area, true);
  }
  else {
    /* new vertices */
    sv1 = screen_geom_vertex_add(screen, split, area->v1->vec.y);
    sv2 = screen_geom_vertex_add(screen, split, area->v2->vec.y);

    /* new edges */
    screen_geom_edge_add(screen, area->v1, sv1);
    screen_geom_edge_add(screen, sv1, area->v4);
    screen_geom_edge_add(screen, area->v2, sv2);
    screen_geom_edge_add(screen, sv2, area->v3);
    screen_geom_edge_add(screen, sv1, sv2);

    if (fac > 0.5f) {
      /* new areas: right */
      newa = screen_addarea(screen, sv1, sv2, area->v3, area->v4, area->spacetype);

      /* area left */
      area->v3 = sv2;
      area->v4 = sv1;
    }
    else {
      /* new areas: left */
      newa = screen_addarea(screen, area->v1, area->v2, sv2, sv1, area->spacetype);

      /* area right */
      area->v1 = sv1;
      area->v2 = sv2;
    }

    ED_area_data_copy(newa, area, true);
  }

  /* remove double vertices en edges */
  if (merge) {
    BKE_screen_remove_double_scrverts(screen);
  }
  BKE_screen_remove_double_scredges(screen);
  BKE_screen_remove_unused_scredges(screen);

  return newa;
}

/**
 * Empty screen, with 1 dummy area without spacedata. Uses window size.
 */
bScreen *screen_add(Main *bmain, const char *name, const rcti *rect)
{
  bScreen *screen;
  ScrVert *sv1, *sv2, *sv3, *sv4;

  screen = BKE_libblock_alloc(bmain, ID_SCR, name, 0);
  screen->do_refresh = true;
  screen->redraws_flag = TIME_ALL_3D_WIN | TIME_ALL_ANIM_WIN;

  sv1 = screen_geom_vertex_add(screen, rect->xmin, rect->ymin);
  sv2 = screen_geom_vertex_add(screen, rect->xmin, rect->ymax - 1);
  sv3 = screen_geom_vertex_add(screen, rect->xmax - 1, rect->ymax - 1);
  sv4 = screen_geom_vertex_add(screen, rect->xmax - 1, rect->ymin);

  screen_geom_edge_add(screen, sv1, sv2);
  screen_geom_edge_add(screen, sv2, sv3);
  screen_geom_edge_add(screen, sv3, sv4);
  screen_geom_edge_add(screen, sv4, sv1);

  /* dummy type, no spacedata */
  screen_addarea(screen, sv1, sv2, sv3, sv4, SPACE_EMPTY);

  return screen;
}

void screen_data_copy(bScreen *to, bScreen *from)
{
  ScrVert *s1, *s2;
  ScrEdge *se;
  ScrArea *area, *saf;

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
  for (area = to->areabase.first; area; area = area->next, saf = saf->next) {
    area->v1 = area->v1->newv;
    area->v2 = area->v2->newv;
    area->v3 = area->v3->newv;
    area->v4 = area->v4->newv;

    BLI_listbase_clear(&area->spacedata);
    BLI_listbase_clear(&area->regionbase);
    BLI_listbase_clear(&area->actionzones);
    BLI_listbase_clear(&area->handlers);

    ED_area_data_copy(area, saf, true);
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

/* with area as center, sb is located at: 0=W, 1=N, 2=E, 3=S */
/* -1 = not valid check */
/* used with join operator */
int area_getorientation(ScrArea *area, ScrArea *sb)
{
  if (area == NULL || sb == NULL) {
    return -1;
  }

  ScrVert *saBL = area->v1;
  ScrVert *saTL = area->v2;
  ScrVert *saTR = area->v3;
  ScrVert *saBR = area->v4;

  ScrVert *sbBL = sb->v1;
  ScrVert *sbTL = sb->v2;
  ScrVert *sbTR = sb->v3;
  ScrVert *sbBR = sb->v4;

  if (saBL->vec.x == sbBR->vec.x && saTL->vec.x == sbTR->vec.x) { /* area to right of sb = W */
    if ((abs(saBL->vec.y - sbBR->vec.y) <= AREAJOINTOLERANCE) &&
        (abs(saTL->vec.y - sbTR->vec.y) <= AREAJOINTOLERANCE)) {
      return 0;
    }
  }
  else if (saTL->vec.y == sbBL->vec.y &&
           saTR->vec.y == sbBR->vec.y) { /* area to bottom of sb = N */
    if ((abs(saTL->vec.x - sbBL->vec.x) <= AREAJOINTOLERANCE) &&
        (abs(saTR->vec.x - sbBR->vec.x) <= AREAJOINTOLERANCE)) {
      return 1;
    }
  }
  else if (saTR->vec.x == sbTL->vec.x && saBR->vec.x == sbBL->vec.x) { /* area to left of sb = E */
    if ((abs(saTR->vec.y - sbTL->vec.y) <= AREAJOINTOLERANCE) &&
        (abs(saBR->vec.y - sbBL->vec.y) <= AREAJOINTOLERANCE)) {
      return 2;
    }
  }
  else if (saBL->vec.y == sbTL->vec.y && saBR->vec.y == sbTR->vec.y) { /* area on top of sb = S*/
    if ((abs(saBL->vec.x - sbTL->vec.x) <= AREAJOINTOLERANCE) &&
        (abs(saBR->vec.x - sbTR->vec.x) <= AREAJOINTOLERANCE)) {
      return 3;
    }
  }

  return -1;
}

/* Screen verts with horizontal position equal to from_x are moved to to_x. */
static void screen_verts_halign(const wmWindow *win,
                                const bScreen *screen,
                                const short from_x,
                                const short to_x)
{
  ED_screen_verts_iter(win, screen, v1)
  {
    if (v1->vec.x == from_x) {
      v1->vec.x = to_x;
    }
  }
}

/* Screen verts with vertical position equal to from_y are moved to to_y. */
static void screen_verts_valign(const wmWindow *win,
                                const bScreen *screen,
                                const short from_y,
                                const short to_y)
{
  ED_screen_verts_iter(win, screen, v1)
  {
    if (v1->vec.y == from_y) {
      v1->vec.y = to_y;
    }
  }
}

/* Adjust all screen edges to allow joining two areas. 'dir' value is like area_getorientation().
 */
static void screen_areas_align(
    bContext *C, bScreen *screen, ScrArea *sa1, ScrArea *sa2, const int dir)
{
  wmWindow *win = CTX_wm_window(C);

  if (dir == 0 || dir == 2) {
    /* horizontal join, use average for new top and bottom. */
    int top = (sa1->v2->vec.y + sa2->v2->vec.y) / 2;
    int bottom = (sa1->v4->vec.y + sa2->v4->vec.y) / 2;

    /* Move edges exactly matching source top and bottom. */
    screen_verts_valign(win, screen, sa1->v2->vec.y, top);
    screen_verts_valign(win, screen, sa1->v4->vec.y, bottom);

    /* Move edges exactly matching target top and bottom. */
    screen_verts_valign(win, screen, sa2->v2->vec.y, top);
    screen_verts_valign(win, screen, sa2->v4->vec.y, bottom);
  }
  else {
    /* Vertical join, use averages for new left and right. */
    int left = (sa1->v1->vec.x + sa2->v1->vec.x) / 2;
    int right = (sa1->v3->vec.x + sa2->v3->vec.x) / 2;

    /* Move edges exactly matching source left and right. */
    screen_verts_halign(win, screen, sa1->v1->vec.x, left);
    screen_verts_halign(win, screen, sa1->v3->vec.x, right);

    /* Move edges exactly matching target left and right */
    screen_verts_halign(win, screen, sa2->v1->vec.x, left);
    screen_verts_halign(win, screen, sa2->v3->vec.x, right);
  }
}

/* Helper function to join 2 areas, it has a return value, 0=failed 1=success
 * used by the split, join operators
 */
int screen_area_join(bContext *C, bScreen *screen, ScrArea *sa1, ScrArea *sa2)
{
  int dir = area_getorientation(sa1, sa2);

  if (dir == -1) {
    return 0;
  }

  /* Align areas if they are not. Do sanity checking before getting here. */
  screen_areas_align(C, screen, sa1, sa2, dir);

  if (dir == 0) {      /* sa1 to right of sa2 = W */
    sa1->v1 = sa2->v1; /* BL */
    sa1->v2 = sa2->v2; /* TL */
    screen_geom_edge_add(screen, sa1->v2, sa1->v3);
    screen_geom_edge_add(screen, sa1->v1, sa1->v4);
  }
  else if (dir == 1) { /* sa1 to bottom of sa2 = N */
    sa1->v2 = sa2->v2; /* TL */
    sa1->v3 = sa2->v3; /* TR */
    screen_geom_edge_add(screen, sa1->v1, sa1->v2);
    screen_geom_edge_add(screen, sa1->v3, sa1->v4);
  }
  else if (dir == 2) { /* sa1 to left of sa2 = E */
    sa1->v3 = sa2->v3; /* TR */
    sa1->v4 = sa2->v4; /* BR */
    screen_geom_edge_add(screen, sa1->v2, sa1->v3);
    screen_geom_edge_add(screen, sa1->v1, sa1->v4);
  }
  else if (dir == 3) { /* sa1 on top of sa2 = S */
    sa1->v1 = sa2->v1; /* BL */
    sa1->v4 = sa2->v4; /* BR */
    screen_geom_edge_add(screen, sa1->v1, sa1->v2);
    screen_geom_edge_add(screen, sa1->v3, sa1->v4);
  }

  screen_delarea(C, screen, sa2);
  BKE_screen_remove_double_scrverts(screen);
  /* Update preview thumbnail */
  BKE_icon_changed(screen->id.icon_id);

  return 1;
}

/* ****************** EXPORTED API TO OTHER MODULES *************************** */

/* screen sets cursor based on active region */
static void region_cursor_set_ex(wmWindow *win, ScrArea *area, ARegion *region, bool swin_changed)
{
  BLI_assert(WM_window_get_active_screen(win)->active_region == region);
  if (win->tag_cursor_refresh || swin_changed || (region->type && region->type->event_cursor)) {
    win->tag_cursor_refresh = false;
    ED_region_cursor_set(win, area, region);
  }
}

static void region_cursor_set(wmWindow *win, bool swin_changed)
{
  bScreen *screen = WM_window_get_active_screen(win);

  ED_screen_areas_iter (win, screen, area) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if (region == screen->active_region) {
        region_cursor_set_ex(win, area, region, swin_changed);
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

    ED_screen_areas_iter (win, screen, area) {
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

/**
 * Utility to exit and free an area-region. Screen level regions (menus/popups) need to be treated
 * slightly differently, see #ui_region_temp_remove().
 */
void ED_region_remove(bContext *C, ScrArea *area, ARegion *region)
{
  ED_region_exit(C, region);
  BKE_area_region_free(area->type, region);
  BLI_freelinkN(&area->regionbase, region);
}

/* *********** exit calls are for closing running stuff ******** */

void ED_region_exit(bContext *C, ARegion *region)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  ARegion *prevar = CTX_wm_region(C);

  if (region->type && region->type->exit) {
    region->type->exit(wm, region);
  }

  CTX_wm_region_set(C, region);

  WM_event_remove_handlers(C, &region->handlers);
  WM_event_modal_handler_region_replace(win, region, NULL);
  WM_draw_region_free(region, true);

  if (region->headerstr) {
    MEM_freeN(region->headerstr);
    region->headerstr = NULL;
  }

  if (region->regiontimer) {
    WM_event_remove_timer(wm, win, region->regiontimer);
    region->regiontimer = NULL;
  }

  WM_msgbus_clear_by_owner(wm->message_bus, region);

  CTX_wm_region_set(C, prevar);
}

void ED_area_exit(bContext *C, ScrArea *area)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  ScrArea *prevsa = CTX_wm_area(C);
  ARegion *region;

  if (area->type && area->type->exit) {
    area->type->exit(wm, area);
  }

  CTX_wm_area_set(C, area);

  for (region = area->regionbase.first; region; region = region->next) {
    ED_region_exit(C, region);
  }

  WM_event_remove_handlers(C, &area->handlers);
  WM_event_modal_handler_area_replace(win, area, NULL);

  CTX_wm_area_set(C, prevsa);
}

void ED_screen_exit(bContext *C, wmWindow *window, bScreen *screen)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *prevwin = CTX_wm_window(C);

  CTX_wm_window_set(C, window);

  if (screen->animtimer) {
    WM_event_remove_timer(wm, window, screen->animtimer);

    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
    Scene *scene = WM_window_get_active_scene(prevwin);
    Scene *scene_eval = (Scene *)DEG_get_evaluated_id(depsgraph, &scene->id);
    BKE_sound_stop_scene(scene_eval);
  }
  screen->animtimer = NULL;
  screen->scrubbing = false;

  screen->active_region = NULL;

  LISTBASE_FOREACH (ARegion *, region, &screen->regionbase) {
    ED_region_exit(C, region);
  }
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    ED_area_exit(C, area);
  }
  /* Don't use ED_screen_areas_iter here, it skips hidden areas. */
  LISTBASE_FOREACH (ScrArea *, area, &window->global_areas.areabase) {
    ED_area_exit(C, area);
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
  ScrArea *area;

  for (area = screen->areabase.first; area; area = area->next) {
    if ((az = ED_area_actionzone_find_xy(area, xy))) {
      break;
    }
  }

  if (area) {
    if (az->type == AZONE_AREA) {
      WM_cursor_set(win, WM_CURSOR_EDIT);
    }
    else if (az->type == AZONE_REGION) {
      if (az->edge == AE_LEFT_TO_TOPRIGHT || az->edge == AE_RIGHT_TO_TOPLEFT) {
        WM_cursor_set(win, WM_CURSOR_X_MOVE);
      }
      else {
        WM_cursor_set(win, WM_CURSOR_Y_MOVE);
      }
    }
  }
  else {
    ScrEdge *actedge = screen_geom_find_active_scredge(win, screen, xy[0], xy[1]);

    if (actedge) {
      if (screen_geom_edge_is_horizontal(actedge)) {
        WM_cursor_set(win, WM_CURSOR_Y_MOVE);
      }
      else {
        WM_cursor_set(win, WM_CURSOR_X_MOVE);
      }
    }
    else {
      WM_cursor_set(win, WM_CURSOR_DEFAULT);
    }
  }
}

/**
 * Called in wm_event_system.c. sets state vars in screen, cursors.
 * event type is mouse move.
 */
void ED_screen_set_active_region(bContext *C, wmWindow *win, const int xy[2])
{
  bScreen *screen = WM_window_get_active_screen(win);
  if (screen == NULL) {
    return;
  }

  ScrArea *area = NULL;
  ARegion *region;
  ARegion *region_prev = screen->active_region;

  ED_screen_areas_iter (win, screen, area_iter) {
    if (xy[0] > (area_iter->totrct.xmin + BORDERPADDING) &&
        xy[0] < (area_iter->totrct.xmax - BORDERPADDING)) {
      if (xy[1] > (area_iter->totrct.ymin + BORDERPADDING) &&
          xy[1] < (area_iter->totrct.ymax - BORDERPADDING)) {
        if (ED_area_azones_update(area_iter, xy) == NULL) {
          area = area_iter;
          break;
        }
      }
    }
  }
  if (area) {
    /* Make overlap active when mouse over. */
    for (region = area->regionbase.first; region; region = region->next) {
      if (ED_region_contains_xy(region, xy)) {
        screen->active_region = region;
        break;
      }
    }
  }
  else {
    screen->active_region = NULL;
  }

  /* Check for redraw headers. */
  if (region_prev != screen->active_region) {

    ED_screen_areas_iter (win, screen, area_iter) {
      bool do_draw = false;

      for (region = area_iter->regionbase.first; region; region = region->next) {

        /* Call old area's deactivate if assigned. */
        if (region == region_prev && area_iter->type->deactivate) {
          area_iter->type->deactivate(area_iter);
        }

        if (region == region_prev && region != screen->active_region) {
          wmGizmoMap *gzmap = region_prev->gizmo_map;
          if (gzmap) {
            if (WM_gizmo_highlight_set(gzmap, NULL)) {
              ED_region_tag_redraw_no_rebuild(region_prev);
            }
          }
        }

        if (region == region_prev || region == screen->active_region) {
          do_draw = true;
        }
      }

      if (do_draw) {
        for (region = area_iter->regionbase.first; region; region = region->next) {
          if (ELEM(region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
            ED_region_tag_redraw_no_rebuild(region);
          }
        }
      }
    }
  }

  /* Cursors, for time being set always on edges,
   * otherwise the active region doesn't switch. */
  if (screen->active_region == NULL) {
    screen_cursor_set(win, xy);
  }
  else {
    /* Notifier invokes freeing the buttons... causing a bit too much redraws. */
    region_cursor_set_ex(win, area, screen->active_region, region_prev != screen->active_region);

    if (region_prev != screen->active_region) {
      /* This used to be a notifier, but needs to be done immediate
       * because it can undo setting the right button as active due
       * to delayed notifier handling. */
      if (C) {
        UI_screen_free_active_but(C, screen);
      }
    }
  }
}

int ED_screen_area_active(const bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);

  if (win && screen && area) {
    AZone *az = ED_area_actionzone_find_xy(area, &win->eventstate->x);
    ARegion *region;

    if (az && az->type == AZONE_REGION) {
      return 1;
    }

    for (region = area->regionbase.first; region; region = region->next) {
      if (region == screen->active_region) {
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

static void screen_area_set_geometry_rect(ScrArea *area, const rcti *rect)
{
  area->v1->vec.x = rect->xmin;
  area->v1->vec.y = rect->ymin;
  area->v2->vec.x = rect->xmin;
  area->v2->vec.y = rect->ymax;
  area->v3->vec.x = rect->xmax;
  area->v3->vec.y = rect->ymax;
  area->v4->vec.x = rect->xmax;
  area->v4->vec.y = rect->ymin;
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

  LISTBASE_FOREACH (ScrArea *, area, &win->global_areas.areabase) {
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
      ScrArea *area = screen_iter->areabase.first;
      if (area && area->full == screen) {
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
    LISTBASE_FOREACH (ScrArea *, area, &screen_old->areabase) {
      WM_event_remove_area_handler(&win->modalhandlers, area);
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

void screen_change_update(bContext *C, wmWindow *win, bScreen *screen)
{
  Scene *scene = WM_window_get_active_scene(win);
  WorkSpace *workspace = BKE_workspace_active_get(win->workspace_hook);
  WorkSpaceLayout *layout = BKE_workspace_layout_find(workspace, screen);

  CTX_wm_window_set(C, win); /* stores C->wm.screen... hrmf */

  ED_screen_refresh(CTX_wm_manager(C), win);

  BKE_screen_view3d_scene_sync(screen, scene); /* sync new screen with scene data */
  WM_event_add_notifier(C, NC_WINDOW, NULL);
  WM_event_add_notifier(C, NC_SCREEN | ND_LAYOUTSET, layout);

  /* makes button hilites work */
  WM_event_add_mousemove(win);
}

/**
 * \brief Change the active screen.
 *
 * Operator call, WM + Window + screen already existed before
 *
 * \warning Do NOT call in area/region queues!
 * \returns if screen changing was successful.
 */
bool ED_screen_change(bContext *C, bScreen *screen)
{
  Main *bmain = CTX_data_main(C);
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen_old = CTX_wm_screen(C);
  bScreen *screen_new = screen_change_prepare(screen_old, screen, bmain, C, win);

  if (screen_new) {
    WorkSpace *workspace = BKE_workspace_active_get(win->workspace_hook);
    WM_window_set_active_screen(win, workspace, screen);
    screen_change_update(C, win, screen_new);

    return true;
  }

  return false;
}

static void screen_set_3dview_camera(Scene *scene,
                                     ViewLayer *view_layer,
                                     ScrArea *area,
                                     View3D *v3d)
{
  /* fix any cameras that are used in the 3d view but not in the scene */
  BKE_screen_view3d_sync(v3d, scene);

  if (!v3d->camera || !BKE_view_layer_base_find(view_layer, v3d->camera)) {
    v3d->camera = BKE_view_layer_camera_find(view_layer);
    // XXX if (screen == curscreen) handle_view3d_lock();
    if (!v3d->camera) {
      ARegion *region;
      ListBase *regionbase;

      /* regionbase is in different place depending if space is active */
      if (v3d == area->spacedata.first) {
        regionbase = &area->regionbase;
      }
      else {
        regionbase = &v3d->regionbase;
      }

      for (region = regionbase->first; region; region = region->next) {
        if (region->regiontype == RGN_TYPE_WINDOW) {
          RegionView3D *rv3d = region->regiondata;
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
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
      if (sl->spacetype == SPACE_VIEW3D) {
        View3D *v3d = (View3D *)sl;
        screen_set_3dview_camera(scene, view_layer, area, v3d);
      }
    }
  }
}

ScrArea *ED_screen_full_newspace(bContext *C, ScrArea *area, int type)
{
  wmWindow *win = CTX_wm_window(C);
  ScrArea *newsa = NULL;
  SpaceLink *newsl;

  if (!area || area->full == NULL) {
    newsa = ED_screen_state_toggle(C, win, area, SCREENMAXIMIZED);
  }

  if (!newsa) {
    newsa = area;
  }

  BLI_assert(newsa);
  newsl = newsa->spacedata.first;

  /* Tag the active space before changing, so we can identify it when user wants to go back. */
  if ((newsl->link_flag & SPACE_FLAG_TYPE_TEMPORARY) == 0) {
    newsl->link_flag |= SPACE_FLAG_TYPE_WAS_ACTIVE;
  }

  ED_area_newspace(C, newsa, type, newsl->link_flag & SPACE_FLAG_TYPE_TEMPORARY);

  return newsa;
}

/**
 * \a was_prev_temp for the case previous space was a temporary fullscreen as well
 */
void ED_screen_full_prevspace(bContext *C, ScrArea *area)
{
  BLI_assert(area->full);

  if (area->flag & AREA_FLAG_STACKED_FULLSCREEN) {
    /* stacked fullscreen -> only go back to previous area and don't toggle out of fullscreen */
    ED_area_prevspace(C, area);
  }
  else {
    ED_screen_restore_temp_type(C, area);
  }
}

void ED_screen_restore_temp_type(bContext *C, ScrArea *area)
{
  SpaceLink *sl = area->spacedata.first;

  /* In case nether functions below run. */
  ED_area_tag_redraw(area);

  if (sl->link_flag & SPACE_FLAG_TYPE_TEMPORARY) {
    ED_area_prevspace(C, area);
  }

  if (area->full) {
    ED_screen_state_toggle(C, CTX_wm_window(C), area, SCREENMAXIMIZED);
  }
}

/* restore a screen / area back to default operation, after temp fullscreen modes */
void ED_screen_full_restore(bContext *C, ScrArea *area)
{
  wmWindow *win = CTX_wm_window(C);
  SpaceLink *sl = area->spacedata.first;
  bScreen *screen = CTX_wm_screen(C);
  short state = (screen ? screen->state : SCREENMAXIMIZED);

  /* if fullscreen area has a temporary space (such as a file browser or fullscreen render
   * overlaid on top of an existing setup) then return to the previous space */

  if (sl->next) {
    if (sl->link_flag & SPACE_FLAG_TYPE_TEMPORARY) {
      ED_screen_full_prevspace(C, area);
    }
    else {
      ED_screen_state_toggle(C, win, area, state);
    }
    /* warning: 'area' may be freed */
  }
  /* otherwise just tile the area again */
  else {
    ED_screen_state_toggle(C, win, area, state);
  }
}

/**
 * this function toggles: if area is maximized/full then the parent will be restored
 *
 * \warning \a area may be freed.
 */
ScrArea *ED_screen_state_toggle(bContext *C, wmWindow *win, ScrArea *area, const short state)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  WorkSpace *workspace = WM_window_get_active_workspace(win);
  bScreen *screen, *oldscreen;
  ARegion *region;

  if (area) {
    /* ensure we don't have a button active anymore, can crash when
     * switching screens with tooltip open because region and tooltip
     * are no longer in the same screen */
    for (region = area->regionbase.first; region; region = region->next) {
      UI_blocklist_free(C, &region->uiblocks);

      if (region->regiontimer) {
        WM_event_remove_timer(wm, NULL, region->regiontimer);
        region->regiontimer = NULL;
      }
    }

    /* prevent hanging status prints */
    ED_area_status_text(area, NULL);
    ED_workspace_status_text(C, NULL);
  }

  if (area && area->full) {
    WorkSpaceLayout *layout_old = WM_window_get_active_layout(win);
    /* restoring back to SCREENNORMAL */
    screen = area->full;                          /* the old screen to restore */
    oldscreen = WM_window_get_active_screen(win); /* the one disappearing */

    BLI_assert(BKE_workspace_layout_screen_get(layout_old) != screen);
    BLI_assert(BKE_workspace_layout_screen_get(layout_old)->state != SCREENNORMAL);

    screen->state = SCREENNORMAL;
    screen->flag = oldscreen->flag;

    /* find old area to restore from */
    ScrArea *fullsa = NULL;
    LISTBASE_FOREACH (ScrArea *, old, &screen->areabase) {
      /* area to restore from is always first */
      if (old->full && !fullsa) {
        fullsa = old;
      }

      /* clear full screen state */
      old->full = NULL;
    }

    area->full = NULL;

    if (fullsa == NULL) {
      if (G.debug & G_DEBUG) {
        printf("%s: something wrong in areafullscreen\n", __func__);
      }
      return NULL;
    }

    if (state == SCREENFULL) {
      /* unhide global areas */
      LISTBASE_FOREACH (ScrArea *, glob_area, &win->global_areas.areabase) {
        glob_area->global->flag &= ~GLOBAL_AREA_IS_HIDDEN;
      }
      /* restore the old side panels/header visibility */
      for (region = area->regionbase.first; region; region = region->next) {
        region->flag = region->flagfullscreen;
      }
    }

    ED_area_data_swap(fullsa, area);

    /* animtimer back */
    screen->animtimer = oldscreen->animtimer;
    oldscreen->animtimer = NULL;

    ED_screen_change(C, screen);
    ED_area_tag_refresh(fullsa);

    BKE_workspace_layout_remove(CTX_data_main(C), workspace, layout_old);

    /* After we've restored back to SCREENNORMAL, we have to wait with
     * screen handling as it uses the area coords which aren't updated yet.
     * Without doing so, the screen handling gets wrong area coords,
     * which in worst case can lead to crashes (see T43139) */
    screen->skip_handling = true;
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

    screen = BKE_workspace_layout_screen_get(layout_new);
    screen->state = state;
    screen->redraws_flag = oldscreen->redraws_flag;
    screen->temp = oldscreen->temp;
    screen->flag = oldscreen->flag;

    /* timer */
    screen->animtimer = oldscreen->animtimer;
    oldscreen->animtimer = NULL;

    /* use random area when we have no active one, e.g. when the
     * mouse is outside of the window and we open a file browser */
    if (!area || area->global) {
      area = oldscreen->areabase.first;
    }

    newa = (ScrArea *)screen->areabase.first;

    /* copy area */
    ED_area_data_swap(newa, area);
    newa->flag = area->flag; /* mostly for AREA_FLAG_WASFULLSCREEN */

    if (state == SCREENFULL) {
      /* temporarily hide global areas */
      LISTBASE_FOREACH (ScrArea *, glob_area, &win->global_areas.areabase) {
        glob_area->global->flag |= GLOBAL_AREA_IS_HIDDEN;
      }
      /* temporarily hide the side panels/header */
      for (region = newa->regionbase.first; region; region = region->next) {
        region->flagfullscreen = region->flag;

        if (ELEM(region->regiontype,
                 RGN_TYPE_UI,
                 RGN_TYPE_HEADER,
                 RGN_TYPE_TOOL_HEADER,
                 RGN_TYPE_FOOTER,
                 RGN_TYPE_TOOLS,
                 RGN_TYPE_NAV_BAR,
                 RGN_TYPE_EXECUTE)) {
          region->flag |= RGN_FLAG_HIDDEN;
        }
      }
    }

    area->full = oldscreen;
    newa->full = oldscreen;

    ED_screen_change(C, screen);
  }

  /* XXX bad code: setscreen() ends with first area active. fullscreen render assumes this too */
  CTX_wm_area_set(C, screen->areabase.first);

  return screen->areabase.first;
}

/**
 * Wrapper to open a temporary space either as fullscreen space, or as separate window, as defined
 * by \a display_type.
 *
 * \param title: Title to set for the window, if a window is spawned.
 * \param x, y: Position of the window, if a window is spawned.
 * \param sizex, sizey: Dimensions of the window, if a window is spawned.
 */
ScrArea *ED_screen_temp_space_open(bContext *C,
                                   const char *title,
                                   int x,
                                   int y,
                                   int sizex,
                                   int sizey,
                                   eSpace_Type space_type,
                                   int display_type,
                                   bool dialog)
{
  ScrArea *area = NULL;

  switch (display_type) {
    case USER_TEMP_SPACE_DISPLAY_WINDOW:
      if (WM_window_open_temp(C, title, x, y, sizex, sizey, (int)space_type, dialog)) {
        area = CTX_wm_area(C);
      }
      break;
    case USER_TEMP_SPACE_DISPLAY_FULLSCREEN: {
      ScrArea *ctx_area = CTX_wm_area(C);

      if (ctx_area != NULL && ctx_area->full) {
        area = ctx_area;
        ED_area_newspace(C, ctx_area, space_type, true);
        area->flag |= AREA_FLAG_STACKED_FULLSCREEN;
        ((SpaceLink *)area->spacedata.first)->link_flag |= SPACE_FLAG_TYPE_TEMPORARY;
      }
      else if (ctx_area != NULL && ctx_area->spacetype == space_type) {
        area = ED_screen_state_toggle(C, CTX_wm_window(C), ctx_area, SCREENMAXIMIZED);
      }
      else {
        area = ED_screen_full_newspace(C, ctx_area, (int)space_type);
        ((SpaceLink *)area->spacedata.first)->link_flag |= SPACE_FLAG_TYPE_TEMPORARY;
      }
      break;
    }
  }

  return area;
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
void ED_screen_animation_timer(bContext *C, int redraws, int sync, int enable)
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

    sad->region = CTX_wm_region(C);
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
    sad->flag |= (enable < 0) ? ANIMPLAY_FLAG_REVERSE : 0;
    sad->flag |= (sync == 0) ? ANIMPLAY_FLAG_NO_SYNC : (sync == 1) ? ANIMPLAY_FLAG_SYNC : 0;

    ScrArea *area = CTX_wm_area(C);

    char spacetype = -1;

    if (area) {
      spacetype = area->spacetype;
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
  ScrArea *area;
  int min = 10000;

  for (area = screen->areabase.first; area; area = area->next) {
    if (area->spacetype == SPACE_VIEW3D) {
      ARegion *region;
      for (region = area->regionbase.first; region; region = region->next) {
        if (region->regiontype == RGN_TYPE_WINDOW) {
          if (region->winrct.xmin - region->winrct.ymin < min) {
            aret = region;
            min = region->winrct.xmin - region->winrct.ymin;
          }
        }
      }
    }
  }

  return aret;
}

void ED_screen_animation_timer_update(bScreen *screen, int redraws)
{
  if (screen && screen->animtimer) {
    wmTimer *wt = screen->animtimer;
    ScreenAnimData *sad = wt->customdata;

    sad->redraws = redraws;
    sad->region = NULL;
    if (redraws & TIME_REGION) {
      sad->region = time_top_left_3dwindow(screen);
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
    bScreen *screen;
    scene->camera = camera;
    /* are there cameras in the views that are not in the scene? */
    for (screen = bmain->screens.first; screen; screen = screen->id.next) {
      BKE_screen_view3d_scene_sync(screen, scene);
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
  ScrArea *area;
  const bool is_multiview = (scene->r.scemode & R_MULTIVIEW) != 0;

  for (area = screen->areabase.first; area; area = area->next) {
    switch (area->spacetype) {
      case SPACE_VIEW3D: {
        View3D *v3d;

        if (!is_multiview) {
          continue;
        }

        v3d = area->spacedata.first;
        if (v3d->camera && v3d->stereo3d_camera == STEREO_3D_ID) {
          ARegion *region;
          for (region = area->regionbase.first; region; region = region->next) {
            if (region->regiondata && region->regiontype == RGN_TYPE_WINDOW) {
              RegionView3D *rv3d = region->regiondata;
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
        sima = area->spacedata.first;
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

        snode = area->spacedata.first;
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

        sseq = area->spacedata.first;
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
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (WM_window_get_active_screen(win) == screen) {
      if (r_window) {
        *r_window = win;
      }
      return WM_window_get_active_scene(win);
    }
  }

  /* Can by NULL when accessing a screen that isn't active. */
  return NULL;
}

ScrArea *ED_screen_area_find_with_spacedata(const bScreen *screen,
                                            const SpaceLink *sl,
                                            const bool only_visible)
{
  if (only_visible) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->spacedata.first == sl) {
        return area;
      }
    }
  }
  else {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (BLI_findindex(&area->spacedata, sl) != -1) {
        return area;
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
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (WM_window_get_active_screen(win) == screen) {
      return win;
    }
  }
  return NULL;
}

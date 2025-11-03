/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscr
 */

#include <cmath>
#include <cstring>
#include <limits>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_icons.hh"
#include "BKE_image.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_sound.hh"
#include "BKE_workspace.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_clip.hh"
#include "ED_node.hh"
#include "ED_screen.hh"
#include "ED_screen_types.hh"
#include "ED_sequencer.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "UI_interface.hh"

#include "WM_message.hh"
#include "WM_toolsystem.hh"

#include "DEG_depsgraph_query.hh"

#include "screen_intern.hh" /* own module include */

/* adds no space data */
static ScrArea *screen_addarea_ex(ScrAreaMap *area_map,
                                  ScrVert *bottom_left,
                                  ScrVert *top_left,
                                  ScrVert *top_right,
                                  ScrVert *bottom_right,
                                  const eSpace_Type space_type)
{
  ScrArea *area = MEM_callocN<ScrArea>("addscrarea");

  area->v1 = bottom_left;
  area->v2 = top_left;
  area->v3 = top_right;
  area->v4 = bottom_right;
  area->spacetype = space_type;

  BLI_addtail(&area_map->areabase, area);

  return area;
}
static ScrArea *screen_addarea(bScreen *screen,
                               ScrVert *left_bottom,
                               ScrVert *left_top,
                               ScrVert *right_top,
                               ScrVert *right_bottom,
                               const eSpace_Type space_type)
{
  return screen_addarea_ex(
      AREAMAP_FROM_SCREEN(screen), left_bottom, left_top, right_top, right_bottom, space_type);
}

static void screen_delarea(bContext *C, bScreen *screen, ScrArea *area)
{

  ED_area_exit(C, area);

  BKE_screen_area_free(area);

  BLI_remlink(&screen->areabase, area);
  MEM_freeN(area);
}

ScrArea *area_split(const wmWindow *win,
                    bScreen *screen,
                    ScrArea *area,
                    const eScreenAxis dir_axis,
                    const float fac,
                    const bool merge)
{
  ScrArea *newa = nullptr;

  if (area == nullptr) {
    return nullptr;
  }

  rcti window_rect;
  WM_window_rect_calc(win, &window_rect);

  short split = screen_geom_find_area_split_point(area, &window_rect, dir_axis, fac);
  if (split == 0) {
    return nullptr;
  }

  /* NOTE(@ideasman42): regarding (fac > 0.5f) checks below.
   * normally it shouldn't matter which is used since the copy should match the original
   * however with viewport rendering and python console this isn't the case. */

  if (dir_axis == SCREEN_AXIS_H) {
    /* new vertices */
    ScrVert *sv1 = screen_geom_vertex_add(screen, area->v1->vec.x, split);
    ScrVert *sv2 = screen_geom_vertex_add(screen, area->v4->vec.x, split);

    /* new edges */
    screen_geom_edge_add(screen, area->v1, sv1);
    screen_geom_edge_add(screen, sv1, area->v2);
    screen_geom_edge_add(screen, area->v3, sv2);
    screen_geom_edge_add(screen, sv2, area->v4);
    screen_geom_edge_add(screen, sv1, sv2);

    if (fac > 0.5f) {
      /* new areas: top */
      newa = screen_addarea(screen, sv1, area->v2, area->v3, sv2, eSpace_Type(area->spacetype));

      /* area below */
      area->v2 = sv1;
      area->v3 = sv2;
    }
    else {
      /* new areas: bottom */
      newa = screen_addarea(screen, area->v1, sv1, sv2, area->v4, eSpace_Type(area->spacetype));

      /* area above */
      area->v1 = sv1;
      area->v4 = sv2;
    }

    ED_area_data_copy(newa, area, true);
  }
  else {
    /* new vertices */
    ScrVert *sv1 = screen_geom_vertex_add(screen, split, area->v1->vec.y);
    ScrVert *sv2 = screen_geom_vertex_add(screen, split, area->v2->vec.y);

    /* new edges */
    screen_geom_edge_add(screen, area->v1, sv1);
    screen_geom_edge_add(screen, sv1, area->v4);
    screen_geom_edge_add(screen, area->v2, sv2);
    screen_geom_edge_add(screen, sv2, area->v3);
    screen_geom_edge_add(screen, sv1, sv2);

    if (fac > 0.5f) {
      /* new areas: right */
      newa = screen_addarea(screen, sv1, sv2, area->v3, area->v4, eSpace_Type(area->spacetype));

      /* area left */
      area->v3 = sv2;
      area->v4 = sv1;
    }
    else {
      /* new areas: left */
      newa = screen_addarea(screen, area->v1, area->v2, sv2, sv1, eSpace_Type(area->spacetype));

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

bScreen *screen_add(Main *bmain, const char *name, const rcti *rect)
{
  bScreen *screen = static_cast<bScreen *>(BKE_libblock_alloc(bmain, ID_SCR, name, 0));
  screen->do_refresh = true;
  screen->redraws_flag = TIME_ALL_3D_WIN | TIME_ALL_ANIM_WIN;

  ScrVert *sv1 = screen_geom_vertex_add(screen, rect->xmin, rect->ymin);
  ScrVert *sv2 = screen_geom_vertex_add(screen, rect->xmin, rect->ymax - 1);
  ScrVert *sv3 = screen_geom_vertex_add(screen, rect->xmax - 1, rect->ymax - 1);
  ScrVert *sv4 = screen_geom_vertex_add(screen, rect->xmax - 1, rect->ymin);

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
  /* Free contents of 'to', is from blenkernel `screen.cc`. */
  BKE_screen_free_data(to);

  to->flag = from->flag;

  BLI_duplicatelist(&to->vertbase, &from->vertbase);
  BLI_duplicatelist(&to->edgebase, &from->edgebase);
  BLI_duplicatelist(&to->areabase, &from->areabase);
  BLI_listbase_clear(&to->regionbase);

  ScrVert *s2 = static_cast<ScrVert *>(to->vertbase.first);
  for (ScrVert *s1 = static_cast<ScrVert *>(from->vertbase.first); s1;
       s1 = s1->next, s2 = s2->next)
  {
    s1->newv = s2;
  }

  LISTBASE_FOREACH (ScrEdge *, se, &to->edgebase) {
    se->v1 = se->v1->newv;
    se->v2 = se->v2->newv;
    BKE_screen_sort_scrvert(&(se->v1), &(se->v2));
  }

  ScrArea *from_area = static_cast<ScrArea *>(from->areabase.first);
  LISTBASE_FOREACH (ScrArea *, area, &to->areabase) {
    area->v1 = area->v1->newv;
    area->v2 = area->v2->newv;
    area->v3 = area->v3->newv;
    area->v4 = area->v4->newv;

    BLI_listbase_clear(&area->spacedata);
    BLI_listbase_clear(&area->regionbase);
    BLI_listbase_clear(&area->actionzones);
    BLI_listbase_clear(&area->handlers);

    ED_area_data_copy(area, from_area, true);

    from_area = from_area->next;
  }

  /* put at zero (needed?) */
  LISTBASE_FOREACH (ScrVert *, s1, &from->vertbase) {
    s1->newv = nullptr;
  }
}

void screen_new_activate_prepare(const wmWindow *win, bScreen *screen_new)
{
  screen_new->winid = win->winid;
  screen_new->do_refresh = true;
  screen_new->do_draw = true;
}

eScreenDir area_getorientation(ScrArea *sa_a, ScrArea *sa_b)
{
  if (sa_a == nullptr || sa_b == nullptr || sa_a == sa_b) {
    return SCREEN_DIR_NONE;
  }

  short left_a = sa_a->v1->vec.x;
  short right_a = sa_a->v3->vec.x;
  short top_a = sa_a->v3->vec.y;
  short bottom_a = sa_a->v1->vec.y;

  short left_b = sa_b->v1->vec.x;
  short right_b = sa_b->v3->vec.x;
  short top_b = sa_b->v3->vec.y;
  short bottom_b = sa_b->v1->vec.y;

  /* How much these areas share a common edge. */
  short overlapx = std::min(right_a, right_b) - std::max(left_a, left_b);
  short overlapy = std::min(top_a, top_b) - std::max(bottom_a, bottom_b);

  /* Minimum overlap required. */
  const short minx = std::min({int(AREAJOINTOLERANCEX), right_a - left_a, right_b - left_b});
  const short miny = std::min({int(AREAJOINTOLERANCEY), top_a - bottom_a, top_b - bottom_b});

  if (top_a == bottom_b && overlapx >= minx) {
    return eScreenDir(1); /* sa_a to bottom of sa_b = N */
  }
  if (bottom_a == top_b && overlapx >= minx) {
    return eScreenDir(3); /* sa_a on top of sa_b = S */
  }
  if (left_a == right_b && overlapy >= miny) {
    return eScreenDir(0); /* sa_a to right of sa_b = W */
  }
  if (right_a == left_b && overlapy >= miny) {
    return eScreenDir(2); /* sa_a to left of sa_b = E */
  }

  return eScreenDir(-1);
}

void area_getoffsets(
    ScrArea *sa_a, ScrArea *sa_b, const eScreenDir dir, int *r_offset1, int *r_offset2)
{
  if (sa_a == nullptr || sa_b == nullptr) {
    *r_offset1 = std::numeric_limits<int>::max();
    *r_offset2 = std::numeric_limits<int>::max();
  }
  else if (dir == SCREEN_DIR_W) { /* West: sa on right and sa_b to the left. */
    *r_offset1 = sa_b->v3->vec.y - sa_a->v2->vec.y;
    *r_offset2 = sa_b->v4->vec.y - sa_a->v1->vec.y;
  }
  else if (dir == SCREEN_DIR_N) { /* North: sa below and sa_b above. */
    *r_offset1 = sa_a->v2->vec.x - sa_b->v1->vec.x;
    *r_offset2 = sa_a->v3->vec.x - sa_b->v4->vec.x;
  }
  else if (dir == SCREEN_DIR_E) { /* East: sa on left and sa_b to the right. */
    *r_offset1 = sa_b->v2->vec.y - sa_a->v3->vec.y;
    *r_offset2 = sa_b->v1->vec.y - sa_a->v4->vec.y;
  }
  else if (dir == SCREEN_DIR_S) { /* South: sa above and sa_b below. */
    *r_offset1 = sa_a->v1->vec.x - sa_b->v2->vec.x;
    *r_offset2 = sa_a->v4->vec.x - sa_b->v3->vec.x;
  }
  else {
    BLI_assert(dir == SCREEN_DIR_NONE);
    *r_offset1 = std::numeric_limits<int>::max();
    *r_offset2 = std::numeric_limits<int>::max();
  }
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

/* Test if two adjoining areas can be aligned by having their screen edges adjusted. */
static bool screen_areas_can_align(
    ReportList *reports, bScreen *screen, ScrArea *sa1, ScrArea *sa2, eScreenDir dir)
{
  if (dir == SCREEN_DIR_NONE) {
    return false;
  }

  int offset1;
  int offset2;
  area_getoffsets(sa1, sa2, dir, &offset1, &offset2);

  const int tolerance = SCREEN_DIR_IS_HORIZONTAL(dir) ? AREAJOINTOLERANCEY : AREAJOINTOLERANCEX;
  if ((abs(offset1) >= tolerance) || (abs(offset2) >= tolerance)) {
    /* Misalignment is too great. */
    return false;
  }

  /* Areas that are _smaller_ than minimum sizes, sharing an edge to be moved. See #100772. */
  if (SCREEN_DIR_IS_VERTICAL(dir)) {
    const short xmin = std::min(sa1->v1->vec.x, sa2->v1->vec.x);
    const short xmax = std::max(sa1->v3->vec.x, sa2->v3->vec.x);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (ELEM(area, sa1, sa2)) {
        continue;
      }
      if (area->v3->vec.x - area->v1->vec.x < tolerance &&
          (area->v1->vec.x == xmin || area->v3->vec.x == xmax))
      {
        BKE_report(reports, RPT_ERROR, "A narrow vertical area interferes with this operation");
        return false;
      }
    }
  }
  else {
    const short ymin = std::min(sa1->v1->vec.y, sa2->v1->vec.y);
    const short ymax = std::max(sa1->v3->vec.y, sa2->v3->vec.y);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (ELEM(area, sa1, sa2)) {
        continue;
      }
      if (area->v3->vec.y - area->v1->vec.y < tolerance &&
          (area->v1->vec.y == ymin || area->v3->vec.y == ymax))
      {
        BKE_report(reports, RPT_ERROR, "A narrow horizontal area interferes with this operation");
        return false;
      }
    }
  }

  return true;
}

/* Adjust all screen edges to allow joining two areas. 'dir' value is like area_getorientation().
 */
static bool screen_areas_align(bContext *C,
                               ReportList *reports,
                               bScreen *screen,
                               ScrArea *sa1,
                               ScrArea *sa2,
                               const eScreenDir dir)
{
  if (!screen_areas_can_align(reports, screen, sa1, sa2, dir)) {
    return false;
  }

  wmWindow *win = CTX_wm_window(C);

  if (SCREEN_DIR_IS_HORIZONTAL(dir)) {
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

  return true;
}

/* Simple join of two areas without any splitting. Will return false if not possible. */
static bool screen_area_join_aligned(
    bContext *C, ReportList *reports, bScreen *screen, ScrArea *sa1, ScrArea *sa2)
{
  const eScreenDir dir = area_getorientation(sa1, sa2);

  /* Ensure that the area edges are exactly aligned. */
  if (!screen_areas_align(C, reports, screen, sa1, sa2, dir)) {
    return false;
  }

  if (dir == SCREEN_DIR_W) { /* sa1 to right of sa2 = West. */
    sa1->v1 = sa2->v1;       /* BL */
    sa1->v2 = sa2->v2;       /* TL */
    screen_geom_edge_add(screen, sa1->v2, sa1->v3);
    screen_geom_edge_add(screen, sa1->v1, sa1->v4);
  }
  else if (dir == SCREEN_DIR_N) { /* sa1 to bottom of sa2 = North. */
    sa1->v2 = sa2->v2;            /* TL */
    sa1->v3 = sa2->v3;            /* TR */
    screen_geom_edge_add(screen, sa1->v1, sa1->v2);
    screen_geom_edge_add(screen, sa1->v3, sa1->v4);
  }
  else if (dir == SCREEN_DIR_E) { /* sa1 to left of sa2 = East. */
    sa1->v3 = sa2->v3;            /* TR */
    sa1->v4 = sa2->v4;            /* BR */
    screen_geom_edge_add(screen, sa1->v2, sa1->v3);
    screen_geom_edge_add(screen, sa1->v1, sa1->v4);
  }
  else if (dir == SCREEN_DIR_S) { /* sa1 on top of sa2 = South. */
    sa1->v1 = sa2->v1;            /* BL */
    sa1->v4 = sa2->v4;            /* BR */
    screen_geom_edge_add(screen, sa1->v1, sa1->v2);
    screen_geom_edge_add(screen, sa1->v3, sa1->v4);
  }

  screen_delarea(C, screen, sa2);
  BKE_screen_remove_double_scrverts(screen);
  /* Update preview thumbnail */
  BKE_icon_changed(screen->id.icon_id);

  return true;
}

/* Slice off and return new area. "Reverse" gives right/bottom, rather than left/top. */
static ScrArea *screen_area_trim(
    bContext *C, bScreen *screen, ScrArea **area, int size, eScreenDir dir, bool reverse)
{
  const bool vertical = SCREEN_DIR_IS_VERTICAL(dir);
  if (abs(size) < (vertical ? AREAJOINTOLERANCEX : AREAJOINTOLERANCEY)) {
    return nullptr;
  }

  /* Measurement with ScrVerts because winx and winy might not be correct at this time. */
  float fac = abs(size) / float(vertical ? ((*area)->v3->vec.x - (*area)->v1->vec.x) :
                                           ((*area)->v3->vec.y - (*area)->v1->vec.y));
  fac = (reverse == vertical) ? 1.0f - fac : fac;
  ScrArea *newsa = area_split(
      CTX_wm_window(C), screen, *area, vertical ? SCREEN_AXIS_V : SCREEN_AXIS_H, fac, true);

  /* area_split always returns smallest of the two areas, so might have to swap. */
  if (((fac > 0.5f) == vertical) != reverse) {
    ScrArea *temp = *area;
    *area = newsa;
    newsa = temp;
  }

  return newsa;
}

/* Join any two neighboring areas. Might create new areas, kept if over min_remainder. */
static bool screen_area_join_ex(bContext *C,
                                ReportList *reports,
                                bScreen *screen,
                                ScrArea *sa1,
                                ScrArea *sa2,
                                bool close_all_remainders)
{
  const eScreenDir dir = area_getorientation(sa1, sa2);
  if (dir == SCREEN_DIR_NONE) {
    return false;
  }

  int offset1;
  int offset2;
  area_getoffsets(sa1, sa2, dir, &offset1, &offset2);

  /* Split Left/Top into new area if overhanging. */
  ScrArea *side1 = screen_area_trim(C, screen, (offset1 > 0) ? &sa2 : &sa1, offset1, dir, false);

  /* Split Right/Bottom into new area if overhanging. */
  ScrArea *side2 = screen_area_trim(C, screen, (offset2 > 0) ? &sa1 : &sa2, offset2, dir, true);

  /* The two areas now line up, so join them. */
  screen_area_join_aligned(C, reports, screen, sa1, sa2);

  if (close_all_remainders || offset1 < 0 || offset2 > 0) {
    /* Close both if trimming `sa1`. */
    float inner[4] = {0.0f, 0.0f, 0.0f, 0.7f};
    if (side1) {
      rcti rect = {side1->v1->vec.x, side1->v3->vec.x, side1->v1->vec.y, side1->v3->vec.y};
      screen_animate_area_highlight(
          CTX_wm_window(C), CTX_wm_screen(C), &rect, inner, nullptr, AREA_CLOSE_FADEOUT);
    }
    screen_area_close(C, reports, screen, side1);
    if (side2) {
      rcti rect = {side2->v1->vec.x, side2->v3->vec.x, side2->v1->vec.y, side2->v3->vec.y};
      screen_animate_area_highlight(
          CTX_wm_window(C), CTX_wm_screen(C), &rect, inner, nullptr, AREA_CLOSE_FADEOUT);
    }
    screen_area_close(C, reports, screen, side2);
  }
  else {
    /* Force full rebuild. #130732 */
    ED_area_tag_redraw(side1);
    ED_area_tag_redraw(side2);
  }

  if (sa1 != CTX_wm_area(C)) {
    /* Active area has changed so active region could be invalid. It is
     * safe to set null and let it be set later by mouse position. #131751. */
    screen->active_region = nullptr;
  }

  BKE_icon_changed(screen->id.icon_id);
  return true;
}

int screen_area_join(bContext *C, ReportList *reports, bScreen *screen, ScrArea *sa1, ScrArea *sa2)
{
  return screen_area_join_ex(C, reports, screen, sa1, sa2, false);
}

bool screen_area_close(bContext *C, ReportList *reports, bScreen *screen, ScrArea *area)
{
  if (area == nullptr) {
    return false;
  }

  ScrArea *sa2 = nullptr;
  float best_alignment = 0.0f;

  LISTBASE_FOREACH (ScrArea *, neighbor, &screen->areabase) {
    const eScreenDir dir = area_getorientation(area, neighbor);
    /* Must at least partially share an edge and not be a global area. */
    if ((dir != SCREEN_DIR_NONE) && (neighbor->global == nullptr)) {
      /* Winx/Winy might not be updated yet, so get lengths from verts. */
      const bool vertical = SCREEN_DIR_IS_VERTICAL(dir);
      const int area_length = vertical ? (area->v3->vec.x - area->v1->vec.x) :
                                         (area->v3->vec.y - area->v1->vec.y);
      const int ar_length = vertical ? (neighbor->v3->vec.x - neighbor->v1->vec.x) :
                                       (neighbor->v3->vec.y - neighbor->v1->vec.y);
      /* Calculate the ratio of the lengths of the shared edges. */
      float alignment = std::min(area_length, ar_length) / float(std::max(area_length, ar_length));
      if (alignment > best_alignment) {
        best_alignment = alignment;
        sa2 = neighbor;
      }
    }
  }

  /* Join from neighbor into this area to close it. */
  return screen_area_join_ex(C, reports, screen, sa2, area, true);
}

void screen_area_spacelink_add(const Scene *scene, ScrArea *area, eSpace_Type space_type)
{
  SpaceType *stype = BKE_spacetype_from_id(space_type);
  SpaceLink *slink = stype->create(area, scene);

  area->regionbase = slink->regionbase;

  BLI_addhead(&area->spacedata, slink);
  BLI_listbase_clear(&slink->regionbase);
}

/* ****************** EXPORTED API TO OTHER MODULES *************************** */

/* screen sets cursor based on active region */
static void region_cursor_set_ex(wmWindow *win, ScrArea *area, ARegion *region, bool swin_changed)
{
  BLI_assert(WM_window_get_active_screen(win)->active_region == region);
  if (win->tag_cursor_refresh || swin_changed ||
      (region->runtime->type && region->runtime->type->event_cursor))
  {
    win->tag_cursor_refresh = false;
    ED_region_cursor_set(win, area, region);
  }
}

static void region_cursor_set(wmWindow *win, bool swin_changed)
{
  bScreen *screen = WM_window_get_active_screen(win);

  /* Don't touch cursor if something else is controlling it, like button handling. See #51739. */
  if (win->grabcursor) {
    return;
  }
  ED_screen_areas_iter (win, screen, area) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if (region == screen->active_region) {
        region_cursor_set_ex(win, area, region, swin_changed);
        return;
      }
    }
  }
}

void ED_screen_do_listen(bContext *C, const wmNotifier *note)
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
      if (WM_capabilities_flag() & WM_CAPABILITY_WINDOW_DECORATION_STYLES) {
        WM_window_decoration_style_apply(win, screen);
      }
      screen->do_draw = true;
      break;
    case NC_SCREEN:
      if (note->action == NA_EDITED) {
        if (WM_capabilities_flag() & WM_CAPABILITY_WINDOW_DECORATION_STYLES) {
          WM_window_decoration_style_apply(win, screen);
        }
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

static bool region_poll(const bContext *C,
                        const bScreen *screen,
                        const ScrArea *area,
                        const ARegion *region)
{
  if (!region->runtime->type) {
    BLI_assert_unreachable();
    return false;
  }
  if (!region->runtime->type->poll) {
    /* Show region by default. */
    return true;
  }

  RegionPollParams params = {nullptr};
  params.screen = screen;
  params.area = area;
  params.region = region;
  params.context = C;

  return region->runtime->type->poll(&params);
}

bool area_regions_poll(bContext *C, const bScreen *screen, ScrArea *area)
{
  bScreen *prev_screen = CTX_wm_screen(C);
  ScrArea *prev_area = CTX_wm_area(C);
  ARegion *prev_region = CTX_wm_region(C);

  CTX_wm_screen_set(C, const_cast<bScreen *>(screen));
  CTX_wm_area_set(C, area);

  bool any_changed = false;
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    const int old_region_flag = region->flag;

    region->flag &= ~RGN_FLAG_POLL_FAILED;

    CTX_wm_region_set(C, region);
    if (region_poll(C, screen, area, region) == false) {
      region->flag |= RGN_FLAG_POLL_FAILED;
    }
    else if (region->runtime->type && region->runtime->type->on_poll_success) {
      region->runtime->type->on_poll_success(C, region);
    }

    if (old_region_flag != region->flag) {
      any_changed = true;

      /* Enforce complete re-init. */
      region->v2d.flag &= ~V2D_IS_INIT;

      const bool is_hidden = region->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_POLL_FAILED);
      /* Don't re-init areas, caller is expected to handle that. In fact, this code might actually
       * run as part of #ED_area_init(). */
      const bool do_init = false;
      ED_region_visibility_change_update_ex(C, area, region, is_hidden, do_init);
    }
  }

  CTX_wm_screen_set(C, prev_screen);
  CTX_wm_area_set(C, prev_area);
  CTX_wm_region_set(C, prev_region);

  return any_changed;
}

/**
 * \return true if any region polling state changed, and a screen refresh is needed.
 */
static bool screen_regions_poll(bContext *C, wmWindow *win, const bScreen *screen)
{
  wmWindow *prev_win = CTX_wm_window(C);
  ScrArea *prev_area = CTX_wm_area(C);
  ARegion *prev_region = CTX_wm_region(C);

  CTX_wm_window_set(C, win);

  bool any_changed = false;
  ED_screen_areas_iter (win, screen, area) {
    if (area_regions_poll(C, screen, area)) {
      any_changed = true;
    }
  }

  CTX_wm_window_set(C, prev_win);
  CTX_wm_area_set(C, prev_area);
  CTX_wm_region_set(C, prev_region);

  return any_changed;
}

/**
 * Refreshes the active screen of \a win if #bScreen.do_refresh is set. Region polling is also done
 * here, which will trigger a refresh on changes.
 *
 * Screen refreshes should only be necessary if the screen layout changes in some way.
 */
static void screen_refresh_if_needed(bContext *C, wmWindowManager *wm, wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);

  /* Exception for background mode, we only need the screen context. */
  if (!G.background) {
    if (screen->do_refresh) {
      ED_screen_areas_iter (win, screen, area) {
        /* Ensure all area and region types are set before polling, it depends on it (see #130583).
         */
        ED_area_and_region_types_init(area);
      }
    }

    /* Returns true if a change was done that requires refreshing. */
    if (screen_regions_poll(C, win, screen)) {
      screen->do_refresh = true;
    }

    if (!screen->do_refresh) {
      return;
    }

    /* Called even when creating the ghost window fails in #WM_window_open. */
    if (win->ghostwin) {
      /* Header size depends on DPI, let's verify. */
      WM_window_dpi_set_userdef(win);
    }

    ED_screen_global_areas_refresh(win);

    screen_geom_vertices_scale(win, screen);

    ED_screen_areas_iter (win, screen, area) {
      /* Set space-type and region callbacks, calls init() */
      /* Sets sub-windows for regions, adds handlers. */
      ED_area_init(C, win, area);
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
  /* Prevent multi-window errors. */
  screen->winid = win->winid;

  screen->context = reinterpret_cast<void *>(ed_screen_context);
}

void ED_screen_refresh(bContext *C, wmWindowManager *wm, wmWindow *win)
{
  bScreen *screen = WM_window_get_active_screen(win);
  /* Enforce full refresh. */
  screen->do_refresh = true;
  screen_refresh_if_needed(C, wm, win);
}

void ED_screens_init(bContext *C, Main *bmain, wmWindowManager *wm)
{
  wmWindow *prev_ctx_win = CTX_wm_window(C);
  BLI_SCOPED_DEFER([&]() { CTX_wm_window_set(C, prev_ctx_win); });

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    /* Region polls may need window/screen context. */
    CTX_wm_window_set(C, win);

    if (BKE_workspace_active_get(win->workspace_hook) == nullptr) {
      BKE_workspace_active_set(win->workspace_hook,
                               static_cast<WorkSpace *>(bmain->workspaces.first));
    }

    ED_screen_refresh(C, wm, win);
    if (win->eventstate) {
      ED_screen_set_active_region(nullptr, win, win->eventstate->xy);
    }
  }

  if (U.uiflag & USER_HEADER_FROM_PREF) {
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      BKE_screen_header_alignment_reset(screen);
    }
  }
}

void ED_screen_ensure_updated(bContext *C, wmWindowManager *wm, wmWindow *win)
{
  screen_refresh_if_needed(C, wm, win);
}

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

  if (region->runtime->type && region->runtime->type->exit) {
    region->runtime->type->exit(wm, region);
  }

  CTX_wm_region_set(C, region);

  WM_event_remove_handlers(C, &region->runtime->handlers);
  WM_event_modal_handler_region_replace(win, region, nullptr);

  /* Stop panel animation in this region if there are any. */
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    UI_panel_stop_animation(C, panel);
  }

  if (region->regiontype == RGN_TYPE_TEMPORARY) {
    /* This may be a popup region such as a popover or splash screen.
     * In the case of popups which spawn popups it's possible for
     * the parent popup to be freed *before* a popup which created it.
     * The child may have a reference to the freed parent unless cleared here, see: #122132.
     *
     * Having parent popups freed before the popups they spawn could be investigated although
     * they're not technically nested as they're both stored in #Screen::regionbase. */
    WM_event_ui_handler_region_popup_replace(win, region, nullptr);
  }

  WM_draw_region_free(region);
  /* The region is not in a state that it can be visible in anymore. Reinitializing is needed. */
  region->runtime->visible = false;

  MEM_SAFE_FREE(region->runtime->headerstr);

  if (region->runtime->regiontimer) {
    WM_event_timer_remove(wm, win, region->runtime->regiontimer);
    region->runtime->regiontimer = nullptr;
  }

  WM_msgbus_clear_by_owner(wm->runtime->message_bus, region);

  CTX_wm_region_set(C, prevar);
}

void ED_area_exit(bContext *C, ScrArea *area)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  ScrArea *prevsa = CTX_wm_area(C);

  if (area->type && area->type->exit) {
    area->type->exit(wm, area);
  }

  CTX_wm_area_set(C, area);

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    ED_region_exit(C, region);
  }

  WM_event_remove_handlers(C, &area->handlers);
  WM_event_modal_handler_area_replace(win, area, nullptr);

  CTX_wm_area_set(C, prevsa);
}

void ED_screen_exit(bContext *C, wmWindow *window, bScreen *screen)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *prevwin = CTX_wm_window(C);

  CTX_wm_window_set(C, window);

  if (screen->animtimer) {
    WM_event_timer_remove(wm, window, screen->animtimer);

    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
    Scene *scene = WM_window_get_active_scene(prevwin);
    Scene *scene_eval = DEG_get_evaluated(depsgraph, scene);
    BKE_sound_stop_scene(scene_eval);
  }
  screen->animtimer = nullptr;
  screen->scrubbing = false;

  screen->active_region = nullptr;

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
    CTX_wm_window_set(C, nullptr);
  }
}

blender::StringRefNull ED_area_name(const ScrArea *area)
{
  if (area->type && area->type->space_name_get) {
    return area->type->space_name_get(area);
  }

  const int index = RNA_enum_from_value(rna_enum_space_type_items, area->spacetype);
  const EnumPropertyItem item = rna_enum_space_type_items[index];
  return item.name;
}

int ED_area_icon(const ScrArea *area)
{
  if (area->type && area->type->space_icon_get) {
    return area->type->space_icon_get(area);
  }

  const int index = RNA_enum_from_value(rna_enum_space_type_items, area->spacetype);
  const EnumPropertyItem item = rna_enum_space_type_items[index];
  return item.icon;
}

/* *********************************** */

/* case when on area-edge or in azones, or outside window */
static void screen_cursor_set(wmWindow *win, const int xy[2])
{
  const bScreen *screen = WM_window_get_active_screen(win);
  AZone *az = nullptr;
  ScrArea *area = nullptr;

  LISTBASE_FOREACH (ScrArea *, area_iter, &screen->areabase) {
    az = ED_area_actionzone_find_xy(area_iter, xy);
    /* We used to exclude AZONE_REGION_SCROLL as those used
     * to overlap screen edges, but they no longer do so. */
    if (az) {
      area = area_iter;
      break;
    }
  }

  if (area) {
    if (az->type == AZONE_AREA) {
      WM_cursor_set(win, WM_CURSOR_EDIT);
    }
    else if (az->type == AZONE_REGION_SCROLL) {
      WM_cursor_set(win, WM_CURSOR_DEFAULT);
    }
    else if (az->type == AZONE_REGION) {
      if (ELEM(az->edge, AE_LEFT_TO_TOPRIGHT, AE_RIGHT_TO_TOPLEFT)) {
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

void ED_screen_set_active_region(bContext *C, wmWindow *win, const int xy[2])
{
  bScreen *screen = WM_window_get_active_screen(win);
  if (screen == nullptr) {
    return;
  }

  ScrArea *area = nullptr;
  ARegion *region_prev = screen->active_region;

  ED_screen_areas_iter (win, screen, area_iter) {
    if (xy[0] > (area_iter->totrct.xmin + BORDERPADDING) &&
        xy[0] < (area_iter->totrct.xmax - BORDERPADDING))
    {
      if (xy[1] > (area_iter->totrct.ymin + BORDERPADDING) &&
          xy[1] < (area_iter->totrct.ymax - BORDERPADDING))
      {
        if (ED_area_azones_update(area_iter, xy) == nullptr) {
          area = area_iter;
          break;
        }
      }
    }
  }
  if (area) {
    /* Make overlap active when mouse over. */
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if (ED_region_contains_xy(region, xy)) {
        screen->active_region = region;
        break;
      }
    }
  }
  else {
    screen->active_region = nullptr;
  }

  if (region_prev != screen->active_region || !screen->active_region) {
    WM_window_status_area_tag_redraw(win);
  }

  /* Check for redraw headers. */
  if (region_prev != screen->active_region) {

    ED_screen_areas_iter (win, screen, area_iter) {
      bool do_draw = false;

      LISTBASE_FOREACH (ARegion *, region, &area_iter->regionbase) {
        /* Call old area's deactivate if assigned. */
        if (region == region_prev && area_iter->type && area_iter->type->deactivate) {
          area_iter->type->deactivate(area_iter);
        }

        if (region == region_prev && region != screen->active_region) {
          wmGizmoMap *gzmap = region_prev->runtime->gizmo_map;
          if (gzmap) {
            if (WM_gizmo_highlight_set(gzmap, nullptr)) {
              ED_region_tag_redraw_no_rebuild(region_prev);
            }
          }
        }

        if (ELEM(region, region_prev, screen->active_region)) {
          do_draw = true;
        }
      }

      if (do_draw) {
        LISTBASE_FOREACH (ARegion *, region, &area_iter->regionbase) {
          if (ELEM(region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
            ED_region_tag_redraw_no_rebuild(region);
          }
        }
      }
    }

    /* Ensure test-motion values are never shared between regions. */
    const int mval[2] = {-1, -1};
    const bool use_cycle = !WM_cursor_test_motion_and_update(mval);
    UNUSED_VARS(use_cycle);
  }

  /* Cursors, for time being set always on edges,
   * otherwise the active region doesn't switch. */
  if (screen->active_region == nullptr) {
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
        UI_screen_free_active_but_highlight(C, screen);
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
    AZone *az = ED_area_actionzone_find_xy(area, win->eventstate->xy);

    if (az && az->type == AZONE_REGION) {
      return 1;
    }

    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
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
                                                 eSpace_Type space_type)
{
  ScrVert *bottom_left = screen_geom_vertex_add_ex(area_map, rect->xmin, rect->ymin);
  ScrVert *top_left = screen_geom_vertex_add_ex(area_map, rect->xmin, rect->ymax);
  ScrVert *top_right = screen_geom_vertex_add_ex(area_map, rect->xmax, rect->ymax);
  ScrVert *bottom_right = screen_geom_vertex_add_ex(area_map, rect->xmax, rect->ymin);

  screen_geom_edge_add_ex(area_map, bottom_left, top_left);
  screen_geom_edge_add_ex(area_map, top_left, top_right);
  screen_geom_edge_add_ex(area_map, top_right, bottom_right);
  screen_geom_edge_add_ex(area_map, bottom_right, bottom_left);

  return screen_addarea_ex(area_map, bottom_left, top_left, top_right, bottom_right, space_type);
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
                                       const eSpace_Type space_type,
                                       GlobalAreaAlign align,
                                       const rcti *rect,
                                       const short height_cur,
                                       const short height_min,
                                       const short height_max)
{
  /* Full-screens shouldn't have global areas. Don't touch them. */
  if (screen->state == SCREENFULL) {
    return;
  }

  ScrArea *area = nullptr;
  LISTBASE_FOREACH (ScrArea *, area_iter, &win->global_areas.areabase) {
    if (area_iter->spacetype == space_type) {
      area = area_iter;
      break;
    }
  }

  if (area) {
    screen_area_set_geometry_rect(area, rect);
  }
  else {
    area = screen_area_create_with_geometry(&win->global_areas, rect, space_type);
    screen_area_spacelink_add(WM_window_get_active_scene(win), area, space_type);

    /* Data specific to global areas. */
    area->global = MEM_callocN<ScrGlobalAreaData>(__func__);
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

static int screen_global_header_size()
{
  return int(ceilf(ED_area_headersize() / UI_SCALE_FAC));
}

static void screen_global_topbar_area_refresh(wmWindow *win, bScreen *screen)
{
  const blender::int2 win_size = WM_window_native_pixel_size(win);
  const short size = screen_global_header_size();
  rcti rect;

  BLI_rcti_init(&rect, 0, win_size[0] - 1, 0, win_size[1] - 1);
  rect.ymin = rect.ymax - size;

  screen_global_area_refresh(
      win, screen, SPACE_TOPBAR, GLOBAL_AREA_ALIGN_TOP, &rect, size, size, size);
}

static void screen_global_statusbar_area_refresh(wmWindow *win, bScreen *screen)
{
  const blender::int2 win_size = WM_window_native_pixel_size(win);
  const short size_min = 1;
  const short size_max = 0.85f * screen_global_header_size();
  const short size = (screen->flag & SCREEN_COLLAPSE_STATUSBAR) ? size_min : size_max;
  rcti rect;

  BLI_rcti_init(&rect, 0, win_size[0] - 1, 0, win_size[1] - 1);
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
  if (!WM_window_is_main_top_level(win)) {
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

void screen_change_prepare(
    bScreen *screen_old, bScreen *screen_new, Main *bmain, bContext *C, wmWindow *win)
{
  UNUSED_VARS_NDEBUG(bmain);
  BLI_assert(BLI_findindex(&bmain->screens, screen_new) != -1);

  if (screen_old != screen_new) {
    wmTimer *wt = screen_old->animtimer;

    /* Remove popup handlers (menus), while unlikely, it's possible an "error"
     * popup is displayed when switching screens.
     * Ideally popups from reported errors would remain so the error isn't hidden from the user.
     * On the other hand this is a rare occurrence, script developers will often show errors
     * in a console too, so it's not such a priority to relocate these to the new screen.
     * See: #144958. */
    UI_popup_handlers_remove_all(C, &win->modalhandlers);

    /* remove handlers referencing areas in old screen */
    LISTBASE_FOREACH (ScrArea *, area, &screen_old->areabase) {
      WM_event_remove_handlers_by_area(&win->modalhandlers, area);
    }

    /* we put timer to sleep, so screen_exit has to think there's no timer */
    screen_old->animtimer = nullptr;
    if (wt) {
      WM_event_timer_sleep(CTX_wm_manager(C), win, wt, true);
    }
    ED_screen_exit(C, win, screen_old);

    /* Same scene, "transfer" playback to new screen. */
    if (wt) {
      screen_new->animtimer = wt;
    }
  }
}

void screen_change_update(bContext *C, wmWindow *win, bScreen *screen)
{
  Scene *scene = WM_window_get_active_scene(win);
  WorkSpace *workspace = BKE_workspace_active_get(win->workspace_hook);
  WorkSpaceLayout *layout = BKE_workspace_layout_find(workspace, screen);

  CTX_wm_window_set(C, win); /* stores C->wm.screen... hrmf */

  ED_screen_refresh(C, CTX_wm_manager(C), win);

  BKE_screen_view3d_scene_sync(screen, scene); /* sync new screen with scene data */
  WM_event_add_notifier(C, NC_WINDOW, nullptr);
  WM_event_add_notifier(C, NC_SCREEN | ND_LAYOUTSET, layout);

  /* Makes button highlights work. */
  WM_event_add_mousemove(win);
}

bool ED_screen_change(bContext *C, bScreen *screen)
{
  Main *bmain = CTX_data_main(C);
  wmWindow *win = CTX_wm_window(C);
  WorkSpace *workspace = BKE_workspace_active_get(win->workspace_hook);
  WorkSpaceLayout *layout = BKE_workspace_layout_find(workspace, screen);
  bScreen *screen_old = CTX_wm_screen(C);

  /* Get the actual layout/screen to be activated (guaranteed to be unused, even if that means
   * having to duplicate an existing one). */
  WorkSpaceLayout *layout_new = ED_workspace_screen_change_ensure_unused_layout(
      bmain, workspace, layout, layout, win);
  bScreen *screen_new = BKE_workspace_layout_screen_get(layout_new);

  screen_change_prepare(screen_old, screen_new, bmain, C, win);

  if (screen_old != screen_new) {
    WM_window_set_active_screen(win, workspace, screen_new);
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
  /* Fix any cameras that are used in the 3d view but not in the scene. */
  BKE_screen_view3d_sync(v3d, scene);

  BKE_view_layer_synced_ensure(scene, view_layer);
  if (!v3d->camera || !BKE_view_layer_base_find(view_layer, v3d->camera)) {
    v3d->camera = BKE_view_layer_camera_find(scene, view_layer);
  }
  ListBase *regionbase;

  /* regionbase is in different place depending if space is active. */
  if (v3d == area->spacedata.first) {
    regionbase = &area->regionbase;
  }
  else {
    regionbase = &v3d->regionbase;
  }

  LISTBASE_FOREACH (ARegion *, region, regionbase) {
    if (region->regiontype != RGN_TYPE_WINDOW) {
      continue;
    }
    RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
    /* Keep the information about RV3D_CAMOB even when no camera can be found.
     * This prevents jumping off the camera view while scrubbing through the
     * Sequencer with some Scene strips without scene. */
    if (!v3d->camera) {
      if (rv3d->persp == RV3D_CAMOB) {
        rv3d->persp = RV3D_PERSP;
        rv3d->rflag |= RV3D_WAS_CAMOB;
      }
    }
    else if ((rv3d->rflag & RV3D_WAS_CAMOB) != 0) {
      rv3d->persp = RV3D_CAMOB;
      rv3d->rflag &= ~RV3D_WAS_CAMOB;
    }
  }
}

void ED_screen_scene_change(bContext *C,
                            wmWindow *win,
                            Scene *scene,
                            const bool refresh_toolsystem)
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
    Object *obact_new = BKE_view_layer_active_object_get(view_layer);
    UNUSED_VARS(obact_new);
    eObjectMode object_mode_old = workspace->object_mode;
    Object *obact_old = BKE_view_layer_active_object_get(view_layer_old);
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

  if (refresh_toolsystem) {
    WM_toolsystem_refresh_screen_window(win);
  }
}

ScrArea *ED_screen_full_newspace(bContext *C, ScrArea *area, int type)
{
  bScreen *newscreen = nullptr;
  ScrArea *newsa = nullptr;
  SpaceLink *newsl;

  if (!area || area->full == nullptr) {
    newscreen = ED_screen_state_maximized_create(C);
    newsa = static_cast<ScrArea *>(newscreen->areabase.first);
    BLI_assert(newsa->spacetype == SPACE_EMPTY);
  }

  if (!newsa) {
    newsa = area;
  }

  BLI_assert(newsa);
  newsl = static_cast<SpaceLink *>(newsa->spacedata.first);

  /* Tag the active space before changing, so we can identify it when user wants to go back. */
  if (newsl && (newsl->link_flag & SPACE_FLAG_TYPE_TEMPORARY) == 0) {
    newsl->link_flag |= SPACE_FLAG_TYPE_WAS_ACTIVE;
  }

  ED_area_newspace(C, newsa, type, (newsl && newsl->link_flag & SPACE_FLAG_TYPE_TEMPORARY));

  if (newscreen) {
    ED_screen_change(C, newscreen);
  }

  return newsa;
}

void ED_screen_full_prevspace(bContext *C, ScrArea *area)
{
  BLI_assert(area->full);

  if (area->flag & AREA_FLAG_STACKED_FULLSCREEN) {
    /* Stacked full-screen -> only go back to previous area and don't toggle out of full-screen. */
    ED_area_prevspace(C, area);
  }
  else {
    ED_screen_restore_temp_type(C, area);
  }
}

void ED_screen_restore_temp_type(bContext *C, ScrArea *area)
{
  SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);

  /* In case nether functions below run. */
  ED_area_tag_redraw(area);

  if (sl->link_flag & SPACE_FLAG_TYPE_TEMPORARY) {
    ED_area_prevspace(C, area);
  }

  if (area->full) {
    ED_screen_state_toggle(C, CTX_wm_window(C), area, SCREENMAXIMIZED);
  }
}

void ED_screen_full_restore(bContext *C, ScrArea *area)
{
  wmWindow *win = CTX_wm_window(C);
  SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
  bScreen *screen = CTX_wm_screen(C);
  short state = (screen ? screen->state : short(SCREENMAXIMIZED));

  /* If full-screen area has a temporary space (such as a file browser or full-screen render
   * overlaid on top of an existing setup) then return to the previous space. */

  if (sl->next) {
    if (sl->link_flag & SPACE_FLAG_TYPE_TEMPORARY) {
      ED_screen_full_prevspace(C, area);
    }
    else {
      ED_screen_state_toggle(C, win, area, state);
    }
    /* WARNING: 'area' may be freed */
  }
  /* otherwise just tile the area again */
  else {
    ED_screen_state_toggle(C, win, area, state);
  }
}

/**
 * \param toggle_area: If this is set, its space data will be swapped with the one of the new empty
 *                     area, when toggling back it can be swapped back again.
 * \return The newly created screen with the non-normal area.
 *
 * \note The caller must run #ED_screen_change this is not done in this function
 * as it would attempt to initialize areas that don't yet have a space-type assigned
 * (converting them to 3D view without creating the space-data).
 */
static bScreen *screen_state_to_nonnormal(bContext *C,
                                          wmWindow *win,
                                          ScrArea *toggle_area,
                                          int state)
{
  Main *bmain = CTX_data_main(C);
  WorkSpace *workspace = WM_window_get_active_workspace(win);

  /* change from SCREENNORMAL to new state */
  WorkSpaceLayout *layout_new;
  ScrArea *newa;
  char newname[MAX_ID_NAME - 2];

  BLI_assert(ELEM(state, SCREENMAXIMIZED, SCREENFULL));

  bScreen *oldscreen = WM_window_get_active_screen(win);

  oldscreen->state = state;
  SNPRINTF_UTF8(newname, "%s-%s", oldscreen->id.name + 2, "nonnormal");

  layout_new = ED_workspace_layout_add(bmain, workspace, win, newname);

  bScreen *screen = BKE_workspace_layout_screen_get(layout_new);
  screen->state = state;
  screen->redraws_flag = oldscreen->redraws_flag;
  screen->temp = oldscreen->temp;
  screen->flag = oldscreen->flag;

  /* timer */
  screen->animtimer = oldscreen->animtimer;
  oldscreen->animtimer = nullptr;

  newa = (ScrArea *)screen->areabase.first;

  /* swap area */
  if (toggle_area) {
    ED_area_data_swap(newa, toggle_area);
    newa->flag = toggle_area->flag; /* mostly for AREA_FLAG_WASFULLSCREEN */
  }

  if (state == SCREENFULL) {
    /* temporarily hide global areas */
    LISTBASE_FOREACH (ScrArea *, glob_area, &win->global_areas.areabase) {
      glob_area->global->flag |= GLOBAL_AREA_IS_HIDDEN;
    }
    /* temporarily hide the side panels/header */
    LISTBASE_FOREACH (ARegion *, region, &newa->regionbase) {
      region->flagfullscreen = region->flag;

      if (ELEM(region->regiontype,
               RGN_TYPE_UI,
               RGN_TYPE_HEADER,
               RGN_TYPE_TOOL_HEADER,
               RGN_TYPE_FOOTER,
               RGN_TYPE_TOOLS,
               RGN_TYPE_NAV_BAR,
               RGN_TYPE_EXECUTE,
               RGN_TYPE_ASSET_SHELF,
               RGN_TYPE_ASSET_SHELF_HEADER))
      {
        region->flag |= RGN_FLAG_HIDDEN;
      }
    }

    /* Temporarily hide gizmos and overlays. */
    screen->fullscreen_flag = 0;
    if (newa->spacetype == SPACE_VIEW3D) {
      View3D *v3d = static_cast<View3D *>(newa->spacedata.first);
      if (v3d && !(v3d->gizmo_flag & V3D_GIZMO_HIDE_NAVIGATE)) {
        screen->fullscreen_flag |= FULLSCREEN_RESTORE_GIZMO_NAVIGATE;
        v3d->gizmo_flag |= V3D_GIZMO_HIDE_NAVIGATE;
      }
      if (v3d && !(v3d->overlay.flag & V3D_OVERLAY_HIDE_TEXT)) {
        screen->fullscreen_flag |= FULLSCREEN_RESTORE_TEXT;
        v3d->overlay.flag |= V3D_OVERLAY_HIDE_TEXT;
      }
      if (v3d && (v3d->overlay.flag & V3D_OVERLAY_STATS)) {
        screen->fullscreen_flag |= FULLSCREEN_RESTORE_STATS;
        v3d->overlay.flag &= ~V3D_OVERLAY_STATS;
      }
    }
    else if (newa->spacetype == SPACE_CLIP) {
      SpaceClip *sc = static_cast<SpaceClip *>(newa->spacedata.first);
      if (sc && !(sc->gizmo_flag & SCLIP_GIZMO_HIDE_NAVIGATE)) {
        screen->fullscreen_flag |= FULLSCREEN_RESTORE_GIZMO_NAVIGATE;
        sc->gizmo_flag |= SCLIP_GIZMO_HIDE_NAVIGATE;
      }
    }
    else if (newa->spacetype == SPACE_SEQ) {
      SpaceSeq *sseq = static_cast<SpaceSeq *>(newa->spacedata.first);
      if (sseq && !(sseq->gizmo_flag & SEQ_GIZMO_HIDE_NAVIGATE)) {
        screen->fullscreen_flag |= FULLSCREEN_RESTORE_GIZMO_NAVIGATE;
        sseq->gizmo_flag |= SEQ_GIZMO_HIDE_NAVIGATE;
      }
    }
    else if (newa->spacetype == SPACE_IMAGE) {
      SpaceImage *sima = static_cast<SpaceImage *>(newa->spacedata.first);
      if (sima && !(sima->gizmo_flag & SI_GIZMO_HIDE_NAVIGATE)) {
        screen->fullscreen_flag |= FULLSCREEN_RESTORE_GIZMO_NAVIGATE;
        sima->gizmo_flag |= SI_GIZMO_HIDE_NAVIGATE;
      }
    }
  }

  if (toggle_area) {
    toggle_area->full = oldscreen;
  }
  newa->full = oldscreen;

  ED_area_tag_refresh(newa);

  return screen;
}

bScreen *ED_screen_state_maximized_create(bContext *C)
{
  return screen_state_to_nonnormal(C, CTX_wm_window(C), nullptr, SCREENMAXIMIZED);
}

ScrArea *ED_screen_state_toggle(bContext *C, wmWindow *win, ScrArea *area, const short state)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  WorkSpace *workspace = WM_window_get_active_workspace(win);

  if (area) {
    /* ensure we don't have a button active anymore, can crash when
     * switching screens with tooltip open because region and tooltip
     * are no longer in the same screen */
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      UI_blocklist_free(C, region);
      if (region->runtime->regiontimer) {
        WM_event_timer_remove(wm, nullptr, region->runtime->regiontimer);
        region->runtime->regiontimer = nullptr;
      }
    }

    /* prevent hanging status prints */
    ED_area_status_text(area, nullptr);
    ED_workspace_status_text(C, nullptr);
  }
  bScreen *screen;
  if (area && area->full) {
    WorkSpaceLayout *layout_old = WM_window_get_active_layout(win);
    /* restoring back to SCREENNORMAL */
    screen = area->full;                                   /* the old screen to restore */
    bScreen *oldscreen = WM_window_get_active_screen(win); /* the one disappearing */

    BLI_assert(BKE_workspace_layout_screen_get(layout_old) != screen);
    BLI_assert(BKE_workspace_layout_screen_get(layout_old)->state != SCREENNORMAL);

    screen->state = SCREENNORMAL;
    screen->flag = oldscreen->flag;
    screen->fullscreen_flag = oldscreen->fullscreen_flag;

    /* Find old area we may have swapped dummy space data to. It's swapped back here. */
    ScrArea *fullsa = nullptr;
    LISTBASE_FOREACH (ScrArea *, old, &screen->areabase) {
      /* area to restore from is always first */
      if (old->full && !fullsa) {
        fullsa = old;
      }

      /* clear full screen state */
      old->full = nullptr;
    }

    area->full = nullptr;

    if (state == SCREENFULL) {
      /* unhide global areas */
      LISTBASE_FOREACH (ScrArea *, glob_area, &win->global_areas.areabase) {
        glob_area->global->flag &= ~GLOBAL_AREA_IS_HIDDEN;
      }
      /* restore the old side panels/header visibility */
      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        region->flag = region->flagfullscreen;
      }
      /* Restore gizmos and overlays to their prior states. */
      if (area->spacetype == SPACE_VIEW3D) {
        View3D *v3d = static_cast<View3D *>(area->spacedata.first);
        if (v3d) {
          v3d->gizmo_flag = (screen->fullscreen_flag & FULLSCREEN_RESTORE_GIZMO_NAVIGATE) ?
                                v3d->gizmo_flag & ~V3D_GIZMO_HIDE_NAVIGATE :
                                v3d->gizmo_flag | V3D_GIZMO_HIDE_NAVIGATE;
          v3d->overlay.flag = (screen->fullscreen_flag & FULLSCREEN_RESTORE_TEXT) ?
                                  v3d->overlay.flag & ~V3D_OVERLAY_HIDE_TEXT :
                                  v3d->overlay.flag | V3D_OVERLAY_HIDE_TEXT;
          v3d->overlay.flag = (screen->fullscreen_flag & FULLSCREEN_RESTORE_STATS) ?
                                  v3d->overlay.flag | V3D_OVERLAY_STATS :
                                  v3d->overlay.flag & ~V3D_OVERLAY_STATS;
        }
      }
      else if (area->spacetype == SPACE_CLIP) {
        SpaceClip *sc = static_cast<SpaceClip *>(area->spacedata.first);
        if (sc) {
          sc->gizmo_flag = (screen->fullscreen_flag & FULLSCREEN_RESTORE_GIZMO_NAVIGATE) ?
                               sc->gizmo_flag & ~SCLIP_GIZMO_HIDE_NAVIGATE :
                               sc->gizmo_flag | SCLIP_GIZMO_HIDE_NAVIGATE;
        }
      }
      else if (area->spacetype == SPACE_SEQ) {
        SpaceSeq *sseq = static_cast<SpaceSeq *>(area->spacedata.first);
        if (sseq) {
          sseq->gizmo_flag = (screen->fullscreen_flag & FULLSCREEN_RESTORE_GIZMO_NAVIGATE) ?
                                 sseq->gizmo_flag & ~SEQ_GIZMO_HIDE_NAVIGATE :
                                 sseq->gizmo_flag | SEQ_GIZMO_HIDE_NAVIGATE;
        }
      }
      else if (area->spacetype == SPACE_IMAGE) {
        SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
        if (sima) {
          sima->gizmo_flag = (screen->fullscreen_flag & FULLSCREEN_RESTORE_GIZMO_NAVIGATE) ?
                                 sima->gizmo_flag & ~SI_GIZMO_HIDE_NAVIGATE :
                                 sima->gizmo_flag | SI_GIZMO_HIDE_NAVIGATE;
        }
      }
    }

    if (fullsa) {
      ED_area_data_swap(fullsa, area);
      ED_area_tag_refresh(fullsa);
    }

    /* animtimer back */
    screen->animtimer = oldscreen->animtimer;
    oldscreen->animtimer = nullptr;

    ED_screen_change(C, screen);

    BKE_workspace_layout_remove(CTX_data_main(C), workspace, layout_old);

    /* After we've restored back to SCREENNORMAL, we have to wait with
     * screen handling as it uses the area coords which aren't updated yet.
     * Without doing so, the screen handling gets wrong area coords,
     * which in worst case can lead to crashes (see #43139) */
    screen->skip_handling = true;
  }
  else {
    ScrArea *toggle_area = area;

    /* use random area when we have no active one, e.g. when the
     * mouse is outside of the window and we open a file browser */
    if (!toggle_area || toggle_area->global) {
      bScreen *oldscreen = WM_window_get_active_screen(win);
      toggle_area = static_cast<ScrArea *>(oldscreen->areabase.first);
    }

    screen = screen_state_to_nonnormal(C, win, toggle_area, state);

    ED_screen_change(C, screen);
  }

  BLI_assert(CTX_wm_screen(C) == screen);
  BLI_assert(CTX_wm_area(C) == nullptr); /* May have been freed. */

  /* Setting the area is only needed for Python scripts that call
   * operators in succession before returning to the main event loop.
   * Without this, scripts can't run any operators that require
   * an area after toggling full-screen for example (see: #89526).
   * NOTE: an old comment stated this was "bad code",
   * however it doesn't cause problems so leave as-is. */
  CTX_wm_area_set(C, static_cast<ScrArea *>(screen->areabase.first));

  return static_cast<ScrArea *>(screen->areabase.first);
}

ScrArea *ED_screen_temp_space_open(
    bContext *C, const char *title, eSpace_Type space_type, int display_type, bool dialog)
{
  switch (display_type) {
    case USER_TEMP_SPACE_DISPLAY_WINDOW:
      if (WM_window_open_temp(C, title, space_type, dialog)) {
        return CTX_wm_area(C);
      }
      break;
    case USER_TEMP_SPACE_DISPLAY_FULLSCREEN: {
      bScreen *ctx_screen = CTX_wm_screen(C);

      if (ctx_screen->state == SCREENMAXIMIZED) {
        /* Find the maximized area, check if it has the same type as the one we want to create. */
        LISTBASE_FOREACH (ScrArea *, screen_area, &ctx_screen->areabase) {
          if (screen_area->full && screen_area->spacetype == space_type) {
            /* Return the existing area instead of recreating an area on top, which would make the
             * "Back to Previous" button seem ineffective. */
            return screen_area;
          }
        }
      }

      ScrArea *ctx_area = CTX_wm_area(C);

      /* The current area is already fullscreen, stack the new area on top of it. */
      if (ctx_area != nullptr && ctx_area->full) {
        ScrArea *area = ctx_area;
        ED_area_newspace(C, ctx_area, space_type, true);
        area->flag |= AREA_FLAG_STACKED_FULLSCREEN;
        ((SpaceLink *)area->spacedata.first)->link_flag |= SPACE_FLAG_TYPE_TEMPORARY;
        return area;
      }

      /* Create a new fullscreen area. */
      ScrArea *area = ED_screen_full_newspace(C, ctx_area, int(space_type));
      ((SpaceLink *)area->spacedata.first)->link_flag |= SPACE_FLAG_TYPE_TEMPORARY;
      return area;
    }
  }

  return nullptr;
}

void ED_screen_animation_timer(
    bContext *C, Scene *scene, ViewLayer *view_layer, int redraws, int sync, int enable)
{
  bScreen *screen = CTX_wm_screen(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  bScreen *stopscreen = ED_screen_animation_playing(wm);

  if (stopscreen) {
    WM_event_timer_remove(wm, win, stopscreen->animtimer);
    stopscreen->animtimer = nullptr;
  }

  if (enable) {
    ScreenAnimData *sad = MEM_callocN<ScreenAnimData>("ScreenAnimData");

    screen->animtimer = WM_event_timer_add(wm, win, TIMER0, (1.0 / scene->frames_per_second()));

    sad->region = CTX_wm_region(C);
    sad->scene = scene;
    sad->view_layer = view_layer;

    sad->do_scene_syncing = blender::ed::vse::is_scene_time_sync_needed(*C);

    sad->sfra = scene->r.cfra;
    /* Make sure that were are inside the scene or preview frame range. */
    CLAMP(scene->r.cfra, PSFRA, PEFRA);
    if (scene->r.cfra != sad->sfra) {
      sad->flag |= ANIMPLAY_FLAG_JUMPED;
    }

    if (sad->flag & ANIMPLAY_FLAG_JUMPED) {
      DEG_id_tag_update(&scene->id, ID_RECALC_FRAME_CHANGE);
    }

    sad->redraws = redraws;
    sad->flag |= (enable < 0) ? ANIMPLAY_FLAG_REVERSE : 0;
    sad->flag |= (sync == 0) ? ANIMPLAY_FLAG_NO_SYNC : (sync == 1) ? ANIMPLAY_FLAG_SYNC : 0;

    ScrArea *area = CTX_wm_area(C);

    char spacetype = -1;

    if (area) {
      spacetype = area->spacetype;
    }

    sad->from_anim_edit = ELEM(spacetype, SPACE_GRAPH, SPACE_ACTION, SPACE_NLA);

    screen->animtimer->customdata = sad;
  }

  /* Notifier caught by top header, for button. */
  WM_event_add_notifier(C, NC_SCREEN | ND_ANIMPLAY, nullptr);
}

/* helper for screen_animation_play() - only to be used for TimeLine */
static ARegion *time_top_left_3dwindow(bScreen *screen)
{
  ARegion *region_top_left = nullptr;
  int min = 10000;

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    if (area->spacetype == SPACE_VIEW3D) {
      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        if (region->regiontype == RGN_TYPE_WINDOW) {
          if (region->winrct.xmin - region->winrct.ymin < min) {
            region_top_left = region;
            min = region->winrct.xmin - region->winrct.ymin;
          }
        }
      }
    }
  }

  return region_top_left;
}

void ED_screen_animation_timer_update(bScreen *screen, int redraws)
{
  if (screen && screen->animtimer) {
    wmTimer *wt = screen->animtimer;
    ScreenAnimData *sad = static_cast<ScreenAnimData *>(wt->customdata);

    sad->redraws = redraws;
    sad->region = nullptr;
    if (redraws & TIME_REGION) {
      sad->region = time_top_left_3dwindow(screen);
    }
  }
}

void ED_update_for_newframe(Main *bmain, Depsgraph *depsgraph)
{
  Scene *scene = DEG_get_input_scene(depsgraph);

  DEG_time_tag_update(bmain);

  void *camera = BKE_scene_camera_switch_find(scene);
  if (camera && scene->camera != camera) {
    scene->camera = static_cast<Object *>(camera);
    /* are there cameras in the views that are not in the scene? */
    LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
      BKE_screen_view3d_scene_sync(screen, scene);
    }
    DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL | ID_RECALC_PARAMETERS);
  }

  ED_clip_update_frame(bmain, scene->r.cfra);

  /* this function applies the changes too */
  BKE_scene_graph_update_for_newframe(depsgraph);
}

bool ED_screen_stereo3d_required(const bScreen *screen, const Scene *scene)
{
  const bool is_multiview = (scene->r.scemode & R_MULTIVIEW) != 0;

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    switch (area->spacetype) {
      case SPACE_VIEW3D: {
        View3D *v3d;

        if (!is_multiview) {
          continue;
        }

        v3d = static_cast<View3D *>(area->spacedata.first);
        if (v3d->camera && v3d->stereo3d_camera == STEREO_3D_ID) {
          LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
            if (region->regiondata && region->regiontype == RGN_TYPE_WINDOW) {
              RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
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
        sima = static_cast<SpaceImage *>(area->spacedata.first);
        if (sima->image && BKE_image_is_stereo(sima->image) &&
            (sima->iuser.flag & IMA_SHOW_STEREO))
        {
          return true;
        }
        break;
      }
      case SPACE_NODE: {
        SpaceNode *snode;

        if (!is_multiview) {
          continue;
        }

        snode = static_cast<SpaceNode *>(area->spacedata.first);
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

        sseq = static_cast<SpaceSeq *>(area->spacedata.first);
        if (ELEM(sseq->view, SEQ_VIEW_PREVIEW, SEQ_VIEW_SEQUENCE_PREVIEW)) {
          return true;
        }

        break;
      }
    }
  }

  return false;
}

Scene *ED_screen_scene_find_with_window(const bScreen *screen,
                                        const wmWindowManager *wm,
                                        wmWindow **r_window)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (WM_window_get_active_screen(win) == screen) {
      if (r_window) {
        *r_window = win;
      }
      return WM_window_get_active_scene(win);
    }
  }

  /* Can by nullptr when accessing a screen that isn't active. */
  return nullptr;
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
  return nullptr;
}

Scene *ED_screen_scene_find(const bScreen *screen, const wmWindowManager *wm)
{
  return ED_screen_scene_find_with_window(screen, wm, nullptr);
}

wmWindow *ED_screen_window_find(const bScreen *screen, const wmWindowManager *wm)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (WM_window_get_active_screen(win) == screen) {
      return win;
    }
  }
  return nullptr;
}

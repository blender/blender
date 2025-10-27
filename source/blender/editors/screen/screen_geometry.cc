/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscr
 * \brief Functions for screen vertices and edges
 *
 * Screen geometry refers to the vertices (ScrVert) and edges (ScrEdge) through
 * which the flexible screen-layout system of Blender is established.
 */

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"

#include "BKE_screen.hh"

#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_screen.hh"

#include "MEM_guardedalloc.h"

#include "WM_api.hh"

#include "screen_intern.hh"

int screen_geom_area_height(const ScrArea *area)
{
  return area->v2->vec.y - area->v1->vec.y + 1;
}
int screen_geom_area_width(const ScrArea *area)
{
  return area->v4->vec.x - area->v1->vec.x + 1;
}

ScrVert *screen_geom_vertex_add_ex(ScrAreaMap *area_map, short x, short y)
{
  ScrVert *sv = MEM_callocN<ScrVert>("addscrvert");
  sv->vec.x = x;
  sv->vec.y = y;

  BLI_addtail(&area_map->vertbase, sv);
  return sv;
}
ScrVert *screen_geom_vertex_add(bScreen *screen, short x, short y)
{
  return screen_geom_vertex_add_ex(AREAMAP_FROM_SCREEN(screen), x, y);
}

ScrEdge *screen_geom_edge_add_ex(ScrAreaMap *area_map, ScrVert *v1, ScrVert *v2)
{
  ScrEdge *se = MEM_callocN<ScrEdge>("addscredge");

  BKE_screen_sort_scrvert(&v1, &v2);
  se->v1 = v1;
  se->v2 = v2;

  BLI_addtail(&area_map->edgebase, se);
  return se;
}
ScrEdge *screen_geom_edge_add(bScreen *screen, ScrVert *v1, ScrVert *v2)
{
  return screen_geom_edge_add_ex(AREAMAP_FROM_SCREEN(screen), v1, v2);
}

bool screen_geom_edge_is_horizontal(ScrEdge *se)
{
  return (se->v1->vec.y == se->v2->vec.y);
}

ScrEdge *screen_geom_area_map_find_active_scredge(
    const ScrAreaMap *area_map, const rcti *bounds_rect, const int mx, const int my, int safety)
{
  CLAMP_MIN(safety, 2);

  LISTBASE_FOREACH (ScrEdge *, se, &area_map->edgebase) {
    if (screen_geom_edge_is_horizontal(se)) {
      if ((se->v1->vec.y > bounds_rect->ymin) && (se->v1->vec.y < (bounds_rect->ymax - 1))) {
        short min, max;
        min = std::min(se->v1->vec.x, se->v2->vec.x);
        max = std::max(se->v1->vec.x, se->v2->vec.x);

        if (abs(my - se->v1->vec.y) <= safety && mx >= min && mx <= max) {
          return se;
        }
      }
    }
    else {
      if ((se->v1->vec.x > bounds_rect->xmin) && (se->v1->vec.x < (bounds_rect->xmax - 1))) {
        short min, max;
        min = std::min(se->v1->vec.y, se->v2->vec.y);
        max = std::max(se->v1->vec.y, se->v2->vec.y);

        if (abs(mx - se->v1->vec.x) <= safety && my >= min && my <= max) {
          return se;
        }
      }
    }
  }

  return nullptr;
}

ScrEdge *screen_geom_find_active_scredge(const wmWindow *win,
                                         const bScreen *screen,
                                         const int mx,
                                         const int my)
{
  if (U.app_flag & USER_APP_LOCK_EDGE_RESIZE) {
    return nullptr;
  }

  /* Use layout size (screen excluding global areas) for screen-layout area edges */
  rcti screen_rect;
  WM_window_screen_rect_calc(win, &screen_rect);
  ScrEdge *se = screen_geom_area_map_find_active_scredge(
      AREAMAP_FROM_SCREEN(screen), &screen_rect, mx, my, BORDERPADDING);

  if (!se) {
    /* Use entire window size (screen including global areas) for global area edges */
    rcti win_rect;
    WM_window_rect_calc(win, &win_rect);
    se = screen_geom_area_map_find_active_scredge(
        &win->global_areas, &win_rect, mx, my, int(BORDERPADDING_GLOBAL));
  }
  return se;
}

/**
 * A single pass for moving all screen vertices to fit into \a screen_rect.
 * \return true if another pass should be run.
 */
static bool screen_geom_vertices_scale_pass(const wmWindow *win,
                                            const bScreen *screen,
                                            const rcti *screen_rect)
{

  const int screen_size_x = BLI_rcti_size_x(screen_rect);
  const int screen_size_y = BLI_rcti_size_y(screen_rect);
  bool needs_another_pass = false;

  /* calculate size */
  float min[2] = {20000.0f, 20000.0f};
  float max[2] = {0.0f, 0.0f};

  LISTBASE_FOREACH (ScrVert *, sv, &screen->vertbase) {
    const float fv[2] = {float(sv->vec.x), float(sv->vec.y)};
    minmax_v2v2_v2(min, max, fv);
  }

  int screen_size_x_prev = (max[0] - min[0]) + 1;
  int screen_size_y_prev = (max[1] - min[1]) + 1;

  if (screen_size_x_prev != screen_size_x || screen_size_y_prev != screen_size_y) {
    const float facx = (float(screen_size_x) - 1) / (float(screen_size_x_prev) - 1);
    const float facy = (float(screen_size_y) - 1) / (float(screen_size_y_prev) - 1);

    /* make sure it fits! */
    LISTBASE_FOREACH (ScrVert *, sv, &screen->vertbase) {
      sv->vec.x = screen_rect->xmin + round_fl_to_short((sv->vec.x - min[0]) * facx);
      CLAMP(sv->vec.x, screen_rect->xmin, screen_rect->xmax - 1);

      sv->vec.y = screen_rect->ymin + round_fl_to_short((sv->vec.y - min[1]) * facy);
      CLAMP(sv->vec.y, screen_rect->ymin, screen_rect->ymax - 1);
    }

    /* test for collapsed areas. This could happen in some blender version... */
    /* ton: removed option now, it needs Context... */

    if (facy > 1) {
      /* Keep timeline small in video edit workspace. */
      LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
        const int border_width = int(ceil(float(U.border_width) * UI_SCALE_FAC));
        int min = ED_area_headersize() + border_width;
        if (area->v1->vec.y > screen_rect->ymin) {
          min += border_width;
        }
        if (area->spacetype == SPACE_ACTION && area->v1->vec.y == screen_rect->ymin &&
            screen_geom_area_height(area) <= int(min * 1.5f))
        {
          ScrEdge *se = BKE_screen_find_edge(screen, area->v2, area->v3);
          if (se) {
            const int yval = area->v1->vec.y + min - 1;

            screen_geom_select_connected_edge(win, se);

            /* all selected vertices get the right offset */
            LISTBASE_FOREACH (ScrVert *, sv, &screen->vertbase) {
              /* if is a collapsed area */
              if (!ELEM(sv, area->v1, area->v4)) {
                if (sv->flag) {
                  sv->vec.y = yval;
                  /* Changed size of a area. Run another pass to ensure everything still fits. */
                  needs_another_pass = true;
                }
              }
            }
          }
        }
      }
    }

    /* Make each window at least ED_area_headersize() high. This
     * should be done whether we are increasing or decreasing the
     * vertical size since this is called on file load, not just
     * during resize operations. */
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      const int border_width = int(ceil(float(U.border_width) * UI_SCALE_FAC));
      int min = ED_area_headersize() + border_width + border_width - U.pixelsize;
      if (area->v3->vec.y >= (screen_rect->ymax - 1)) {
        /* Area aligned to top screen edge. */
        min = ED_area_headersize() + border_width;
      }
      else if (area->v4->vec.y <= (screen_rect->ymin + 1)) {
        /* Area aligned to bottom screen edge. */
        min = ED_area_headersize() + border_width + 1;
      }

      const int height = screen_geom_area_height(area);
      if (height < min) {
        /* lower edge */
        ScrEdge *se = BKE_screen_find_edge(screen, area->v4, area->v1);
        if (se && area->v1 != area->v2) {
          const int yval = area->v2->vec.y - min;

          screen_geom_select_connected_edge(win, se);

          /* all selected vertices get the right offset */
          LISTBASE_FOREACH (ScrVert *, sv, &screen->vertbase) {
            /* if is not a collapsed area */
            if (!ELEM(sv, area->v2, area->v3)) {
              if (sv->flag) {
                sv->vec.y = yval;
                /* Changed size of a area. Run another pass to ensure everything still fits. */
                needs_another_pass = true;
              }
            }
          }
        }
      }
    }
  }

  return needs_another_pass;
}

void screen_geom_vertices_scale(const wmWindow *win, bScreen *screen)
{
  rcti window_rect, screen_rect;
  WM_window_rect_calc(win, &window_rect);
  WM_window_screen_rect_calc(win, &screen_rect);

  bool needs_another_pass;
  int max_passes_left = 10; /* Avoids endless loop. Number is rather arbitrary. */
  do {
    needs_another_pass = screen_geom_vertices_scale_pass(win, screen, &screen_rect);
    max_passes_left--;
  } while (needs_another_pass && (max_passes_left > 0));

  /* Global areas have a fixed size that only changes with the DPI.
   * Here we ensure that exactly this size is set. */
  LISTBASE_FOREACH (ScrArea *, area, &win->global_areas.areabase) {
    if (area->global->flag & GLOBAL_AREA_IS_HIDDEN) {
      continue;
    }

    int height = ED_area_global_size_y(area) - 1;

    if (area->v1->vec.y > window_rect.ymin) {
      height += U.pixelsize;
    }
    if (area->v2->vec.y < (window_rect.ymax - 1)) {
      height += U.pixelsize;
    }

    /* width */
    area->v1->vec.x = area->v2->vec.x = window_rect.xmin;
    area->v3->vec.x = area->v4->vec.x = window_rect.xmax - 1;
    /* height */
    area->v1->vec.y = area->v4->vec.y = window_rect.ymin;
    area->v2->vec.y = area->v3->vec.y = window_rect.ymax - 1;

    switch (area->global->align) {
      case GLOBAL_AREA_ALIGN_TOP:
        area->v1->vec.y = area->v4->vec.y = area->v2->vec.y - height;
        break;
      case GLOBAL_AREA_ALIGN_BOTTOM:
        area->v2->vec.y = area->v3->vec.y = area->v1->vec.y + height;
        break;
    }
  }
}

short screen_geom_find_area_split_point(const ScrArea *area,
                                        const rcti *window_rect,
                                        const eScreenAxis dir_axis,
                                        float fac)
{
  const int cur_area_width = screen_geom_area_width(area);
  const int cur_area_height = screen_geom_area_height(area);
  const short area_min_x = AREAMINX * UI_SCALE_FAC;
  const short area_min_y = ED_area_headersize();

  /* area big enough? */
  if (dir_axis == SCREEN_AXIS_V) {
    if (cur_area_width <= 2 * area_min_x) {
      return 0;
    }
  }
  else if (dir_axis == SCREEN_AXIS_H) {
    if (cur_area_height <= 2 * area_min_y) {
      return 0;
    }
  }

  /* to be sure */
  CLAMP(fac, 0.0f, 1.0f);

  if (dir_axis == SCREEN_AXIS_H) {
    short y = area->v1->vec.y + round_fl_to_short(fac * cur_area_height);

    int area_min = area_min_y;

    if (area->v1->vec.y > window_rect->ymin) {
      area_min += U.pixelsize;
    }
    if (area->v2->vec.y < (window_rect->ymax - 1)) {
      area_min += U.pixelsize;
    }

    if (y - area->v1->vec.y < area_min) {
      y = area->v1->vec.y + area_min;
    }
    else if (area->v2->vec.y - y < area_min) {
      y = area->v2->vec.y - area_min;
    }

    return y;
  }

  short x = area->v1->vec.x + round_fl_to_short(fac * cur_area_width);

  int area_min = area_min_x;

  if (area->v1->vec.x > window_rect->xmin) {
    area_min += U.pixelsize;
  }
  if (area->v4->vec.x < (window_rect->xmax - 1)) {
    area_min += U.pixelsize;
  }

  if (x - area->v1->vec.x < area_min) {
    x = area->v1->vec.x + area_min;
  }
  else if (area->v4->vec.x - x < area_min) {
    x = area->v4->vec.x - area_min;
  }

  return x;
}

void screen_geom_select_connected_edge(const wmWindow *win, ScrEdge *edge)
{
  bScreen *screen = WM_window_get_active_screen(win);

  /* 'dir_axis' is the direction of EDGE */
  eScreenAxis dir_axis;
  if (edge->v1->vec.x == edge->v2->vec.x) {
    dir_axis = SCREEN_AXIS_V;
  }
  else {
    dir_axis = SCREEN_AXIS_H;
  }

  ED_screen_verts_iter(win, screen, sv)
  {
    sv->flag = 0;
  }

  edge->v1->flag = 1;
  edge->v2->flag = 1;

  /* select connected, only in the right direction */
  bool oneselected = true;
  while (oneselected) {
    oneselected = false;
    LISTBASE_FOREACH (ScrEdge *, se, &screen->edgebase) {
      if (se->v1->flag + se->v2->flag == 1) {
        if (dir_axis == SCREEN_AXIS_H) {
          if (se->v1->vec.y == se->v2->vec.y) {
            se->v1->flag = se->v2->flag = 1;
            oneselected = true;
          }
        }
        else if (dir_axis == SCREEN_AXIS_V) {
          if (se->v1->vec.x == se->v2->vec.x) {
            se->v1->flag = se->v2->flag = 1;
            oneselected = true;
          }
        }
      }
    }
  }
}

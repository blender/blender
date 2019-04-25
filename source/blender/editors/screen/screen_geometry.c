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
 */

/** \file
 * \ingroup edscr
 * \brief Functions for screen vertices and edges
 *
 * Screen geometry refers to the vertices (ScrVert) and edges (ScrEdge) through
 * which the flexible screen-layout system of Blender is established.
 */

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"

#include "BKE_screen.h"

#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_screen.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"

#include "screen_intern.h"

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
  ScrVert *sv = MEM_callocN(sizeof(ScrVert), "addscrvert");
  sv->vec.x = x;
  sv->vec.y = y;

  BLI_addtail(&area_map->vertbase, sv);
  return sv;
}
ScrVert *screen_geom_vertex_add(bScreen *sc, short x, short y)
{
  return screen_geom_vertex_add_ex(AREAMAP_FROM_SCREEN(sc), x, y);
}

ScrEdge *screen_geom_edge_add_ex(ScrAreaMap *area_map, ScrVert *v1, ScrVert *v2)
{
  ScrEdge *se = MEM_callocN(sizeof(ScrEdge), "addscredge");

  BKE_screen_sort_scrvert(&v1, &v2);
  se->v1 = v1;
  se->v2 = v2;

  BLI_addtail(&area_map->edgebase, se);
  return se;
}
ScrEdge *screen_geom_edge_add(bScreen *sc, ScrVert *v1, ScrVert *v2)
{
  return screen_geom_edge_add_ex(AREAMAP_FROM_SCREEN(sc), v1, v2);
}

bool screen_geom_edge_is_horizontal(ScrEdge *se)
{
  return (se->v1->vec.y == se->v2->vec.y);
}

/**
 * \param bounds_rect: Either window or screen bounds.
 * Used to exclude edges along window/screen edges.
 */
ScrEdge *screen_geom_area_map_find_active_scredge(const ScrAreaMap *area_map,
                                                  const rcti *bounds_rect,
                                                  const int mx,
                                                  const int my)
{
  int safety = U.widget_unit / 10;

  CLAMP_MIN(safety, 2);

  for (ScrEdge *se = area_map->edgebase.first; se; se = se->next) {
    if (screen_geom_edge_is_horizontal(se)) {
      if ((se->v1->vec.y > bounds_rect->ymin) && (se->v1->vec.y < (bounds_rect->ymax - 1))) {
        short min, max;
        min = MIN2(se->v1->vec.x, se->v2->vec.x);
        max = MAX2(se->v1->vec.x, se->v2->vec.x);

        if (abs(my - se->v1->vec.y) <= safety && mx >= min && mx <= max) {
          return se;
        }
      }
    }
    else {
      if ((se->v1->vec.x > bounds_rect->xmin) && (se->v1->vec.x < (bounds_rect->xmax - 1))) {
        short min, max;
        min = MIN2(se->v1->vec.y, se->v2->vec.y);
        max = MAX2(se->v1->vec.y, se->v2->vec.y);

        if (abs(mx - se->v1->vec.x) <= safety && my >= min && my <= max) {
          return se;
        }
      }
    }
  }

  return NULL;
}

/* need win size to make sure not to include edges along screen edge */
ScrEdge *screen_geom_find_active_scredge(const wmWindow *win,
                                         const bScreen *screen,
                                         const int mx,
                                         const int my)
{
  /* Use layout size (screen excluding global areas) for screen-layout area edges */
  rcti screen_rect;
  ScrEdge *se;

  WM_window_screen_rect_calc(win, &screen_rect);
  se = screen_geom_area_map_find_active_scredge(AREAMAP_FROM_SCREEN(screen), &screen_rect, mx, my);

  if (!se) {
    /* Use entire window size (screen including global areas) for global area edges */
    rcti win_rect;
    WM_window_rect_calc(win, &win_rect);
    se = screen_geom_area_map_find_active_scredge(&win->global_areas, &win_rect, mx, my);
  }
  return se;
}

/**
 * \brief Main screen-layout calculation function.
 *
 * * Scale areas nicely on window size and DPI changes.
 * * Ensure areas have a minimum height.
 * * Correctly set global areas to their fixed height.
 */
void screen_geom_vertices_scale(const wmWindow *win, bScreen *sc)
{
  rcti window_rect, screen_rect;

  WM_window_rect_calc(win, &window_rect);
  WM_window_screen_rect_calc(win, &screen_rect);

  const int screen_size_x = BLI_rcti_size_x(&screen_rect);
  const int screen_size_y = BLI_rcti_size_y(&screen_rect);
  ScrVert *sv = NULL;
  ScrArea *sa;
  int screen_size_x_prev, screen_size_y_prev;
  float min[2], max[2];

  /* calculate size */
  min[0] = min[1] = 20000.0f;
  max[0] = max[1] = 0.0f;

  for (sv = sc->vertbase.first; sv; sv = sv->next) {
    const float fv[2] = {(float)sv->vec.x, (float)sv->vec.y};
    minmax_v2v2_v2(min, max, fv);
  }

  screen_size_x_prev = (max[0] - min[0]) + 1;
  screen_size_y_prev = (max[1] - min[1]) + 1;

  if (screen_size_x_prev != screen_size_x || screen_size_y_prev != screen_size_y) {
    const float facx = ((float)screen_size_x - 1) / ((float)screen_size_x_prev - 1);
    const float facy = ((float)screen_size_y - 1) / ((float)screen_size_y_prev - 1);

    /* make sure it fits! */
    for (sv = sc->vertbase.first; sv; sv = sv->next) {
      sv->vec.x = screen_rect.xmin + round_fl_to_short((sv->vec.x - min[0]) * facx);
      CLAMP(sv->vec.x, screen_rect.xmin, screen_rect.xmax - 1);

      sv->vec.y = screen_rect.ymin + round_fl_to_short((sv->vec.y - min[1]) * facy);
      CLAMP(sv->vec.y, screen_rect.ymin, screen_rect.ymax - 1);
    }

    /* test for collapsed areas. This could happen in some blender version... */
    /* ton: removed option now, it needs Context... */

    int headery = ED_area_headersize() + (U.pixelsize * 2);

    if (facy > 1) {
      /* Keep timeline small in video edit workspace. */
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        if (sa->spacetype == SPACE_ACTION && sa->v1->vec.y == screen_rect.ymin &&
            screen_geom_area_height(sa) <= headery * facy + 1) {
          ScrEdge *se = BKE_screen_find_edge(sc, sa->v2, sa->v3);
          if (se) {
            const int yval = sa->v1->vec.y + headery - 1;

            screen_geom_select_connected_edge(win, se);

            /* all selected vertices get the right offset */
            for (sv = sc->vertbase.first; sv; sv = sv->next) {
              /* if is a collapsed area */
              if (sv != sa->v1 && sv != sa->v4) {
                if (sv->flag) {
                  sv->vec.y = yval;
                }
              }
            }
          }
        }
      }
    }
    if (facy < 1) {
      /* make each window at least ED_area_headersize() high */
      for (sa = sc->areabase.first; sa; sa = sa->next) {
        if (screen_geom_area_height(sa) < headery) {
          /* lower edge */
          ScrEdge *se = BKE_screen_find_edge(sc, sa->v4, sa->v1);
          if (se && sa->v1 != sa->v2) {
            const int yval = sa->v2->vec.y - headery + 1;

            screen_geom_select_connected_edge(win, se);

            /* all selected vertices get the right offset */
            for (sv = sc->vertbase.first; sv; sv = sv->next) {
              /* if is not a collapsed area */
              if (sv != sa->v2 && sv != sa->v3) {
                if (sv->flag) {
                  sv->vec.y = yval;
                }
              }
            }
          }
        }
      }
    }
  }

  /* Global areas have a fixed size that only changes with the DPI.
   * Here we ensure that exactly this size is set. */
  for (ScrArea *area = win->global_areas.areabase.first; area; area = area->next) {
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

/**
 * \return 0 if no split is possible, otherwise the screen-coordinate at which to split.
 */
short screen_geom_find_area_split_point(const ScrArea *sa,
                                        const rcti *window_rect,
                                        char dir,
                                        float fac)
{
  short x, y;
  const int cur_area_width = screen_geom_area_width(sa);
  const int cur_area_height = screen_geom_area_height(sa);
  const short area_min_x = AREAMINX;
  const short area_min_y = ED_area_headersize();
  int area_min;

  // area big enough?
  if ((dir == 'v') && (cur_area_width <= 2 * area_min_x)) {
    return 0;
  }
  if ((dir == 'h') && (cur_area_height <= 2 * area_min_y)) {
    return 0;
  }

  // to be sure
  CLAMP(fac, 0.0f, 1.0f);

  if (dir == 'h') {
    y = sa->v1->vec.y + round_fl_to_short(fac * cur_area_height);

    area_min = area_min_y;

    if (sa->v1->vec.y > window_rect->ymin) {
      area_min += U.pixelsize;
    }
    if (sa->v2->vec.y < (window_rect->ymax - 1)) {
      area_min += U.pixelsize;
    }

    if (y - sa->v1->vec.y < area_min) {
      y = sa->v1->vec.y + area_min;
    }
    else if (sa->v2->vec.y - y < area_min) {
      y = sa->v2->vec.y - area_min;
    }

    return y;
  }
  else {
    x = sa->v1->vec.x + round_fl_to_short(fac * cur_area_width);

    area_min = area_min_x;

    if (sa->v1->vec.x > window_rect->xmin) {
      area_min += U.pixelsize;
    }
    if (sa->v4->vec.x < (window_rect->xmax - 1)) {
      area_min += U.pixelsize;
    }

    if (x - sa->v1->vec.x < area_min) {
      x = sa->v1->vec.x + area_min;
    }
    else if (sa->v4->vec.x - x < area_min) {
      x = sa->v4->vec.x - area_min;
    }

    return x;
  }
}

/**
 * Select all edges that are directly or indirectly connected to \a edge.
 */
void screen_geom_select_connected_edge(const wmWindow *win, ScrEdge *edge)
{
  bScreen *sc = WM_window_get_active_screen(win);
  bool oneselected = true;
  char dir;

  /* select connected, only in the right direction */
  /* 'dir' is the direction of EDGE */

  if (edge->v1->vec.x == edge->v2->vec.x) {
    dir = 'v';
  }
  else {
    dir = 'h';
  }

  ED_screen_verts_iter(win, sc, sv)
  {
    sv->flag = 0;
  }

  edge->v1->flag = 1;
  edge->v2->flag = 1;

  while (oneselected) {
    oneselected = false;
    for (ScrEdge *se = sc->edgebase.first; se; se = se->next) {
      if (se->v1->flag + se->v2->flag == 1) {
        if (dir == 'h') {
          if (se->v1->vec.y == se->v2->vec.y) {
            se->v1->flag = se->v2->flag = 1;
            oneselected = true;
          }
        }
        if (dir == 'v') {
          if (se->v1->vec.x == se->v2->vec.x) {
            se->v1->flag = se->v2->flag = 1;
            oneselected = true;
          }
        }
      }
    }
  }
}

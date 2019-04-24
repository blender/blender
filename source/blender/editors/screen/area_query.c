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
 *
 * Query functions for area/region.
 */

#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math_base.h"

#include "RNA_types.h"

#include "WM_api.h"

#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_view2d.h"

bool ED_region_overlap_isect_x(const ARegion *ar, const int event_x)
{
  BLI_assert(ar->overlap);
  /* No contents, skip it. */
  if (ar->v2d.mask.xmin == ar->v2d.mask.xmax) {
    return false;
  }
  return BLI_rctf_isect_x(&ar->v2d.tot,
                          UI_view2d_region_to_view_x(&ar->v2d, event_x - ar->winrct.xmin));
}

bool ED_region_overlap_isect_y(const ARegion *ar, const int event_y)
{
  BLI_assert(ar->overlap);
  /* No contents, skip it. */
  if (ar->v2d.mask.ymin == ar->v2d.mask.ymax) {
    return false;
  }
  return BLI_rctf_isect_y(&ar->v2d.tot,
                          UI_view2d_region_to_view_y(&ar->v2d, event_y - ar->winrct.ymin));
}

bool ED_region_overlap_isect_xy(const ARegion *ar, const int event_xy[2])
{
  return (ED_region_overlap_isect_x(ar, event_xy[0]) &&
          ED_region_overlap_isect_y(ar, event_xy[1]));
}

bool ED_region_panel_category_gutter_calc_rect(const ARegion *ar, rcti *r_ar_gutter)
{
  *r_ar_gutter = ar->winrct;
  if (UI_panel_category_is_visible(ar)) {
    const int category_tabs_width = round_fl_to_int(UI_view2d_scale_get_x(&ar->v2d) *
                                                    UI_PANEL_CATEGORY_MARGIN_WIDTH);
    if (ar->alignment == RGN_ALIGN_LEFT) {
      r_ar_gutter->xmax = r_ar_gutter->xmin + category_tabs_width;
    }
    else if (ar->alignment == RGN_ALIGN_RIGHT) {
      r_ar_gutter->xmin = r_ar_gutter->xmax - category_tabs_width;
    }
    else {
      BLI_assert(!"Unsupported alignment");
    }
    return true;
  }
  return false;
}

bool ED_region_panel_category_gutter_isect_xy(const ARegion *ar, const int event_xy[2])
{
  rcti ar_gutter;
  if (ED_region_panel_category_gutter_calc_rect(ar, &ar_gutter)) {
    return BLI_rcti_isect_pt_v(&ar_gutter, event_xy);
  }
  return false;
}

bool ED_region_overlap_isect_x_with_margin(const ARegion *ar, const int event_x, const int margin)
{
  BLI_assert(ar->overlap);
  /* No contents, skip it. */
  if (ar->v2d.mask.xmin == ar->v2d.mask.xmax) {
    return false;
  }
  int region_x = event_x - ar->winrct.xmin;
  return ((ar->v2d.tot.xmin <= UI_view2d_region_to_view_x(&ar->v2d, region_x + margin)) &&
          (ar->v2d.tot.xmax >= UI_view2d_region_to_view_x(&ar->v2d, region_x - margin)));
}

bool ED_region_overlap_isect_y_with_margin(const ARegion *ar, const int event_y, const int margin)
{
  BLI_assert(ar->overlap);
  /* No contents, skip it. */
  if (ar->v2d.mask.ymin == ar->v2d.mask.ymax) {
    return false;
  }
  int region_y = event_y - ar->winrct.ymin;
  return ((ar->v2d.tot.ymin <= UI_view2d_region_to_view_y(&ar->v2d, region_y + margin)) &&
          (ar->v2d.tot.ymax >= UI_view2d_region_to_view_y(&ar->v2d, region_y - margin)));
}

bool ED_region_overlap_isect_xy_with_margin(const ARegion *ar,
                                            const int event_xy[2],
                                            const int margin)
{
  return (ED_region_overlap_isect_x_with_margin(ar, event_xy[0], margin) &&
          ED_region_overlap_isect_y_with_margin(ar, event_xy[1], margin));
}

bool ED_region_contains_xy(const ARegion *ar, const int event_xy[2])
{
  /* Only use the margin when inside the region. */
  if (BLI_rcti_isect_pt_v(&ar->winrct, event_xy)) {
    if (ar->overlap) {
      const int overlap_margin = UI_REGION_OVERLAP_MARGIN;
      /* Note the View2D.tot isn't reliable for headers with spacers otherwise
       * we'd check #ED_region_overlap_isect_xy_with_margin for both bases. */
      if (ar->v2d.keeptot == V2D_KEEPTOT_STRICT) {
        /* Header. */
        rcti rect;
        BLI_rcti_init_pt_radius(&rect, event_xy, overlap_margin);
        if (UI_region_but_find_rect_over(ar, &rect) == NULL) {
          return false;
        }
      }
      else {
        /* Side-bar & any other kind of overlapping region. */

        /* Check alignment to avoid region tabs being clipped out
         * by only clipping a single axis for aligned regions. */
        if (ELEM(ar->alignment, RGN_ALIGN_TOP, RGN_ALIGN_BOTTOM)) {
          if (!ED_region_overlap_isect_x_with_margin(ar, event_xy[0], overlap_margin)) {
            return false;
          }
        }
        else if (ELEM(ar->alignment, RGN_ALIGN_LEFT, RGN_ALIGN_RIGHT)) {
          if (ED_region_panel_category_gutter_isect_xy(ar, event_xy)) {
            /* pass */
          }
          else if (!ED_region_overlap_isect_y_with_margin(ar, event_xy[1], overlap_margin)) {
            return false;
          }
        }
        else {
          /* No panel categories for horizontal regions currently. */
          if (!ED_region_overlap_isect_xy_with_margin(ar, event_xy, overlap_margin)) {
            return false;
          }
        }
      }
    }
    return true;
  }
  return false;
}

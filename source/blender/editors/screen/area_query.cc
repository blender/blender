/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscr
 *
 * Query functions for area/region.
 */

#include "BKE_screen.hh"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_view2d.hh"

bool ED_region_overlap_isect_x(const ARegion *region, const int event_x)
{
  BLI_assert(region->overlap);
  /* No contents, skip it. */
  if (region->v2d.mask.xmin == region->v2d.mask.xmax) {
    return false;
  }
  if ((event_x < region->winrct.xmin) || (event_x > region->winrct.xmax)) {
    return false;
  }
  return BLI_rctf_isect_x(&region->v2d.tot,
                          UI_view2d_region_to_view_x(&region->v2d, event_x - region->winrct.xmin));
}

bool ED_region_overlap_isect_y(const ARegion *region, const int event_y)
{
  BLI_assert(region->overlap);
  /* No contents, skip it. */
  if (region->v2d.mask.ymin == region->v2d.mask.ymax) {
    return false;
  }
  if ((event_y < region->winrct.ymin) || (event_y > region->winrct.ymax)) {
    return false;
  }
  return BLI_rctf_isect_y(&region->v2d.tot,
                          UI_view2d_region_to_view_y(&region->v2d, event_y - region->winrct.ymin));
}

bool ED_region_overlap_isect_xy(const ARegion *region, const int event_xy[2])
{
  return (ED_region_overlap_isect_x(region, event_xy[0]) &&
          ED_region_overlap_isect_y(region, event_xy[1]));
}

bool ED_region_overlap_isect_any_xy(const ScrArea *area, const int event_xy[2])
{
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (!region->runtime->visible) {
      continue;
    }
    if (ED_region_is_overlap(area->spacetype, region->regiontype)) {
      if (ED_region_overlap_isect_xy(region, event_xy)) {
        return true;
      }
    }
  }
  return false;
}

bool ED_region_panel_category_gutter_calc_rect(const ARegion *region, rcti *r_region_gutter)
{
  *r_region_gutter = region->winrct;
  if (UI_panel_category_is_visible(region)) {
    const int category_tabs_width = round_fl_to_int(UI_view2d_scale_get_x(&region->v2d) *
                                                    UI_PANEL_CATEGORY_MARGIN_WIDTH);
    const int alignment = RGN_ALIGN_ENUM_FROM_MASK(region->alignment);

    if (alignment == RGN_ALIGN_LEFT) {
      r_region_gutter->xmax = r_region_gutter->xmin + category_tabs_width;
    }
    else if (alignment == RGN_ALIGN_RIGHT) {
      r_region_gutter->xmin = r_region_gutter->xmax - category_tabs_width;
    }
    else {
      BLI_assert_msg(0, "Unsupported alignment");
    }
    return true;
  }
  return false;
}

bool ED_region_panel_category_gutter_isect_xy(const ARegion *region, const int event_xy[2])
{
  rcti region_gutter;
  if (ED_region_panel_category_gutter_calc_rect(region, &region_gutter)) {
    return BLI_rcti_isect_pt_v(&region_gutter, event_xy);
  }
  return false;
}

bool ED_region_overlap_isect_x_with_margin(const ARegion *region,
                                           const int event_x,
                                           const int margin)
{
  BLI_assert(region->overlap);
  /* No contents, skip it. */
  if (region->v2d.mask.xmin == region->v2d.mask.xmax) {
    return false;
  }
  if ((event_x < region->winrct.xmin) || (event_x > region->winrct.xmax)) {
    return false;
  }
  const int region_x = event_x - region->winrct.xmin;
  return ((region->v2d.tot.xmin <= UI_view2d_region_to_view_x(&region->v2d, region_x + margin)) &&
          (region->v2d.tot.xmax >= UI_view2d_region_to_view_x(&region->v2d, region_x - margin)));
}

bool ED_region_overlap_isect_y_with_margin(const ARegion *region,
                                           const int event_y,
                                           const int margin)
{
  BLI_assert(region->overlap);
  /* No contents, skip it. */
  if (region->v2d.mask.ymin == region->v2d.mask.ymax) {
    return false;
  }
  if ((event_y < region->winrct.ymin) || (event_y > region->winrct.ymax)) {
    return false;
  }
  const int region_y = event_y - region->winrct.ymin;
  return (region->v2d.tot.ymin <= UI_view2d_region_to_view_y(&region->v2d, region_y + margin)) &&
         (region->v2d.tot.ymax >= UI_view2d_region_to_view_y(&region->v2d, region_y - margin));
}

bool ED_region_overlap_isect_xy_with_margin(const ARegion *region,
                                            const int event_xy[2],
                                            const int margin)
{
  return (ED_region_overlap_isect_x_with_margin(region, event_xy[0], margin) &&
          ED_region_overlap_isect_y_with_margin(region, event_xy[1], margin));
}

bool ED_region_contains_xy(const ARegion *region, const int event_xy[2])
{
  /* Only use the margin when inside the region. */
  if (BLI_rcti_isect_pt_v(&region->winrct, event_xy)) {
    if (region->overlap) {
      const int overlap_margin = UI_REGION_OVERLAP_MARGIN;
      /* Note the View2D.tot isn't reliable for headers with spacers otherwise
       * we'd check #ED_region_overlap_isect_xy_with_margin for both bases. */
      if (region->v2d.keeptot == V2D_KEEPTOT_STRICT) {
        /* Header. */
        rcti rect;
        BLI_rcti_init_pt_radius(&rect, event_xy, overlap_margin);
        if (UI_region_but_find_rect_over(region, &rect) == nullptr) {
          return false;
        }
      }
      else {
        /* Side-bar & any other kind of overlapping region. */

        const int alignment = RGN_ALIGN_ENUM_FROM_MASK(region->alignment);

        /* Check alignment to avoid region tabs being clipped out
         * by only clipping a single axis for aligned regions. */
        if (ELEM(alignment, RGN_ALIGN_TOP, RGN_ALIGN_BOTTOM)) {
          if (!ED_region_overlap_isect_x_with_margin(region, event_xy[0], overlap_margin)) {
            return false;
          }
        }
        else if (ELEM(alignment, RGN_ALIGN_LEFT, RGN_ALIGN_RIGHT)) {
          if (ED_region_panel_category_gutter_isect_xy(region, event_xy)) {
            /* pass */
          }
          else if (!ED_region_overlap_isect_y_with_margin(region, event_xy[1], overlap_margin)) {
            return false;
          }
        }
        else {
          /* No panel categories for horizontal regions currently. */
          if (!ED_region_overlap_isect_xy_with_margin(region, event_xy, overlap_margin)) {
            return false;
          }
        }
      }
    }
    return true;
  }
  return false;
}

ARegion *ED_area_find_region_xy_visual(const ScrArea *area,
                                       const int regiontype,
                                       const int event_xy[2])
{
  if (!area) {
    return nullptr;
  }

  /* Check overlapped regions first. */
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (!region->overlap) {
      continue;
    }
    if (ELEM(regiontype, RGN_TYPE_ANY, region->regiontype)) {
      if (ED_region_contains_xy(region, event_xy)) {
        return region;
      }
    }
  }
  /* Now non-overlapping ones. */
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->overlap) {
      continue;
    }
    if (ELEM(regiontype, RGN_TYPE_ANY, region->regiontype)) {
      if (ED_region_contains_xy(region, event_xy)) {
        return region;
      }
    }
  }

  return nullptr;
}

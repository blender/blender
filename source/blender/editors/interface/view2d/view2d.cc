/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_link_utils.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_memarena.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_screen.hh"

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "WM_api.hh"

#include "BLF_api.hh"

#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_view2d.hh"

#include "view2d_intern.hh"

static void ui_view2d_curRect_validate_resize(View2D *v2d, bool resize);

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

BLI_INLINE int clamp_float_to_int(const float f)
{
  const float min = float(INT_MIN);
  const float max = float(INT_MAX);

  if (UNLIKELY(f < min)) {
    return min;
  }
  if (UNLIKELY(f > max)) {
    return int(max);
  }
  return int(f);
}

/**
 * use instead of #BLI_rcti_rctf_copy so we have consistent behavior
 * with users of #clamp_float_to_int.
 */
BLI_INLINE void clamp_rctf_to_rcti(rcti *dst, const rctf *src)
{
  dst->xmin = clamp_float_to_int(src->xmin);
  dst->xmax = clamp_float_to_int(src->xmax);
  dst->ymin = clamp_float_to_int(src->ymin);
  dst->ymax = clamp_float_to_int(src->ymax);
}

float view2d_page_size_y(const View2D &v2d)
{
  return v2d.page_size_y ? v2d.page_size_y : BLI_rcti_size_y(&v2d.mask);
}

/* XXX still unresolved: scrolls hide/unhide vs region mask handling */
/* XXX there's V2D_SCROLL_HORIZONTAL_HIDE and V2D_SCROLL_HORIZONTAL_FULLR ... */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Scroll & Mask Utilities
 * \{ */

/**
 * Helper to allow scroll-bars to dynamically hide:
 * - Returns a copy of the scroll-bar settings with the flags to display
 *   horizontal/vertical scroll-bars removed.
 * - Input scroll value is the v2d->scroll var.
 * - Hide flags are set per region at draw-time.
 */
static int view2d_scroll_mapped(int scroll)
{
  if (scroll & V2D_SCROLL_HORIZONTAL_FULLR) {
    scroll &= ~V2D_SCROLL_HORIZONTAL;
  }
  if (scroll & V2D_SCROLL_VERTICAL_FULLR) {
    scroll &= ~V2D_SCROLL_VERTICAL;
  }
  return scroll;
}

void UI_view2d_mask_from_win(const View2D *v2d, rcti *r_mask)
{
  r_mask->xmin = 0;
  r_mask->ymin = 0;
  r_mask->xmax = v2d->winx - 1; /* -1 yes! masks are pixels */
  r_mask->ymax = v2d->winy - 1;
}

/**
 * Called each time #View2D.cur changes, to dynamically update masks.
 *
 * \param mask_scroll: Optionally clamp scroll-bars by this region.
 */
static void view2d_masks(View2D *v2d, const rcti *mask_scroll)
{
  int scroll;

  /* mask - view frame */
  UI_view2d_mask_from_win(v2d, &v2d->mask);
  if (mask_scroll == nullptr) {
    mask_scroll = &v2d->mask;
  }

  /* check size if hiding flag is set: */
  if (v2d->scroll & V2D_SCROLL_HORIZONTAL_HIDE) {
    if (!(v2d->scroll & V2D_SCROLL_HORIZONTAL_HANDLES)) {
      if (BLI_rctf_size_x(&v2d->tot) > BLI_rctf_size_x(&v2d->cur)) {
        v2d->scroll &= ~V2D_SCROLL_HORIZONTAL_FULLR;
      }
      else {
        v2d->scroll |= V2D_SCROLL_HORIZONTAL_FULLR;
      }
    }
  }
  if (v2d->scroll & V2D_SCROLL_VERTICAL_HIDE) {
    if (!(v2d->scroll & V2D_SCROLL_VERTICAL_HANDLES)) {
      if (BLI_rctf_size_y(&v2d->tot) + 0.01f > BLI_rctf_size_y(&v2d->cur)) {
        v2d->scroll &= ~V2D_SCROLL_VERTICAL_FULLR;
      }
      else {
        v2d->scroll |= V2D_SCROLL_VERTICAL_FULLR;
      }
    }
  }

  /* Do not use mapped scroll here because we want to update scroller rects
   * even if they are not displayed. For initialization purposes. See #75003. */
  scroll = v2d->scroll;

  /* Scrollers are based off region-size:
   * - they can only be on one to two edges of the region they define
   * - if they overlap, they must not occupy the corners (which are reserved for other widgets)
   */
  if (scroll) {
    float scroll_width, scroll_height;

    UI_view2d_scroller_size_get(v2d, false, &scroll_width, &scroll_height);

    /* vertical scroller */
    if (scroll & V2D_SCROLL_LEFT) {
      /* on left-hand edge of region */
      v2d->vert = *mask_scroll;
      v2d->vert.xmax = v2d->vert.xmin + scroll_width;
    }
    else if (scroll & V2D_SCROLL_RIGHT) {
      /* on right-hand edge of region */
      v2d->vert = *mask_scroll;
      v2d->vert.xmax++; /* one pixel extra... was leaving a minor gap... */
      v2d->vert.xmin = v2d->vert.xmax - scroll_width;
    }

    /* horizontal scroller */
    if (scroll & V2D_SCROLL_BOTTOM) {
      /* on bottom edge of region */
      v2d->hor = *mask_scroll;
      v2d->hor.ymax = scroll_height;
    }
    else if (scroll & V2D_SCROLL_TOP) {
      /* on upper edge of region */
      v2d->hor = *mask_scroll;
      v2d->hor.ymin = v2d->hor.ymax - scroll_height;
    }

    /* Adjust horizontal scroller to avoid interfering with splitter areas. */
    if (scroll & V2D_SCROLL_HORIZONTAL) {
      v2d->hor.xmin += UI_AZONESPOTW_LEFT;
      v2d->hor.xmax -= UI_AZONESPOTW_RIGHT;
    }

    /* Adjust vertical scroller to avoid horizontal scrollers and splitter areas. */
    if (scroll & V2D_SCROLL_VERTICAL) {
      /* Note that top splitter areas are in the header,
       * outside of `mask_scroll`, so we can ignore them. */
      v2d->vert.ymin += UI_AZONESPOTH;
      if (scroll & V2D_SCROLL_BOTTOM) {
        /* on bottom edge of region */
        v2d->vert.ymin = max_ii(v2d->hor.ymax, v2d->vert.ymin);
      }
      else if (scroll & V2D_SCROLL_TOP) {
        /* on upper edge of region */
        v2d->vert.ymax = v2d->hor.ymin;
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View2D Refresh and Validation (Spatial)
 * \{ */

void UI_view2d_region_reinit(View2D *v2d, short type, int winx, int winy)
{
  bool tot_changed = false, do_init;
  const uiStyle *style = UI_style_get();

  do_init = (v2d->flag & V2D_IS_INIT) == 0;

  /* see eView2D_CommonViewTypes in UI_view2d.hh for available view presets */
  switch (type) {
    /* 'standard view' - optimum setup for 'standard' view behavior,
     * that should be used new views as basis for their
     * own unique View2D settings, which should be used instead of this in most cases...
     */
    case V2D_COMMONVIEW_STANDARD: {
      /* for now, aspect ratio should be maintained,
       * and zoom is clamped within sane default limits */
      v2d->keepzoom = (V2D_KEEPASPECT | V2D_LIMITZOOM);
      v2d->minzoom = 0.01f;
      v2d->maxzoom = 1000.0f;

      /* View2D tot rect and cur should be same size,
       * and aligned using 'standard' OpenGL coordinates for now:
       * - region can resize 'tot' later to fit other data
       * - keeptot is only within bounds, as strict locking is not that critical
       * - view is aligned for (0,0) -> (winx-1, winy-1) setup
       */
      v2d->align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_NEG_Y);
      v2d->keeptot = V2D_KEEPTOT_BOUNDS;
      if (do_init) {
        v2d->tot.xmin = v2d->tot.ymin = 0.0f;
        v2d->tot.xmax = float(winx - 1);
        v2d->tot.ymax = float(winy - 1);

        v2d->cur = v2d->tot;
      }
      /* scrollers - should we have these by default? */
      /* XXX for now, we don't override this, or set it either! */
      break;
    }
    /* 'list/channel view' - zoom, aspect ratio, and alignment restrictions are set here */
    case V2D_COMMONVIEW_LIST: {
      /* zoom + aspect ratio are locked */
      v2d->keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
      v2d->minzoom = v2d->maxzoom = 1.0f;

      /* tot rect has strictly regulated placement, and must only occur in +/- quadrant */
      v2d->align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y);
      v2d->keeptot = V2D_KEEPTOT_STRICT;
      tot_changed = do_init;

      /* scroller settings are currently not set here... that is left for regions... */
      break;
    }
    /* 'stack view' - practically the same as list/channel view,
     * except is located in the pos y half instead.
     * Zoom, aspect ratio, and alignment restrictions are set here. */
    case V2D_COMMONVIEW_STACK: {
      /* zoom + aspect ratio are locked */
      v2d->keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
      v2d->minzoom = v2d->maxzoom = 1.0f;

      /* tot rect has strictly regulated placement, and must only occur in +/+ quadrant */
      v2d->align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_NEG_Y);
      v2d->keeptot = V2D_KEEPTOT_STRICT;
      tot_changed = do_init;

      /* scroller settings are currently not set here... that is left for regions... */
      break;
    }
    /* 'header' regions - zoom, aspect ratio,
     * alignment, and panning restrictions are set here */
    case V2D_COMMONVIEW_HEADER: {
      /* zoom + aspect ratio are locked */
      v2d->keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
      v2d->minzoom = v2d->maxzoom = 1.0f;

      if (do_init) {
        v2d->tot.xmin = 0.0f;
        v2d->tot.xmax = winx;
        v2d->tot.ymin = 0.0f;
        v2d->tot.ymax = winy;
        v2d->cur = v2d->tot;

        v2d->min[0] = v2d->max[0] = float(winx - 1);
        v2d->min[1] = v2d->max[1] = float(winy - 1);
      }
      /* tot rect has strictly regulated placement, and must only occur in +/+ quadrant */
      v2d->align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_NEG_Y);
      v2d->keeptot = V2D_KEEPTOT_STRICT;
      tot_changed = do_init;

      /* panning in y-axis is prohibited */
      v2d->keepofs = V2D_LOCKOFS_Y;

      /* absolutely no scrollers allowed */
      v2d->scroll = 0;
      break;
    }
    /* panels view, with horizontal/vertical align */
    case V2D_COMMONVIEW_PANELS_UI: {

      /* for now, aspect ratio should be maintained,
       * and zoom is clamped within sane default limits */
      v2d->keepzoom = (V2D_KEEPASPECT | V2D_LIMITZOOM | V2D_KEEPZOOM);
      v2d->minzoom = 0.5f;
      v2d->maxzoom = 2.0f;

      v2d->align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y);
      v2d->keeptot = V2D_KEEPTOT_BOUNDS;

      /* NOTE: scroll is being flipped in #ED_region_panels() drawing. */
      v2d->scroll |= (V2D_SCROLL_HORIZONTAL_HIDE | V2D_SCROLL_VERTICAL_HIDE);

      if (do_init) {
        const float panelzoom = (style) ? style->panelzoom : 1.0f;

        v2d->tot.xmin = 0.0f;
        v2d->tot.xmax = winx;

        v2d->tot.ymax = 0.0f;
        v2d->tot.ymin = -winy;

        v2d->cur.xmin = 0.0f;
        v2d->cur.xmax = (winx)*panelzoom;

        v2d->cur.ymax = 0.0f;
        v2d->cur.ymin = (-winy) * panelzoom;
      }
      break;
    }
    /* other view types are completely defined using their own settings already */
    default:
      /* we don't do anything here,
       * as settings should be fine, but just make sure that rect */
      break;
  }

  /* set initialized flag so that View2D doesn't get reinitialized next time again */
  v2d->flag |= V2D_IS_INIT;

  /* store view size */
  v2d->winx = winx;
  v2d->winy = winy;

  view2d_masks(v2d, nullptr);

  if (do_init) {
    /* Visible by default. */
    v2d->alpha_hor = v2d->alpha_vert = 255;
  }

  /* set 'tot' rect before setting cur? */
  /* XXX confusing stuff here still */
  if (tot_changed) {
    view2d_totRect_set_resize(v2d, winx, winy, !do_init);
  }
  else {
    ui_view2d_curRect_validate_resize(v2d, !do_init);
  }
}

/**
 * Ensure View2D rects remain in a viable configuration
 * 'cur' is not allowed to be: larger than max, smaller than min, or outside of 'tot'
 */
static void ui_view2d_curRect_validate_resize(View2D *v2d, bool resize)
{
  /* NOTE: #calculateZfac uses this logic, keep in sync. */
  float curwidth, curheight, width, height;
  float winx, winy;

  /* use mask as size of region that View2D resides in, as it takes into account
   * scroll-bars already - keep in sync with `zoomx/zoomy` in #view_zoomstep_apply_ex! */
  winx = float(BLI_rcti_size_x(&v2d->mask) + 1);
  winy = float(BLI_rcti_size_y(&v2d->mask) + 1);

  /* get pointers to rcts for less typing */
  rctf *cur = &v2d->cur;
  rctf *tot = &v2d->tot;

  /* we must satisfy the following constraints (in decreasing order of importance):
   * - alignment restrictions are respected
   * - cur must not fall outside of tot
   * - axis locks (zoom and offset) must be maintained
   * - zoom must not be excessive (check either sizes or zoom values)
   * - aspect ratio should be respected (NOTE: this is quite closely related to zoom too)
   */

  /* Step 1: if keepzoom, adjust the sizes of the rects only
   * - firstly, we calculate the sizes of the rects
   * - curwidth and curheight are saved as reference... modify width and height values here
   */
  /* Keep in sync with `zoomx/zoomy` in #view_zoomstep_apply_ex! */
  curwidth = width = BLI_rctf_size_x(cur);
  curheight = height = BLI_rctf_size_y(cur);

  /* if zoom is locked, size on the appropriate axis is reset to mask size */
  if (v2d->keepzoom & V2D_LOCKZOOM_X) {
    width = winx;
  }
  if (v2d->keepzoom & V2D_LOCKZOOM_Y) {
    height = winy;
  }

  /* values used to divide, so make it safe
   * NOTE: width and height must use FLT_MIN instead of 1, otherwise it is impossible to
   *       get enough resolution in Graph Editor for editing some curves
   */
  if (width < FLT_MIN) {
    width = 1;
  }
  if (height < FLT_MIN) {
    height = 1;
  }
  winx = std::max<float>(winx, 1);
  winy = std::max<float>(winy, 1);
  if (v2d->oldwinx == 0) {
    v2d->oldwinx = winx;
  }
  if (v2d->oldwiny == 0) {
    v2d->oldwiny = winy;
  }

  /* V2D_KEEPZOOM indicates that zoom level should be preserved when the window size changes. */
  if (resize && (v2d->keepzoom & V2D_KEEPZOOM)) {
    float zoom, oldzoom;

    if ((v2d->keepzoom & V2D_LOCKZOOM_X) == 0) {
      zoom = winx / width;
      oldzoom = v2d->oldwinx / curwidth;

      if (oldzoom != zoom) {
        width *= zoom / oldzoom;
      }
    }

    if ((v2d->keepzoom & V2D_LOCKZOOM_Y) == 0) {
      zoom = winy / height;
      oldzoom = v2d->oldwiny / curheight;

      if (oldzoom != zoom) {
        height *= zoom / oldzoom;
      }
    }
  }
  /* keepzoom (V2D_LIMITZOOM set), indicates that zoom level on each axis must not exceed limits
   * NOTE: in general, it is not expected that the lock-zoom will be used in conjunction with this
   */
  else if (v2d->keepzoom & V2D_LIMITZOOM) {

    /* check if excessive zoom on x-axis */
    if ((v2d->keepzoom & V2D_LOCKZOOM_X) == 0) {
      const float zoom = winx / width;
      if (zoom < v2d->minzoom) {
        width = winx / v2d->minzoom;
      }
      else if (zoom > v2d->maxzoom) {
        width = winx / v2d->maxzoom;
      }
    }

    /* check if excessive zoom on y-axis */
    if ((v2d->keepzoom & V2D_LOCKZOOM_Y) == 0) {
      const float zoom = winy / height;
      if (zoom < v2d->minzoom) {
        height = winy / v2d->minzoom;
      }
      else if (zoom > v2d->maxzoom) {
        height = winy / v2d->maxzoom;
      }
    }
  }
  else {
    /* make sure sizes don't exceed that of the min/max sizes
     * (even though we're not doing zoom clamping) */
    CLAMP(width, v2d->min[0], v2d->max[0]);
    CLAMP(height, v2d->min[1], v2d->max[1]);
  }

  /* check if we should restore aspect ratio (if view size changed) */
  if (v2d->keepzoom & V2D_KEEPASPECT) {
    bool do_x = false, do_y = false, do_cur;
    float curRatio, winRatio;

    /* when a window edge changes, the aspect ratio can't be used to
     * find which is the best new 'cur' rect. that's why it stores 'old'
     */
    if (winx != v2d->oldwinx) {
      do_x = true;
    }
    if (winy != v2d->oldwiny) {
      do_y = true;
    }

    curRatio = height / width;
    winRatio = winy / winx;

    /* Both sizes change (area/region maximized). */
    if (do_x == do_y) {
      if (do_x && do_y) {
        /* here is 1,1 case, so all others must be 0,0 */
        if (fabsf(winx - v2d->oldwinx) > fabsf(winy - v2d->oldwiny)) {
          do_y = false;
        }
        else {
          do_x = false;
        }
      }
      else if (winRatio > curRatio) {
        do_x = false;
      }
      else {
        do_x = true;
      }
    }
    do_cur = do_x;
    // do_win = do_y; /* UNUSED. */

    if (do_cur) {
      if ((v2d->keeptot == V2D_KEEPTOT_STRICT) && (winx != v2d->oldwinx)) {
        /* Special exception for Outliner (and later channel-lists):
         * - The view may be moved left to avoid contents
         *   being pushed out of view when view shrinks.
         * - The keeptot code will make sure cur->xmin will not be less than tot->xmin
         *   (which cannot be allowed).
         * - width is not adjusted for changed ratios here.
         */
        if (winx < v2d->oldwinx) {
          const float temp = v2d->oldwinx - winx;

          cur->xmin -= temp;
          cur->xmax -= temp;

          /* Width does not get modified, as keep-aspect here is just set to make
           * sure visible area adjusts to changing view shape! */
        }
      }
      else {
        /* portrait window: correct for x */
        width = height / winRatio;
      }
    }
    else {
      if ((v2d->keeptot == V2D_KEEPTOT_STRICT) && (winy != v2d->oldwiny)) {
        /* special exception for Outliner (and later channel-lists):
         * - Currently, no actions need to be taken here...
         */

        if (winy < v2d->oldwiny) {
          const float temp = v2d->oldwiny - winy;

          if (v2d->align & V2D_ALIGN_NO_NEG_Y) {
            cur->ymin -= temp;
            cur->ymax -= temp;
          }
          else { /* Assume V2D_ALIGN_NO_POS_Y or combination */
            cur->ymin += temp;
            cur->ymax += temp;
          }
        }
      }
      else {
        /* landscape window: correct for y */
        height = width * winRatio;
      }
    }
  }

  /* Store region size for next time. */
  v2d->oldwinx = short(winx);
  v2d->oldwiny = short(winy);

  /* Step 2: apply new sizes to cur rect,
   * but need to take into account alignment settings here... */
  const bool do_keepofs = resize || !(v2d->flag & V2D_ZOOM_IGNORE_KEEPOFS);
  if ((width != curwidth) || (height != curheight)) {
    float temp, dh;

    /* Resize from center-point, unless otherwise specified. */
    if (width != curwidth) {
      if (v2d->keepofs & V2D_LOCKOFS_X) {
        cur->xmax += width - BLI_rctf_size_x(cur);
      }
      else if ((v2d->keepofs & V2D_KEEPOFS_X) && do_keepofs) {
        if (v2d->align & V2D_ALIGN_NO_POS_X) {
          cur->xmin -= width - BLI_rctf_size_x(cur);
        }
        else {
          cur->xmax += width - BLI_rctf_size_x(cur);
        }
      }
      else {
        temp = BLI_rctf_cent_x(cur);
        dh = width * 0.5f;

        cur->xmin = temp - dh;
        cur->xmax = temp + dh;
      }
    }
    if (height != curheight) {
      if (v2d->keepofs & V2D_LOCKOFS_Y) {
        cur->ymax += height - BLI_rctf_size_y(cur);
      }
      else if ((v2d->keepofs & V2D_KEEPOFS_Y) && do_keepofs) {
        if (v2d->align & V2D_ALIGN_NO_POS_Y) {
          cur->ymin -= height - BLI_rctf_size_y(cur);
        }
        else {
          cur->ymax += height - BLI_rctf_size_y(cur);
        }
      }
      else {
        temp = BLI_rctf_cent_y(cur);
        dh = height * 0.5f;

        cur->ymin = temp - dh;
        cur->ymax = temp + dh;
      }
    }
  }

  const float totwidth = BLI_rctf_size_x(tot);
  const float totheight = BLI_rctf_size_y(tot);

  /* Step 3: adjust so that it doesn't fall outside of bounds of 'tot' */
  if (v2d->keeptot) {
    float temp, diff;

    /* recalculate extents of cur */
    curwidth = BLI_rctf_size_x(cur);
    curheight = BLI_rctf_size_y(cur);

    /* width */
    if ((curwidth > totwidth) &&
        !(v2d->keepzoom & (V2D_KEEPZOOM | V2D_LOCKZOOM_X | V2D_LIMITZOOM)))
    {
      /* if zoom doesn't have to be maintained, just clamp edges */
      cur->xmin = std::max(cur->xmin, tot->xmin);
      cur->xmax = std::min(cur->xmax, tot->xmax);
    }
    else if (v2d->keeptot == V2D_KEEPTOT_STRICT) {
      /* This is an exception for the outliner (and later channel-lists, headers)
       * - must clamp within tot rect (absolutely no excuses)
       * --> therefore, cur->xmin must not be less than tot->xmin
       */
      if (cur->xmin < tot->xmin) {
        /* move cur across so that it sits at minimum of tot */
        temp = tot->xmin - cur->xmin;

        cur->xmin += temp;
        cur->xmax += temp;
      }
      else if (cur->xmax > tot->xmax) {
        /* - only offset by difference of cur-xmax and tot-xmax if that would not move
         *   cur-xmin to lie past tot-xmin
         * - otherwise, simply shift to tot-xmin???
         */
        temp = cur->xmax - tot->xmax;

        if ((cur->xmin - temp) < tot->xmin) {
          /* only offset by difference from cur-min and tot-min */
          temp = cur->xmin - tot->xmin;

          cur->xmin -= temp;
          cur->xmax -= temp;
        }
        else {
          cur->xmin -= temp;
          cur->xmax -= temp;
        }
      }
    }
    else {
      /* This here occurs when:
       * - width too big, but maintaining zoom (i.e. widths cannot be changed)
       * - width is OK, but need to check if outside of boundaries
       *
       * So, resolution is to just shift view by the gap between the extremities.
       * We favor moving the 'minimum' across, as that's origin for most things.
       * (XXX: in the past, max was favored... if there are bugs, swap!)
       */
      if ((cur->xmin < tot->xmin) && (cur->xmax > tot->xmax)) {
        /* outside boundaries on both sides,
         * so take middle-point of tot, and place in balanced way */
        temp = BLI_rctf_cent_x(tot);
        diff = curwidth * 0.5f;

        cur->xmin = temp - diff;
        cur->xmax = temp + diff;
      }
      else if (cur->xmin < tot->xmin) {
        /* move cur across so that it sits at minimum of tot */
        temp = tot->xmin - cur->xmin;

        cur->xmin += temp;
        cur->xmax += temp;
      }
      else if (cur->xmax > tot->xmax) {
        /* - only offset by difference of cur-xmax and tot-xmax if that would not move
         *   cur-xmin to lie past tot-xmin
         * - otherwise, simply shift to tot-xmin???
         */
        temp = cur->xmax - tot->xmax;

        if ((cur->xmin - temp) < tot->xmin) {
          /* only offset by difference from cur-min and tot-min */
          temp = cur->xmin - tot->xmin;

          cur->xmin -= temp;
          cur->xmax -= temp;
        }
        else {
          cur->xmin -= temp;
          cur->xmax -= temp;
        }
      }
    }

    /* height */
    if ((curheight > totheight) &&
        !(v2d->keepzoom & (V2D_KEEPZOOM | V2D_LOCKZOOM_Y | V2D_LIMITZOOM)))
    {
      /* if zoom doesn't have to be maintained, just clamp edges */
      cur->ymin = std::max(cur->ymin, tot->ymin);
      cur->ymax = std::min(cur->ymax, tot->ymax);
    }
    else {
      /* This here occurs when:
       * - height too big, but maintaining zoom (i.e. heights cannot be changed)
       * - height is OK, but need to check if outside of boundaries
       *
       * So, resolution is to just shift view by the gap between the extremities.
       * We favor moving the 'minimum' across, as that's origin for most things.
       */
      if ((cur->ymin < tot->ymin) && (cur->ymax > tot->ymax)) {
        /* outside boundaries on both sides,
         * so take middle-point of tot, and place in balanced way */
        temp = BLI_rctf_cent_y(tot);
        diff = curheight * 0.5f;

        cur->ymin = temp - diff;
        cur->ymax = temp + diff;
      }
      else if (cur->ymin < tot->ymin) {
        /* there's still space remaining, so shift up */
        temp = tot->ymin - cur->ymin;

        cur->ymin += temp;
        cur->ymax += temp;
      }
      else if (cur->ymax > tot->ymax) {
        /* there's still space remaining, so shift down */
        temp = cur->ymax - tot->ymax;

        cur->ymin -= temp;
        cur->ymax -= temp;
      }
    }
  }

  /* Step 4: Make sure alignment restrictions are respected */
  if (v2d->align) {
    /* If alignment flags are set (but keeptot is not), they must still be respected, as although
     * they don't specify any particular bounds to stay within, they do define ranges which are
     * invalid.
     *
     * Here, we only check to make sure that on each axis, the 'cur' rect doesn't stray into these
     * invalid zones, otherwise we offset.
     */

    /* handle width - posx and negx flags are mutually exclusive, so watch out */
    if ((v2d->align & V2D_ALIGN_NO_POS_X) && !(v2d->align & V2D_ALIGN_NO_NEG_X)) {
      /* width is in negative-x half */
      if (v2d->cur.xmax > 0) {
        v2d->cur.xmin -= v2d->cur.xmax;
        v2d->cur.xmax = 0.0f;
      }
    }
    else if ((v2d->align & V2D_ALIGN_NO_NEG_X) && !(v2d->align & V2D_ALIGN_NO_POS_X)) {
      /* width is in positive-x half */
      if (v2d->cur.xmin < 0) {
        v2d->cur.xmax -= v2d->cur.xmin;
        v2d->cur.xmin = 0.0f;
      }
    }

    /* handle height - posx and negx flags are mutually exclusive, so watch out */
    if ((v2d->align & V2D_ALIGN_NO_POS_Y) && !(v2d->align & V2D_ALIGN_NO_NEG_Y)) {
      /* height is in negative-y half */
      if (v2d->cur.ymax > 0) {
        v2d->cur.ymin -= v2d->cur.ymax;
        v2d->cur.ymax = 0.0f;
      }
    }
    else if ((v2d->align & V2D_ALIGN_NO_NEG_Y) && !(v2d->align & V2D_ALIGN_NO_POS_Y)) {
      /* height is in positive-y half */
      if (v2d->cur.ymin < 0) {
        v2d->cur.ymax -= v2d->cur.ymin;
        v2d->cur.ymin = 0.0f;
      }
    }
  }

  /* set masks */
  view2d_masks(v2d, nullptr);
}

void UI_view2d_curRect_validate(View2D *v2d)
{
  ui_view2d_curRect_validate_resize(v2d, false);
}

void UI_view2d_curRect_changed(const bContext *C, View2D *v2d)
{
  UI_view2d_curRect_validate(v2d);

  ARegion *region = CTX_wm_region(C);

  if (region->runtime->type->on_view2d_changed != nullptr) {
    region->runtime->type->on_view2d_changed(C, region);
  }
}

void UI_view2d_curRect_clamp_y(View2D *v2d)
{
  const float cur_height_y = BLI_rctf_size_y(&v2d->cur);

  if (BLI_rctf_size_y(&v2d->cur) > BLI_rctf_size_y(&v2d->tot)) {
    v2d->cur.ymin = -cur_height_y;
    v2d->cur.ymax = 0;
  }
  else if (v2d->cur.ymin < v2d->tot.ymin) {
    v2d->cur.ymin = v2d->tot.ymin;
    v2d->cur.ymax = v2d->cur.ymin + cur_height_y;
  }
}

/* ------------------ */

bool UI_view2d_area_supports_sync(ScrArea *area)
{
  return ELEM(area->spacetype, SPACE_ACTION, SPACE_NLA, SPACE_SEQ, SPACE_CLIP, SPACE_GRAPH);
}

void UI_view2d_sync(bScreen *screen, ScrArea *area, View2D *v2dcur, int flag)
{
  /* don't continue if no view syncing to be done */
  if ((v2dcur->flag & (V2D_VIEWSYNC_SCREEN_TIME | V2D_VIEWSYNC_AREA_VERTICAL)) == 0) {
    return;
  }

  /* check if doing within area syncing (i.e. channels/vertical) */
  if ((v2dcur->flag & V2D_VIEWSYNC_AREA_VERTICAL) && (area)) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      /* don't operate on self */
      if (v2dcur != &region->v2d) {
        /* only if view has vertical locks enabled */
        if (region->v2d.flag & V2D_VIEWSYNC_AREA_VERTICAL) {
          if (flag == V2D_LOCK_COPY) {
            /* other views with locks on must copy active */
            region->v2d.cur.ymin = v2dcur->cur.ymin;
            region->v2d.cur.ymax = v2dcur->cur.ymax;
          }
          else { /* V2D_LOCK_SET */
                 /* active must copy others */
            v2dcur->cur.ymin = region->v2d.cur.ymin;
            v2dcur->cur.ymax = region->v2d.cur.ymax;
          }

          /* region possibly changed, so refresh */
          ED_region_tag_redraw_no_rebuild(region);
        }
      }
    }
  }

  /* check if doing whole screen syncing (i.e. time/horizontal) */
  if ((v2dcur->flag & V2D_VIEWSYNC_SCREEN_TIME) && (screen)) {
    LISTBASE_FOREACH (ScrArea *, area_iter, &screen->areabase) {
      if (!UI_view2d_area_supports_sync(area_iter)) {
        continue;
      }
      LISTBASE_FOREACH (ARegion *, region, &area_iter->regionbase) {
        /* don't operate on self */
        if (v2dcur != &region->v2d) {
          /* only if view has horizontal locks enabled */
          if (region->v2d.flag & V2D_VIEWSYNC_SCREEN_TIME) {
            if (flag == V2D_LOCK_COPY) {
              /* other views with locks on must copy active */
              region->v2d.cur.xmin = v2dcur->cur.xmin;
              region->v2d.cur.xmax = v2dcur->cur.xmax;
            }
            else { /* V2D_LOCK_SET */
                   /* active must copy others */
              v2dcur->cur.xmin = region->v2d.cur.xmin;
              v2dcur->cur.xmax = region->v2d.cur.xmax;
            }

            /* region possibly changed, so refresh */
            ED_region_tag_redraw_no_rebuild(region);
          }
        }
      }
    }
  }
}

void UI_view2d_curRect_reset(View2D *v2d)
{
  float width, height;

  /* assume width and height of 'cur' rect by default, should be same size as mask */
  width = float(BLI_rcti_size_x(&v2d->mask) + 1);
  height = float(BLI_rcti_size_y(&v2d->mask) + 1);

  /* handle width - posx and negx flags are mutually exclusive, so watch out */
  if ((v2d->align & V2D_ALIGN_NO_POS_X) && !(v2d->align & V2D_ALIGN_NO_NEG_X)) {
    /* width is in negative-x half */
    v2d->cur.xmin = -width;
    v2d->cur.xmax = 0.0f;
  }
  else if ((v2d->align & V2D_ALIGN_NO_NEG_X) && !(v2d->align & V2D_ALIGN_NO_POS_X)) {
    /* width is in positive-x half */
    v2d->cur.xmin = 0.0f;
    v2d->cur.xmax = width;
  }
  else {
    /* width is centered around (x == 0) */
    const float dx = width / 2.0f;

    v2d->cur.xmin = -dx;
    v2d->cur.xmax = dx;
  }

  /* handle height - posx and negx flags are mutually exclusive, so watch out */
  if ((v2d->align & V2D_ALIGN_NO_POS_Y) && !(v2d->align & V2D_ALIGN_NO_NEG_Y)) {
    /* height is in negative-y half */
    v2d->cur.ymin = -height;
    v2d->cur.ymax = 0.0f;
  }
  else if ((v2d->align & V2D_ALIGN_NO_NEG_Y) && !(v2d->align & V2D_ALIGN_NO_POS_Y)) {
    /* height is in positive-y half */
    v2d->cur.ymin = 0.0f;
    v2d->cur.ymax = height;
  }
  else {
    /* height is centered around (y == 0) */
    const float dy = height / 2.0f;

    v2d->cur.ymin = -dy;
    v2d->cur.ymax = dy;
  }
}

/* ------------------ */

void view2d_totRect_set_resize(View2D *v2d, int width, int height, bool resize)
{
  /* don't do anything if either value is 0 */
  width = abs(width);
  height = abs(height);

  if (ELEM(0, width, height)) {
    if (G.debug & G_DEBUG) {
      /* XXX: temp debug info. */
      printf("Error: View2D totRect set exiting: v2d=%p width=%d height=%d\n",
             (void *)v2d,
             width,
             height);
    }
    return;
  }

  /* handle width - posx and negx flags are mutually exclusive, so watch out */
  if ((v2d->align & V2D_ALIGN_NO_POS_X) && !(v2d->align & V2D_ALIGN_NO_NEG_X)) {
    /* width is in negative-x half */
    v2d->tot.xmin = float(-width);
    v2d->tot.xmax = 0.0f;
  }
  else if ((v2d->align & V2D_ALIGN_NO_NEG_X) && !(v2d->align & V2D_ALIGN_NO_POS_X)) {
    /* width is in positive-x half */
    v2d->tot.xmin = 0.0f;
    v2d->tot.xmax = float(width);
  }
  else {
    /* width is centered around (x == 0) */
    const float dx = float(width) / 2.0f;

    v2d->tot.xmin = -dx;
    v2d->tot.xmax = dx;
  }

  /* handle height - posx and negx flags are mutually exclusive, so watch out */
  if ((v2d->align & V2D_ALIGN_NO_POS_Y) && !(v2d->align & V2D_ALIGN_NO_NEG_Y)) {
    /* height is in negative-y half */
    v2d->tot.ymin = float(-height);
    v2d->tot.ymax = 0.0f;
  }
  else if ((v2d->align & V2D_ALIGN_NO_NEG_Y) && !(v2d->align & V2D_ALIGN_NO_POS_Y)) {
    /* height is in positive-y half */
    v2d->tot.ymin = 0.0f;
    v2d->tot.ymax = float(height);
  }
  else {
    /* height is centered around (y == 0) */
    const float dy = float(height) / 2.0f;

    v2d->tot.ymin = -dy;
    v2d->tot.ymax = dy;
  }

  /* make sure that 'cur' rect is in a valid state as a result of these changes */
  ui_view2d_curRect_validate_resize(v2d, resize);
}

void UI_view2d_totRect_set(View2D *v2d, int width, int height)
{
  view2d_totRect_set_resize(v2d, width, height, false);
}

void UI_view2d_zoom_cache_reset()
{
  /* TODO(sergey): This way we avoid threading conflict with sequencer rendering
   * text strip. But ideally we want to make glyph cache to be fully safe
   * for threading.
   */
  if (G.is_rendering) {
    return;
  }
  /* While scaling we can accumulate fonts at many sizes (~20 or so).
   * Not an issue with embedded font, but can use over 500Mb with i18n ones! See #38244. */

  /* NOTE: only some views draw text, we could check for this case to avoid cleaning cache. */
  BLF_cache_clear();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View2D Matrix Setup
 * \{ */

/* mapping function to ensure 'cur' draws extended over the area where sliders are */
static void view2d_map_cur_using_mask(const View2D *v2d, rctf *r_curmasked)
{
  *r_curmasked = v2d->cur;

  if (view2d_scroll_mapped(v2d->scroll)) {
    const float sizex = BLI_rcti_size_x(&v2d->mask);
    const float sizey = BLI_rcti_size_y(&v2d->mask);

    /* prevent tiny or narrow regions to get
     * invalid coordinates - mask can get negative even... */
    if (sizex > 0.0f && sizey > 0.0f) {
      const float dx = BLI_rctf_size_x(&v2d->cur) / (sizex + 1);
      const float dy = BLI_rctf_size_y(&v2d->cur) / (sizey + 1);

      if (v2d->mask.xmin != 0) {
        r_curmasked->xmin -= dx * float(v2d->mask.xmin);
      }
      if (v2d->mask.xmax + 1 != v2d->winx) {
        r_curmasked->xmax += dx * float(v2d->winx - v2d->mask.xmax - 1);
      }

      if (v2d->mask.ymin != 0) {
        r_curmasked->ymin -= dy * float(v2d->mask.ymin);
      }
      if (v2d->mask.ymax + 1 != v2d->winy) {
        r_curmasked->ymax += dy * float(v2d->winy - v2d->mask.ymax - 1);
      }
    }
  }
}

void UI_view2d_view_ortho(const View2D *v2d)
{
  rctf curmasked;
  const int sizex = BLI_rcti_size_x(&v2d->mask);
  const int sizey = BLI_rcti_size_y(&v2d->mask);
  const float eps = 0.001f;
  float xofs = 0.0f, yofs = 0.0f;

  /* Pixel offsets (-GLA_PIXEL_OFS) are needed to get 1:1
   * correspondence with pixels for smooth UI drawing,
   * but only applied where requested.
   */
  /* XXX(@brecht): instead of zero at least use a tiny offset, otherwise
   * pixel rounding is effectively random due to float inaccuracy */
  if (sizex > 0) {
    xofs = eps * BLI_rctf_size_x(&v2d->cur) / sizex;
  }
  if (sizey > 0) {
    yofs = eps * BLI_rctf_size_y(&v2d->cur) / sizey;
  }

  /* apply mask-based adjustments to cur rect (due to scrollers),
   * to eliminate scaling artifacts */
  view2d_map_cur_using_mask(v2d, &curmasked);

  BLI_rctf_translate(&curmasked, -xofs, -yofs);

  /* XXX ton: this flag set by outliner, for icons */
  if (v2d->flag & V2D_PIXELOFS_X) {
    curmasked.xmin = floorf(curmasked.xmin) - (eps + xofs);
    curmasked.xmax = floorf(curmasked.xmax) - (eps + xofs);
  }
  if (v2d->flag & V2D_PIXELOFS_Y) {
    curmasked.ymin = floorf(curmasked.ymin) - (eps + yofs);
    curmasked.ymax = floorf(curmasked.ymax) - (eps + yofs);
  }

  /* set matrix on all appropriate axes */
  wmOrtho2(curmasked.xmin, curmasked.xmax, curmasked.ymin, curmasked.ymax);
}

void UI_view2d_view_orthoSpecial(ARegion *region, View2D *v2d, const bool xaxis)
{
  rctf curmasked;
  float xofs, yofs;

  /* Pixel offsets (-GLA_PIXEL_OFS) are needed to get 1:1
   * correspondence with pixels for smooth UI drawing,
   * but only applied where requested.
   */
  /* XXX(ton): temp. */
  xofs = 0.0f;  // (v2d->flag & V2D_PIXELOFS_X) ? GLA_PIXEL_OFS : 0.0f;
  yofs = 0.0f;  // (v2d->flag & V2D_PIXELOFS_Y) ? GLA_PIXEL_OFS : 0.0f;

  /* apply mask-based adjustments to cur rect (due to scrollers),
   * to eliminate scaling artifacts */
  view2d_map_cur_using_mask(v2d, &curmasked);

  /* only set matrix with 'cur' coordinates on relevant axes */
  if (xaxis) {
    wmOrtho2(curmasked.xmin - xofs, curmasked.xmax - xofs, -yofs, region->winy - yofs);
  }
  else {
    wmOrtho2(-xofs, region->winx - xofs, curmasked.ymin - yofs, curmasked.ymax - yofs);
  }
}

void UI_view2d_view_restore(const bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  const int width = BLI_rcti_size_x(&region->winrct) + 1;
  const int height = BLI_rcti_size_y(&region->winrct) + 1;

  wmOrtho2(0.0f, float(width), 0.0f, float(height));
  GPU_matrix_identity_set();

  //  ED_region_pixelspace(CTX_wm_region(C));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grid-Line Drawing
 * \{ */

void UI_view2d_multi_grid_draw(
    const View2D *v2d, int colorid, float step, int level_size, int totlevels)
{
  /* Exit if there is nothing to draw */
  if (totlevels == 0) {
    return;
  }

  int offset = -10;
  float lstep = step;
  uchar grid_line_color[3];

  /* Make an estimate of at least how many vertices will be needed */
  uint vertex_count = 4;
  vertex_count += 2 * (int((v2d->cur.xmax - v2d->cur.xmin) / lstep) + 1);
  vertex_count += 2 * (int((v2d->cur.ymax - v2d->cur.ymin) / lstep) + 1);

  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(
      format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  uint color = GPU_vertformat_attr_add(format, "color", blender::gpu::VertAttrType::UNORM_8_8_8_8);

  GPU_line_width(1.0f);

  immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);
  immBeginAtMost(GPU_PRIM_LINES, vertex_count);

  for (int level = 0; level < totlevels; level++) {
    /* Blend the background color (colorid) with the grid color, to avoid either too low contrast
     * or high contrast grid lines. This only has an effect if colorid != TH_GRID. */
    UI_GetThemeColorBlendShade3ubv(colorid, TH_GRID, 0.25f, offset, grid_line_color);

    int i = int(v2d->cur.xmin / lstep);
    if (v2d->cur.xmin > 0.0f) {
      i++;
    }
    float start = i * lstep;

    for (; start < v2d->cur.xmax; start += lstep, i++) {
      if (i == 0 || (level < totlevels - 1 && i % level_size == 0)) {
        continue;
      }

      immAttrSkip(color);
      immVertex2f(pos, start, v2d->cur.ymin);
      immAttr4ub(color, UNPACK3(grid_line_color), 255);
      immVertex2f(pos, start, v2d->cur.ymax);
    }

    i = int(v2d->cur.ymin / lstep);
    if (v2d->cur.ymin > 0.0f) {
      i++;
    }
    start = i * lstep;

    for (; start < v2d->cur.ymax; start += lstep, i++) {
      if (i == 0 || (level < totlevels - 1 && i % level_size == 0)) {
        continue;
      }

      immAttrSkip(color);
      immVertex2f(pos, v2d->cur.xmin, start);
      immAttr4ub(color, UNPACK3(grid_line_color), 255);
      immVertex2f(pos, v2d->cur.xmax, start);
    }

    lstep *= level_size;
    offset -= 6;
  }

  /* X and Y axis */
  UI_GetThemeColorBlendShade3ubv(
      colorid, TH_GRID, 0.5f, -18 + ((totlevels - 1) * -6), grid_line_color);

  immAttrSkip(color);
  immVertex2f(pos, 0.0f, v2d->cur.ymin);
  immAttr4ub(color, UNPACK3(grid_line_color), 255);
  immVertex2f(pos, 0.0f, v2d->cur.ymax);

  immAttrSkip(color);
  immVertex2f(pos, v2d->cur.xmin, 0.0f);
  immAttr4ub(color, UNPACK3(grid_line_color), 255);
  immVertex2f(pos, v2d->cur.xmax, 0.0f);

  immEnd();
  immUnbindProgram();
}

static void grid_axis_start_and_count(
    const float step, const float min, const float max, float *r_start, int *r_count)
{
  *r_start = min;
  if (*r_start < 0.0f) {
    *r_start += -fmod(min, step);
  }
  else {
    *r_start += step - fabs(fmod(min, step));
  }

  if (*r_start > max) {
    *r_count = 0;
  }
  else {
    *r_count = (max - *r_start) / step + 1;
  }
}

void UI_view2d_dot_grid_draw(const View2D *v2d,
                             const int grid_color_id,
                             const float min_step,
                             const int grid_subdivisions)
{
  BLI_assert(grid_subdivisions >= 0 && grid_subdivisions < 4);
  if (grid_subdivisions == 0) {
    return;
  }

  const float zoom_x = float(BLI_rcti_size_x(&v2d->mask) + 1) / BLI_rctf_size_x(&v2d->cur);

  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(
      format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  GPU_program_point_size(true);
  immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);

  /* Scaling the dots fully with the zoom looks too busy, but a bit of size variation is nice. */
  const float min_point_size = 2.0f * U.pixelsize;
  const float point_size_factor = 1.5f;
  const float max_point_size = point_size_factor * min_point_size;

  /* Each consecutive grid level is five times larger than the previous. */
  const int subdivision_scale = 5;

  const float view_level = logf(min_step / zoom_x) / logf(subdivision_scale);
  const int largest_visible_level = int(view_level);

  for (int level_offset = 0; level_offset <= grid_subdivisions; level_offset++) {
    const int level = largest_visible_level - level_offset;

    if (level < 0) {
      break;
    }

    const float level_scale = powf(subdivision_scale, level);
    const float point_size_precise = min_point_size * level_scale * zoom_x;
    const float point_size_draw = ceilf(
        clamp_f(point_size_precise, min_point_size, max_point_size));

    /* Offset point by this amount to better align centers as size changes. */
    const float point_size_offset = (point_size_draw / 2.0f) - U.pixelsize;

    /* To compensate the for the clamped point_size we adjust the alpha to make the overall
     * brightness of the grid background more consistent. */
    const float alpha = pow2f(point_size_precise / point_size_draw);

    /* Make sure we don't draw points once the alpha gets too low. */
    const float alpha_cutoff = 0.01f;
    if (alpha < alpha_cutoff) {
      break;
    }
    const float alpha_clamped = clamp_f((1.0f + alpha_cutoff) * alpha - alpha_cutoff, 0.0f, 1.0f);

    /* If we have don't draw enough subdivision levels so they fade out naturally, we apply an
     * additional fade to the last level to avoid pop in. */
    const bool last_level = level_offset == grid_subdivisions;
    const float subdivision_fade = last_level ? (1.0f - fractf(view_level)) : 1.0f;

    float color[4];
    UI_GetThemeColor3fv(grid_color_id, color);
    color[3] = alpha_clamped * subdivision_fade;

    const float step = min_step * level_scale;
    int count_x;
    float start_x;

    /* Count points that fit in viewport. */
    grid_axis_start_and_count(step, v2d->cur.xmin, v2d->cur.xmax, &start_x, &count_x);
    int count_y;
    float start_y;
    grid_axis_start_and_count(step, v2d->cur.ymin, v2d->cur.ymax, &start_y, &count_y);
    if (count_x == 0 || count_y == 0) {
      continue;
    }

    immUniform1f("size", point_size_draw);
    immUniform4fv("color", color);
    immBegin(GPU_PRIM_POINTS, count_x * count_y);

    /* Theoretically drawing on top of lower grid levels could be avoided, but it would also
     * increase the complexity of this loop, which isn't worth the time at the moment. */
    for (int i_y = 0; i_y < count_y; i_y++) {
      const float y = start_y + step * i_y;
      for (int i_x = 0; i_x < count_x; i_x++) {
        const float x = start_x + step * i_x;
        immVertex2f(pos, x + point_size_offset, y + point_size_offset);
      }
    }

    immEnd();
  }

  immUnbindProgram();
  GPU_program_point_size(false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scrollers
 * \{ */

void view2d_scrollers_calc(View2D *v2d, const rcti *mask_custom, View2DScrollers *r_scrollers)
{
  rcti vert, hor;
  float fac1, fac2, totsize, scrollsize;
  const int scroll = view2d_scroll_mapped(v2d->scroll);

  /* Always update before drawing (for dynamically sized scrollers). */
  view2d_masks(v2d, mask_custom);

  vert = v2d->vert;
  hor = v2d->hor;

  /* Pad scroll-bar drawing away from region edges. */
  const int edge_pad = int(0.1f * U.widget_unit);
  if (scroll & V2D_SCROLL_BOTTOM) {
    hor.ymin += edge_pad;
  }
  else {
    hor.ymax -= edge_pad;
  }

  if (scroll & V2D_SCROLL_LEFT) {
    vert.xmin += edge_pad;
  }
  else {
    vert.xmax -= edge_pad;
  }

  CLAMP_MAX(vert.ymin, vert.ymax - V2D_SCROLL_HANDLE_SIZE_HOTSPOT);
  CLAMP_MAX(hor.xmin, hor.xmax - V2D_SCROLL_HANDLE_SIZE_HOTSPOT);

  /* store in scrollers, used for drawing */
  r_scrollers->vert = vert;
  r_scrollers->hor = hor;

  /* scroller 'buttons':
   * - These should always remain within the visible region of the scroll-bar
   * - They represent the region of 'tot' that is visible in 'cur'
   */

  /* horizontal scrollers */
  if (scroll & V2D_SCROLL_HORIZONTAL) {
    /* scroller 'button' extents */
    totsize = BLI_rctf_size_x(&v2d->tot);
    scrollsize = float(BLI_rcti_size_x(&hor));
    if (totsize == 0.0f) {
      totsize = 1.0f; /* avoid divide by zero */
    }

    fac1 = (v2d->cur.xmin - v2d->tot.xmin) / totsize;
    if (fac1 <= 0.0f) {
      r_scrollers->hor_min = hor.xmin;
    }
    else {
      r_scrollers->hor_min = int(hor.xmin + (fac1 * scrollsize));
    }

    fac2 = (v2d->cur.xmax - v2d->tot.xmin) / totsize;
    if (fac2 >= 1.0f) {
      r_scrollers->hor_max = hor.xmax;
    }
    else {
      r_scrollers->hor_max = int(hor.xmin + (fac2 * scrollsize));
    }

    /* prevent inverted sliders */
    r_scrollers->hor_min = std::min(r_scrollers->hor_min, r_scrollers->hor_max);
    /* prevent sliders from being too small to grab */
    if ((r_scrollers->hor_max - r_scrollers->hor_min) < V2D_SCROLL_THUMB_SIZE_MIN) {
      r_scrollers->hor_max = r_scrollers->hor_min + V2D_SCROLL_THUMB_SIZE_MIN;

      CLAMP(r_scrollers->hor_max, hor.xmin + V2D_SCROLL_THUMB_SIZE_MIN, hor.xmax);
      CLAMP(r_scrollers->hor_min, hor.xmin, hor.xmax - V2D_SCROLL_THUMB_SIZE_MIN);
    }
  }

  /* vertical scrollers */
  if (scroll & V2D_SCROLL_VERTICAL) {
    /* scroller 'button' extents */
    totsize = BLI_rctf_size_y(&v2d->tot);
    scrollsize = float(BLI_rcti_size_y(&vert));
    if (totsize == 0.0f) {
      totsize = 1.0f; /* avoid divide by zero */
    }

    fac1 = (v2d->cur.ymin - v2d->tot.ymin) / totsize;
    if (fac1 <= 0.0f) {
      r_scrollers->vert_min = vert.ymin;
    }
    else {
      r_scrollers->vert_min = int(vert.ymin + (fac1 * scrollsize));
    }

    fac2 = (v2d->cur.ymax - v2d->tot.ymin) / totsize;
    if (fac2 >= 1.0f) {
      r_scrollers->vert_max = vert.ymax;
    }
    else {
      r_scrollers->vert_max = int(vert.ymin + (fac2 * scrollsize));
    }

    /* prevent inverted sliders */
    r_scrollers->vert_min = std::min(r_scrollers->vert_min, r_scrollers->vert_max);
    /* prevent sliders from being too small to grab */
    if ((r_scrollers->vert_max - r_scrollers->vert_min) < V2D_SCROLL_THUMB_SIZE_MIN) {
      r_scrollers->vert_max = r_scrollers->vert_min + V2D_SCROLL_THUMB_SIZE_MIN;

      CLAMP(r_scrollers->vert_max, vert.ymin + V2D_SCROLL_THUMB_SIZE_MIN, vert.ymax);
      CLAMP(r_scrollers->vert_min, vert.ymin, vert.ymax - V2D_SCROLL_THUMB_SIZE_MIN);
    }
  }
}

void UI_view2d_scrollers_draw(View2D *v2d, const rcti *mask_custom)
{
  View2DScrollers scrollers;
  view2d_scrollers_calc(v2d, mask_custom, &scrollers);
  bTheme *btheme = UI_GetTheme();
  rcti vert, hor;
  const int scroll = view2d_scroll_mapped(v2d->scroll);
  const char emboss_alpha = btheme->tui.widget_emboss[3];
  const float alpha_min = V2D_SCROLL_MIN_ALPHA;

  uchar scrollers_back_color[4];

  /* Color for scroll-bar backs. */
  UI_GetThemeColor4ubv(TH_BACK, scrollers_back_color);

  /* make copies of rects for less typing */
  vert = scrollers.vert;
  hor = scrollers.hor;

  /* Horizontal scroll-bar. */
  if (scroll & V2D_SCROLL_HORIZONTAL) {
    uiWidgetColors wcol = btheme->tui.wcol_scroll;
    /* 0..255 -> min...1 */
    const float alpha_fac = ((v2d->alpha_hor / 255.0f) * (1.0f - alpha_min)) + alpha_min;
    rcti slider;
    int state;

    slider.xmin = scrollers.hor_min;
    slider.xmax = scrollers.hor_max;
    slider.ymin = hor.ymin;
    slider.ymax = hor.ymax;

    state = (v2d->scroll_ui & V2D_SCROLL_H_ACTIVE) ? UI_SCROLL_PRESSED : 0;

    /* In the case that scroll-bar track is invisible, range from 0 ->`final_alpha` instead to
     * avoid errors with users trying to click into the underlying view. */
    if (wcol.inner[3] == 0) {
      const float final_alpha = 0.25f;
      wcol.inner[3] = final_alpha * v2d->alpha_hor;
    }
    else {
      wcol.inner[3] *= alpha_fac;
    }
    wcol.item[3] *= alpha_fac;
    wcol.outline[3] = 0;
    btheme->tui.widget_emboss[3] = 0; /* will be reset later */

    /* show zoom handles if:
     * - zooming on x-axis is allowed (no scroll otherwise)
     * - slider bubble is large enough (no overdraw confusion)
     * - scale is shown on the scroller
     *   (workaround to make sure that button windows don't show these,
     *   and only the time-grids with their zoom-ability can do so).
     */
    if ((v2d->keepzoom & V2D_LOCKZOOM_X) == 0 && (v2d->scroll & V2D_SCROLL_HORIZONTAL_HANDLES) &&
        (BLI_rcti_size_x(&slider) > V2D_SCROLL_HANDLE_SIZE_HOTSPOT))
    {
      state |= UI_SCROLL_ARROWS;
    }

    UI_draw_widget_scroll(&wcol, &hor, &slider, state);
  }

  /* Vertical scroll-bar. */
  if (scroll & V2D_SCROLL_VERTICAL) {
    uiWidgetColors wcol = btheme->tui.wcol_scroll;
    rcti slider;
    /* 0..255 -> min...1 */
    const float alpha_fac = ((v2d->alpha_vert / 255.0f) * (1.0f - alpha_min)) + alpha_min;
    int state;

    slider.xmin = vert.xmin;
    slider.xmax = vert.xmax;
    slider.ymin = scrollers.vert_min;
    slider.ymax = scrollers.vert_max;

    state = (v2d->scroll_ui & V2D_SCROLL_V_ACTIVE) ? UI_SCROLL_PRESSED : 0;

    /* In the case that scroll-bar track is invisible, range from 0 ->`final_alpha` instead to
     * avoid errors with users trying to click into the underlying view. */
    if (wcol.inner[3] == 0) {
      const float final_alpha = 0.25f;
      wcol.inner[3] = final_alpha * v2d->alpha_vert;
    }
    else {
      wcol.inner[3] *= alpha_fac;
    }
    wcol.item[3] *= alpha_fac;
    wcol.outline[3] = 0;
    btheme->tui.widget_emboss[3] = 0; /* will be reset later */

    /* show zoom handles if:
     * - zooming on y-axis is allowed (no scroll otherwise)
     * - slider bubble is large enough (no overdraw confusion)
     * - scale is shown on the scroller
     *   (workaround to make sure that button windows don't show these,
     *   and only the time-grids with their zoomability can do so)
     */
    if ((v2d->keepzoom & V2D_LOCKZOOM_Y) == 0 && (v2d->scroll & V2D_SCROLL_VERTICAL_HANDLES) &&
        (BLI_rcti_size_y(&slider) > V2D_SCROLL_HANDLE_SIZE_HOTSPOT))
    {
      state |= UI_SCROLL_ARROWS;
    }

    UI_draw_widget_scroll(&wcol, &vert, &slider, state);
  }

  /* Was changed above, so reset. */
  btheme->tui.widget_emboss[3] = emboss_alpha;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name List View Utilities
 * \{ */

void UI_view2d_listview_view_to_cell(float columnwidth,
                                     float rowheight,
                                     float startx,
                                     float starty,
                                     float viewx,
                                     float viewy,
                                     int *r_column,
                                     int *r_row)
{
  if (r_column) {
    if (columnwidth > 0) {
      /* Columns go from left to right (x increases). */
      *r_column = floorf((viewx - startx) / columnwidth);
    }
    else {
      *r_column = 0;
    }
  }

  if (r_row) {
    if (rowheight > 0) {
      /* Rows got from top to bottom (y decreases). */
      *r_row = floorf((starty - viewy) / rowheight);
    }
    else {
      *r_row = 0;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Coordinate Conversions
 * \{ */

float UI_view2d_region_to_view_x(const View2D *v2d, float x)
{
  return (v2d->cur.xmin +
          (BLI_rctf_size_x(&v2d->cur) * (x - v2d->mask.xmin) / BLI_rcti_size_x(&v2d->mask)));
}
float UI_view2d_region_to_view_y(const View2D *v2d, float y)
{
  return (v2d->cur.ymin +
          (BLI_rctf_size_y(&v2d->cur) * (y - v2d->mask.ymin) / BLI_rcti_size_y(&v2d->mask)));
}

void UI_view2d_region_to_view(
    const View2D *v2d, float x, float y, float *r_view_x, float *r_view_y)
{
  *r_view_x = UI_view2d_region_to_view_x(v2d, x);
  *r_view_y = UI_view2d_region_to_view_y(v2d, y);
}

void UI_view2d_region_to_view_rctf(const View2D *v2d, const rctf *rect_src, rctf *rect_dst)
{
  const float cur_size[2] = {BLI_rctf_size_x(&v2d->cur), BLI_rctf_size_y(&v2d->cur)};
  const float mask_size[2] = {float(BLI_rcti_size_x(&v2d->mask)),
                              float(BLI_rcti_size_y(&v2d->mask))};

  rect_dst->xmin = (v2d->cur.xmin +
                    (cur_size[0] * (rect_src->xmin - v2d->mask.xmin) / mask_size[0]));
  rect_dst->xmax = (v2d->cur.xmin +
                    (cur_size[0] * (rect_src->xmax - v2d->mask.xmin) / mask_size[0]));
  rect_dst->ymin = (v2d->cur.ymin +
                    (cur_size[1] * (rect_src->ymin - v2d->mask.ymin) / mask_size[1]));
  rect_dst->ymax = (v2d->cur.ymin +
                    (cur_size[1] * (rect_src->ymax - v2d->mask.ymin) / mask_size[1]));
}

float UI_view2d_view_to_region_x(const View2D *v2d, float x)
{
  return (v2d->mask.xmin + (((x - v2d->cur.xmin) / BLI_rctf_size_x(&v2d->cur)) *
                            float(BLI_rcti_size_x(&v2d->mask) + 1)));
}
float UI_view2d_view_to_region_y(const View2D *v2d, float y)
{
  return (v2d->mask.ymin + (((y - v2d->cur.ymin) / BLI_rctf_size_y(&v2d->cur)) *
                            float(BLI_rcti_size_y(&v2d->mask) + 1)));
}

bool UI_view2d_view_to_region_clip(
    const View2D *v2d, float x, float y, int *r_region_x, int *r_region_y)
{
  /* express given coordinates as proportional values */
  x = (x - v2d->cur.xmin) / BLI_rctf_size_x(&v2d->cur);
  y = (y - v2d->cur.ymin) / BLI_rctf_size_y(&v2d->cur);

  /* check if values are within bounds */
  if ((x >= 0.0f) && (x <= 1.0f) && (y >= 0.0f) && (y <= 1.0f)) {
    *r_region_x = int(v2d->mask.xmin + (x * BLI_rcti_size_x(&v2d->mask)));
    *r_region_y = int(v2d->mask.ymin + (y * BLI_rcti_size_y(&v2d->mask)));

    return true;
  }

  /* set initial value in case coordinate lies outside of bounds */
  *r_region_x = *r_region_y = V2D_IS_CLIPPED;

  return false;
}

void UI_view2d_view_to_region(
    const View2D *v2d, float x, float y, int *r_region_x, int *r_region_y)
{
  /* Step 1: express given coordinates as proportional values. */
  x = (x - v2d->cur.xmin) / BLI_rctf_size_x(&v2d->cur);
  y = (y - v2d->cur.ymin) / BLI_rctf_size_y(&v2d->cur);

  /* Step 2: convert proportional distances to screen coordinates. */
  x = v2d->mask.xmin + (x * BLI_rcti_size_x(&v2d->mask));
  y = v2d->mask.ymin + (y * BLI_rcti_size_y(&v2d->mask));

  /* Although we don't clamp to lie within region bounds, we must avoid exceeding size of ints. */
  *r_region_x = clamp_float_to_int(x);
  *r_region_y = clamp_float_to_int(y);
}

void UI_view2d_view_to_region_fl(
    const View2D *v2d, float x, float y, float *r_region_x, float *r_region_y)
{
  /* express given coordinates as proportional values */
  x = (x - v2d->cur.xmin) / BLI_rctf_size_x(&v2d->cur);
  y = (y - v2d->cur.ymin) / BLI_rctf_size_y(&v2d->cur);

  /* convert proportional distances to screen coordinates */
  *r_region_x = v2d->mask.xmin + (x * BLI_rcti_size_x(&v2d->mask));
  *r_region_y = v2d->mask.ymin + (y * BLI_rcti_size_y(&v2d->mask));
}

bool UI_view2d_view_to_region_segment_clip(const View2D *v2d,
                                           const float xy_a[2],
                                           const float xy_b[2],
                                           int r_region_a[2],
                                           int r_region_b[2])
{
  rctf rect_unit;
  rect_unit.xmin = rect_unit.ymin = 0.0f;
  rect_unit.xmax = rect_unit.ymax = 1.0f;

  /* Express given coordinates as proportional values. */
  const float s_a[2] = {
      (xy_a[0] - v2d->cur.xmin) / BLI_rctf_size_x(&v2d->cur),
      (xy_a[1] - v2d->cur.ymin) / BLI_rctf_size_y(&v2d->cur),
  };
  const float s_b[2] = {
      (xy_b[0] - v2d->cur.xmin) / BLI_rctf_size_x(&v2d->cur),
      (xy_b[1] - v2d->cur.ymin) / BLI_rctf_size_y(&v2d->cur),
  };

  /* Set initial value in case coordinates lie outside bounds. */
  r_region_a[0] = r_region_b[0] = r_region_a[1] = r_region_b[1] = V2D_IS_CLIPPED;

  if (BLI_rctf_isect_segment(&rect_unit, s_a, s_b)) {
    r_region_a[0] = int(v2d->mask.xmin + (s_a[0] * BLI_rcti_size_x(&v2d->mask)));
    r_region_a[1] = int(v2d->mask.ymin + (s_a[1] * BLI_rcti_size_y(&v2d->mask)));
    r_region_b[0] = int(v2d->mask.xmin + (s_b[0] * BLI_rcti_size_x(&v2d->mask)));
    r_region_b[1] = int(v2d->mask.ymin + (s_b[1] * BLI_rcti_size_y(&v2d->mask)));

    return true;
  }

  return false;
}

void UI_view2d_view_to_region_rcti(const View2D *v2d, const rctf *rect_src, rcti *rect_dst)
{
  const float cur_size[2] = {BLI_rctf_size_x(&v2d->cur), BLI_rctf_size_y(&v2d->cur)};
  const float mask_size[2] = {float(BLI_rcti_size_x(&v2d->mask)),
                              float(BLI_rcti_size_y(&v2d->mask))};
  rctf rect_tmp;

  /* Step 1: express given coordinates as proportional values. */
  rect_tmp.xmin = (rect_src->xmin - v2d->cur.xmin) / cur_size[0];
  rect_tmp.xmax = (rect_src->xmax - v2d->cur.xmin) / cur_size[0];
  rect_tmp.ymin = (rect_src->ymin - v2d->cur.ymin) / cur_size[1];
  rect_tmp.ymax = (rect_src->ymax - v2d->cur.ymin) / cur_size[1];

  /* Step 2: convert proportional distances to screen coordinates. */
  rect_tmp.xmin = v2d->mask.xmin + (rect_tmp.xmin * mask_size[0]);
  rect_tmp.xmax = v2d->mask.xmin + (rect_tmp.xmax * mask_size[0]);
  rect_tmp.ymin = v2d->mask.ymin + (rect_tmp.ymin * mask_size[1]);
  rect_tmp.ymax = v2d->mask.ymin + (rect_tmp.ymax * mask_size[1]);

  clamp_rctf_to_rcti(rect_dst, &rect_tmp);
}

void UI_view2d_view_to_region_m4(const View2D *v2d, float matrix[4][4])
{
  rctf mask;
  unit_m4(matrix);
  BLI_rctf_rcti_copy(&mask, &v2d->mask);
  BLI_rctf_transform_calc_m4_pivot_min(&v2d->cur, &mask, matrix);
}

bool UI_view2d_view_to_region_rcti_clip(const View2D *v2d, const rctf *rect_src, rcti *rect_dst)
{
  const float cur_size[2] = {BLI_rctf_size_x(&v2d->cur), BLI_rctf_size_y(&v2d->cur)};
  const float mask_size[2] = {float(BLI_rcti_size_x(&v2d->mask) + 1),
                              float(BLI_rcti_size_y(&v2d->mask) + 1)};
  rctf rect_tmp;

  BLI_assert(rect_src->xmin <= rect_src->xmax && rect_src->ymin <= rect_src->ymax);

  /* Step 1: express given coordinates as proportional values. */
  rect_tmp.xmin = (rect_src->xmin - v2d->cur.xmin) / cur_size[0];
  rect_tmp.xmax = (rect_src->xmax - v2d->cur.xmin) / cur_size[0];
  rect_tmp.ymin = (rect_src->ymin - v2d->cur.ymin) / cur_size[1];
  rect_tmp.ymax = (rect_src->ymax - v2d->cur.ymin) / cur_size[1];

  if (((rect_tmp.xmax < 0.0f) || (rect_tmp.xmin > 1.0f) || (rect_tmp.ymax < 0.0f) ||
       (rect_tmp.ymin > 1.0f)) == 0)
  {
    /* Step 2: convert proportional distances to screen coordinates. */
    rect_tmp.xmin = v2d->mask.xmin + (rect_tmp.xmin * mask_size[0]);
    rect_tmp.xmax = v2d->mask.ymin + (rect_tmp.xmax * mask_size[0]);
    rect_tmp.ymin = v2d->mask.ymin + (rect_tmp.ymin * mask_size[1]);
    rect_tmp.ymax = v2d->mask.ymin + (rect_tmp.ymax * mask_size[1]);

    clamp_rctf_to_rcti(rect_dst, &rect_tmp);

    return true;
  }

  rect_dst->xmin = rect_dst->xmax = rect_dst->ymin = rect_dst->ymax = V2D_IS_CLIPPED;
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

View2D *UI_view2d_fromcontext(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  if (area == nullptr) {
    return nullptr;
  }
  if (region == nullptr) {
    return nullptr;
  }
  return &(region->v2d);
}

View2D *UI_view2d_fromcontext_rwin(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  if (area == nullptr) {
    return nullptr;
  }
  if (region == nullptr) {
    return nullptr;
  }
  if (region->regiontype != RGN_TYPE_WINDOW) {
    ARegion *region_win = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
    return region_win ? &(region_win->v2d) : nullptr;
  }
  return &(region->v2d);
}

void UI_view2d_scroller_size_get(const View2D *v2d, bool mapped, float *r_x, float *r_y)
{
  const int scroll = (mapped) ? view2d_scroll_mapped(v2d->scroll) : v2d->scroll;

  if (r_x) {
    if (scroll & V2D_SCROLL_VERTICAL) {
      *r_x = (scroll & V2D_SCROLL_VERTICAL_HANDLES) ? V2D_SCROLL_HANDLE_WIDTH : V2D_SCROLL_WIDTH;
      *r_x = ((*r_x - V2D_SCROLL_MIN_WIDTH) * (v2d->alpha_vert / 255.0f)) + V2D_SCROLL_MIN_WIDTH;
    }
    else {
      *r_x = 0;
    }
  }
  if (r_y) {
    if (scroll & V2D_SCROLL_HORIZONTAL) {
      *r_y = (scroll & V2D_SCROLL_HORIZONTAL_HANDLES) ? V2D_SCROLL_HANDLE_HEIGHT :
                                                        V2D_SCROLL_HEIGHT;
      *r_y = ((*r_y - V2D_SCROLL_MIN_WIDTH) * (v2d->alpha_hor / 255.0f)) + V2D_SCROLL_MIN_WIDTH;
    }
    else {
      *r_y = 0;
    }
  }
}

void UI_view2d_scale_get(const View2D *v2d, float *r_x, float *r_y)
{
  if (r_x) {
    *r_x = UI_view2d_scale_get_x(v2d);
  }
  if (r_y) {
    *r_y = UI_view2d_scale_get_y(v2d);
  }
}
float UI_view2d_scale_get_x(const View2D *v2d)
{
  return BLI_rcti_size_x(&v2d->mask) / BLI_rctf_size_x(&v2d->cur);
}
float UI_view2d_scale_get_y(const View2D *v2d)
{
  return BLI_rcti_size_y(&v2d->mask) / BLI_rctf_size_y(&v2d->cur);
}
void UI_view2d_scale_get_inverse(const View2D *v2d, float *r_x, float *r_y)
{
  if (r_x) {
    *r_x = BLI_rctf_size_x(&v2d->cur) / BLI_rcti_size_x(&v2d->mask);
  }
  if (r_y) {
    *r_y = BLI_rctf_size_y(&v2d->cur) / BLI_rcti_size_y(&v2d->mask);
  }
}

void UI_view2d_center_get(const View2D *v2d, float *r_x, float *r_y)
{
  /* get center */
  if (r_x) {
    *r_x = BLI_rctf_cent_x(&v2d->cur);
  }
  if (r_y) {
    *r_y = BLI_rctf_cent_y(&v2d->cur);
  }
}
void UI_view2d_center_set(View2D *v2d, float x, float y)
{
  BLI_rctf_recenter(&v2d->cur, x, y);

  /* make sure that 'cur' rect is in a valid state as a result of these changes */
  UI_view2d_curRect_validate(v2d);
}

void UI_view2d_offset(View2D *v2d, float xfac, float yfac)
{
  if (xfac != -1.0f) {
    const float xsize = BLI_rctf_size_x(&v2d->cur);
    const float xmin = v2d->tot.xmin;
    const float xmax = v2d->tot.xmax - xsize;

    v2d->cur.xmin = (xmin * (1.0f - xfac)) + (xmax * xfac);
    v2d->cur.xmax = v2d->cur.xmin + xsize;
  }

  if (yfac != -1.0f) {
    const float ysize = BLI_rctf_size_y(&v2d->cur);
    const float ymin = v2d->tot.ymin;
    const float ymax = v2d->tot.ymax - ysize;

    v2d->cur.ymin = (ymin * (1.0f - yfac)) + (ymax * yfac);
    v2d->cur.ymax = v2d->cur.ymin + ysize;
  }

  UI_view2d_curRect_validate(v2d);
}

void UI_view2d_offset_y_snap_to_closest_page(View2D *v2d)
{
  const float cur_size_y = BLI_rctf_size_y(&v2d->cur);
  const float page_size_y = view2d_page_size_y(*v2d);

  v2d->cur.ymax = roundf(v2d->cur.ymax / page_size_y) * page_size_y;
  v2d->cur.ymin = v2d->cur.ymax - cur_size_y;

  UI_view2d_curRect_validate(v2d);
}

char UI_view2d_mouse_in_scrollers_ex(const ARegion *region,
                                     const View2D *v2d,
                                     const int xy[2],
                                     int *r_scroll)
{
  const int scroll = view2d_scroll_mapped(v2d->scroll);
  *r_scroll = scroll;

  if (scroll) {
    /* Move to region-coordinates. */
    const int co[2] = {
        xy[0] - region->winrct.xmin,
        xy[1] - region->winrct.ymin,
    };
    if (scroll & V2D_SCROLL_HORIZONTAL) {
      if (IN_2D_HORIZ_SCROLL(v2d, co)) {
        return 'h';
      }
    }
    if (scroll & V2D_SCROLL_VERTICAL) {
      if (IN_2D_VERT_SCROLL(v2d, co)) {
        return 'v';
      }
    }
  }

  return 0;
}

char UI_view2d_rect_in_scrollers_ex(const ARegion *region,
                                    const View2D *v2d,
                                    const rcti *rect,
                                    int *r_scroll)
{
  const int scroll = view2d_scroll_mapped(v2d->scroll);
  *r_scroll = scroll;

  if (scroll) {
    /* Move to region-coordinates. */
    rcti rect_region = *rect;
    BLI_rcti_translate(&rect_region, -region->winrct.xmin, region->winrct.ymin);
    if (scroll & V2D_SCROLL_HORIZONTAL) {
      if (IN_2D_HORIZ_SCROLL_RECT(v2d, &rect_region)) {
        return 'h';
      }
    }
    if (scroll & V2D_SCROLL_VERTICAL) {
      if (IN_2D_VERT_SCROLL_RECT(v2d, &rect_region)) {
        return 'v';
      }
    }
  }

  return 0;
}

char UI_view2d_mouse_in_scrollers(const ARegion *region, const View2D *v2d, const int xy[2])
{
  int scroll_dummy = 0;
  return UI_view2d_mouse_in_scrollers_ex(region, v2d, xy, &scroll_dummy);
}

char UI_view2d_rect_in_scrollers(const ARegion *region, const View2D *v2d, const rcti *rect)
{
  int scroll_dummy = 0;
  return UI_view2d_rect_in_scrollers_ex(region, v2d, rect, &scroll_dummy);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View2D Text Drawing Cache
 * \{ */

struct View2DString {
  View2DString *next;
  union {
    uchar ub[4];
    int pack;
  } col;
  rcti rect;
  int mval[2];

  /* str is allocated past the end */
  char str[0];
};

/* assumes caches are used correctly, so for time being no local storage in v2d */
static MemArena *g_v2d_strings_arena = nullptr;
static View2DString *g_v2d_strings = nullptr;

void UI_view2d_text_cache_add(
    View2D *v2d, float x, float y, const char *str, size_t str_len, const uchar col[4])
{
  int mval[2];

  BLI_assert(str_len == strlen(str));

  if (UI_view2d_view_to_region_clip(v2d, x, y, &mval[0], &mval[1])) {
    const int alloc_len = str_len + 1;

    if (g_v2d_strings_arena == nullptr) {
      g_v2d_strings_arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 14), __func__);
    }

    View2DString *v2s = static_cast<View2DString *>(
        BLI_memarena_alloc(g_v2d_strings_arena, sizeof(View2DString) + alloc_len));

    BLI_LINKS_PREPEND(g_v2d_strings, v2s);

    v2s->col.pack = *((const int *)col);

    v2s->rect = rcti{};

    v2s->mval[0] = mval[0];
    v2s->mval[1] = mval[1];

    memcpy(v2s->str, str, alloc_len);
  }
}

void UI_view2d_text_cache_add_rectf(
    View2D *v2d, const rctf *rect_view, const char *str, size_t str_len, const uchar col[4])
{
  rcti rect;

  BLI_assert(str_len == strlen(str));

  if (UI_view2d_view_to_region_rcti_clip(v2d, rect_view, &rect)) {
    const int alloc_len = str_len + 1;

    if (g_v2d_strings_arena == nullptr) {
      g_v2d_strings_arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 14), __func__);
    }

    View2DString *v2s = static_cast<View2DString *>(
        BLI_memarena_alloc(g_v2d_strings_arena, sizeof(View2DString) + alloc_len));

    BLI_LINKS_PREPEND(g_v2d_strings, v2s);

    v2s->col.pack = *((const int *)col);

    v2s->rect = rect;

    v2s->mval[0] = v2s->rect.xmin;
    v2s->mval[1] = v2s->rect.ymin;

    memcpy(v2s->str, str, alloc_len);
  }
}

void UI_view2d_text_cache_draw(ARegion *region)
{
  View2DString *v2s;
  int col_pack_prev = 0;

  /* investigate using BLF_ascender() */
  const int font_id = BLF_default();

  BLF_set_default();
  const float default_height = g_v2d_strings ? BLF_height(font_id, "28", 3) : 0.0f;

  wmOrtho2_region_pixelspace(region);

  for (v2s = g_v2d_strings; v2s; v2s = v2s->next) {
    int xofs = 0, yofs;

    yofs = ceil(0.5f * (BLI_rcti_size_y(&v2s->rect) - default_height));
    yofs = std::max(yofs, 1);

    if (col_pack_prev != v2s->col.pack) {
      BLF_color4ubv(font_id, v2s->col.ub);
      col_pack_prev = v2s->col.pack;
    }

    /* Don't use clipping if `v2s->rect` is not set. */
    if (BLI_rcti_size_x(&v2s->rect) == 0 && BLI_rcti_size_y(&v2s->rect) == 0) {
      BLF_draw_default(float(v2s->mval[0] + xofs),
                       float(v2s->mval[1] + yofs),
                       0.0,
                       v2s->str,
                       BLF_DRAW_STR_DUMMY_MAX);
    }
    else {
      BLF_enable(font_id, BLF_CLIPPING);
      BLF_clipping(
          font_id, v2s->rect.xmin - 4, v2s->rect.ymin - 4, v2s->rect.xmax + 4, v2s->rect.ymax + 4);
      BLF_draw_default(
          v2s->rect.xmin + xofs, v2s->rect.ymin + yofs, 0.0f, v2s->str, BLF_DRAW_STR_DUMMY_MAX);
      BLF_disable(font_id, BLF_CLIPPING);
    }
  }
  g_v2d_strings = nullptr;

  if (g_v2d_strings_arena) {
    BLI_memarena_free(g_v2d_strings_arena);
    g_v2d_strings_arena = nullptr;
  }
}

/** \} */

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
 * \ingroup edinterface
 */

#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BLI_array.h"
#include "BLI_utildefines.h"
#include "BLI_link_utils.h"
#include "BLI_rect.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_timecode.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_global.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "WM_api.h"

#include "BLF_api.h"

#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "interface_intern.h"

static void ui_view2d_curRect_validate_resize(View2D *v2d, bool resize, bool mask_scrollers);

/* *********************************************************************** */

BLI_INLINE int clamp_float_to_int(const float f)
{
  const float min = INT_MIN;
  const float max = INT_MAX;

  if (UNLIKELY(f < min)) {
    return min;
  }
  else if (UNLIKELY(f > max)) {
    return (int)max;
  }
  else {
    return (int)f;
  }
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

/* XXX still unresolved: scrolls hide/unhide vs region mask handling */
/* XXX there's V2D_SCROLL_HORIZONTAL_HIDE and V2D_SCROLL_HORIZONTAL_FULLR ... */

/**
 * helper to allow scrollbars to dynamically hide
 * - returns a copy of the scrollbar settings with the flags to display
 *   horizontal/vertical scrollbars removed
 * - input scroll value is the v2d->scroll var
 * - hide flags are set per region at drawtime
 */
static int view2d_scroll_mapped(int scroll)
{
  if (scroll & V2D_SCROLL_HORIZONTAL_FULLR) {
    scroll &= ~(V2D_SCROLL_HORIZONTAL);
  }
  if (scroll & V2D_SCROLL_VERTICAL_FULLR) {
    scroll &= ~(V2D_SCROLL_VERTICAL);
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
 * \param mask_scroll: Optionally clamp scrollbars by this region.
 */
static void view2d_masks(View2D *v2d, bool check_scrollers, const rcti *mask_scroll)
{
  int scroll;

  /* mask - view frame */
  UI_view2d_mask_from_win(v2d, &v2d->mask);
  if (mask_scroll == NULL) {
    mask_scroll = &v2d->mask;
  }

  if (check_scrollers) {
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
  }

  scroll = view2d_scroll_mapped(v2d->scroll);

  /* scrollers are based off regionsize
   * - they can only be on one to two edges of the region they define
   * - if they overlap, they must not occupy the corners (which are reserved for other widgets)
   */
  if (scroll) {
    const int scroll_width = (v2d->scroll & V2D_SCROLL_VERTICAL_HANDLES) ?
                                 V2D_SCROLL_WIDTH_HANDLES :
                                 V2D_SCROLL_WIDTH;
    const int scroll_height = (v2d->scroll & V2D_SCROLL_HORIZONTAL_HANDLES) ?
                                  V2D_SCROLL_HEIGHT_HANDLES :
                                  V2D_SCROLL_HEIGHT;

    /* vertical scroller */
    if (scroll & V2D_SCROLL_LEFT) {
      /* on left-hand edge of region */
      v2d->vert = *mask_scroll;
      v2d->vert.xmax = scroll_width;
    }
    else if (scroll & V2D_SCROLL_RIGHT) {
      /* on right-hand edge of region */
      v2d->vert = *mask_scroll;
      v2d->vert.xmax++; /* one pixel extra... was leaving a minor gap... */
      v2d->vert.xmin = v2d->vert.xmax - scroll_width;
    }

    /* Currently, all regions that have vertical scale handles,
     * also have the scrubbing area at the top.
     * So the scrollbar has to move down a bit. */
    if (scroll & V2D_SCROLL_VERTICAL_HANDLES) {
      v2d->vert.ymax -= UI_SCRUBBING_MARGIN_Y;
    }

    /* horizontal scroller */
    if (scroll & (V2D_SCROLL_BOTTOM)) {
      /* on bottom edge of region */
      v2d->hor = *mask_scroll;
      v2d->hor.ymax = scroll_height;
    }
    else if (scroll & V2D_SCROLL_TOP) {
      /* on upper edge of region */
      v2d->hor = *mask_scroll;
      v2d->hor.ymin = v2d->hor.ymax - scroll_height;
    }

    /* adjust vertical scroller if there's a horizontal scroller, to leave corner free */
    if (scroll & V2D_SCROLL_VERTICAL) {
      if (scroll & (V2D_SCROLL_BOTTOM)) {
        /* on bottom edge of region */
        v2d->vert.ymin = v2d->hor.ymax;
      }
      else if (scroll & V2D_SCROLL_TOP) {
        /* on upper edge of region */
        v2d->vert.ymax = v2d->hor.ymin;
      }
    }
  }
}

/* Refresh and Validation */

/**
 * Initialize all relevant View2D data (including view rects if first time)
 * and/or refresh mask sizes after view resize.
 *
 * - For some of these presets, it is expected that the region will have defined some
 *   additional settings necessary for the customization of the 2D viewport to its requirements
 * - This function should only be called from region init() callbacks, where it is expected that
 *   this is called before #UI_view2d_size_update(),
 *   as this one checks that the rects are properly initialized.
 */
void UI_view2d_region_reinit(View2D *v2d, short type, int winx, int winy)
{
  bool tot_changed = false, do_init;
  uiStyle *style = UI_style_get();

  do_init = (v2d->flag & V2D_IS_INITIALISED) == 0;

  /* see eView2D_CommonViewTypes in UI_view2d.h for available view presets */
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
        v2d->tot.xmax = (float)(winx - 1);
        v2d->tot.ymax = (float)(winy - 1);

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

        v2d->min[0] = v2d->max[0] = (float)(winx - 1);
        v2d->min[1] = v2d->max[1] = (float)(winy - 1);
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

      /* note, scroll is being flipped in ED_region_panels() drawing */
      v2d->scroll |= (V2D_SCROLL_HORIZONTAL_HIDE | V2D_SCROLL_VERTICAL_HIDE);

      /* initialize without scroll bars (interferes with zoom level see: T47047) */
      if (do_init) {
        v2d->scroll |= (V2D_SCROLL_VERTICAL_FULLR | V2D_SCROLL_HORIZONTAL_FULLR);
      }

      if (do_init) {
        float panelzoom = (style) ? style->panelzoom : 1.0f;

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

  /* set initialized flag so that View2D doesn't get reinitialised next time again */
  v2d->flag |= V2D_IS_INITIALISED;

  /* store view size */
  v2d->winx = winx;
  v2d->winy = winy;

  /* set masks (always do), but leave scroller scheck to totrect_set */
  view2d_masks(v2d, 0, NULL);

  if (do_init) {
    /* Visible by default. */
    v2d->alpha_hor = v2d->alpha_vert = 255;
  }

  /* set 'tot' rect before setting cur? */
  /* XXX confusing stuff here still -
   * I made this function not check scroller hide - that happens in totrect_set */
  if (tot_changed) {
    UI_view2d_totRect_set_resize(v2d, winx, winy, !do_init);
  }
  else {
    ui_view2d_curRect_validate_resize(v2d, !do_init, 0);
  }
}

/**
 * Ensure View2D rects remain in a viable configuration
 * 'cur' is not allowed to be: larger than max, smaller than min, or outside of 'tot'
 */
// XXX pre2.5 -> this used to be called  test_view2d()
static void ui_view2d_curRect_validate_resize(View2D *v2d, bool resize, bool mask_scrollers)
{
  float totwidth, totheight, curwidth, curheight, width, height;
  float winx, winy;
  rctf *cur, *tot;

  /* use mask as size of region that View2D resides in, as it takes into account
   * scrollbars already - keep in sync with zoomx/zoomy in view_zoomstep_apply_ex! */
  winx = (float)(BLI_rcti_size_x(&v2d->mask) + 1);
  winy = (float)(BLI_rcti_size_y(&v2d->mask) + 1);

  /* get pointers to rcts for less typing */
  cur = &v2d->cur;
  tot = &v2d->tot;

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
  totwidth = BLI_rctf_size_x(tot);
  totheight = BLI_rctf_size_y(tot);
  /* keep in sync with zoomx/zoomy in view_zoomstep_apply_ex! */
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
  if (winx < 1) {
    winx = 1;
  }
  if (winy < 1) {
    winy = 1;
  }

  /* V2D_LIMITZOOM indicates that zoom level should be preserved when the window size changes */
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
    bool do_x = false, do_y = false, do_cur /* , do_win */ /* UNUSED */;
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

    /* both sizes change (area/region maximized)  */
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
    /* do_win = do_y; */ /* UNUSED */

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
          float temp = v2d->oldwinx - winx;

          cur->xmin -= temp;
          cur->xmax -= temp;

          /* width does not get modified, as keepaspect here is just set to make
           * sure visible area adjusts to changing view shape!
           */
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
          float temp = v2d->oldwiny - winy;

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

    /* store region size for next time */
    v2d->oldwinx = (short)winx;
    v2d->oldwiny = (short)winy;
  }

  /* Step 2: apply new sizes to cur rect,
   * but need to take into account alignment settings here... */
  if ((width != curwidth) || (height != curheight)) {
    float temp, dh;

    /* resize from centerpoint, unless otherwise specified */
    if (width != curwidth) {
      if (v2d->keepofs & V2D_LOCKOFS_X) {
        cur->xmax += width - BLI_rctf_size_x(cur);
      }
      else if (v2d->keepofs & V2D_KEEPOFS_X) {
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
      else if (v2d->keepofs & V2D_KEEPOFS_Y) {
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

  /* Step 3: adjust so that it doesn't fall outside of bounds of 'tot' */
  if (v2d->keeptot) {
    float temp, diff;

    /* recalculate extents of cur */
    curwidth = BLI_rctf_size_x(cur);
    curheight = BLI_rctf_size_y(cur);

    /* width */
    if ((curwidth > totwidth) &&
        !(v2d->keepzoom & (V2D_KEEPZOOM | V2D_LOCKZOOM_X | V2D_LIMITZOOM))) {
      /* if zoom doesn't have to be maintained, just clamp edges */
      if (cur->xmin < tot->xmin) {
        cur->xmin = tot->xmin;
      }
      if (cur->xmax > tot->xmax) {
        cur->xmax = tot->xmax;
      }
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
       * We favour moving the 'minimum' across, as that's origin for most things
       * (XXX - in the past, max was favored... if there are bugs, swap!)
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
        !(v2d->keepzoom & (V2D_KEEPZOOM | V2D_LOCKZOOM_Y | V2D_LIMITZOOM))) {
      /* if zoom doesn't have to be maintained, just clamp edges */
      if (cur->ymin < tot->ymin) {
        cur->ymin = tot->ymin;
      }
      if (cur->ymax > tot->ymax) {
        cur->ymax = tot->ymax;
      }
    }
    else {
      /* This here occurs when:
       * - height too big, but maintaining zoom (i.e. heights cannot be changed)
       * - height is OK, but need to check if outside of boundaries
       *
       * So, resolution is to just shift view by the gap between the extremities.
       * We favour moving the 'minimum' across, as that's origin for most things
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
  view2d_masks(v2d, mask_scrollers, NULL);
}

void UI_view2d_curRect_validate(View2D *v2d)
{
  ui_view2d_curRect_validate_resize(v2d, 0, 1);
}

/* ------------------ */

/* Called by menus to activate it, or by view2d operators
 * to make sure 'related' views stay in synchrony */
void UI_view2d_sync(bScreen *screen, ScrArea *area, View2D *v2dcur, int flag)
{
  ScrArea *sa;
  ARegion *ar;

  /* don't continue if no view syncing to be done */
  if ((v2dcur->flag & (V2D_VIEWSYNC_SCREEN_TIME | V2D_VIEWSYNC_AREA_VERTICAL)) == 0) {
    return;
  }

  /* check if doing within area syncing (i.e. channels/vertical) */
  if ((v2dcur->flag & V2D_VIEWSYNC_AREA_VERTICAL) && (area)) {
    for (ar = area->regionbase.first; ar; ar = ar->next) {
      /* don't operate on self */
      if (v2dcur != &ar->v2d) {
        /* only if view has vertical locks enabled */
        if (ar->v2d.flag & V2D_VIEWSYNC_AREA_VERTICAL) {
          if (flag == V2D_LOCK_COPY) {
            /* other views with locks on must copy active */
            ar->v2d.cur.ymin = v2dcur->cur.ymin;
            ar->v2d.cur.ymax = v2dcur->cur.ymax;
          }
          else { /* V2D_LOCK_SET */
                 /* active must copy others */
            v2dcur->cur.ymin = ar->v2d.cur.ymin;
            v2dcur->cur.ymax = ar->v2d.cur.ymax;
          }

          /* region possibly changed, so refresh */
          ED_region_tag_redraw_no_rebuild(ar);
        }
      }
    }
  }

  /* check if doing whole screen syncing (i.e. time/horizontal) */
  if ((v2dcur->flag & V2D_VIEWSYNC_SCREEN_TIME) && (screen)) {
    for (sa = screen->areabase.first; sa; sa = sa->next) {
      for (ar = sa->regionbase.first; ar; ar = ar->next) {
        /* don't operate on self */
        if (v2dcur != &ar->v2d) {
          /* only if view has horizontal locks enabled */
          if (ar->v2d.flag & V2D_VIEWSYNC_SCREEN_TIME) {
            if (flag == V2D_LOCK_COPY) {
              /* other views with locks on must copy active */
              ar->v2d.cur.xmin = v2dcur->cur.xmin;
              ar->v2d.cur.xmax = v2dcur->cur.xmax;
            }
            else { /* V2D_LOCK_SET */
                   /* active must copy others */
              v2dcur->cur.xmin = ar->v2d.cur.xmin;
              v2dcur->cur.xmax = ar->v2d.cur.xmax;
            }

            /* region possibly changed, so refresh */
            ED_region_tag_redraw_no_rebuild(ar);
          }
        }
      }
    }
  }
}

/**
 * Restore 'cur' rect to standard orientation (i.e. optimal maximum view of tot).
 * This does not take into account if zooming the view on an axis
 * will improve the view (if allowed).
 */
void UI_view2d_curRect_reset(View2D *v2d)
{
  float width, height;

  /* assume width and height of 'cur' rect by default, should be same size as mask */
  width = (float)(BLI_rcti_size_x(&v2d->mask) + 1);
  height = (float)(BLI_rcti_size_y(&v2d->mask) + 1);

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

/* Change the size of the maximum viewable area (i.e. 'tot' rect) */
void UI_view2d_totRect_set_resize(View2D *v2d, int width, int height, bool resize)
{
  //  int scroll = view2d_scroll_mapped(v2d->scroll);

  /* don't do anything if either value is 0 */
  width = abs(width);
  height = abs(height);

  /* hrumf! */
  /* XXX: there are work arounds for this in the panel and file browse code. */
  /* round to int, because this is called with width + V2D_SCROLL_WIDTH */
  //  if (scroll & V2D_SCROLL_HORIZONTAL) {
  //      width -= (int)V2D_SCROLL_WIDTH;
  //  }
  //  if (scroll & V2D_SCROLL_VERTICAL) {
  //      height -= (int)V2D_SCROLL_HEIGHT;
  //  }

  if (ELEM(0, width, height)) {
    if (G.debug & G_DEBUG) {
      printf("Error: View2D totRect set exiting: v2d=%p width=%d height=%d\n",
             (void *)v2d,
             width,
             height);  // XXX temp debug info
    }
    return;
  }

  /* handle width - posx and negx flags are mutually exclusive, so watch out */
  if ((v2d->align & V2D_ALIGN_NO_POS_X) && !(v2d->align & V2D_ALIGN_NO_NEG_X)) {
    /* width is in negative-x half */
    v2d->tot.xmin = (float)-width;
    v2d->tot.xmax = 0.0f;
  }
  else if ((v2d->align & V2D_ALIGN_NO_NEG_X) && !(v2d->align & V2D_ALIGN_NO_POS_X)) {
    /* width is in positive-x half */
    v2d->tot.xmin = 0.0f;
    v2d->tot.xmax = (float)width;
  }
  else {
    /* width is centered around (x == 0) */
    const float dx = (float)width / 2.0f;

    v2d->tot.xmin = -dx;
    v2d->tot.xmax = dx;
  }

  /* handle height - posx and negx flags are mutually exclusive, so watch out */
  if ((v2d->align & V2D_ALIGN_NO_POS_Y) && !(v2d->align & V2D_ALIGN_NO_NEG_Y)) {
    /* height is in negative-y half */
    v2d->tot.ymin = (float)-height;
    v2d->tot.ymax = 0.0f;
  }
  else if ((v2d->align & V2D_ALIGN_NO_NEG_Y) && !(v2d->align & V2D_ALIGN_NO_POS_Y)) {
    /* height is in positive-y half */
    v2d->tot.ymin = 0.0f;
    v2d->tot.ymax = (float)height;
  }
  else {
    /* height is centered around (y == 0) */
    const float dy = (float)height / 2.0f;

    v2d->tot.ymin = -dy;
    v2d->tot.ymax = dy;
  }

  /* make sure that 'cur' rect is in a valid state as a result of these changes */
  ui_view2d_curRect_validate_resize(v2d, resize, 1);
}

void UI_view2d_totRect_set(View2D *v2d, int width, int height)
{
  int scroll = view2d_scroll_mapped(v2d->scroll);

  UI_view2d_totRect_set_resize(v2d, width, height, 0);

  /* solve bad recursion... if scroller state changed,
   * mask is different, so you get different rects */
  if (scroll != view2d_scroll_mapped(v2d->scroll)) {
    UI_view2d_totRect_set_resize(v2d, width, height, 0);
  }
}

bool UI_view2d_tab_set(View2D *v2d, int tab)
{
  float default_offset[2] = {0.0f, 0.0f};
  float *offset, *new_offset;
  bool changed = false;

  /* if tab changed, change offset */
  if (tab != v2d->tab_cur && v2d->tab_offset) {
    if (tab < v2d->tab_num) {
      offset = &v2d->tab_offset[tab * 2];
    }
    else {
      offset = default_offset;
    }

    v2d->cur.xmax += offset[0] - v2d->cur.xmin;
    v2d->cur.xmin = offset[0];

    v2d->cur.ymin += offset[1] - v2d->cur.ymax;
    v2d->cur.ymax = offset[1];

    /* validation should happen in subsequent totRect_set */

    changed = true;
  }

  /* resize array if needed */
  if (tab >= v2d->tab_num) {
    new_offset = MEM_callocN(sizeof(float) * (tab + 1) * 2, "view2d tab offset");

    if (v2d->tab_offset) {
      memcpy(new_offset, v2d->tab_offset, sizeof(float) * v2d->tab_num * 2);
      MEM_freeN(v2d->tab_offset);
    }

    v2d->tab_offset = new_offset;
    v2d->tab_num = tab + 1;
  }

  /* set current tab and offset */
  v2d->tab_cur = tab;
  v2d->tab_offset[2 * tab + 0] = v2d->cur.xmin;
  v2d->tab_offset[2 * tab + 1] = v2d->cur.ymax;

  return changed;
}

void UI_view2d_zoom_cache_reset(void)
{
  /* TODO(sergey): This way we avoid threading conflict with sequencer rendering
   * text strip. But ideally we want to make glyph cache to be fully safe
   * for threading.
   */
  if (G.is_rendering) {
    return;
  }
  /* While scaling we can accumulate fonts at many sizes (~20 or so).
   * Not an issue with embedded font, but can use over 500Mb with i18n ones! See [#38244]. */

  /* note: only some views draw text, we could check for this case to avoid clearning cache */
  BLF_cache_clear();
}

/* *********************************************************************** */
/* View Matrix Setup */

/* mapping function to ensure 'cur' draws extended over the area where sliders are */
static void view2d_map_cur_using_mask(const View2D *v2d, rctf *r_curmasked)
{
  *r_curmasked = v2d->cur;

  if (view2d_scroll_mapped(v2d->scroll)) {
    float sizex = BLI_rcti_size_x(&v2d->mask);
    float sizey = BLI_rcti_size_y(&v2d->mask);

    /* prevent tiny or narrow regions to get
     * invalid coordinates - mask can get negative even... */
    if (sizex > 0.0f && sizey > 0.0f) {
      float dx = BLI_rctf_size_x(&v2d->cur) / (sizex + 1);
      float dy = BLI_rctf_size_y(&v2d->cur) / (sizey + 1);

      if (v2d->mask.xmin != 0) {
        r_curmasked->xmin -= dx * (float)v2d->mask.xmin;
      }
      if (v2d->mask.xmax + 1 != v2d->winx) {
        r_curmasked->xmax += dx * (float)(v2d->winx - v2d->mask.xmax - 1);
      }

      if (v2d->mask.ymin != 0) {
        r_curmasked->ymin -= dy * (float)v2d->mask.ymin;
      }
      if (v2d->mask.ymax + 1 != v2d->winy) {
        r_curmasked->ymax += dy * (float)(v2d->winy - v2d->mask.ymax - 1);
      }
    }
  }
}

/* Set view matrices to use 'cur' rect as viewing frame for View2D drawing */
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
  /* XXX brecht: instead of zero at least use a tiny offset, otherwise
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

/**
 * Set view matrices to only use one axis of 'cur' only
 *
 * \param xaxis: if non-zero, only use cur x-axis,
 * otherwise use cur-yaxis (mostly this will be used for x).
 */
void UI_view2d_view_orthoSpecial(ARegion *ar, View2D *v2d, const bool xaxis)
{
  rctf curmasked;
  float xofs, yofs;

  /* Pixel offsets (-GLA_PIXEL_OFS) are needed to get 1:1
   * correspondence with pixels for smooth UI drawing,
   * but only applied where requested.
   */
  /* XXX temp (ton) */
  xofs = 0.0f;  // (v2d->flag & V2D_PIXELOFS_X) ? GLA_PIXEL_OFS : 0.0f;
  yofs = 0.0f;  // (v2d->flag & V2D_PIXELOFS_Y) ? GLA_PIXEL_OFS : 0.0f;

  /* apply mask-based adjustments to cur rect (due to scrollers),
   * to eliminate scaling artifacts */
  view2d_map_cur_using_mask(v2d, &curmasked);

  /* only set matrix with 'cur' coordinates on relevant axes */
  if (xaxis) {
    wmOrtho2(curmasked.xmin - xofs, curmasked.xmax - xofs, -yofs, ar->winy - yofs);
  }
  else {
    wmOrtho2(-xofs, ar->winx - xofs, curmasked.ymin - yofs, curmasked.ymax - yofs);
  }
}

/* Restore view matrices after drawing */
void UI_view2d_view_restore(const bContext *C)
{
  ARegion *ar = CTX_wm_region(C);
  int width = BLI_rcti_size_x(&ar->winrct) + 1;
  int height = BLI_rcti_size_y(&ar->winrct) + 1;

  wmOrtho2(0.0f, (float)width, 0.0f, (float)height);
  GPU_matrix_identity_set();

  //  ED_region_pixelspace(CTX_wm_region(C));
}

/* *********************************************************************** */
/* Gridlines */

/* Draw a constant grid in given 2d-region */
void UI_view2d_constant_grid_draw(View2D *v2d, float step)
{
  float start_x, start_y;
  int count_x, count_y;

  start_x = v2d->cur.xmin;
  if (start_x < 0.0) {
    start_x += -(float)fmod(v2d->cur.xmin, step);
  }
  else {
    start_x += (step - (float)fmod(v2d->cur.xmin, step));
  }

  if (start_x > v2d->cur.xmax) {
    count_x = 0;
  }
  else {
    count_x = (v2d->cur.xmax - start_x) / step + 1;
  }

  start_y = v2d->cur.ymin;
  if (start_y < 0.0) {
    start_y += -(float)fmod(v2d->cur.ymin, step);
  }
  else {
    start_y += (step - (float)fabs(fmod(v2d->cur.ymin, step)));
  }

  if (start_y > v2d->cur.ymax) {
    count_y = 0;
  }
  else {
    count_y = (v2d->cur.ymax - start_y) / step + 1;
  }

  if (count_x > 0 || count_y > 0) {
    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    float theme_color[3];

    UI_GetThemeColorShade3fv(TH_BACK, -10, theme_color);

    immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);
    immBegin(GPU_PRIM_LINES, count_x * 2 + count_y * 2 + 4);

    immAttr3fv(color, theme_color);
    for (int i = 0; i < count_x; start_x += step, i++) {
      immVertex2f(pos, start_x, v2d->cur.ymin);
      immVertex2f(pos, start_x, v2d->cur.ymax);
    }

    for (int i = 0; i < count_y; start_y += step, i++) {
      immVertex2f(pos, v2d->cur.xmin, start_y);
      immVertex2f(pos, v2d->cur.xmax, start_y);
    }

    /* X and Y axis */
    UI_GetThemeColorShade3fv(TH_BACK, -18, theme_color);

    immAttr3fv(color, theme_color);
    immVertex2f(pos, 0.0f, v2d->cur.ymin);
    immVertex2f(pos, 0.0f, v2d->cur.ymax);
    immVertex2f(pos, v2d->cur.xmin, 0.0f);
    immVertex2f(pos, v2d->cur.xmax, 0.0f);

    immEnd();
    immUnbindProgram();
  }
}

/* Draw a multi-level grid in given 2d-region */
void UI_view2d_multi_grid_draw(View2D *v2d, int colorid, float step, int level_size, int totlevels)
{
  /* Exit if there is nothing to draw */
  if (totlevels == 0) {
    return;
  }

  int offset = -10;
  float lstep = step;
  uchar grid_line_color[3];

  /* Make an estimate of at least how many vertices will be needed */
  unsigned vertex_count = 4;
  vertex_count += 2 * ((int)((v2d->cur.xmax - v2d->cur.xmin) / lstep) + 1);
  vertex_count += 2 * ((int)((v2d->cur.ymax - v2d->cur.ymin) / lstep) + 1);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint color = GPU_vertformat_attr_add(
      format, "color", GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);

  GPU_line_width(1.0f);

  immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);
  immBeginAtMost(GPU_PRIM_LINES, vertex_count);

  for (int level = 0; level < totlevels; ++level) {
    UI_GetThemeColorShade3ubv(colorid, offset, grid_line_color);

    int i = (int)(v2d->cur.xmin / lstep);
    if (v2d->cur.xmin > 0.0f) {
      i++;
    }
    float start = i * lstep;

    for (; start < v2d->cur.xmax; start += lstep, ++i) {
      if (i == 0 || (level < totlevels - 1 && i % level_size == 0)) {
        continue;
      }

      immAttrSkip(color);
      immVertex2f(pos, start, v2d->cur.ymin);
      immAttr3ubv(color, grid_line_color);
      immVertex2f(pos, start, v2d->cur.ymax);
    }

    i = (int)(v2d->cur.ymin / lstep);
    if (v2d->cur.ymin > 0.0f) {
      i++;
    }
    start = i * lstep;

    for (; start < v2d->cur.ymax; start += lstep, ++i) {
      if (i == 0 || (level < totlevels - 1 && i % level_size == 0)) {
        continue;
      }

      immAttrSkip(color);
      immVertex2f(pos, v2d->cur.xmin, start);
      immAttr3ubv(color, grid_line_color);
      immVertex2f(pos, v2d->cur.xmax, start);
    }

    lstep *= level_size;
    offset -= 6;
  }

  /* X and Y axis */
  UI_GetThemeColorShade3ubv(colorid, -18 + ((totlevels - 1) * -6), grid_line_color);

  immAttrSkip(color);
  immVertex2f(pos, 0.0f, v2d->cur.ymin);
  immAttr3ubv(color, grid_line_color);
  immVertex2f(pos, 0.0f, v2d->cur.ymax);

  immAttrSkip(color);
  immVertex2f(pos, v2d->cur.xmin, 0.0f);
  immAttr3ubv(color, grid_line_color);
  immVertex2f(pos, v2d->cur.xmax, 0.0f);

  immEnd();
  immUnbindProgram();
}

/* *********************************************************************** */
/* Scrollers */

/**
 * View2DScrollers is typedef'd in UI_view2d.h
 *
 * \warning The start of this struct must not change, as view2d_ops.c uses this too.
 * For now, we don't need to have a separate (internal) header for structs like this...
 */
struct View2DScrollers {
  /* focus bubbles */
  int vert_min, vert_max; /* vertical scrollbar */
  int hor_min, hor_max;   /* horizontal scrollbar */

  rcti hor, vert;        /* exact size of slider backdrop */
  int horfull, vertfull; /* set if sliders are full, we don't draw them */
};

/* Calculate relevant scroller properties */
View2DScrollers *UI_view2d_scrollers_calc(View2D *v2d, const rcti *mask_custom)
{
  View2DScrollers *scrollers;
  rcti vert, hor;
  float fac1, fac2, totsize, scrollsize;
  int scroll = view2d_scroll_mapped(v2d->scroll);
  int smaller;

  /* scrollers is allocated here... */
  scrollers = MEM_callocN(sizeof(View2DScrollers), "View2DScrollers");

  /* Always update before drawing (for dynamically sized scrollers). */
  view2d_masks(v2d, false, mask_custom);

  vert = v2d->vert;
  hor = v2d->hor;

  /* slider rects need to be smaller than region and not interfere with splitter areas */
  hor.xmin += UI_HEADER_OFFSET;
  hor.xmax -= UI_HEADER_OFFSET;
  vert.ymin += UI_HEADER_OFFSET;
  vert.ymax -= UI_HEADER_OFFSET;

  /* width of sliders */
  smaller = (int)(0.1f * U.widget_unit);
  if (scroll & V2D_SCROLL_BOTTOM) {
    hor.ymin += smaller;
  }
  else {
    hor.ymax -= smaller;
  }

  if (scroll & V2D_SCROLL_LEFT) {
    vert.xmin += smaller;
  }
  else {
    vert.xmax -= smaller;
  }

  CLAMP(vert.ymin, vert.ymin, vert.ymax - V2D_SCROLLER_HANDLE_SIZE);
  CLAMP(hor.xmin, hor.xmin, hor.xmax - V2D_SCROLLER_HANDLE_SIZE);

  /* store in scrollers, used for drawing */
  scrollers->vert = vert;
  scrollers->hor = hor;

  /* scroller 'buttons':
   * - These should always remain within the visible region of the scrollbar
   * - They represent the region of 'tot' that is visible in 'cur'
   */

  /* horizontal scrollers */
  if (scroll & V2D_SCROLL_HORIZONTAL) {
    /* scroller 'button' extents */
    totsize = BLI_rctf_size_x(&v2d->tot);
    scrollsize = (float)BLI_rcti_size_x(&hor);
    if (totsize == 0.0f) {
      totsize = 1.0f; /* avoid divide by zero */
    }

    fac1 = (v2d->cur.xmin - v2d->tot.xmin) / totsize;
    if (fac1 <= 0.0f) {
      scrollers->hor_min = hor.xmin;
    }
    else {
      scrollers->hor_min = (int)(hor.xmin + (fac1 * scrollsize));
    }

    fac2 = (v2d->cur.xmax - v2d->tot.xmin) / totsize;
    if (fac2 >= 1.0f) {
      scrollers->hor_max = hor.xmax;
    }
    else {
      scrollers->hor_max = (int)(hor.xmin + (fac2 * scrollsize));
    }

    /* prevent inverted sliders */
    if (scrollers->hor_min > scrollers->hor_max) {
      scrollers->hor_min = scrollers->hor_max;
    }
    /* prevent sliders from being too small, and disappearing */
    if ((scrollers->hor_max - scrollers->hor_min) < V2D_SCROLLER_HANDLE_SIZE) {
      scrollers->hor_max = scrollers->hor_min + V2D_SCROLLER_HANDLE_SIZE;

      CLAMP(scrollers->hor_max, hor.xmin + V2D_SCROLLER_HANDLE_SIZE, hor.xmax);
      CLAMP(scrollers->hor_min, hor.xmin, hor.xmax - V2D_SCROLLER_HANDLE_SIZE);
    }
  }

  /* vertical scrollers */
  if (scroll & V2D_SCROLL_VERTICAL) {
    /* scroller 'button' extents */
    totsize = BLI_rctf_size_y(&v2d->tot);
    scrollsize = (float)BLI_rcti_size_y(&vert);
    if (totsize == 0.0f) {
      totsize = 1.0f; /* avoid divide by zero */
    }

    fac1 = (v2d->cur.ymin - v2d->tot.ymin) / totsize;
    if (fac1 <= 0.0f) {
      scrollers->vert_min = vert.ymin;
    }
    else {
      scrollers->vert_min = (int)(vert.ymin + (fac1 * scrollsize));
    }

    fac2 = (v2d->cur.ymax - v2d->tot.ymin) / totsize;
    if (fac2 >= 1.0f) {
      scrollers->vert_max = vert.ymax;
    }
    else {
      scrollers->vert_max = (int)(vert.ymin + (fac2 * scrollsize));
    }

    /* prevent inverted sliders */
    if (scrollers->vert_min > scrollers->vert_max) {
      scrollers->vert_min = scrollers->vert_max;
    }
    /* prevent sliders from being too small, and disappearing */
    if ((scrollers->vert_max - scrollers->vert_min) < V2D_SCROLLER_HANDLE_SIZE) {

      scrollers->vert_max = scrollers->vert_min + V2D_SCROLLER_HANDLE_SIZE;

      CLAMP(scrollers->vert_max, vert.ymin + V2D_SCROLLER_HANDLE_SIZE, vert.ymax);
      CLAMP(scrollers->vert_min, vert.ymin, vert.ymax - V2D_SCROLLER_HANDLE_SIZE);
    }
  }

  return scrollers;
}

/* Draw scrollbars in the given 2d-region */
void UI_view2d_scrollers_draw(View2D *v2d, View2DScrollers *vs)
{
  bTheme *btheme = UI_GetTheme();
  rcti vert, hor;
  const int scroll = view2d_scroll_mapped(v2d->scroll);
  const char emboss_alpha = btheme->tui.widget_emboss[3];
  uchar scrollers_back_color[4];

  /* Color for scrollbar backs */
  UI_GetThemeColor4ubv(TH_BACK, scrollers_back_color);

  /* make copies of rects for less typing */
  vert = vs->vert;
  hor = vs->hor;

  /* horizontal scrollbar */
  if (scroll & V2D_SCROLL_HORIZONTAL) {
    uiWidgetColors wcol = btheme->tui.wcol_scroll;
    const float alpha_fac = v2d->alpha_hor / 255.0f;
    rcti slider;
    int state;

    slider.xmin = vs->hor_min;
    slider.xmax = vs->hor_max;
    slider.ymin = hor.ymin;
    slider.ymax = hor.ymax;

    state = (v2d->scroll_ui & V2D_SCROLL_H_ACTIVE) ? UI_SCROLL_PRESSED : 0;

    wcol.inner[3] *= alpha_fac;
    wcol.item[3] *= alpha_fac;
    wcol.outline[3] *= alpha_fac;
    btheme->tui.widget_emboss[3] *= alpha_fac; /* will be reset later */

    /* show zoom handles if:
     * - zooming on x-axis is allowed (no scroll otherwise)
     * - slider bubble is large enough (no overdraw confusion)
     * - scale is shown on the scroller
     *   (workaround to make sure that button windows don't show these,
     *   and only the time-grids with their zoomability can do so)
     */
    if ((v2d->keepzoom & V2D_LOCKZOOM_X) == 0 && (v2d->scroll & V2D_SCROLL_HORIZONTAL_HANDLES) &&
        (BLI_rcti_size_x(&slider) > V2D_SCROLLER_HANDLE_SIZE)) {
      state |= UI_SCROLL_ARROWS;
    }

    UI_draw_widget_scroll(&wcol, &hor, &slider, state);
  }

  /* vertical scrollbar */
  if (scroll & V2D_SCROLL_VERTICAL) {
    uiWidgetColors wcol = btheme->tui.wcol_scroll;
    rcti slider;
    const float alpha_fac = v2d->alpha_vert / 255.0f;
    int state;

    slider.xmin = vert.xmin;
    slider.xmax = vert.xmax;
    slider.ymin = vs->vert_min;
    slider.ymax = vs->vert_max;

    state = (v2d->scroll_ui & V2D_SCROLL_V_ACTIVE) ? UI_SCROLL_PRESSED : 0;

    wcol.inner[3] *= alpha_fac;
    wcol.item[3] *= alpha_fac;
    wcol.outline[3] *= alpha_fac;
    btheme->tui.widget_emboss[3] *= alpha_fac; /* will be reset later */

    /* show zoom handles if:
     * - zooming on y-axis is allowed (no scroll otherwise)
     * - slider bubble is large enough (no overdraw confusion)
     * - scale is shown on the scroller
     *   (workaround to make sure that button windows don't show these,
     *   and only the time-grids with their zoomability can do so)
     */
    if ((v2d->keepzoom & V2D_LOCKZOOM_Y) == 0 && (v2d->scroll & V2D_SCROLL_VERTICAL_HANDLES) &&
        (BLI_rcti_size_y(&slider) > V2D_SCROLLER_HANDLE_SIZE)) {
      state |= UI_SCROLL_ARROWS;
    }

    UI_draw_widget_scroll(&wcol, &vert, &slider, state);
  }

  /* Was changed above, so reset. */
  btheme->tui.widget_emboss[3] = emboss_alpha;
}

/* free temporary memory used for drawing scrollers */
void UI_view2d_scrollers_free(View2DScrollers *scrollers)
{
  MEM_freeN(scrollers);
}

/* *********************************************************************** */
/* List View Utilities */

/**
 * Get the 'cell' (row, column) that the given 2D-view coordinates
 * (i.e. in 'tot' rect space) lie in.
 *
 * \param columnwidth, rowheight: size of each 'cell'
 * \param startx, starty: coordinates (in 'tot' rect space) that the list starts from.
 * This should be (0,0) for most views. However, for those where the starting row was offsetted
 * (like for Animation Editor channel lists, to make the first entry more visible), these will be
 * the min-coordinates of the first item.
 * \param viewx, viewy: 2D-coordinates (in 2D-view / 'tot' rect space) to get the cell for
 * \param r_column, r_row: the 'coordinates' of the relevant 'cell'
 */
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

/* *********************************************************************** */
/* Coordinate Conversions */

float UI_view2d_region_to_view_x(const struct View2D *v2d, float x)
{
  return (v2d->cur.xmin +
          (BLI_rctf_size_x(&v2d->cur) * (x - v2d->mask.xmin) / BLI_rcti_size_x(&v2d->mask)));
}
float UI_view2d_region_to_view_y(const struct View2D *v2d, float y)
{
  return (v2d->cur.ymin +
          (BLI_rctf_size_y(&v2d->cur) * (y - v2d->mask.ymin) / BLI_rcti_size_y(&v2d->mask)));
}

/**
 * Convert from screen/region space to 2d-View space
 *
 * \param x, y: coordinates to convert
 * \param r_view_x, r_view_y: resultant coordinates
 */
void UI_view2d_region_to_view(
    const View2D *v2d, float x, float y, float *r_view_x, float *r_view_y)
{
  *r_view_x = UI_view2d_region_to_view_x(v2d, x);
  *r_view_y = UI_view2d_region_to_view_y(v2d, y);
}

void UI_view2d_region_to_view_rctf(const View2D *v2d, const rctf *rect_src, rctf *rect_dst)
{
  const float cur_size[2] = {BLI_rctf_size_x(&v2d->cur), BLI_rctf_size_y(&v2d->cur)};
  const float mask_size[2] = {BLI_rcti_size_x(&v2d->mask), BLI_rcti_size_y(&v2d->mask)};

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
  return (v2d->mask.xmin +
          (((x - v2d->cur.xmin) / BLI_rctf_size_x(&v2d->cur)) * BLI_rcti_size_x(&v2d->mask)));
}
float UI_view2d_view_to_region_y(const View2D *v2d, float y)
{
  return (v2d->mask.ymin +
          (((y - v2d->cur.ymin) / BLI_rctf_size_y(&v2d->cur)) * BLI_rcti_size_y(&v2d->mask)));
}

/**
 * Convert from 2d-View space to screen/region space
 * \note Coordinates are clamped to lie within bounds of region
 *
 * \param x, y: Coordinates to convert.
 * \param r_region_x, r_region_y: Resultant coordinates.
 */
bool UI_view2d_view_to_region_clip(
    const View2D *v2d, float x, float y, int *r_region_x, int *r_region_y)
{
  /* express given coordinates as proportional values */
  x = (x - v2d->cur.xmin) / BLI_rctf_size_x(&v2d->cur);
  y = (y - v2d->cur.ymin) / BLI_rctf_size_y(&v2d->cur);

  /* check if values are within bounds */
  if ((x >= 0.0f) && (x <= 1.0f) && (y >= 0.0f) && (y <= 1.0f)) {
    *r_region_x = (int)(v2d->mask.xmin + (x * BLI_rcti_size_x(&v2d->mask)));
    *r_region_y = (int)(v2d->mask.ymin + (y * BLI_rcti_size_y(&v2d->mask)));

    return true;
  }
  else {
    /* set initial value in case coordinate lies outside of bounds */
    *r_region_x = *r_region_y = V2D_IS_CLIPPED;

    return false;
  }
}

/**
 * Convert from 2d-view space to screen/region space
 *
 * \note Coordinates are NOT clamped to lie within bounds of region.
 *
 * \param x, y: Coordinates to convert.
 * \param r_region_x, r_region_y: Resultant coordinates.
 */
void UI_view2d_view_to_region(View2D *v2d, float x, float y, int *r_region_x, int *r_region_y)
{
  /* step 1: express given coordinates as proportional values */
  x = (x - v2d->cur.xmin) / BLI_rctf_size_x(&v2d->cur);
  y = (y - v2d->cur.ymin) / BLI_rctf_size_y(&v2d->cur);

  /* step 2: convert proportional distances to screen coordinates  */
  x = v2d->mask.xmin + (x * BLI_rcti_size_x(&v2d->mask));
  y = v2d->mask.ymin + (y * BLI_rcti_size_y(&v2d->mask));

  /* although we don't clamp to lie within region bounds, we must avoid exceeding size of ints */
  *r_region_x = clamp_float_to_int(x);
  *r_region_y = clamp_float_to_int(y);
}

void UI_view2d_view_to_region_fl(
    View2D *v2d, float x, float y, float *r_region_x, float *r_region_y)
{
  /* express given coordinates as proportional values */
  x = (x - v2d->cur.xmin) / BLI_rctf_size_x(&v2d->cur);
  y = (y - v2d->cur.ymin) / BLI_rctf_size_y(&v2d->cur);

  /* convert proportional distances to screen coordinates */
  *r_region_x = v2d->mask.xmin + (x * BLI_rcti_size_x(&v2d->mask));
  *r_region_y = v2d->mask.ymin + (y * BLI_rcti_size_y(&v2d->mask));
}

void UI_view2d_view_to_region_rcti(View2D *v2d, const rctf *rect_src, rcti *rect_dst)
{
  const float cur_size[2] = {BLI_rctf_size_x(&v2d->cur), BLI_rctf_size_y(&v2d->cur)};
  const float mask_size[2] = {BLI_rcti_size_x(&v2d->mask), BLI_rcti_size_y(&v2d->mask)};
  rctf rect_tmp;

  /* step 1: express given coordinates as proportional values */
  rect_tmp.xmin = (rect_src->xmin - v2d->cur.xmin) / cur_size[0];
  rect_tmp.xmax = (rect_src->xmax - v2d->cur.xmin) / cur_size[0];
  rect_tmp.ymin = (rect_src->ymin - v2d->cur.ymin) / cur_size[1];
  rect_tmp.ymax = (rect_src->ymax - v2d->cur.ymin) / cur_size[1];

  /* step 2: convert proportional distances to screen coordinates  */
  rect_tmp.xmin = v2d->mask.xmin + (rect_tmp.xmin * mask_size[0]);
  rect_tmp.xmax = v2d->mask.xmin + (rect_tmp.xmax * mask_size[0]);
  rect_tmp.ymin = v2d->mask.ymin + (rect_tmp.ymin * mask_size[1]);
  rect_tmp.ymax = v2d->mask.ymin + (rect_tmp.ymax * mask_size[1]);

  clamp_rctf_to_rcti(rect_dst, &rect_tmp);
}

void UI_view2d_view_to_region_m4(View2D *v2d, float matrix[4][4])
{
  rctf mask;
  unit_m4(matrix);
  BLI_rctf_rcti_copy(&mask, &v2d->mask);
  BLI_rctf_transform_calc_m4_pivot_min(&v2d->cur, &mask, matrix);
}

bool UI_view2d_view_to_region_rcti_clip(View2D *v2d, const rctf *rect_src, rcti *rect_dst)
{
  const float cur_size[2] = {BLI_rctf_size_x(&v2d->cur), BLI_rctf_size_y(&v2d->cur)};
  const float mask_size[2] = {BLI_rcti_size_x(&v2d->mask), BLI_rcti_size_y(&v2d->mask)};
  rctf rect_tmp;

  BLI_assert(rect_src->xmin <= rect_src->xmax && rect_src->ymin <= rect_src->ymax);

  /* step 1: express given coordinates as proportional values */
  rect_tmp.xmin = (rect_src->xmin - v2d->cur.xmin) / cur_size[0];
  rect_tmp.xmax = (rect_src->xmax - v2d->cur.xmin) / cur_size[0];
  rect_tmp.ymin = (rect_src->ymin - v2d->cur.ymin) / cur_size[1];
  rect_tmp.ymax = (rect_src->ymax - v2d->cur.ymin) / cur_size[1];

  if (((rect_tmp.xmax < 0.0f) || (rect_tmp.xmin > 1.0f) || (rect_tmp.ymax < 0.0f) ||
       (rect_tmp.ymin > 1.0f)) == 0) {
    /* step 2: convert proportional distances to screen coordinates  */
    rect_tmp.xmin = v2d->mask.xmin + (rect_tmp.xmin * mask_size[0]);
    rect_tmp.xmax = v2d->mask.ymin + (rect_tmp.xmax * mask_size[0]);
    rect_tmp.ymin = v2d->mask.ymin + (rect_tmp.ymin * mask_size[1]);
    rect_tmp.ymax = v2d->mask.ymin + (rect_tmp.ymax * mask_size[1]);

    clamp_rctf_to_rcti(rect_dst, &rect_tmp);

    return true;
  }
  else {
    rect_dst->xmin = rect_dst->xmax = rect_dst->ymin = rect_dst->ymax = V2D_IS_CLIPPED;

    return false;
  }
}

/* *********************************************************************** */
/* Utilities */

/* View2D data by default resides in region, so get from region stored in context */
View2D *UI_view2d_fromcontext(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  if (area == NULL) {
    return NULL;
  }
  if (region == NULL) {
    return NULL;
  }
  return &(region->v2d);
}

/* same as above, but it returns regionwindow. Utility for pulldowns or buttons */
View2D *UI_view2d_fromcontext_rwin(const bContext *C)
{
  ScrArea *sa = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  if (sa == NULL) {
    return NULL;
  }
  if (region == NULL) {
    return NULL;
  }
  if (region->regiontype != RGN_TYPE_WINDOW) {
    ARegion *ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
    return ar ? &(ar->v2d) : NULL;
  }
  return &(region->v2d);
}

/**
 * Calculate the scale per-axis of the drawing-area
 *
 * Is used to inverse correct drawing of icons, etc. that need to follow view
 * but not be affected by scale
 *
 * \param r_x, r_y: scale on each axis
 */
void UI_view2d_scale_get(View2D *v2d, float *r_x, float *r_y)
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
/**
 * Same as ``UI_view2d_scale_get() - 1.0f / x, y``
 */
void UI_view2d_scale_get_inverse(View2D *v2d, float *r_x, float *r_y)
{
  if (r_x) {
    *r_x = BLI_rctf_size_x(&v2d->cur) / BLI_rcti_size_x(&v2d->mask);
  }
  if (r_y) {
    *r_y = BLI_rctf_size_y(&v2d->cur) / BLI_rcti_size_y(&v2d->mask);
  }
}

/**
 * Simple functions for consistent center offset access.
 * Used by node editor to shift view center for each individual node tree.
 */
void UI_view2d_center_get(struct View2D *v2d, float *r_x, float *r_y)
{
  /* get center */
  if (r_x) {
    *r_x = BLI_rctf_cent_x(&v2d->cur);
  }
  if (r_y) {
    *r_y = BLI_rctf_cent_y(&v2d->cur);
  }
}
void UI_view2d_center_set(struct View2D *v2d, float x, float y)
{
  BLI_rctf_recenter(&v2d->cur, x, y);

  /* make sure that 'cur' rect is in a valid state as a result of these changes */
  UI_view2d_curRect_validate(v2d);
}

/**
 * Simple pan function
 *  (0.0, 0.0) bottom left
 *  (0.5, 0.5) center
 *  (1.0, 1.0) top right.
 */
void UI_view2d_offset(struct View2D *v2d, float xfac, float yfac)
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

/**
 * Check if mouse is within scrollers
 *
 * \param x, y: Mouse coordinates in screen (not region) space.
 * \param r_scroll: Mapped view2d scroll flag.
 *
 * \return appropriate code for match.
 * - 'h' = in horizontal scroller.
 * - 'v' = in vertical scroller.
 * - 0 = not in scroller.
 */
char UI_view2d_mouse_in_scrollers_ex(
    const ARegion *ar, const View2D *v2d, int x, int y, int *r_scroll)
{
  const int scroll = view2d_scroll_mapped(v2d->scroll);
  *r_scroll = scroll;

  if (scroll) {
    /* Move to region-coordinates. */
    const int co[2] = {
        x - ar->winrct.xmin,
        y - ar->winrct.ymin,
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

char UI_view2d_rect_in_scrollers_ex(const ARegion *ar,
                                    const View2D *v2d,
                                    const rcti *rect,
                                    int *r_scroll)
{
  const int scroll = view2d_scroll_mapped(v2d->scroll);
  *r_scroll = scroll;

  if (scroll) {
    /* Move to region-coordinates. */
    rcti rect_region = *rect;
    BLI_rcti_translate(&rect_region, -ar->winrct.xmin, ar->winrct.ymin);
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

char UI_view2d_mouse_in_scrollers(const ARegion *ar, const View2D *v2d, int x, int y)
{
  int scroll_dummy = 0;
  return UI_view2d_mouse_in_scrollers_ex(ar, v2d, x, y, &scroll_dummy);
}

char UI_view2d_rect_in_scrollers(const ARegion *ar, const View2D *v2d, const rcti *rect)
{
  int scroll_dummy = 0;
  return UI_view2d_rect_in_scrollers_ex(ar, v2d, rect, &scroll_dummy);
}

/* ******************* view2d text drawing cache ******************** */

typedef struct View2DString {
  struct View2DString *next;
  union {
    uchar ub[4];
    int pack;
  } col;
  rcti rect;
  int mval[2];

  /* str is allocated past the end */
  char str[0];
} View2DString;

/* assumes caches are used correctly, so for time being no local storage in v2d */
static MemArena *g_v2d_strings_arena = NULL;
static View2DString *g_v2d_strings = NULL;

void UI_view2d_text_cache_add(
    View2D *v2d, float x, float y, const char *str, size_t str_len, const char col[4])
{
  int mval[2];

  BLI_assert(str_len == strlen(str));

  if (UI_view2d_view_to_region_clip(v2d, x, y, &mval[0], &mval[1])) {
    int alloc_len = str_len + 1;
    View2DString *v2s;

    if (g_v2d_strings_arena == NULL) {
      g_v2d_strings_arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 14), __func__);
    }

    v2s = BLI_memarena_alloc(g_v2d_strings_arena, sizeof(View2DString) + alloc_len);

    BLI_LINKS_PREPEND(g_v2d_strings, v2s);

    v2s->col.pack = *((const int *)col);

    memset(&v2s->rect, 0, sizeof(v2s->rect));

    v2s->mval[0] = mval[0];
    v2s->mval[1] = mval[1];

    memcpy(v2s->str, str, alloc_len);
  }
}

/* no clip (yet) */
void UI_view2d_text_cache_add_rectf(
    View2D *v2d, const rctf *rect_view, const char *str, size_t str_len, const char col[4])
{
  rcti rect;

  BLI_assert(str_len == strlen(str));

  if (UI_view2d_view_to_region_rcti_clip(v2d, rect_view, &rect)) {
    int alloc_len = str_len + 1;
    View2DString *v2s;

    if (g_v2d_strings_arena == NULL) {
      g_v2d_strings_arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 14), __func__);
    }

    v2s = BLI_memarena_alloc(g_v2d_strings_arena, sizeof(View2DString) + alloc_len);

    BLI_LINKS_PREPEND(g_v2d_strings, v2s);

    v2s->col.pack = *((const int *)col);

    v2s->rect = rect;

    v2s->mval[0] = v2s->rect.xmin;
    v2s->mval[1] = v2s->rect.ymin;

    memcpy(v2s->str, str, alloc_len);
  }
}

void UI_view2d_text_cache_draw(ARegion *ar)
{
  View2DString *v2s;
  int col_pack_prev = 0;

  /* investigate using BLF_ascender() */
  const int font_id = BLF_default();

  BLF_set_default();
  const float default_height = g_v2d_strings ? BLF_height(font_id, "28", 3) : 0.0f;

  wmOrtho2_region_pixelspace(ar);

  for (v2s = g_v2d_strings; v2s; v2s = v2s->next) {
    int xofs = 0, yofs;

    yofs = ceil(0.5f * (BLI_rcti_size_y(&v2s->rect) - default_height));
    if (yofs < 1) {
      yofs = 1;
    }

    if (col_pack_prev != v2s->col.pack) {
      BLF_color3ubv(font_id, v2s->col.ub);
      col_pack_prev = v2s->col.pack;
    }

    if (v2s->rect.xmin >= v2s->rect.xmax) {
      BLF_draw_default((float)(v2s->mval[0] + xofs),
                       (float)(v2s->mval[1] + yofs),
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
  g_v2d_strings = NULL;

  if (g_v2d_strings_arena) {
    BLI_memarena_free(g_v2d_strings_arena);
    g_v2d_strings_arena = NULL;
  }
}

/* ******************************************************** */

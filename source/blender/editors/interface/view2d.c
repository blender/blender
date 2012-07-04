/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * 
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/view2d.c
 *  \ingroup edinterface
 */


#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_global.h"


#include "WM_api.h"

#include "BIF_gl.h"

#include "BLF_api.h"

#include "ED_anim_api.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "interface_intern.h"

/* *********************************************************************** */

/* XXX still unresolved: scrolls hide/unhide vs region mask handling */
/* XXX there's V2D_SCROLL_HORIZONTAL_HIDE and V2D_SCROLL_HORIZONTAL_FULLR ... */

/* helper to allow scrollbars to dynamically hide
 *  - returns a copy of the scrollbar settings with the flags to display
 *	  horizontal/vertical scrollbars removed
 *	- input scroll value is the v2d->scroll var
 *	- hide flags are set per region at drawtime
 */
static int view2d_scroll_mapped(int scroll)
{
	if (scroll & V2D_SCROLL_HORIZONTAL_HIDE)
		scroll &= ~(V2D_SCROLL_HORIZONTAL);
	if (scroll & V2D_SCROLL_VERTICAL_HIDE)
		scroll &= ~(V2D_SCROLL_VERTICAL);
	return scroll;
}

/* called each time cur changes, to dynamically update masks */
static void view2d_masks(View2D *v2d)
{
	int scroll;
	
	/* mask - view frame */
	v2d->mask.xmin = v2d->mask.ymin = 0;
	v2d->mask.xmax = v2d->winx - 1; /* -1 yes! masks are pixels */
	v2d->mask.ymax = v2d->winy - 1;

#if 0
	// XXX see above
	v2d->scroll &= ~(V2D_SCROLL_HORIZONTAL_HIDE | V2D_SCROLL_VERTICAL_HIDE);
	/* check size if: */
	if (v2d->scroll & V2D_SCROLL_HORIZONTAL)
		if (!(v2d->scroll & V2D_SCROLL_SCALE_HORIZONTAL))
			if (v2d->tot.xmax - v2d->tot.xmin <= v2d->cur.xmax - v2d->cur.xmin)
				v2d->scroll |= V2D_SCROLL_HORIZONTAL_HIDE;
	if (v2d->scroll & V2D_SCROLL_VERTICAL)
		if (!(v2d->scroll & V2D_SCROLL_SCALE_VERTICAL))
			if (v2d->tot.ymax - v2d->tot.ymin <= v2d->cur.ymax - v2d->cur.ymin)
				v2d->scroll |= V2D_SCROLL_VERTICAL_HIDE;
#endif
	scroll = view2d_scroll_mapped(v2d->scroll);
	
	/* scrollers shrink mask area, but should be based off regionsize 
	 *	- they can only be on one to two edges of the region they define
	 *	- if they overlap, they must not occupy the corners (which are reserved for other widgets)
	 */
	if (scroll) {
		/* vertical scroller */
		if (scroll & V2D_SCROLL_LEFT) {
			/* on left-hand edge of region */
			v2d->vert = v2d->mask;
			v2d->vert.xmax = V2D_SCROLL_WIDTH;
			v2d->mask.xmin = v2d->vert.xmax + 1;
		}
		else if (scroll & V2D_SCROLL_RIGHT) {
			/* on right-hand edge of region */
			v2d->vert = v2d->mask;
			v2d->vert.xmax++; /* one pixel extra... was leaving a minor gap... */
			v2d->vert.xmin = v2d->vert.xmax - V2D_SCROLL_WIDTH;
			v2d->mask.xmax = v2d->vert.xmin - 1;
		}
		
		/* horizontal scroller */
		if (scroll & (V2D_SCROLL_BOTTOM | V2D_SCROLL_BOTTOM_O)) {
			/* on bottom edge of region (V2D_SCROLL_BOTTOM_O is outliner, the other is for standard) */
			v2d->hor = v2d->mask;
			v2d->hor.ymax = V2D_SCROLL_HEIGHT;
			v2d->mask.ymin = v2d->hor.ymax + 1;
		}
		else if (scroll & V2D_SCROLL_TOP) {
			/* on upper edge of region */
			v2d->hor = v2d->mask;
			v2d->hor.ymin = v2d->hor.ymax - V2D_SCROLL_HEIGHT;
			v2d->mask.ymax = v2d->hor.ymin - 1;
		}
		
		/* adjust vertical scroller if there's a horizontal scroller, to leave corner free */
		if (scroll & V2D_SCROLL_VERTICAL) {
			/* just set y min/max for vertical scroller to y min/max of mask as appropriate */
			if (scroll & (V2D_SCROLL_BOTTOM | V2D_SCROLL_BOTTOM_O)) {
				/* on bottom edge of region (V2D_SCROLL_BOTTOM_O is outliner, the other is for standard) */
				v2d->vert.ymin = v2d->mask.ymin;
			}
			else if (scroll & V2D_SCROLL_TOP) {
				/* on upper edge of region */
				v2d->vert.ymax = v2d->mask.ymax;
			}
		}
	}
	
}

/* Refresh and Validation */

/* Initialize all relevant View2D data (including view rects if first time) and/or refresh mask sizes after view resize
 *	- for some of these presets, it is expected that the region will have defined some
 *    additional settings necessary for the customization of the 2D viewport to its requirements
 *	- this function should only be called from region init() callbacks, where it is expected that
 *	  this is called before UI_view2d_size_update(), as this one checks that the rects are properly initialized. 
 */
void UI_view2d_region_reinit(View2D *v2d, short type, int winx, int winy)
{
	short tot_changed = 0, init = 0;
	uiStyle *style = UI_GetStyle();

	/* initialize data if there is a need for such */
	if ((v2d->flag & V2D_IS_INITIALISED) == 0) {
		/* set initialized flag so that View2D doesn't get reinitialised next time again */
		v2d->flag |= V2D_IS_INITIALISED;

		init = 1;
		
		/* see eView2D_CommonViewTypes in UI_view2d.h for available view presets */
		switch (type) {
			/* 'standard view' - optimum setup for 'standard' view behavior,
			 *  that should be used new views as basis for their
			 *  own unique View2D settings, which should be used instead of this in most cases...
			 */
			case V2D_COMMONVIEW_STANDARD:
			{
				/* for now, aspect ratio should be maintained, and zoom is clamped within sane default limits */
				v2d->keepzoom = (V2D_KEEPASPECT | V2D_LIMITZOOM);
				v2d->minzoom = 0.01f;
				v2d->maxzoom = 1000.0f;
				
				/* tot rect and cur should be same size, and aligned using 'standard' OpenGL coordinates for now 
				 *	- region can resize 'tot' later to fit other data
				 *	- keeptot is only within bounds, as strict locking is not that critical
				 *	- view is aligned for (0,0) -> (winx-1, winy-1) setup
				 */
				v2d->align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_NEG_Y);
				v2d->keeptot = V2D_KEEPTOT_BOUNDS;
				
				v2d->tot.xmin = v2d->tot.ymin = 0.0f;
				v2d->tot.xmax = (float)(winx - 1);
				v2d->tot.ymax = (float)(winy - 1);
				
				v2d->cur = v2d->tot;
				
				/* scrollers - should we have these by default? */
				// XXX for now, we don't override this, or set it either!
			}
			break;
			
			/* 'list/channel view' - zoom, aspect ratio, and alignment restrictions are set here */
			case V2D_COMMONVIEW_LIST:
			{
				/* zoom + aspect ratio are locked */
				v2d->keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
				v2d->minzoom = v2d->maxzoom = 1.0f;
				
				/* tot rect has strictly regulated placement, and must only occur in +/- quadrant */
				v2d->align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y);
				v2d->keeptot = V2D_KEEPTOT_STRICT;
				tot_changed = 1;
				
				/* scroller settings are currently not set here... that is left for regions... */
			}
			break;
				
			/* 'stack view' - practically the same as list/channel view, except is located in the pos y half instead. 
			 *  zoom, aspect ratio, and alignment restrictions are set here */
			case V2D_COMMONVIEW_STACK:
			{
				/* zoom + aspect ratio are locked */
				v2d->keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
				v2d->minzoom = v2d->maxzoom = 1.0f;
				
				/* tot rect has strictly regulated placement, and must only occur in +/+ quadrant */
				v2d->align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_NEG_Y);
				v2d->keeptot = V2D_KEEPTOT_STRICT;
				tot_changed = 1;
				
				/* scroller settings are currently not set here... that is left for regions... */
			}
			break;
				
			/* 'header' regions - zoom, aspect ratio, alignment, and panning restrictions are set here */
			case V2D_COMMONVIEW_HEADER:
			{
				/* zoom + aspect ratio are locked */
				v2d->keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
				v2d->minzoom = v2d->maxzoom = 1.0f;
				v2d->min[0] = v2d->max[0] = (float)(winx - 1);
				v2d->min[1] = v2d->max[1] = (float)(winy - 1);
				
				/* tot rect has strictly regulated placement, and must only occur in +/+ quadrant */
				v2d->align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_NEG_Y);
				v2d->keeptot = V2D_KEEPTOT_STRICT;
				tot_changed = 1;
				
				/* panning in y-axis is prohibited */
				v2d->keepofs = V2D_LOCKOFS_Y;
				
				/* absolutely no scrollers allowed */
				v2d->scroll = 0;
				
			}
			break;
			
			/* panels view, with horizontal/vertical align */
			case V2D_COMMONVIEW_PANELS_UI:
			{
				float panelzoom = (style) ? style->panelzoom : 1.0f;
				
				/* for now, aspect ratio should be maintained, and zoom is clamped within sane default limits */
				v2d->keepzoom = (V2D_KEEPASPECT | V2D_LIMITZOOM | V2D_KEEPZOOM);
				v2d->minzoom = 0.5f;
				v2d->maxzoom = 2.0f;
				//tot_changed = 1;
				
				v2d->align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y);
				v2d->keeptot = V2D_KEEPTOT_BOUNDS;
				
				v2d->scroll |= (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
				v2d->scroll |= V2D_SCROLL_HORIZONTAL_HIDE;
				v2d->scroll &= ~V2D_SCROLL_VERTICAL_HIDE;

				v2d->tot.xmin = 0.0f;
				v2d->tot.xmax = winx;
				
				v2d->tot.ymax = 0.0f;
				v2d->tot.ymin = -winy;
				
				v2d->cur.xmin = 0.0f;
				/* bad workaround for keeping zoom level with scrollers */
				v2d->cur.xmax = (winx - V2D_SCROLL_WIDTH) * panelzoom;
				
				v2d->cur.ymax = 0.0f;
				v2d->cur.ymin = (-winy) * panelzoom;
			}
			break;
				
			/* other view types are completely defined using their own settings already */
			default:
				/* we don't do anything here, as settings should be fine, but just make sure that rect */
				break;	
		}
	}
	
	/* store view size */
	v2d->winx = winx;
	v2d->winy = winy;
	
	/* set masks */
	view2d_masks(v2d);
	
	/* set 'tot' rect before setting cur? */
	if (tot_changed) 
		UI_view2d_totRect_set_resize(v2d, winx, winy, !init);
	else
		UI_view2d_curRect_validate_resize(v2d, !init);
}

/* Ensure View2D rects remain in a viable configuration 
 *	- cur is not allowed to be: larger than max, smaller than min, or outside of tot
 */
// XXX pre2.5 -> this used to be called  test_view2d()
void UI_view2d_curRect_validate_resize(View2D *v2d, int resize)
{
	float totwidth, totheight, curwidth, curheight, width, height;
	float winx, winy;
	rctf *cur, *tot;
	
	/* use mask as size of region that View2D resides in, as it takes into account scrollbars already  */
	winx = (float)(v2d->mask.xmax - v2d->mask.xmin + 1);
	winy = (float)(v2d->mask.ymax - v2d->mask.ymin + 1);
	
	/* get pointers to rcts for less typing */
	cur = &v2d->cur;
	tot = &v2d->tot;
	
	/* we must satisfy the following constraints (in decreasing order of importance):
	 *	- alignment restrictions are respected
	 *	- cur must not fall outside of tot
	 *	- axis locks (zoom and offset) must be maintained
	 *	- zoom must not be excessive (check either sizes or zoom values)
	 *	- aspect ratio should be respected (NOTE: this is quite closely realted to zoom too)
	 */
	
	/* Step 1: if keepzoom, adjust the sizes of the rects only
	 *	- firstly, we calculate the sizes of the rects
	 *	- curwidth and curheight are saved as reference... modify width and height values here
	 */
	totwidth = tot->xmax - tot->xmin;
	totheight = tot->ymax - tot->ymin;
	curwidth = width = cur->xmax - cur->xmin;
	curheight = height = cur->ymax - cur->ymin;
	
	/* if zoom is locked, size on the appropriate axis is reset to mask size */
	if (v2d->keepzoom & V2D_LOCKZOOM_X)
		width = winx;
	if (v2d->keepzoom & V2D_LOCKZOOM_Y)
		height = winy;
		
	/* values used to divide, so make it safe 
	 * NOTE: width and height must use FLT_MIN instead of 1, otherwise it is impossible to
	 *       get enough resolution in Graph Editor for editing some curves
	 */
	if (width < FLT_MIN) width = 1;
	if (height < FLT_MIN) height = 1;
	if (winx < 1) winx = 1;
	if (winy < 1) winy = 1;
	
	/* V2D_LIMITZOOM indicates that zoom level should be preserved when the window size changes */
	if (resize && (v2d->keepzoom & V2D_KEEPZOOM)) {
		float zoom, oldzoom;

		if ((v2d->keepzoom & V2D_LOCKZOOM_X) == 0) {
			zoom = winx / width;
			oldzoom = v2d->oldwinx / curwidth;

			if (oldzoom != zoom)
				width *= zoom / oldzoom;
		}

		if ((v2d->keepzoom & V2D_LOCKZOOM_Y) == 0) {
			zoom = winy / height;
			oldzoom = v2d->oldwiny / curheight;

			if (oldzoom != zoom)
				height *= zoom / oldzoom;
		}
	}
	/* keepzoom (V2D_LIMITZOOM set), indicates that zoom level on each axis must not exceed limits 
	 * NOTE: in general, it is not expected that the lock-zoom will be used in conjunction with this
	 */
	else if (v2d->keepzoom & V2D_LIMITZOOM) {
		float zoom, fac;
		
		/* check if excessive zoom on x-axis */
		if ((v2d->keepzoom & V2D_LOCKZOOM_X) == 0) {
			zoom = winx / width;
			if ((zoom < v2d->minzoom) || (zoom > v2d->maxzoom)) {
				fac = (zoom < v2d->minzoom) ? (zoom / v2d->minzoom) : (zoom / v2d->maxzoom);
				width *= fac;
			}
		}
		
		/* check if excessive zoom on y-axis */
		if ((v2d->keepzoom & V2D_LOCKZOOM_Y) == 0) {
			zoom = winy / height;
			if ((zoom < v2d->minzoom) || (zoom > v2d->maxzoom)) {
				fac = (zoom < v2d->minzoom) ? (zoom / v2d->minzoom) : (zoom / v2d->maxzoom);
				height *= fac;
			}
		}
	}
	else {
		/* make sure sizes don't exceed that of the min/max sizes (even though we're not doing zoom clamping) */
		CLAMP(width, v2d->min[0], v2d->max[0]);
		CLAMP(height, v2d->min[1], v2d->max[1]);
	}
	
	/* check if we should restore aspect ratio (if view size changed) */
	if (v2d->keepzoom & V2D_KEEPASPECT) {
		short do_x = FALSE, do_y = FALSE, do_cur /* , do_win */ /* UNUSED */;
		float /* curRatio, */ /* UNUSED */ winRatio;
		
		/* when a window edge changes, the aspect ratio can't be used to
		 * find which is the best new 'cur' rect. thats why it stores 'old' 
		 */
		if (winx != v2d->oldwinx) do_x = TRUE;
		if (winy != v2d->oldwiny) do_y = TRUE;
		
		/* curRatio = height / width; */ /* UNUSED */
		winRatio = winy / winx;
		
		/* both sizes change (area/region maximized)  */
		if (do_x == do_y) {
			if (do_x && do_y) {
				/* here is 1,1 case, so all others must be 0,0 */
				if (ABS(winx - v2d->oldwinx) > ABS(winy - v2d->oldwiny)) do_y = FALSE;
				else do_x = FALSE;
			}
			else if (winRatio > 1.0f) do_x = FALSE;
			else do_x = TRUE;
		}
		do_cur = do_x;
		/* do_win = do_y; */ /* UNUSED */
		
		if (do_cur) {
			if ((v2d->keeptot == V2D_KEEPTOT_STRICT) && (winx != v2d->oldwinx)) {
				/* special exception for Outliner (and later channel-lists):
				 *  - The view may be moved left to avoid contents being pushed out of view when view shrinks.
				 *	- The keeptot code will make sure cur->xmin will not be less than tot->xmin (which cannot be allowed)
				 *	- width is not adjusted for changed ratios here...
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
				 *	- Currently, no actions need to be taken here...
				 */

				if (winy < v2d->oldwiny) {
					float temp = v2d->oldwiny - winy;
					
					cur->ymin += temp;
					cur->ymax += temp;
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
	
	/* Step 2: apply new sizes to cur rect, but need to take into account alignment settings here... */
	if ((width != curwidth) || (height != curheight)) {
		float temp, dh;
		
		/* resize from centerpoint, unless otherwise specified */
		if (width != curwidth) {
			if (v2d->keepofs & V2D_LOCKOFS_X) {
				cur->xmax += width - (cur->xmax - cur->xmin);
			}
			else if (v2d->keepofs & V2D_KEEPOFS_X) {
				if (v2d->align & V2D_ALIGN_NO_POS_X)
					cur->xmin -= width - (cur->xmax - cur->xmin);
				else
					cur->xmax += width - (cur->xmax - cur->xmin);
			}
			else {
				temp = (cur->xmax + cur->xmin) * 0.5f;
				dh = width * 0.5f;
				
				cur->xmin = temp - dh;
				cur->xmax = temp + dh;
			}
		}
		if (height != curheight) {
			if (v2d->keepofs & V2D_LOCKOFS_Y) {
				cur->ymax += height - (cur->ymax - cur->ymin);
			}
			else if (v2d->keepofs & V2D_KEEPOFS_Y) {
				if (v2d->align & V2D_ALIGN_NO_POS_Y)
					cur->ymin -= height - (cur->ymax - cur->ymin);
				else
					cur->ymax += height - (cur->ymax - cur->ymin);
			}
			else {
				temp = (cur->ymax + cur->ymin) * 0.5f;
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
		curwidth = cur->xmax - cur->xmin;
		curheight = cur->ymax - cur->ymin;
		
		/* width */
		if ( (curwidth > totwidth) && !(v2d->keepzoom & (V2D_KEEPZOOM | V2D_LOCKZOOM_X | V2D_LIMITZOOM)) ) {
			/* if zoom doesn't have to be maintained, just clamp edges */
			if (cur->xmin < tot->xmin) cur->xmin = tot->xmin;
			if (cur->xmax > tot->xmax) cur->xmax = tot->xmax;
		}
		else if (v2d->keeptot == V2D_KEEPTOT_STRICT) {
			/* This is an exception for the outliner (and later channel-lists, headers) 
			 *	- must clamp within tot rect (absolutely no excuses)
			 *	--> therefore, cur->xmin must not be less than tot->xmin
			 */
			if (cur->xmin < tot->xmin) {
				/* move cur across so that it sits at minimum of tot */
				temp = tot->xmin - cur->xmin;
				
				cur->xmin += temp;
				cur->xmax += temp;
			}
			else if (cur->xmax > tot->xmax) {
				/* - only offset by difference of cur-xmax and tot-xmax if that would not move 
				 *  cur-xmin to lie past tot-xmin
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
			 *  - width too big, but maintaining zoom (i.e. widths cannot be changed)
			 *	- width is OK, but need to check if outside of boundaries
			 * 
			 * So, resolution is to just shift view by the gap between the extremities.
			 * We favour moving the 'minimum' across, as that's origin for most things
			 * (XXX - in the past, max was favored... if there are bugs, swap!)
			 */
			if ((cur->xmin < tot->xmin) && (cur->xmax > tot->xmax)) {
				/* outside boundaries on both sides, so take middle-point of tot, and place in balanced way */
				temp = (tot->xmax + tot->xmin) * 0.5f;
				diff = curheight * 0.5f;
				
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
				 *  cur-xmin to lie past tot-xmin
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
		if ( (curheight > totheight) && !(v2d->keepzoom & (V2D_KEEPZOOM | V2D_LOCKZOOM_Y | V2D_LIMITZOOM)) ) {
			/* if zoom doesn't have to be maintained, just clamp edges */
			if (cur->ymin < tot->ymin) cur->ymin = tot->ymin;
			if (cur->ymax > tot->ymax) cur->ymax = tot->ymax;
		}
		else {
			/* This here occurs when:
			 *  - height too big, but maintaining zoom (i.e. heights cannot be changed)
			 *	- height is OK, but need to check if outside of boundaries
			 * 
			 * So, resolution is to just shift view by the gap between the extremities.
			 * We favour moving the 'minimum' across, as that's origin for most things
			 */
			if ((cur->ymin < tot->ymin) && (cur->ymax > tot->ymax)) {
				/* outside boundaries on both sides, so take middle-point of tot, and place in balanced way */
				temp = (tot->ymax + tot->ymin) * 0.5f;
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
	view2d_masks(v2d);
}

void UI_view2d_curRect_validate(View2D *v2d)
{
	UI_view2d_curRect_validate_resize(v2d, 0);
}

/* ------------------ */

/* Called by menus to activate it, or by view2d operators to make sure 'related' views stay in synchrony */
void UI_view2d_sync(bScreen *screen, ScrArea *area, View2D *v2dcur, int flag)
{
	ScrArea *sa;
	ARegion *ar;
	
	/* don't continue if no view syncing to be done */
	if ((v2dcur->flag & (V2D_VIEWSYNC_SCREEN_TIME | V2D_VIEWSYNC_AREA_VERTICAL)) == 0)
		return;
		
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
					ED_region_tag_redraw(ar);
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
						ED_region_tag_redraw(ar);
					}
				}
			}
		}
	}
}


/* Restore 'cur' rect to standard orientation (i.e. optimal maximum view of tot) 
 * This does not take into account if zooming the view on an axis will improve the view (if allowed)
 */
void UI_view2d_curRect_reset(View2D *v2d)
{
	float width, height;
	
	/* assume width and height of 'cur' rect by default, should be same size as mask */
	width = (float)(v2d->mask.xmax - v2d->mask.xmin + 1);
	height = (float)(v2d->mask.ymax - v2d->mask.ymin + 1);
	
	/* handle width - posx and negx flags are mutually exclusive, so watch out */
	if ((v2d->align & V2D_ALIGN_NO_POS_X) && !(v2d->align & V2D_ALIGN_NO_NEG_X)) {
		/* width is in negative-x half */
		v2d->cur.xmin = (float)-width;
		v2d->cur.xmax = 0.0f;
	}
	else if ((v2d->align & V2D_ALIGN_NO_NEG_X) && !(v2d->align & V2D_ALIGN_NO_POS_X)) {
		/* width is in positive-x half */
		v2d->cur.xmin = 0.0f;
		v2d->cur.xmax = (float)width;
	}
	else {
		/* width is centered around (x == 0) */
		const float dx = (float)width / 2.0f;
		
		v2d->cur.xmin = -dx;
		v2d->cur.xmax = dx;
	}
	
	/* handle height - posx and negx flags are mutually exclusive, so watch out */
	if ((v2d->align & V2D_ALIGN_NO_POS_Y) && !(v2d->align & V2D_ALIGN_NO_NEG_Y)) {
		/* height is in negative-y half */
		v2d->cur.ymin = (float)-height;
		v2d->cur.ymax = 0.0f;
	}
	else if ((v2d->align & V2D_ALIGN_NO_NEG_Y) && !(v2d->align & V2D_ALIGN_NO_POS_Y)) {
		/* height is in positive-y half */
		v2d->cur.ymin = 0.0f;
		v2d->cur.ymax = (float)height;
	}
	else {
		/* height is centered around (y == 0) */
		const float dy = (float)height / 2.0f;
		
		v2d->cur.ymin = -dy;
		v2d->cur.ymax = dy;
	}
}

/* ------------------ */

/* Change the size of the maximum viewable area (i.e. 'tot' rect) */
void UI_view2d_totRect_set_resize(View2D *v2d, int width, int height, int resize)
{
	int scroll = view2d_scroll_mapped(v2d->scroll);
	
	/* don't do anything if either value is 0 */
	width = abs(width);
	height = abs(height);
	
	/* hrumf! */
	/* XXX: there are work arounds for this in the panel and file browse code. */
	if (scroll & V2D_SCROLL_HORIZONTAL) 
		width -= V2D_SCROLL_WIDTH;
	if (scroll & V2D_SCROLL_VERTICAL) 
		height -= V2D_SCROLL_HEIGHT;
	
	if (ELEM3(0, v2d, width, height)) {
		if (G.debug & G_DEBUG)
			printf("Error: View2D totRect set exiting: v2d=%p width=%d height=%d\n", (void *)v2d, width, height);  // XXX temp debug info
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
	UI_view2d_curRect_validate_resize(v2d, resize);
}

void UI_view2d_totRect_set(View2D *v2d, int width, int height)
{
	UI_view2d_totRect_set_resize(v2d, width, height, 0);
}

int UI_view2d_tab_set(View2D *v2d, int tab)
{
	float default_offset[2] = {0.0f, 0.0f};
	float *offset, *new_offset;
	int changed = 0;

	/* if tab changed, change offset */
	if (tab != v2d->tab_cur && v2d->tab_offset) {
		if (tab < v2d->tab_num)
			offset = &v2d->tab_offset[tab * 2];
		else
			offset = default_offset;

		v2d->cur.xmax += offset[0] - v2d->cur.xmin;
		v2d->cur.xmin = offset[0];

		v2d->cur.ymin += offset[1] - v2d->cur.ymax;
		v2d->cur.ymax = offset[1];

		/* validation should happen in subsequent totRect_set */

		changed = 1;
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

/* *********************************************************************** */
/* View Matrix Setup */

/* mapping function to ensure 'cur' draws extended over the area where sliders are */
static void view2d_map_cur_using_mask(View2D *v2d, rctf *curmasked)
{
	*curmasked = v2d->cur;
	
	if (view2d_scroll_mapped(v2d->scroll)) {
		float dx = (v2d->cur.xmax - v2d->cur.xmin) / ((float)(v2d->mask.xmax - v2d->mask.xmin + 1));
		float dy = (v2d->cur.ymax - v2d->cur.ymin) / ((float)(v2d->mask.ymax - v2d->mask.ymin + 1));
		
		if (v2d->mask.xmin != 0)
			curmasked->xmin -= dx * (float)v2d->mask.xmin;
		if (v2d->mask.xmax + 1 != v2d->winx)
			curmasked->xmax += dx * (float)(v2d->winx - v2d->mask.xmax - 1);
		
		if (v2d->mask.ymin != 0)
			curmasked->ymin -= dy * (float)v2d->mask.ymin;
		if (v2d->mask.ymax + 1 != v2d->winy)
			curmasked->ymax += dy * (float)(v2d->winy - v2d->mask.ymax - 1);
		
	}
}

/* Set view matrices to use 'cur' rect as viewing frame for View2D drawing */
void UI_view2d_view_ortho(View2D *v2d)
{
	rctf curmasked;
	float xofs, yofs;
	
	/* pixel offsets (-0.375f) are needed to get 1:1 correspondence with pixels for smooth UI drawing, 
	 * but only applied where requested
	 */
	/* XXX brecht: instead of zero at least use a tiny offset, otherwise
	 * pixel rounding is effectively random due to float inaccuracy */
	xofs = 0.001f * (v2d->cur.xmax - v2d->cur.xmin) / (v2d->mask.xmax - v2d->mask.xmin);
	yofs = 0.001f * (v2d->cur.ymax - v2d->cur.ymin) / (v2d->mask.ymax - v2d->mask.ymin);
	
	/* apply mask-based adjustments to cur rect (due to scrollers), to eliminate scaling artifacts */
	view2d_map_cur_using_mask(v2d, &curmasked);
	
	curmasked.xmin -= xofs; curmasked.xmax -= xofs;
	curmasked.ymin -= yofs; curmasked.ymax -= yofs;
	
	/* XXX ton: this flag set by outliner, for icons */
	if (v2d->flag & V2D_PIXELOFS_X) {
		curmasked.xmin = floorf(curmasked.xmin) - 0.001f;
		curmasked.xmax = floorf(curmasked.xmax) - 0.001f;
	}
	if (v2d->flag & V2D_PIXELOFS_Y) {
		curmasked.ymin = floorf(curmasked.ymin) - 0.001f;
		curmasked.ymax = floorf(curmasked.ymax) - 0.001f;
	}
	
	/* set matrix on all appropriate axes */
	wmOrtho2(curmasked.xmin - xofs, curmasked.xmax - xofs, curmasked.ymin - yofs, curmasked.ymax - yofs);

	/* XXX is this necessary? */
	glLoadIdentity();
}

/* Set view matrices to only use one axis of 'cur' only
 *	- xaxis     = if non-zero, only use cur x-axis, otherwise use cur-yaxis (mostly this will be used for x)
 */
void UI_view2d_view_orthoSpecial(ARegion *ar, View2D *v2d, short xaxis)
{
	rctf curmasked;
	float xofs, yofs;
	
	/* pixel offsets (-0.375f) are needed to get 1:1 correspondence with pixels for smooth UI drawing, 
	 * but only applied where requested
	 */
	/* XXX temp (ton) */
	xofs = 0.0f; // (v2d->flag & V2D_PIXELOFS_X) ? 0.375f : 0.0f;
	yofs = 0.0f; // (v2d->flag & V2D_PIXELOFS_Y) ? 0.375f : 0.0f;
	
	/* apply mask-based adjustments to cur rect (due to scrollers), to eliminate scaling artifacts */
	view2d_map_cur_using_mask(v2d, &curmasked);
	
	/* only set matrix with 'cur' coordinates on relevant axes */
	if (xaxis)
		wmOrtho2(curmasked.xmin - xofs, curmasked.xmax - xofs, -yofs, ar->winy - yofs);
	else
		wmOrtho2(-xofs, ar->winx - xofs, curmasked.ymin - yofs, curmasked.ymax - yofs);
		
	/* XXX is this necessary? */
	glLoadIdentity();
} 


/* Restore view matrices after drawing */
void UI_view2d_view_restore(const bContext *C)
{
	ARegion *ar = CTX_wm_region(C);
	int width = ar->winrct.xmax - ar->winrct.xmin + 1;
	int height = ar->winrct.ymax - ar->winrct.ymin + 1;
	
	wmOrtho2(0.0f, (float)width, 0.0f, (float)height);
	glLoadIdentity();
	
	//	ED_region_pixelspace(CTX_wm_region(C));
}

/* *********************************************************************** */
/* Gridlines */

/* View2DGrid is typedef'd in UI_view2d.h */
struct View2DGrid {
	float dx, dy;           /* stepsize (in pixels) between gridlines */
	float startx, starty;   /* initial coordinates to start drawing grid from */
	int powerx, powery;     /* step as power of 10 */
};

/* --------------- */

/* try to write step as a power of 10 */
static void step_to_grid(float *step, int *power, int unit)
{
	const float loga = (float)log10(*step);
	float rem;
	
	*power = (int)(loga);
	
	rem = loga - (*power);
	rem = (float)pow(10.0, rem);
	
	if (loga < 0.0f) {
		if (rem < 0.2f) rem = 0.2f;
		else if (rem < 0.5f) rem = 0.5f;
		else rem = 1.0f;
		
		*step = rem * (float)pow(10.0, (*power));
		
		/* for frames, we want 1.0 frame intervals only */
		if (unit == V2D_UNIT_FRAMES) {
			rem = 1.0f;
			*step = 2.0f; /* use 2 since there are grid lines drawn in between, this way to get 1 line per frane */
		}
		
		/* prevents printing 1.0 2.0 3.0 etc */
		if (rem == 1.0f) (*power)++;	
	}
	else {
		if (rem < 2.0f) rem = 2.0f;
		else if (rem < 5.0f) rem = 5.0f;
		else rem = 10.0f;
		
		*step = rem * (float)pow(10.0, (*power));
		
		(*power)++;
		/* prevents printing 1.0, 2.0, 3.0, etc. */
		if (rem == 10.0f) (*power)++;	
	}
}

/* Initialize settings necessary for drawing gridlines in a 2d-view
 *	- Currently, will return pointer to View2DGrid struct that needs to 
 *	  be freed with UI_view2d_grid_free()
 *	- Is used for scrollbar drawing too (for units drawing)
 *	- Units + clamping args will be checked, to make sure they are valid values that can be used
 *	  so it is very possible that we won't return grid at all!
 *	
 *	- xunits,yunits	= V2D_UNIT_*  grid steps in seconds or frames 
 *	- xclamp,yclamp	= V2D_CLAMP_* only show whole-number intervals
 *	- winx			= width of region we're drawing to, note: not used but keeping for completeness.
 *	- winy			= height of region we're drawing into
 */
View2DGrid *UI_view2d_grid_calc(Scene *scene, View2D *v2d, short xunits, short xclamp, short yunits, short yclamp, int UNUSED(winx), int winy)
{

	View2DGrid *grid;
	float space, pixels, seconddiv;
	
	/* check that there are at least some workable args */
	if (ELEM(V2D_ARG_DUMMY, xunits, xclamp) && ELEM(V2D_ARG_DUMMY, yunits, yclamp))
		return NULL;
	
	/* grid here is allocated... */
	grid = MEM_callocN(sizeof(View2DGrid), "View2DGrid");
	
	/* rule: gridstep is minimal GRIDSTEP pixels */
	if (xunits == V2D_UNIT_SECONDS) {
		seconddiv = (float)(0.01 * FPS);
	}
	else {
		seconddiv = 1.0f;
	}
	
	/* calculate x-axis grid scale (only if both args are valid) */
	if (ELEM(V2D_ARG_DUMMY, xunits, xclamp) == 0) {
		space = v2d->cur.xmax - v2d->cur.xmin;
		pixels = (float)(v2d->mask.xmax - v2d->mask.xmin);
		
		if (pixels != 0.0f) {
			grid->dx = (U.v2d_min_gridsize * space) / (seconddiv * pixels);
			step_to_grid(&grid->dx, &grid->powerx, xunits);
			grid->dx *= seconddiv;
		}
		
		if (xclamp == V2D_GRID_CLAMP) {
			if (grid->dx < 0.1f) grid->dx = 0.1f;
			grid->powerx -= 2;
			if (grid->powerx < -2) grid->powerx = -2;
		}
	}
	
	/* calculate y-axis grid scale (only if both args are valid) */
	if (ELEM(V2D_ARG_DUMMY, yunits, yclamp) == 0) {
		space = v2d->cur.ymax - v2d->cur.ymin;
		pixels = (float)winy;
		
		grid->dy = U.v2d_min_gridsize * space / pixels;
		step_to_grid(&grid->dy, &grid->powery, yunits);
		
		if (yclamp == V2D_GRID_CLAMP) {
			if (grid->dy < 1.0f) grid->dy = 1.0f;
			if (grid->powery < 1) grid->powery = 1;
		}
	}
	
	/* calculate start position */
	if (ELEM(V2D_ARG_DUMMY, xunits, xclamp) == 0) {
		grid->startx = seconddiv * (v2d->cur.xmin / seconddiv - (float)fmod(v2d->cur.xmin / seconddiv, grid->dx / seconddiv));
		if (v2d->cur.xmin < 0.0f) grid->startx -= grid->dx;
	}
	else
		grid->startx = v2d->cur.xmin;
		
	if (ELEM(V2D_ARG_DUMMY, yunits, yclamp) == 0) {
		grid->starty = (v2d->cur.ymin - (float)fmod(v2d->cur.ymin, grid->dy));
		if (v2d->cur.ymin < 0.0f) grid->starty -= grid->dy;
	}
	else
		grid->starty = v2d->cur.ymin;
	
	return grid;
}

/* Draw gridlines in the given 2d-region */
void UI_view2d_grid_draw(View2D *v2d, View2DGrid *grid, int flag)
{
	float vec1[2], vec2[2];
	int a, step;
	
	/* check for grid first, as it may not exist */
	if (grid == NULL)
		return;
	
	/* vertical lines */
	if (flag & V2D_VERTICAL_LINES) {
		/* initialize initial settings */
		vec1[0] = vec2[0] = grid->startx;
		vec1[1] = grid->starty;
		vec2[1] = v2d->cur.ymax;
		
		/* minor gridlines */
		step = (v2d->mask.xmax - v2d->mask.xmin + 1) / U.v2d_min_gridsize;
		UI_ThemeColor(TH_GRID);
		
		for (a = 0; a < step; a++) {
			glBegin(GL_LINE_STRIP);
			glVertex2fv(vec1);
			glVertex2fv(vec2);
			glEnd();
			
			vec2[0] = vec1[0] += grid->dx;
		}
		
		/* major gridlines */
		vec2[0] = vec1[0] -= 0.5f * grid->dx;
		UI_ThemeColorShade(TH_GRID, 16);
		
		step++;
		for (a = 0; a <= step; a++) {
			glBegin(GL_LINE_STRIP);
			glVertex2fv(vec1);
			glVertex2fv(vec2);
			glEnd();
			
			vec2[0] = vec1[0] -= grid->dx;
		}
	}
	
	/* horizontal lines */
	if (flag & V2D_HORIZONTAL_LINES) {
		/* only major gridlines */
		vec1[1] = vec2[1] = grid->starty;
		vec1[0] = grid->startx;
		vec2[0] = v2d->cur.xmax;
		
		step = (v2d->mask.ymax - v2d->mask.ymin + 1) / U.v2d_min_gridsize;
		
		UI_ThemeColor(TH_GRID);
		for (a = 0; a <= step; a++) {
			glBegin(GL_LINE_STRIP);
			glVertex2fv(vec1);
			glVertex2fv(vec2);
			glEnd();
			
			vec2[1] = vec1[1] += grid->dy;
		}
		
		/* fine grid lines */
		vec2[1] = vec1[1] -= 0.5f * grid->dy;
		step++;
		
		if (flag & V2D_HORIZONTAL_FINELINES) { 
			UI_ThemeColorShade(TH_GRID, 16);
			for (a = 0; a < step; a++) {
				glBegin(GL_LINE_STRIP);
				glVertex2fv(vec1);
				glVertex2fv(vec2);
				glEnd();
				
				vec2[1] = vec1[1] -= grid->dy;
			}
		}
	}
	
	/* Axes are drawn as darker lines */
	UI_ThemeColorShade(TH_GRID, -50);
	
	/* horizontal axis */
	if (flag & V2D_HORIZONTAL_AXIS) {
		vec1[0] = v2d->cur.xmin;
		vec2[0] = v2d->cur.xmax;
		vec1[1] = vec2[1] = 0.0f;
		
		glBegin(GL_LINE_STRIP);
		glVertex2fv(vec1);
		glVertex2fv(vec2);
		glEnd();
	}
	
	/* vertical axis */
	if (flag & V2D_VERTICAL_AXIS) {
		vec1[1] = v2d->cur.ymin;
		vec2[1] = v2d->cur.ymax;
		vec1[0] = vec2[0] = 0.0f;
		
		glBegin(GL_LINE_STRIP);
		glVertex2fv(vec1);
		glVertex2fv(vec2);
		glEnd();
	}
}

/* Draw a constant grid in given 2d-region */
void UI_view2d_constant_grid_draw(View2D *v2d)
{
	float start, step = 25.0f;

	UI_ThemeColorShade(TH_BACK, -10);
	
	start = v2d->cur.xmin - (float)fmod(v2d->cur.xmin, step);
	
	glBegin(GL_LINES);
	for (; start < v2d->cur.xmax; start += step) {
		glVertex2f(start, v2d->cur.ymin);
		glVertex2f(start, v2d->cur.ymax);
	}

	start = v2d->cur.ymin - (float)fmod(v2d->cur.ymin, step);
	for (; start < v2d->cur.ymax; start += step) {
		glVertex2f(v2d->cur.xmin, start);
		glVertex2f(v2d->cur.xmax, start);
	}
	
	/* X and Y axis */
	UI_ThemeColorShade(TH_BACK, -18);
	glVertex2f(0.0f, v2d->cur.ymin);
	glVertex2f(0.0f, v2d->cur.ymax);
	glVertex2f(v2d->cur.xmin, 0.0f);
	glVertex2f(v2d->cur.xmax, 0.0f);
	
	glEnd();
}

/* Draw a multi-level grid in given 2d-region */
void UI_view2d_multi_grid_draw(View2D *v2d, float step, int level_size, int totlevels)
{
	int offset = -10;
	float lstep = step;
	int level;
	
	for (level = 0; level < totlevels; ++level) {
		int i;
		float start;
		
		UI_ThemeColorShade(TH_BACK, offset);
		
		i = (v2d->cur.xmin >= 0.0f ? -(int)(-v2d->cur.xmin / lstep) : (int)(v2d->cur.xmin / lstep));
		start = i * lstep;
		
		glBegin(GL_LINES);
		for (; start < v2d->cur.xmax; start += lstep, ++i) {
			if (i == 0 || (level < totlevels - 1 && i % level_size == 0))
				continue;
			glVertex2f(start, v2d->cur.ymin);
			glVertex2f(start, v2d->cur.ymax);
		}
		
		i = (v2d->cur.ymin >= 0.0f ? -(int)(-v2d->cur.ymin / lstep) : (int)(v2d->cur.ymin / lstep));
		start = i * lstep;
		
		for (; start < v2d->cur.ymax; start += lstep, ++i) {
			if (i == 0 || (level < totlevels - 1 && i % level_size == 0))
				continue;
			glVertex2f(v2d->cur.xmin, start);
			glVertex2f(v2d->cur.xmax, start);
		}
		
		/* X and Y axis */
		UI_ThemeColorShade(TH_BACK, offset - 8);
		glVertex2f(0.0f, v2d->cur.ymin);
		glVertex2f(0.0f, v2d->cur.ymax);
		glVertex2f(v2d->cur.xmin, 0.0f);
		glVertex2f(v2d->cur.xmax, 0.0f);
		
		glEnd();
		
		lstep *= level_size;
		offset -= 6;
	}
}

/* the price we pay for not exposting structs :( */
void UI_view2d_grid_size(View2DGrid *grid, float *r_dx, float *r_dy)
{
	*r_dx = grid->dx;
	*r_dy = grid->dy;
}

/* free temporary memory used for drawing grid */
void UI_view2d_grid_free(View2DGrid *grid)
{
	/* only free if there's a grid */
	if (grid) {
		MEM_freeN(grid);
	}
}

/* *********************************************************************** */
/* Scrollers */

/* View2DScrollers is typedef'd in UI_view2d.h 
 * WARNING: the start of this struct must not change, as view2d_ops.c uses this too. 
 *         For now, we don't need to have a separate (internal) header for structs like this...
 */
struct View2DScrollers {
	/* focus bubbles */
	int vert_min, vert_max; /* vertical scrollbar */
	int hor_min, hor_max;   /* horizontal scrollbar */
	
	rcti hor, vert;         /* exact size of slider backdrop */
	int horfull, vertfull;  /* set if sliders are full, we don't draw them */
	
	/* scales */
	View2DGrid *grid;       /* grid for coordinate drawing */
	short xunits, xclamp;   /* units and clamping options for x-axis */
	short yunits, yclamp;   /* units and clamping options for y-axis */
};

/* Calculate relevant scroller properties */
View2DScrollers *UI_view2d_scrollers_calc(const bContext *C, View2D *v2d, short xunits, short xclamp, short yunits, short yclamp)
{
	View2DScrollers *scrollers;
	rcti vert, hor;
	float fac1, fac2, totsize, scrollsize;
	int scroll = view2d_scroll_mapped(v2d->scroll);
	
	/* scrollers is allocated here... */
	scrollers = MEM_callocN(sizeof(View2DScrollers), "View2DScrollers");
	
	vert = v2d->vert;
	hor = v2d->hor;
	
	/* slider rects need to be smaller than region */
	hor.xmin += 4;
	hor.xmax -= 4;
	if (scroll & V2D_SCROLL_BOTTOM)
		hor.ymin += 4;
	else
		hor.ymax -= 4;
	
	if (scroll & V2D_SCROLL_LEFT)
		vert.xmin += 4;
	else
		vert.xmax -= 4;
	vert.ymin += 4;
	vert.ymax -= 4;
	
	CLAMP(vert.ymin, vert.ymin, vert.ymax - V2D_SCROLLER_HANDLE_SIZE);
	CLAMP(hor.xmin, hor.xmin, hor.xmax - V2D_SCROLLER_HANDLE_SIZE);
	
	/* store in scrollers, used for drawing */
	scrollers->vert = vert;
	scrollers->hor = hor;
	
	/* scroller 'buttons':
	 *	- These should always remain within the visible region of the scrollbar
	 *	- They represent the region of 'tot' that is visible in 'cur'
	 */
	
	/* horizontal scrollers */
	if (scroll & V2D_SCROLL_HORIZONTAL) {
		/* scroller 'button' extents */
		totsize = v2d->tot.xmax - v2d->tot.xmin;
		scrollsize = (float)(hor.xmax - hor.xmin);
		if (totsize == 0.0f) totsize = 1.0f;  /* avoid divide by zero */

		fac1 = (v2d->cur.xmin - v2d->tot.xmin) / totsize;
		if (fac1 <= 0.0f)
			scrollers->hor_min = hor.xmin;
		else
			scrollers->hor_min = (int)(hor.xmin + (fac1 * scrollsize));
		
		fac2 = (v2d->cur.xmax - v2d->tot.xmin) / totsize;
		if (fac2 >= 1.0f)
			scrollers->hor_max = hor.xmax;
		else
			scrollers->hor_max = (int)(hor.xmin + (fac2 * scrollsize));
		
		/* prevent inverted sliders */
		if (scrollers->hor_min > scrollers->hor_max) 
			scrollers->hor_min = scrollers->hor_max;
		/* prevent sliders from being too small, and disappearing */
		if ((scrollers->hor_max - scrollers->hor_min) < V2D_SCROLLER_HANDLE_SIZE) {
			scrollers->hor_max = scrollers->hor_min + V2D_SCROLLER_HANDLE_SIZE;

			CLAMP(scrollers->hor_max, hor.xmin + V2D_SCROLLER_HANDLE_SIZE, hor.xmax);
			CLAMP(scrollers->hor_min, hor.xmin, hor.xmax - V2D_SCROLLER_HANDLE_SIZE);
		}
		
		/* check whether sliders can disappear due to the full-range being used */
		if (v2d->keeptot) {
			if ((fac1 <= 0.0f) && (fac2 >= 1.0f)) { 
				v2d->scroll |= V2D_SCROLL_HORIZONTAL_FULLR;
				scrollers->horfull = 1;
			}
			else
				v2d->scroll &= ~V2D_SCROLL_HORIZONTAL_FULLR;
		}
	}
	
	/* vertical scrollers */
	if (scroll & V2D_SCROLL_VERTICAL) {
		/* scroller 'button' extents */
		totsize = v2d->tot.ymax - v2d->tot.ymin;
		scrollsize = (float)(vert.ymax - vert.ymin);
		if (totsize == 0.0f) totsize = 1.0f;  /* avoid divide by zero */

		fac1 = (v2d->cur.ymin - v2d->tot.ymin) / totsize;
		if (fac1 <= 0.0f)
			scrollers->vert_min = vert.ymin;
		else
			scrollers->vert_min = (int)(vert.ymin + (fac1 * scrollsize));
		
		fac2 = (v2d->cur.ymax - v2d->tot.ymin) / totsize;
		if (fac2 >= 1.0f)
			scrollers->vert_max = vert.ymax;
		else
			scrollers->vert_max = (int)(vert.ymin + (fac2 * scrollsize));
		
		/* prevent inverted sliders */
		if (scrollers->vert_min > scrollers->vert_max) 
			scrollers->vert_min = scrollers->vert_max;
		/* prevent sliders from being too small, and disappearing */
		if ((scrollers->vert_max - scrollers->vert_min) < V2D_SCROLLER_HANDLE_SIZE) {
			
			scrollers->vert_max = scrollers->vert_min + V2D_SCROLLER_HANDLE_SIZE;
			
			CLAMP(scrollers->vert_max, vert.ymin + V2D_SCROLLER_HANDLE_SIZE, vert.ymax);
			CLAMP(scrollers->vert_min, vert.ymin, vert.ymax - V2D_SCROLLER_HANDLE_SIZE);
		}

		/* check whether sliders can disappear due to the full-range being used */
		if (v2d->keeptot) {
			if ((fac1 <= 0.0f) && (fac2 >= 1.0f)) { 
				v2d->scroll |= V2D_SCROLL_VERTICAL_FULLR;
				scrollers->vertfull = 1;
			}
			else
				v2d->scroll &= ~V2D_SCROLL_VERTICAL_FULLR;
		}
	}
	
	/* grid markings on scrollbars */
	if (scroll & (V2D_SCROLL_SCALE_HORIZONTAL | V2D_SCROLL_SCALE_VERTICAL)) {
		/* store clamping */
		scrollers->xclamp = xclamp;
		scrollers->xunits = xunits;
		scrollers->yclamp = yclamp;
		scrollers->yunits = yunits;
		
		scrollers->grid = UI_view2d_grid_calc(CTX_data_scene(C), v2d, xunits, xclamp, yunits, yclamp, (hor.xmax - hor.xmin), (vert.ymax - vert.ymin));
	}
	
	/* return scrollers */
	return scrollers;
}

/* Print scale marking along a time scrollbar */
static void scroll_printstr(Scene *scene, float x, float y, float val, int power, short unit, char dir)
{
	int len;
	char timecode_str[32];
	
	/* adjust the scale unit to work ok */
	if (dir == 'v') {
		/* here we bump up the power by factor of 10, as 
		 * rotation values (hence 'degrees') are divided by 10 to 
		 * be able to show the curves at the same time
		 */
		if (ELEM(unit, V2D_UNIT_DEGREES, V2D_UNIT_TIME)) {
			power += 1;
			val *= 10;
		}
	}
	
	/* get string to print */
	ANIM_timecode_string_from_frame(timecode_str, scene, power, (unit == V2D_UNIT_SECONDS), val);
	
	/* get length of string, and adjust printing location to fit it into the horizontal scrollbar */
	len = strlen(timecode_str);
	if (dir == 'h') {
		/* seconds/timecode display has slightly longer strings... */
		if (unit == V2D_UNIT_SECONDS)
			x -= 3 * len;
		else
			x -= 4 * len;
	}
	
	/* Add degree sympbol to end of string for vertical scrollbar? */
	if ((dir == 'v') && (unit == V2D_UNIT_DEGREES)) {
		timecode_str[len] = 186;
		timecode_str[len + 1] = 0;
	}
	
	/* draw it */
	BLF_draw_default_ascii(x, y, 0.0f, timecode_str, sizeof(timecode_str));
}

/* Draw scrollbars in the given 2d-region */
void UI_view2d_scrollers_draw(const bContext *C, View2D *v2d, View2DScrollers *vs)
{
	Scene *scene = CTX_data_scene(C);
	rcti vert, hor;
	int scroll = view2d_scroll_mapped(v2d->scroll);
	
	/* make copies of rects for less typing */
	vert = vs->vert;
	hor = vs->hor;
	
	/* horizontal scrollbar */
	if (scroll & V2D_SCROLL_HORIZONTAL) {
		/* only draw scrollbar when it doesn't fill the entire space */
		if (vs->horfull == 0) {
			bTheme *btheme = UI_GetTheme();
			uiWidgetColors wcol = btheme->tui.wcol_scroll;
			rcti slider;
			int state;
			
			slider.xmin = vs->hor_min;
			slider.xmax = vs->hor_max;
			slider.ymin = hor.ymin;
			slider.ymax = hor.ymax;
			
			state = (v2d->scroll_ui & V2D_SCROLL_H_ACTIVE) ? UI_SCROLL_PRESSED : 0;
			
			/* show zoom handles if:
			 *	- zooming on x-axis is allowed (no scroll otherwise)
			 *	- slider bubble is large enough (no overdraw confusion)
			 *	- scale is shown on the scroller 
			 *	  (workaround to make sure that button windows don't show these, 
			 *		and only the time-grids with their zoomability can do so)
			 */
			if ((v2d->keepzoom & V2D_LOCKZOOM_X) == 0 &&
			    (v2d->scroll & V2D_SCROLL_SCALE_HORIZONTAL) &&
			    (slider.xmax - slider.xmin > V2D_SCROLLER_HANDLE_SIZE))
			{
				state |= UI_SCROLL_ARROWS;
			}
			
			UI_ThemeColor(TH_BACK);
			glRecti(v2d->hor.xmin, v2d->hor.ymin, v2d->hor.xmax, v2d->hor.ymax);
			
			uiWidgetScrollDraw(&wcol, &hor, &slider, state);
		}
		
		/* scale indicators */
		if ((scroll & V2D_SCROLL_SCALE_HORIZONTAL) && (vs->grid)) {
			View2DGrid *grid = vs->grid;
			float fac, dfac, fac2, val;
			
			/* the numbers: convert grid->startx and -dx to scroll coordinates 
			 *	- fac is x-coordinate to draw to
			 *	- dfac is gap between scale markings
			 */
			fac = (grid->startx - v2d->cur.xmin) / (v2d->cur.xmax - v2d->cur.xmin);
			fac = (float)hor.xmin + fac * (hor.xmax - hor.xmin);
			
			dfac = (grid->dx) / (v2d->cur.xmax - v2d->cur.xmin);
			dfac = dfac * (hor.xmax - hor.xmin);
			
			/* set starting value, and text color */
			UI_ThemeColor(TH_TEXT);
			val = grid->startx;
			
			/* if we're clamping to whole numbers only, make sure entries won't be repeated */
			if (vs->xclamp == V2D_GRID_CLAMP) {
				while (grid->dx < 0.9999f) {
					grid->dx *= 2.0f;
					dfac *= 2.0f;
				}
			}
			if (vs->xunits == V2D_UNIT_FRAMES)
				grid->powerx = 1;
			
			/* draw numbers in the appropriate range */
			if (dfac > 0.0f) {
				float h = 2.0f + (float)(hor.ymin);
				
				for (; fac < hor.xmax - 10; fac += dfac, val += grid->dx) {
					
					/* make prints look nicer for scrollers */
					if (fac < hor.xmin + 10)
						continue;
					
					switch (vs->xunits) {							
						case V2D_UNIT_FRAMES:       /* frames (as whole numbers)*/
							scroll_printstr(scene, fac, h, val, grid->powerx, V2D_UNIT_FRAMES, 'h');
							break;
							
						case V2D_UNIT_FRAMESCALE:   /* frames (not always as whole numbers) */
							scroll_printstr(scene, fac, h, val, grid->powerx, V2D_UNIT_FRAMESCALE, 'h');
							break;
						
						case V2D_UNIT_SECONDS:      /* seconds */
							fac2 = val / (float)FPS;
							scroll_printstr(scene, fac, h, fac2, grid->powerx, V2D_UNIT_SECONDS, 'h');
							break;
							
						case V2D_UNIT_SECONDSSEQ:   /* seconds with special calculations (only used for sequencer only) */
						{
							float time;
							
							fac2 = val / (float)FPS;
							time = (float)floor(fac2);
							fac2 = fac2 - time;
							
							scroll_printstr(scene, fac, h, time + (float)FPS * fac2 / 100.0f, grid->powerx, V2D_UNIT_SECONDSSEQ, 'h');
						}
						break;
							
						case V2D_UNIT_DEGREES:      /* Graph Editor for rotation Drivers */
							/* HACK: although we're drawing horizontal, we make this draw as 'vertical', just to get degree signs */
							scroll_printstr(scene, fac, h, val, grid->powerx, V2D_UNIT_DEGREES, 'v');
							break;
					}
				}
			}
		}
	}
	
	/* vertical scrollbar */
	if (scroll & V2D_SCROLL_VERTICAL) {
		/* only draw scrollbar when it doesn't fill the entire space */
		if (vs->vertfull == 0) {
			bTheme *btheme = UI_GetTheme();
			uiWidgetColors wcol = btheme->tui.wcol_scroll;
			rcti slider;
			int state;
			
			slider.xmin = vert.xmin;
			slider.xmax = vert.xmax;
			slider.ymin = vs->vert_min;
			slider.ymax = vs->vert_max;
			
			state = (v2d->scroll_ui & V2D_SCROLL_V_ACTIVE) ? UI_SCROLL_PRESSED : 0;
			
			/* show zoom handles if:
			 *	- zooming on y-axis is allowed (no scroll otherwise)
			 *	- slider bubble is large enough (no overdraw confusion)
			 *	- scale is shown on the scroller 
			 *	  (workaround to make sure that button windows don't show these, 
			 *		and only the time-grids with their zoomability can do so)
			 */
			if ((v2d->keepzoom & V2D_LOCKZOOM_Y) == 0 &&
			    (v2d->scroll & V2D_SCROLL_SCALE_VERTICAL) &&
			    (slider.ymax - slider.ymin > V2D_SCROLLER_HANDLE_SIZE))
			{
				state |= UI_SCROLL_ARROWS;
			}
				
			UI_ThemeColor(TH_BACK);
			glRecti(v2d->vert.xmin, v2d->vert.ymin, v2d->vert.xmax, v2d->vert.ymax);
			
			uiWidgetScrollDraw(&wcol, &vert, &slider, state);
		}
		
		
		/* scale indiators */
		if ((scroll & V2D_SCROLL_SCALE_VERTICAL) && (vs->grid)) {
			View2DGrid *grid = vs->grid;
			float fac, dfac, val;
			
			/* the numbers: convert grid->starty and dy to scroll coordinates 
			 *	- fac is y-coordinate to draw to
			 *	- dfac is gap between scale markings
			 *	- these involve a correction for horizontal scrollbar
			 *	  NOTE: it's assumed that that scrollbar is there if this is involved!
			 */
			fac = (grid->starty - v2d->cur.ymin) / (v2d->cur.ymax - v2d->cur.ymin);
			fac = vert.ymin + fac * (vert.ymax - vert.ymin);
			
			dfac = (grid->dy) / (v2d->cur.ymax - v2d->cur.ymin);
			dfac = dfac * (vert.ymax - vert.ymin);
			
			/* set starting value, and text color */
			UI_ThemeColor(TH_TEXT);
			val = grid->starty;
			
			/* if vertical clamping (to whole numbers) is used (i.e. in Sequencer), apply correction */
			if (vs->yclamp == V2D_GRID_CLAMP)
				fac += 0.5f * dfac;
				
			/* draw vertical steps */
			if (dfac > 0.0f) {
				
				BLF_rotation_default(90.0f);
				BLF_enable_default(BLF_ROTATION);

				for (; fac < vert.ymax - 10; fac += dfac, val += grid->dy) {
					
					/* make prints look nicer for scrollers */
					if (fac < vert.ymin + 10)
						continue;
					
					scroll_printstr(scene, (float)(vert.xmax) - 2.0f, fac, val, grid->powery, vs->yunits, 'v');
				}
				
				BLF_disable_default(BLF_ROTATION);
			}
		}	
	}
	
}

/* free temporary memory used for drawing scrollers */
void UI_view2d_scrollers_free(View2DScrollers *scrollers)
{
	/* need to free grid as well... */
	if (scrollers->grid) {
		MEM_freeN(scrollers->grid);
	}
	MEM_freeN(scrollers);
}

/* *********************************************************************** */
/* List View Utilities */

/* Get the view-coordinates of the nominated cell 
 *	- columnwidth, rowheight	= size of each 'cell'
 *	- startx, starty			= coordinates (in 'tot' rect space) that the list starts from
 *							  This should be (0,0) for most views. However, for those where the starting row was offsetted
 *							  (like for Animation Editor channel lists, to make the first entry more visible), these will be 
 *							  the min-coordinates of the first item.
 *	- column, row				= the 2d-corodinates (in 2D-view / 'tot' rect space) the cell exists at
 *	- rect					= coordinates of the cell (passed as single var instead of 4 separate, as it's more useful this way)
 */
void UI_view2d_listview_cell_to_view(View2D *v2d, short columnwidth, short rowheight, float startx, float starty, int column, int row, rctf *rect)
{
	/* sanity checks */
	if (ELEM(NULL, v2d, rect)) {
		return;
	}

	if ((columnwidth <= 0) && (rowheight <= 0)) {
		rect->xmin = rect->xmax = 0.0f;
		rect->ymin = rect->ymax = 0.0f;
		return;
	}
	
	/* x-coordinates */
	rect->xmin = startx + (float)(columnwidth * column);
	rect->xmax = startx + (float)(columnwidth * (column + 1));
	
	if ((v2d->align & V2D_ALIGN_NO_POS_X) && !(v2d->align & V2D_ALIGN_NO_NEG_X)) {
		/* simply negate the values for the coordinates if in negative half */
		rect->xmin = -rect->xmin;
		rect->xmax = -rect->xmax;
	}
	
	/* y-coordinates */
	rect->ymin = starty + (float)(rowheight * row);
	rect->ymax = starty + (float)(rowheight * (row + 1));
	
	if ((v2d->align & V2D_ALIGN_NO_POS_Y) && !(v2d->align & V2D_ALIGN_NO_NEG_Y)) {
		/* simply negate the values for the coordinates if in negative half */
		rect->ymin = -rect->ymin;
		rect->ymax = -rect->ymax;
	}
}

/* Get the 'cell' (row, column) that the given 2D-view coordinates (i.e. in 'tot' rect space) lie in.
 *	- columnwidth, rowheight	= size of each 'cell'
 *	- startx, starty			= coordinates (in 'tot' rect space) that the list starts from
 *							  This should be (0,0) for most views. However, for those where the starting row was offsetted
 *							  (like for Animation Editor channel lists, to make the first entry more visible), these will be 
 *							  the min-coordinates of the first item.
 *	- viewx, viewy			= 2D-coordinates (in 2D-view / 'tot' rect space) to get the cell for
 *	- column, row				= the 'coordinates' of the relevant 'cell'
 */
void UI_view2d_listview_view_to_cell(View2D *v2d, short columnwidth, short rowheight, float startx, float starty, 
                                     float viewx, float viewy, int *column, int *row)
{
	/* adjust view coordinates to be all positive ints, corrected for the start offset */
	const int x = (int)(floorf(fabsf(viewx) + 0.5f) - startx);
	const int y = (int)(floorf(fabsf(viewy) + 0.5f) - starty);
	
	/* sizes must not be negative */
	if ( (v2d == NULL) || ((columnwidth <= 0) && (rowheight <= 0)) ) {
		if (column) *column = 0;
		if (row) *row = 0;
		
		return;
	}
	
	/* get column */
	if ((column) && (columnwidth > 0))
		*column = x / columnwidth;
	else if (column)
		*column = 0;
	
	/* get row */
	if ((row) && (rowheight > 0))
		*row = y / rowheight;
	else if (row)
		*row = 0;
}

/* Get the 'extreme' (min/max) column and row indices which are visible within the 'cur' rect 
 *	- columnwidth, rowheight	= size of each 'cell'
 *	- startx, starty			= coordinates that the list starts from, which should be (0,0) for most views
 *	- column/row_min/max		= the starting and ending column/row indices
 */
void UI_view2d_listview_visible_cells(View2D *v2d, short columnwidth, short rowheight, float startx, float starty, 
                                      int *column_min, int *column_max, int *row_min, int *row_max)
{
	/* using 'cur' rect coordinates, call the cell-getting function to get the cells for this */
	if (v2d) {
		/* min */
		UI_view2d_listview_view_to_cell(v2d, columnwidth, rowheight, startx, starty, 
		                                v2d->cur.xmin, v2d->cur.ymin, column_min, row_min);
					
		/* max*/
		UI_view2d_listview_view_to_cell(v2d, columnwidth, rowheight, startx, starty, 
		                                v2d->cur.xmax, v2d->cur.ymax, column_max, row_max);
	}
}

/* *********************************************************************** */
/* Coordinate Conversions */

/* Convert from screen/region space to 2d-View space 
 *	
 *	- x,y           = coordinates to convert
 *	- viewx,viewy		= resultant coordinates
 */
void UI_view2d_region_to_view(View2D *v2d, int x, int y, float *r_viewx, float *r_viewy)
{
	float div, ofs;

	if (r_viewx) {
		div = (float)(v2d->mask.xmax - v2d->mask.xmin);
		ofs = (float)v2d->mask.xmin;
		
		*r_viewx = v2d->cur.xmin + (v2d->cur.xmax - v2d->cur.xmin) * ((float)x - ofs) / div;
	}

	if (r_viewy) {
		div = (float)(v2d->mask.ymax - v2d->mask.ymin);
		ofs = (float)v2d->mask.ymin;
		
		*r_viewy = v2d->cur.ymin + (v2d->cur.ymax - v2d->cur.ymin) * ((float)y - ofs) / div;
	}
}

/* Convert from 2d-View space to screen/region space
 *	- Coordinates are clamped to lie within bounds of region
 *
 *	- x,y               = coordinates to convert
 *	- regionx,regiony   = resultant coordinates
 */
void UI_view2d_view_to_region(View2D *v2d, float x, float y, int *regionx, int *regiony)
{
	/* set initial value in case coordinate lies outside of bounds */
	if (regionx)
		*regionx = V2D_IS_CLIPPED;
	if (regiony)
		*regiony = V2D_IS_CLIPPED;
	
	/* express given coordinates as proportional values */
	x = (x - v2d->cur.xmin) / (v2d->cur.xmax - v2d->cur.xmin);
	y = (y - v2d->cur.ymin) / (v2d->cur.ymax - v2d->cur.ymin);
	
	/* check if values are within bounds */
	if ((x >= 0.0f) && (x <= 1.0f) && (y >= 0.0f) && (y <= 1.0f)) {
		if (regionx)
			*regionx = (int)(v2d->mask.xmin + x * (v2d->mask.xmax - v2d->mask.xmin));
		if (regiony)
			*regiony = (int)(v2d->mask.ymin + y * (v2d->mask.ymax - v2d->mask.ymin));
	}
}

/* Convert from 2d-view space to screen/region space
 *	- Coordinates are NOT clamped to lie within bounds of region
 *
 *	- x,y               = coordinates to convert
 *	- regionx,regiony   = resultant coordinates
 */
void UI_view2d_to_region_no_clip(View2D *v2d, float x, float y, int *regionx, int *regiony)
{
	/* step 1: express given coordinates as proportional values */
	x = (x - v2d->cur.xmin) / (v2d->cur.xmax - v2d->cur.xmin);
	y = (y - v2d->cur.ymin) / (v2d->cur.ymax - v2d->cur.ymin);
	
	/* step 2: convert proportional distances to screen coordinates  */
	x = v2d->mask.xmin + x * (v2d->mask.xmax - v2d->mask.xmin);
	y = v2d->mask.ymin + y * (v2d->mask.ymax - v2d->mask.ymin);
	
	/* although we don't clamp to lie within region bounds, we must avoid exceeding size of ints */
	if (regionx) {
		if (x < INT_MIN) *regionx = INT_MIN;
		else if (x > INT_MAX) *regionx = INT_MAX;
		else *regionx = (int)x;
	}
	if (regiony) {
		if (y < INT_MIN) *regiony = INT_MIN;
		else if (y > INT_MAX) *regiony = INT_MAX;
		else *regiony = (int)y;
	}
}

/* *********************************************************************** */
/* Utilities */

/* View2D data by default resides in region, so get from region stored in context */
View2D *UI_view2d_fromcontext(const bContext *C)
{
	ScrArea *area = CTX_wm_area(C);
	ARegion *region = CTX_wm_region(C);

	if (area == NULL) return NULL;
	if (region == NULL) return NULL;
	return &(region->v2d);
}

/* same as above, but it returns regionwindow. Utility for pulldowns or buttons */
View2D *UI_view2d_fromcontext_rwin(const bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *region = CTX_wm_region(C);

	if (sa == NULL) return NULL;
	if (region == NULL) return NULL;
	if (region->regiontype != RGN_TYPE_WINDOW) {
		ARegion *ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
		return ar ? &(ar->v2d) : NULL;
	}
	return &(region->v2d);
}


/* Calculate the scale per-axis of the drawing-area
 *	- Is used to inverse correct drawing of icons, etc. that need to follow view 
 *	  but not be affected by scale
 *
 *	- x,y	= scale on each axis
 */
void UI_view2d_getscale(View2D *v2d, float *x, float *y) 
{
	if (x) *x = (v2d->mask.xmax - v2d->mask.xmin) / (v2d->cur.xmax - v2d->cur.xmin);
	if (y) *y = (v2d->mask.ymax - v2d->mask.ymin) / (v2d->cur.ymax - v2d->cur.ymin);
}

/* Check if mouse is within scrollers
 *	- Returns appropriate code for match
 *		'h' = in horizontal scroller
 *		'v' = in vertical scroller
 *		0 = not in scroller
 *	
 *	- x,y	= mouse coordinates in screen (not region) space
 */
short UI_view2d_mouse_in_scrollers(const bContext *C, View2D *v2d, int x, int y)
{
	ARegion *ar = CTX_wm_region(C);
	int co[2];
	int scroll = view2d_scroll_mapped(v2d->scroll);
	
	/* clamp x,y to region-coordinates first */
	co[0] = x - ar->winrct.xmin;
	co[1] = y - ar->winrct.ymin;
	
	/* check if within scrollbars */
	if (scroll & V2D_SCROLL_HORIZONTAL) {
		if (IN_2D_HORIZ_SCROLL(v2d, co)) return 'h';
	}
	if (scroll & V2D_SCROLL_VERTICAL) {
		if (IN_2D_VERT_SCROLL(v2d, co)) return 'v';
	}	
	
	/* not found */
	return 0;
}

/* ******************* view2d text drawing cache ******************** */

/* assumes caches are used correctly, so for time being no local storage in v2d */
static ListBase strings = {NULL, NULL};

typedef struct View2DString {
	struct View2DString *next, *prev;
	union {
		unsigned char ub[4];
		int pack;
	} col;
	int mval[2];
	rcti rect;
} View2DString;


void UI_view2d_text_cache_add(View2D *v2d, float x, float y, const char *str, const char col[4])
{
	int mval[2];
	
	UI_view2d_view_to_region(v2d, x, y, mval, mval + 1);
	
	if (mval[0] != V2D_IS_CLIPPED && mval[1] != V2D_IS_CLIPPED) {
		int len = strlen(str) + 1;
		/* use calloc, rect has to be zeroe'd */
		View2DString *v2s = MEM_callocN(sizeof(View2DString) + len, "View2DString");
		char *v2s_str = (char *)(v2s + 1);
		memcpy(v2s_str, str, len);

		BLI_addtail(&strings, v2s);
		v2s->col.pack = *((int *)col);
		v2s->mval[0] = mval[0];
		v2s->mval[1] = mval[1];
	}
}

/* no clip (yet) */
void UI_view2d_text_cache_rectf(View2D *v2d, rctf *rect, const char *str, const char col[4])
{
	int len = strlen(str) + 1;
	View2DString *v2s = MEM_callocN(sizeof(View2DString) + len, "View2DString");
	char *v2s_str = (char *)(v2s + 1);
	memcpy(v2s_str, str, len);

	UI_view2d_to_region_no_clip(v2d, rect->xmin, rect->ymin, &v2s->rect.xmin, &v2s->rect.ymin);
	UI_view2d_to_region_no_clip(v2d, rect->xmax, rect->ymax, &v2s->rect.xmax, &v2s->rect.ymax);

	v2s->col.pack = *((int *)col);
	v2s->mval[0] = v2s->rect.xmin;
	v2s->mval[1] = v2s->rect.ymin;

	BLI_addtail(&strings, v2s);
}


void UI_view2d_text_cache_draw(ARegion *ar)
{
	View2DString *v2s;
	int col_pack_prev = 0;

	/* investigate using BLF_ascender() */
	const float default_height = strings.first ? BLF_height_default("28") : 0.0f;
	
	// glMatrixMode(GL_PROJECTION);
	// glPushMatrix();
	// glMatrixMode(GL_MODELVIEW);
	// glPushMatrix();
	ED_region_pixelspace(ar);

	for (v2s = strings.first; v2s; v2s = v2s->next) {
		const char *str = (const char *)(v2s + 1);
		int xofs = 0, yofs;

		yofs = ceil(0.5f * (v2s->rect.ymax - v2s->rect.ymin - default_height));
		if (yofs < 1) yofs = 1;

		if (col_pack_prev != v2s->col.pack) {
			glColor3ubv(v2s->col.ub);
			col_pack_prev = v2s->col.pack;
		}

		if (v2s->rect.xmin >= v2s->rect.xmax)
			BLF_draw_default((float)v2s->mval[0] + xofs, (float)v2s->mval[1] + yofs, 0.0, str, BLF_DRAW_STR_DUMMY_MAX);
		else {
			BLF_clipping_default(v2s->rect.xmin - 4, v2s->rect.ymin - 4, v2s->rect.xmax + 4, v2s->rect.ymax + 4);
			BLF_enable_default(BLF_CLIPPING);
			BLF_draw_default(v2s->rect.xmin + xofs, v2s->rect.ymin + yofs, 0.0f, str, BLF_DRAW_STR_DUMMY_MAX);
			BLF_disable_default(BLF_CLIPPING);
		}
	}
	
	// glMatrixMode(GL_PROJECTION);
	// glPopMatrix();
	// glMatrixMode(GL_MODELVIEW);
	// glPopMatrix();
	
	if (strings.first) 
		BLI_freelistN(&strings);
}


/* ******************************************************** */



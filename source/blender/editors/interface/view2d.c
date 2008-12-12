/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"

#include "BLI_blenlib.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "WM_api.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_screen.h"

#include "UI_resources.h"
#include "UI_text.h"
#include "UI_view2d.h"

#include "UI_interface.h"
#include "interface.h"

/* *********************************************************************** */
/* Refresh and Validation */

/* Adjust mask size in response to view size changes 
 *	- This should only be called in region init() callbacks, which are
 * 	  called when the region is resized or area changes...
 */
// XXX pre2.5 -> this used to be called  calc_scrollrcts()
void UI_view2d_size_update(View2D *v2d, int winx, int winy)
{
	v2d->winx= winx;
	v2d->winy= winy;
	
	/* mask - view frame */
	v2d->mask.xmin= v2d->mask.ymin= 0;
	v2d->mask.xmax= winx - 1;	/* -1 yes! masks are pixels */
	v2d->mask.ymax= winy - 1;
	
	/* scrollbars shrink mask area, but should be based off regionsize 
	 *	- they can only be on one edge of the region they define
	 */
	if (v2d->scroll) {
		/* vertical scrollbar */
		if (v2d->scroll & V2D_SCROLL_LEFT) {
			/* on left-hand edge of region */
			v2d->vert= v2d->mask;
			v2d->vert.xmax= V2D_SCROLL_WIDTH;
			v2d->mask.xmin= v2d->vert.xmax + 1;
		}
		else if (v2d->scroll & V2D_SCROLL_RIGHT) {
			/* on right-hand edge of region */
			v2d->vert= v2d->mask;
			v2d->vert.xmin= v2d->vert.xmax - V2D_SCROLL_WIDTH;
			v2d->mask.xmax= v2d->vert.xmin - 1;
		}
		
		/* horizontal scrollbar */
		if (v2d->scroll & (V2D_SCROLL_BOTTOM|V2D_SCROLL_BOTTOM_O)) {
			/* on bottom edge of region (V2D_SCROLL_BOTTOM_O is outliner, the ohter is for standard) */
			v2d->hor= v2d->mask;
			v2d->hor.ymax= V2D_SCROLL_HEIGHT;
			v2d->mask.ymin= v2d->hor.ymax + 1;
		}
		else if (v2d->scroll & V2D_SCROLL_TOP) {
			/* on upper edge of region */
			v2d->hor= v2d->mask;
			v2d->hor.ymin= v2d->hor.ymax - V2D_SCROLL_HEIGHT;
			v2d->mask.ymax= v2d->hor.ymin - 1;
		}
	}
	
	/* cope with unitialized veriables for simple cases, like header or outliner */
	if(v2d->tot.xmin==v2d->tot.xmax || v2d->cur.xmin==v2d->cur.xmax) {
		if(v2d->keepzoom) {
			BLI_init_rctf(&v2d->tot, v2d->mask.xmin, v2d->mask.xmax, v2d->mask.ymin, v2d->mask.ymax);
			BLI_init_rctf(&v2d->cur, v2d->mask.xmin, v2d->mask.xmax, v2d->mask.ymin, v2d->mask.ymax);
			
			v2d->min[0]= v2d->max[0]= winx;
			v2d->min[1]= v2d->max[1]= winy;
			v2d->minzoom= 1.0f;
			v2d->maxzoom= 1.0f;
		}
	}
	
	/* make sure that 'cur' rect is in a valid state as a result of these changes */
	UI_view2d_curRect_validate(v2d);
}

/* Ensure View2D rects remain in a viable configuration 
 *	- cur is not allowed to be: larger than max, smaller than min, or outside of tot
 */
// XXX pre2.5 -> this used to be called  test_view2d()
void UI_view2d_curRect_validate(View2D *v2d)
{
	float totwidth, totheight, curwidth, curheight, width, height;
	float winx, winy;
	rctf *cur, *tot;
	
	/* use mask as size of region that View2D resides in, as it takes into account scrollbars already  */
	winx= (float)(v2d->mask.xmax - v2d->mask.xmin + 1);
	winy= (float)(v2d->mask.ymax - v2d->mask.ymin + 1);
	
	/* get pointers to rcts for less typing */
	cur= &v2d->cur;
	tot= &v2d->tot;
	
	/* we must satisfy the following constraints (in decreasing order of importance):
	 *	- cur must not fall outside of tot
	 *	- axis locks (zoom and offset) must be maintained
	 *	- zoom must not be excessive (check either sizes or zoom values)
	 *	- aspect ratio should be respected (NOTE: this is quite closely realted to zoom too)
	 */
	
	/* Step 1: if keepzoom, adjust the sizes of the rects only
	 *	- firstly, we calculate the sizes of the rects
	 *	- curwidth and curheight are saved as reference... modify width and height values here
	 */
	totwidth= tot->xmax - tot->xmin;
	totheight= tot->ymax - tot->ymin;
	curwidth= width= cur->xmax - cur->xmin;
	curheight= height= cur->ymax - cur->ymin;
	
	/* if zoom is locked, size on the appropriate axis is reset to mask size */
	if (v2d->keepzoom & V2D_LOCKZOOM_X)
		width= winx;
	if (v2d->keepzoom & V2D_LOCKZOOM_Y)
		height= winy;
		
	/* keepzoom (V2D_KEEPZOOM set), indicates that zoom level on each axis must not exceed limits 
	 * NOTE: in general, it is not expected that the lock-zoom will be used in conjunction with this
	 */
	if (v2d->keepzoom & V2D_KEEPZOOM) {
		float zoom, fac;
		
		/* check if excessive zoom on x-axis */
		if ((v2d->keepzoom & V2D_LOCKZOOM_X)==0) {
			zoom= winx / width;
			if ((zoom < v2d->minzoom) || (zoom > v2d->maxzoom)) {
				fac= (zoom < v2d->minzoom) ? (zoom / v2d->minzoom) : (zoom / v2d->maxzoom);
				width *= fac;
			}
		}
		
		/* check if excessive zoom on y-axis */
		if ((v2d->keepzoom & V2D_LOCKZOOM_Y)==0) {
			zoom= winy / height;
			if ((zoom < v2d->minzoom) || (zoom > v2d->maxzoom)) {
				fac= (zoom < v2d->minzoom) ? (zoom / v2d->minzoom) : (zoom / v2d->maxzoom);
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
	if (v2d->keepaspect) {
		short do_x=0, do_y=0, do_cur, do_win;
		float curRatio, winRatio;
		
		/* when a window edge changes, the aspect ratio can't be used to
		 * find which is the best new 'cur' rect. thats why it stores 'old' 
		 */
		if (winx != v2d->oldwinx) do_x= 1;
		if (winy != v2d->oldwiny) do_y= 1;
		
		curRatio= height / width;
		winRatio= winy / winx;
		
		/* both sizes change (area/region maximised)  */
		if (do_x == do_y) {
			if (do_x && do_y) {
				/* here is 1,1 case, so all others must be 0,0 */
				if (ABS(winx - v2d->oldwinx) > ABS(winy - v2d->oldwiny)) do_y= 0;
				else do_x= 0;
			}
			else if (winRatio > 1.0f) do_x= 0; 
			else do_x= 1;
		}
		do_cur= do_x;
		do_win= do_y;
		
		if (do_cur) {
			if ((v2d->keeptot == 2) && (winx != v2d->oldwinx)) {
				/* special exception for Outliner (and later channel-lists):
				 * 	- The view may be moved left to avoid contents being pushed out of view when view shrinks. 
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
				width= height / winRatio;
			}
		}
		else {
			/* landscape window: correct for y */
			height = width * winRatio;
		}
		
		/* store region size for next time */
		v2d->oldwinx= winx; 
		v2d->oldwiny= winy;
	}
	
	/* Step 2: apply new sizes of cur rect to cur rect */
	if ((width != curwidth) || (height != curheight)) {
		float temp, dh;
		
		/* resize around 'center' of frame */
		if (width != curwidth) {
			temp= (cur->xmax + cur->xmin) * 0.5f;
			dh= width * 0.5f;
			
			cur->xmin = temp - dh;
			cur->xmax = temp + dh;
		}
		if (height != curheight) {
			temp= (cur->ymax + cur->ymin) * 0.5f;
			dh= height * 0.5f;
			
			cur->ymin = temp - dh;
			cur->ymax = temp + dh;
		}
	}
	
	/* Step 3: adjust so that it doesn't fall outside of bounds of tot */
	if (v2d->keeptot) {
		float temp, diff;
		
		/* recalculate extents of cur */
		curwidth= cur->xmax - cur->xmin;
		curheight= cur->ymax - cur->ymin;
		
		/* width */
		if ( (curwidth > totwidth) && !(v2d->keepzoom & (V2D_KEEPZOOM|V2D_LOCKZOOM_X)) ) {
			/* if zoom doesn't have to be maintained, just clamp edges */
			if (cur->xmin < tot->xmin) cur->xmin= tot->xmin;
			if (cur->xmax > tot->xmax) cur->xmax= tot->xmax;
		}
		else if (v2d->keeptot == 2) {
			/* This is an exception for the outliner (and later channel-lists) 
			 *	- must clamp within tot rect (absolutely no excuses)
			 *	--> therefore, cur->xmin must not be less than tot->xmin
			 */
			if (cur->xmin < tot->xmin) {
				/* move cur across so that it sits at minimum of tot */
				temp= tot->xmin - cur->xmin;
				
				cur->xmin += temp;
				cur->xmax += temp;
			}
			else if (cur->xmax > tot->xmax) {
				/* - only offset by difference of cur-xmax and tot-xmax if that would not move 
				 * 	cur-xmin to lie past tot-xmin
				 * - otherwise, simply shift to tot-xmin???
				 */
				temp= cur->xmax - tot->xmax;
				
				if ((cur->xmin - temp) < tot->xmin) {
					/* only offset by difference from cur-min and tot-min */
					temp= cur->xmin - tot->xmin;
					
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
			 * 	- width too big, but maintaining zoom (i.e. widths cannot be changed)
			 *	- width is OK, but need to check if outside of boundaries
			 * 
			 * So, resolution is to just shift view by the gap between the extremities.
			 * We favour moving the 'minimum' across, as that's origin for most things
			 * (XXX - in the past, max was favoured... if there are bugs, swap!)
			 */
			if ((cur->ymin < tot->ymin) && (cur->ymax > tot->ymax)) {
				/* outside boundaries on both sides, so take middle-point of tot, and place in balanced way */
				temp= (tot->ymax + tot->ymin) * 0.5f;
				diff= curheight * 0.5f;
				
				cur->ymin= temp - diff;
				cur->ymax= temp + diff;
			}
			else if (cur->xmin > tot->xmin) {
				/* there's still space remaining, so shift left */
				temp= cur->xmin - tot->xmin;
				
				cur->xmin -= temp;
				cur->xmax -= temp;
			}
			else if (cur->xmax < tot->xmax) {
				/* there's still space remaining, so shift right */
				temp= tot->xmax - cur->xmax;
				
				cur->xmin += temp;
				cur->xmax += temp;
			}
		}
		
		/* height */
		if ( (curheight > totheight) && !(v2d->keepzoom & (V2D_KEEPZOOM|V2D_LOCKZOOM_Y)) ) {
			/* if zoom doesn't have to be maintained, just clamp edges */
			if (cur->ymin < tot->ymin) cur->ymin= tot->ymin;
			if (cur->ymax > tot->ymax) cur->ymax= tot->ymax;
		}
		else {
			/* This here occurs when:
			 * 	- height too big, but maintaining zoom (i.e. heights cannot be changed)
			 *	- height is OK, but need to check if outside of boundaries
			 * 
			 * So, resolution is to just shift view by the gap between the extremities.
			 * We favour moving the 'minimum' across, as that's origin for most things
			 */
			if ((cur->ymin < tot->ymin) && (cur->ymax > tot->ymax)) {
				/* outside boundaries on both sides, so take middle-point of tot, and place in balanced way */
				temp= (tot->ymax + tot->ymin) * 0.5f;
				diff= curheight * 0.5f;
				
				cur->ymin= temp - diff;
				cur->ymax= temp + diff;
			}
			else if (cur->ymin < tot->ymin) {
				/* there's still space remaining, so shift up */
				temp= tot->ymin - cur->ymin;
				
				cur->ymin += temp;
				cur->ymax += temp;
			}
			else if (cur->ymax > tot->ymax) {
				/* there's still space remaining, so shift down */
				temp= cur->ymax - tot->ymax;
				
				cur->ymin -= temp;
				cur->ymax -= temp;
			}
		}
	}
	
}

/* ------------------ */

/* Restore 'cur' rect to standard orientation (i.e. optimal maximum view of tot) */
void UI_view2d_curRect_reset (View2D *v2d)
{
	float width, height;
	
	/* assume width and height of 'cur' rect by default, should be same size as mask */
	width= (float)(v2d->mask.xmax - v2d->mask.xmin + 1);
	height= (float)(v2d->mask.ymax - v2d->mask.ymin + 1);
	
	/* handle width - posx and negx flags are mutually exclusive, so watch out */
	if ((v2d->align & V2D_ALIGN_NO_POS_X) && !(v2d->align & V2D_ALIGN_NO_NEG_X)) {
		/* width is in negative-x half */
		v2d->cur.xmin= (float)-width;
		v2d->cur.xmax= 0.0f;
	}
	else if ((v2d->align & V2D_ALIGN_NO_NEG_X) && !(v2d->align & V2D_ALIGN_NO_POS_X)) {
		/* width is in positive-x half */
		v2d->cur.xmin= 0.0f;
		v2d->cur.xmax= (float)width;
	}
	else {
		/* width is centered around x==0 */
		const float dx= (float)width / 2.0f;
		
		v2d->cur.xmin= -dx;
		v2d->cur.xmax= dx;
	}
	
	/* handle height - posx and negx flags are mutually exclusive, so watch out */
	if ((v2d->align & V2D_ALIGN_NO_POS_Y) && !(v2d->align & V2D_ALIGN_NO_NEG_Y)) {
		/* height is in negative-y half */
		v2d->cur.ymin= (float)-height;
		v2d->cur.ymax= 0.0f;
	}
	else if ((v2d->align & V2D_ALIGN_NO_NEG_Y) && !(v2d->align & V2D_ALIGN_NO_POS_Y)) {
		/* height is in positive-y half */
		v2d->cur.ymin= 0.0f;
		v2d->cur.ymax= (float)height;
	}
	else {
		/* height is centered around y==0 */
		const float dy= (float)height / 2.0f;
		
		v2d->cur.ymin= -dy;
		v2d->cur.ymax= dy;
	}
}

/* ------------------ */

/* Change the size of the maximum viewable area (i.e. 'tot' rect) */
void UI_view2d_totRect_set (View2D *v2d, int width, int height)
{
	/* don't do anything if either value is 0 */
	if (ELEM3(0, v2d, width, height))
		return;
	
	/* handle width - posx and negx flags are mutually exclusive, so watch out */
	if ((v2d->align & V2D_ALIGN_NO_POS_X) && !(v2d->align & V2D_ALIGN_NO_NEG_X)) {
		/* width is in negative-x half */
		v2d->tot.xmin= (float)-width;
		v2d->tot.xmax= 0.0f;
	}
	else if ((v2d->align & V2D_ALIGN_NO_NEG_X) && !(v2d->align & V2D_ALIGN_NO_POS_X)) {
		/* width is in positive-x half */
		v2d->tot.xmin= 0.0f;
		v2d->tot.xmax= (float)width;
	}
	else {
		/* width is centered around x==0 */
		const float dx= (float)width / 2.0f;
		
		v2d->tot.xmin= -dx;
		v2d->tot.xmax= dx;
	}
	
	/* handle height - posx and negx flags are mutually exclusive, so watch out */
	if ((v2d->align & V2D_ALIGN_NO_POS_Y) && !(v2d->align & V2D_ALIGN_NO_NEG_Y)) {
		/* height is in negative-y half */
		v2d->tot.ymin= (float)-height;
		v2d->tot.ymax= 0.0f;
	}
	else if ((v2d->align & V2D_ALIGN_NO_NEG_Y) && !(v2d->align & V2D_ALIGN_NO_POS_Y)) {
		/* height is in positive-y half */
		v2d->tot.ymin= 0.0f;
		v2d->tot.ymax= (float)height;
	}
	else {
		/* height is centered around y==0 */
		const float dy= (float)height / 2.0f;
		
		v2d->tot.ymin= -dy;
		v2d->tot.ymax= dy;
	}
	
	/* make sure that 'cur' rect is in a valid state as a result of these changes */
	UI_view2d_curRect_validate(v2d);
}

/* *********************************************************************** */
/* View Matrix Setup */

/* mapping function to ensure 'cur' draws extended over the area where sliders are */
static void view2d_map_cur_using_mask(View2D *v2d, rctf *curmasked)
{
	*curmasked= v2d->cur;
	
	/* currently, the following 'hack' is only necessary for Outliner, and will cause
	 * errors in all other views...
	 */
	if ((v2d->scroll) && (v2d->keeptot==2)) {
		float dx= ((float)(v2d->mask.xmax-v2d->mask.xmin+1))/(v2d->cur.xmax-v2d->cur.xmin);
		float dy= ((float)(v2d->mask.ymax-v2d->mask.ymin+1))/(v2d->cur.ymax-v2d->cur.ymin);
		
		if (v2d->mask.xmin != 0)
			curmasked->xmin -= dx*(float)v2d->mask.xmin;
		if (v2d->mask.xmax+1 != v2d->winx)
			curmasked->xmax += dx*(float)(v2d->winx - v2d->mask.xmax-1);
		
		if (v2d->mask.ymin != 0)
			curmasked->ymin -= dy*(float)v2d->mask.ymin;
		if (v2d->mask.ymax+1 != v2d->winy)
			curmasked->ymax += dy*(float)(v2d->winy - v2d->mask.ymax-1);
		
	}
}

/* Set view matrices to use 'cur' rect as viewing frame for View2D drawing 
*	- this assumes viewport/scissor been set for the region, taking scrollbars into account
*/

void UI_view2d_view_ortho(const bContext *C, View2D *v2d)
{
	rctf curmasked;
	
	/* set the matrix - pixel offsets (-0.375) for 1:1 correspondance are not applied, 
	 * as they were causing some unwanted offsets when drawing 
	 */
	view2d_map_cur_using_mask(v2d, &curmasked);
	wmOrtho2(C->window, curmasked.xmin, curmasked.xmax, curmasked.ymin, curmasked.ymax);
	
	/* XXX is this necessary? */
	wmLoadIdentity(C->window);
}

/* Set view matrices to only use one axis of 'cur' only
 *	- this assumes viewport/scissor been set for the region, taking scrollbars into account
 *
 *	- xaxis 	= if non-zero, only use cur x-axis, otherwise use cur-yaxis (mostly this will be used for x)
 */
void UI_view2d_view_orthoSpecial(const bContext *C, View2D *v2d, short xaxis)
{
	ARegion *ar= C->region;
	rctf curmasked;
	
	/* set the matrix - pixel offsets (-0.375) for 1:1 correspondance are not applied, 
	 * as they were causing some unwanted offsets when drawing 
	 */
	view2d_map_cur_using_mask(v2d, &curmasked);
	if (xaxis)
		wmOrtho2(C->window, curmasked.xmin, curmasked.xmax, 0, ar->winy);
	else
		wmOrtho2(C->window, 0, ar->winx, curmasked.ymin, curmasked.ymax);
		
	/* XXX is this necessary? */
	wmLoadIdentity(C->window);
} 


/* Restore view matrices after drawing */
void UI_view2d_view_restore(const bContext *C)
{
	ED_region_pixelspace(C, C->region);
}

/* allowing horizontal pan */
void UI_view2d_header_default(View2D *v2d)
{
	v2d->keepaspect= 1;
	v2d->keepzoom = (V2D_LOCKZOOM_X|V2D_LOCKZOOM_Y|V2D_KEEPZOOM);
	v2d->keepofs = V2D_LOCKOFS_Y;
	v2d->keeptot = 2; // this keeps the view in place when region size changes...
	v2d->align = V2D_ALIGN_NO_NEG_X;
	
}

/* *********************************************************************** */
/* Gridlines */

/* minimum pixels per gridstep */
#define MINGRIDSTEP 	35

/* View2DGrid is typedef'd in UI_view2d.h */
struct View2DGrid {
	float dx, dy;			/* stepsize (in pixels) between gridlines */
	float startx, starty;	/* initial coordinates to start drawing grid from */
	int powerx, powery;		/* step as power of 10 */
};

/* --------------- */

/* try to write step as a power of 10 */
static void step_to_grid(float *step, int *power, int unit)
{
	const float loga= log10(*step);
	float rem;
	
	*power= (int)(loga);
	
	rem= loga - (*power);
	rem= pow(10.0, rem);
	
	if (loga < 0.0) {
		if (rem < 0.2) rem= 0.2;
		else if(rem < 0.5) rem= 0.5;
		else rem= 1.0;
		
		*step= rem * pow(10.0, (float)(*power));
		
		/* for frames, we want 1.0 frame intervals only */
		if (unit == V2D_UNIT_FRAMES) {
			rem = 1.0;
			*step = 1.0;
		}
		
		/* prevents printing 1.0 2.0 3.0 etc */
		if (rem == 1.0) (*power)++;	
	}
	else {
		if (rem < 2.0) rem= 2.0;
		else if(rem < 5.0) rem= 5.0;
		else rem= 10.0;
		
		*step= rem * pow(10.0, (float)(*power));
		
		(*power)++;
		/* prevents printing 1.0, 2.0, 3.0, etc. */
		if (rem == 10.0) (*power)++;	
	}
}

/* Intialise settings necessary for drawing gridlines in a 2d-view 
 *	- Currently, will return pointer to View2DGrid struct that needs to 
 *	  be freed with UI_view2d_grid_free()
 *	- Is used for scrollbar drawing too (for units drawing)
 *	
 *	- unit	= V2D_UNIT_*  grid steps in seconds or frames 
 *	- clamp	= V2D_CLAMP_* only show whole-number intervals
 *	- winx	= width of region we're drawing to
 *	- winy	= height of region we're drawing into
 */
View2DGrid *UI_view2d_grid_calc(const bContext *C, View2D *v2d, short unit, short clamp, int winx, int winy)
{
	View2DGrid *grid;
	float space, pixels, seconddiv;
	int secondgrid;
	
	/* grid here is allocated... */
	grid= MEM_callocN(sizeof(View2DGrid), "View2DGrid");
	
	/* rule: gridstep is minimal GRIDSTEP pixels */
	if (unit == V2D_UNIT_SECONDS) {
		secondgrid= 1;
		seconddiv= 0.01f * FPS;
	}
	else {
		secondgrid= 0;
		seconddiv= 1.0f;
	}
	
	/* calculate x-axis grid scale */
	space= v2d->cur.xmax - v2d->cur.xmin;
	pixels= v2d->mask.xmax - v2d->mask.xmin;
	
	grid->dx= (MINGRIDSTEP * space) / (seconddiv * pixels);
	step_to_grid(&grid->dx, &grid->powerx, unit);
	grid->dx *= seconddiv;
	
	if (clamp == V2D_GRID_CLAMP) {
		if (grid->dx < 0.1f) grid->dx= 0.1f;
		grid->powerx-= 2;
		if (grid->powerx < -2) grid->powerx= -2;
	}
	
	/* calculate y-axis grid scale */
	space= v2d->cur.ymax - v2d->cur.ymin;
	pixels= winy;
	
	grid->dy= MINGRIDSTEP * space / pixels;
	step_to_grid(&grid->dy, &grid->powery, unit);
	
	if (clamp == V2D_GRID_CLAMP) {
		if (grid->dy < 1.0f) grid->dy= 1.0f;
		if (grid->powery < 1) grid->powery= 1;
	}
	
	/* calculate start position */
	grid->startx= seconddiv*(v2d->cur.xmin/seconddiv - fmod(v2d->cur.xmin/seconddiv, grid->dx/seconddiv));
	if (v2d->cur.xmin < 0.0f) grid->startx-= grid->dx;
	
	grid->starty= (v2d->cur.ymin - fmod(v2d->cur.ymin, grid->dy));
	if (v2d->cur.ymin < 0.0f) grid->starty-= grid->dy;

	return grid;
}

/* Draw gridlines in the given 2d-region */
void UI_view2d_grid_draw(const bContext *C, View2D *v2d, View2DGrid *grid, int flag)
{
	float vec1[2], vec2[2];
	int a, step;
	
	/* vertical lines */
	if (flag & V2D_VERTICAL_LINES) {
		/* initialise initial settings */
		vec1[0]= vec2[0]= grid->startx;
		vec1[1]= grid->starty;
		vec2[1]= v2d->cur.ymax;
		
		/* minor gridlines */
		step= (v2d->mask.xmax - v2d->mask.xmin + 1) / MINGRIDSTEP;
		UI_ThemeColor(TH_GRID);
		
		for (a=0; a<step; a++) {
			glBegin(GL_LINE_STRIP);
				glVertex2fv(vec1); 
				glVertex2fv(vec2);
			glEnd();
			
			vec2[0]= vec1[0]+= grid->dx;
		}
		
		/* major gridlines */
		vec2[0]= vec1[0]-= 0.5f*grid->dx;
		UI_ThemeColorShade(TH_GRID, 16);
		
		step++;
		for (a=0; a<=step; a++) {
			glBegin(GL_LINE_STRIP);
				glVertex2fv(vec1); 
				glVertex2fv(vec2);
			glEnd();
			
			vec2[0]= vec1[0]-= grid->dx;
		}
	}
	
	/* horizontal lines */
	if (flag & V2D_HORIZONTAL_LINES) {
		puts("draw horizontal lines");
		/* only major gridlines */
		vec1[1]= vec2[1]= grid->starty;
		vec1[0]= grid->startx;
		vec2[0]= v2d->cur.xmax;
		
		step= (v2d->mask.ymax - v2d->mask.ymin + 1) / MINGRIDSTEP;
		
		UI_ThemeColor(TH_GRID);
		for (a=0; a<=step; a++) {
			printf("\t a = %d \n", a);
			glBegin(GL_LINE_STRIP);
				glVertex2fv(vec1); 
				glVertex2fv(vec2);
			glEnd();
			
			vec2[1]= vec1[1]+= grid->dy;
		}
		
		/* fine grid lines */
		vec2[1]= vec1[1]-= 0.5f*grid->dy;
		step++;
		
		if (flag & V2D_HORIZONTAL_FINELINES) { 
			UI_ThemeColorShade(TH_GRID, 16);
			for (a=0; a<step; a++) {
				glBegin(GL_LINE_STRIP);
					glVertex2fv(vec1); 
					glVertex2fv(vec2);
				glEnd();
				
				vec2[1]= vec1[1]-= grid->dy;
			}
		}
	}
	
	/* Axes are drawn as darker lines */
	UI_ThemeColorShade(TH_GRID, -50);
	
	/* horizontal axis */
	if (flag & V2D_HORIZONTAL_AXIS) {
		vec1[0]= v2d->cur.xmin;
		vec2[0]= v2d->cur.xmax;
		vec1[1]= vec2[1]= 0.0f;
		
		glBegin(GL_LINE_STRIP);
			glVertex2fv(vec1);
			glVertex2fv(vec2);
		glEnd();
	}
	
	/* vertical axis */
	if (flag & V2D_VERTICAL_AXIS) {
		vec1[1]= v2d->cur.ymin;
		vec2[1]= v2d->cur.ymax;
		vec1[0]= vec2[0]= 0.0f;
		
		glBegin(GL_LINE_STRIP);
			glVertex2fv(vec1); 
			glVertex2fv(vec2);
		glEnd();
	}
}

/* free temporary memory used for drawing grid */
void UI_view2d_grid_free(View2DGrid *grid)
{
	MEM_freeN(grid);
}

/* *********************************************************************** */
/* Scrollbars */

/* View2DScrollers is typedef'd in UI_view2d.h 
 * WARNING: the start of this struct must not change, as view2d_ops.c uses this too. 
 * 		   For now, we don't need to have a separate (internal) header for structs like this...
 */
struct View2DScrollers {	
		/* focus bubbles */
	int vert_min, vert_max;	/* vertical scrollbar */
	int hor_min, hor_max;	/* horizontal scrollbar */
	
		/* scales */
	View2DGrid *grid;		/* grid for coordinate drawing */
	short xunits, xclamp;	/* units and clamping options for x-axis */
	short yunits, yclamp;	/* units and clamping options for y-axis */
};

/* Calculate relevant scroller properties */
View2DScrollers *UI_view2d_scrollers_calc(const bContext *C, View2D *v2d, short xunits, short xclamp, short yunits, short yclamp)
{
	View2DScrollers *scrollers;
	rcti vert, hor;
	float fac, totsize, scrollsize;
	
	vert= v2d->vert;
	hor= v2d->hor;
	
	/* scrollers is allocated here... */
	scrollers= MEM_callocN(sizeof(View2DScrollers), "View2DScrollers");
	
	/* scroller 'buttons':
	 *	- These should always remain within the visible region of the scrollbar
	 *	- They represent the region of 'tot' that is visible in 'cur'
	 */
	
	/* horizontal scrollers */
	if (v2d->scroll & (V2D_SCROLL_HORIZONTAL|V2D_SCROLL_HORIZONTAL_O)) {
		/* scroller 'button' extents */
		totsize= v2d->tot.xmax - v2d->tot.xmin;
		scrollsize= hor.xmax - hor.xmin;
		
		fac= (v2d->cur.xmin- v2d->tot.xmin) / totsize;
		scrollers->hor_min= hor.xmin + (fac * scrollsize);
		
		fac= (v2d->cur.xmax - v2d->tot.xmin) / totsize;
		scrollers->hor_max= hor.xmin + (fac * scrollsize);
		
		if (scrollers->hor_min > scrollers->hor_max) 
			scrollers->hor_min= scrollers->hor_max;
	}
	
	/* vertical scrollers */
	if (v2d->scroll & V2D_SCROLL_VERTICAL) {
		/* scroller 'button' extents */
		totsize= v2d->tot.ymax - v2d->tot.ymin;
		scrollsize= vert.ymax - vert.ymin;
		
		fac= (v2d->cur.ymin- v2d->tot.ymin) / totsize;
		scrollers->vert_min= vert.ymin + (fac * scrollsize);
		
		fac= (v2d->cur.ymax - v2d->tot.ymin) / totsize;
		scrollers->vert_max= vert.ymin + (fac * scrollsize);
		
		if (scrollers->vert_min > scrollers->vert_max) 
			scrollers->vert_min= scrollers->vert_max;
	}
	
	/* grid markings on scrollbars */
	if (v2d->scroll & (V2D_SCROLL_SCALE_HORIZONTAL|V2D_SCROLL_SCALE_VERTICAL)) {
		/* store clamping */
		scrollers->xclamp= xclamp;
		scrollers->xunits= xunits;
		scrollers->yclamp= yclamp;
		scrollers->yunits= yunits;
		
		/* calculate grid only if clamping + units are valid arguments */
		if ( !((xclamp == V2D_ARG_DUMMY) && (xunits == V2D_ARG_DUMMY) && (yclamp == V2D_ARG_DUMMY) && (yunits == V2D_ARG_DUMMY)) ) { 
			/* if both axes show scale, give priority to horizontal.. */
			// FIXME: this doesn't do justice to the vertical scroller calculations...
			if ((v2d->scroll & V2D_SCROLL_SCALE_HORIZONTAL) && ELEM(V2D_ARG_DUMMY, xclamp, xunits)==0)
				scrollers->grid= UI_view2d_grid_calc(C, v2d, xunits, xclamp, (hor.xmax - hor.xmin), (vert.ymax - vert.ymin));
			else if (v2d->scroll & V2D_SCROLL_SCALE_VERTICAL && ELEM(V2D_ARG_DUMMY, yclamp, yunits)==0)
				scrollers->grid= UI_view2d_grid_calc(C, v2d, yunits, yclamp, (hor.xmax - hor.xmin), (vert.ymax - vert.ymin));
		}
	}
	
	/* return scrollers */
	return scrollers;
}

/* XXX */
extern void ui_rasterpos_safe(float x, float y, float aspect);

/* Print scale marking along a time scrollbar */
static void scroll_printstr(View2DScrollers *scrollers, float x, float y, float val, int power, short unit, char dir)
{
	int len;
	char str[32];
	
	/* adjust the scale unit to work ok */
	if (dir == 'v') {
		/* here we bump up the power by factor of 10, as 
		 * rotation values (hence 'degrees') are divided by 10 to 
		 * be able to show the curves at the same time
		 */
		if ELEM(unit, V2D_UNIT_DEGREES, V2D_UNIT_TIME) {
			power += 1;
			val *= 10;
		}
	}
	
	/* get string to print */
	if (unit == V2D_UNIT_SECONDS) {
		/* SMPTE timecode style:
		 *	- In general, minutes and seconds should be shown, as most clips will be
		 *	  within this length. Hours will only be included if relevant.
		 *	- Only show frames when zoomed in enough for them to be relevant 
		 *	  (using separator convention of ';' for frames, ala QuickTime).
		 *	  When showing frames, use slightly different display to avoid confusion with mm:ss format
		 */
		int hours=0, minutes=0, seconds=0, frames=0;
		char neg[2]= "";
		
		/* get values */
		if (val < 0) {
			/* correction for negative values */
			sprintf(neg, "-");
			val = -val;
		}
		if (val >= 3600) {
			/* hours */
			/* XXX should we only display a single digit for hours since clips are 
			 * 	   VERY UNLIKELY to be more than 1-2 hours max? However, that would 
			 *	   go against conventions...
			 */
			hours= (int)val / 3600;
			val= fmod(val, 3600);
		}
		if (val >= 60) {
			/* minutes */
			minutes= (int)val / 60;
			val= fmod(val, 60);
		}
		if (power <= 0) {
			/* seconds + frames
			 *	Frames are derived from 'fraction' of second. We need to perform some additional rounding
			 *	to cope with 'half' frames, etc., which should be fine in most cases
			 */
			seconds= (int)val;
			frames= (int)floor( ((val - seconds) * FPS) + 0.5f );
		}
		else {
			/* seconds (with pixel offset) */
			seconds= (int)floor(val + 0.375f);
		}
		
		/* print timecode to temp string buffer */
		if (power <= 0) {
			/* include "frames" in display */
			if (hours) sprintf(str, "%s%02d:%02d:%02d;%02d", neg, hours, minutes, seconds, frames);
			else if (minutes) sprintf(str, "%s%02d:%02d;%02d", neg, minutes, seconds, frames);
			else sprintf(str, "%s%d;%02d", neg, seconds, frames);
		}
		else {
			/* don't include 'frames' in display */
			if (hours) sprintf(str, "%s%02d:%02d:%02d", neg, hours, minutes, seconds);
			else sprintf(str, "%s%02d:%02d", neg, minutes, seconds);
		}
	}
	else {
		/* round to whole numbers if power is >= 1 (i.e. scale is coarse) */
		if (power <= 0) sprintf(str, "%.*f", 1-power, val);
		else sprintf(str, "%d", (int)floor(val + 0.375f));
	}
	
	/* get length of string, and adjust printing location to fit it into the horizontal scrollbar */
	len= strlen(str);
	if (dir == 'h') {
		/* seconds/timecode display has slightly longer strings... */
		if (unit == V2D_UNIT_SECONDS)
			x-= 3*len;
		else
			x-= 4*len;
	}
	
	/* Add degree sympbol to end of string for vertical scrollbar? */
	if ((dir == 'v') && (unit == V2D_UNIT_DEGREES)) {
		str[len]= 186;
		str[len+1]= 0;
	}
	
	/* draw it */
	ui_rasterpos_safe(x, y, 1.0);
	UI_DrawString(G.fonts, str, 0); // XXX check this again when new text-drawing api is done
}

/* local define for radius of scroller 'button' caps */
#define V2D_SCROLLCAP_RAD	5

/* Draw scrollbars in the given 2d-region */
void UI_view2d_scrollers_draw(const bContext *C, View2D *v2d, View2DScrollers *vs)
{
	const int darker= -50, dark= -10, light= 20, lighter= 50;
	rcti vert, hor;
	
	/* make copies of rects for less typing */
	vert= v2d->vert;
	hor= v2d->hor;
	
	/* horizontal scrollbar */
	if (v2d->scroll & (V2D_SCROLL_HORIZONTAL|V2D_SCROLL_HORIZONTAL_O)) {
		/* scroller backdrop */
		UI_ThemeColorShade(TH_SHADE1, light);
		glRecti(hor.xmin,  hor.ymin,  hor.xmax,  hor.ymax);
		
		/* scroller 'button' 
		 *	- if view is zoomable in x, draw handles too 
		 *	- handles are drawn darker
		 */
		if (v2d->keepzoom & V2D_LOCKZOOM_X) {
			/* draw base bar as rounded shape */
			UI_ThemeColorShade(TH_SHADE1, dark);
			
			uiSetRoundBox(15);
			gl_round_box_shade(GL_POLYGON, 
					vs->hor_min, hor.ymin+2, 
					vs->hor_max, hor.ymax-2, 
					V2D_SCROLLCAP_RAD, 0.05f, -0.05f);
		}
		else {
			/* base bar drawn as shaded rect */
			UI_ThemeColorShade(TH_SHADE1, dark);
			uiSetRoundBox(0);
			gl_round_box_shade(GL_POLYGON, 
					vs->hor_min, hor.ymin+2, 
					vs->hor_max, hor.ymax-2, 
					V2D_SCROLLCAP_RAD, 0.05f, -0.05f);
			
			/* handles draw darker */
			// XXX handles are drawn with the two last args set to same values, otherwise, max appears darker than min
			UI_ThemeColorShade(TH_SHADE1, darker);
			
			/* 'minimum' handle */
			uiSetRoundBox(9);
			gl_round_box_shade(GL_POLYGON, 
					vs->hor_min-V2D_SCROLLER_HANDLE_SIZE, hor.ymin+2, 
					vs->hor_min+V2D_SCROLLER_HANDLE_SIZE, hor.ymax-2, 
					V2D_SCROLLCAP_RAD, 0.07f, -0.07f);
					//V2D_SCROLLCAP_RAD, 0, 0);
			
			/* maximum handle */
			uiSetRoundBox(6);
			gl_round_box_shade(GL_POLYGON, 
					vs->hor_max-V2D_SCROLLER_HANDLE_SIZE, hor.ymin+2, 
					vs->hor_max+V2D_SCROLLER_HANDLE_SIZE, hor.ymax-2, 
					V2D_SCROLLCAP_RAD, 0.07f, -0.07f);
					//V2D_SCROLLCAP_RAD, 0, 0);
		}
		
		/* scale indicators */
		// XXX will need to update the font drawing when the new stuff comes in
		if (v2d->scroll & V2D_SCROLL_SCALE_HORIZONTAL) {
			View2DGrid *grid= vs->grid;
			float fac, dfac, fac2, val;
			
			/* the numbers: convert grid->startx and -dx to scroll coordinates 
			 *	- fac is x-coordinate to draw to
			 *	- dfac is gap between scale markings
			 */
			fac= (grid->startx- v2d->cur.xmin) / (v2d->cur.xmax - v2d->cur.xmin);
			fac= hor.xmin + fac*(hor.xmax - hor.xmin);
			
			dfac= (grid->dx) / (v2d->cur.xmax - v2d->cur.xmin);
			dfac= dfac * (hor.xmax - hor.xmin);
			
			/* set starting value, and text color */
			UI_ThemeColor(TH_TEXT);
			val= grid->startx;
			
			/* if we're clamping to whole numbers only, make sure entries won't be repeated */
			if (vs->xclamp == V2D_GRID_CLAMP) {
				while (grid->dx < 0.9999f) {
					grid->dx *= 2.0f;
					dfac *= 2.0f;
				}
			}
			if (vs->xunits == V2D_UNIT_FRAMES)
				grid->powerx= 1;
			
			/* draw numbers in the appropriate range */
			if (dfac != 0.0f) {
				for (; fac < hor.xmax; fac+=dfac, val+=grid->dx) {
					switch (vs->xunits) {
						case V2D_UNIT_FRAMES:		/* frames (as whole numbers)*/
							scroll_printstr(vs, fac, 3.0+(float)(hor.ymin), val, grid->powerx, V2D_UNIT_FRAMES, 'h');
							break;
						
						case V2D_UNIT_SECONDS:		/* seconds */
							fac2= val/FPS;
							scroll_printstr(vs, fac, 3.0+(float)(hor.ymin), fac2, grid->powerx, V2D_UNIT_SECONDS, 'h');
							break;
							
						case V2D_UNIT_SECONDSSEQ:	/* seconds with special calculations (only used for sequencer only) */
						{
							float time;
							
							fac2= val/FPS;
							time= floor(fac2);
							fac2= fac2-time;
							
							scroll_printstr(vs, fac, 3.0+(float)(hor.ymin), time+FPS*fac2/100.0, grid->powerx, V2D_UNIT_SECONDSSEQ, 'h');
						}
							break;
							
						case V2D_UNIT_DEGREES:		/* IPO-Editor for rotation IPO-Drivers */
							/* HACK: although we're drawing horizontal, we make this draw as 'vertical', just to get degree signs */
							scroll_printstr(vs, fac, 3.0+(float)(hor.ymin), val, grid->powerx, V2D_UNIT_DEGREES, 'v');
							break;
					}
				}
			}
		}
		
		/* decoration outer bevel line */
		UI_ThemeColorShade(TH_SHADE1, lighter);
		if (v2d->scroll & (V2D_SCROLL_BOTTOM|V2D_SCROLL_BOTTOM_O))
			sdrawline(hor.xmin, hor.ymax, hor.xmax, hor.ymax);
		else if (v2d->scroll & V2D_SCROLL_TOP)
			sdrawline(hor.xmin, hor.ymin, hor.xmax, hor.ymin);
	}
	
	/* vertical scrollbar */
	if (v2d->scroll & V2D_SCROLL_VERTICAL) {
		/* scroller backdrop  */
		UI_ThemeColorShade(TH_SHADE1, light);
		glRecti(vert.xmin,  vert.ymin,  vert.xmax,  vert.ymax);
		
		/* scroller 'button' 
		 *	- if view is zoomable in y, draw handles too 
		 *	- handles are drawn darker
		 */
		// XXX gl_round_box_shade is currently hardcoded to do horizontal buts only... need a vertical region
		if (v2d->keepzoom & V2D_LOCKZOOM_Y) {
			/* draw base bar as rounded shape */
			UI_ThemeColorShade(TH_SHADE1, dark);
			
			uiSetRoundBox(15);
			gl_round_box_shade(GL_POLYGON, 
					vert.xmin+2, vs->vert_min, 
					vert.xmax-2, vs->vert_max, 
					V2D_SCROLLCAP_RAD, 0.05f, -0.05f);
		}
		else {
			/* for now, draw base bar as unshaded rect */
			// FIXME: this should be shaded horizontally too
			UI_ThemeColorShade(TH_SHADE1, dark);
			glRecti(vert.xmin+2, vs->vert_min,  
					vert.xmax-2, vs->vert_max);
			
			/* handles draw darker */
			// XXX handles are drawn with the two last args set to same values, otherwise, max appears darker than min
			UI_ThemeColorShade(TH_SHADE1, darker);
			
			/* 'minimum' handle */
			uiSetRoundBox(12);
			gl_round_box_shade(GL_POLYGON, 
					vert.xmin+2, vs->vert_min-V2D_SCROLLER_HANDLE_SIZE, 
					vert.xmax-2, vs->vert_min+V2D_SCROLLER_HANDLE_SIZE, 
					V2D_SCROLLCAP_RAD, 0, 0);
			
			/* maximum handle */
			uiSetRoundBox(3);
			gl_round_box_shade(GL_POLYGON, 
					vert.xmin+2, vs->vert_max-V2D_SCROLLER_HANDLE_SIZE, 
					vert.xmax-2, vs->vert_max+V2D_SCROLLER_HANDLE_SIZE, 
					V2D_SCROLLCAP_RAD, 0, 0);
		}
		
		/* scale indiators */
		// XXX will need to update the font drawing when the new stuff comes in
		if (v2d->scroll & V2D_SCROLL_SCALE_VERTICAL) {
			View2DGrid *grid= vs->grid;
			float fac, dfac, val;
			
			/* the numbers: convert grid->starty and dy to scroll coordinates 
			 *	- fac is y-coordinate to draw to
			 *	- dfac is gap between scale markings
			 *	- these involve a correction for horizontal scrollbar
			 *	  NOTE: it's assumed that that scrollbar is there if this is involved!
			 */
			fac= (grid->starty- v2d->cur.ymin) / (v2d->cur.ymax - v2d->cur.ymin);
			fac= (vert.ymin + V2D_SCROLL_HEIGHT) + fac*(vert.ymax - vert.ymin - V2D_SCROLL_HEIGHT);
			
			dfac= (grid->dy) / (v2d->cur.ymax - v2d->cur.ymin);
			dfac= dfac * (vert.ymax - vert.ymin - V2D_SCROLL_HEIGHT);
			
			/* set starting value, and text color */
			UI_ThemeColor(TH_TEXT);
			val= grid->starty;
			
			/* if vertical clamping (to whole numbers) is used (i.e. in Sequencer), apply correction */
			// XXX only relevant to Sequencer, so need to review this when we port that code
			if (vs->yclamp == V2D_GRID_CLAMP)
				fac += 0.5f * dfac;
				
			/* draw vertical steps */
			for (; fac < vert.ymax; fac+= dfac, val += grid->dy) {
				scroll_printstr(vs, (float)(vert.xmax)-14.0, fac, val, grid->powery, vs->yunits, 'v');
			}			
		}	
		
		/* decoration outer bevel line */
		UI_ThemeColorShade(TH_SHADE1, lighter);
		if (v2d->scroll & V2D_SCROLL_RIGHT)
			sdrawline(vert.xmin, vert.ymin, vert.xmin, vert.ymax);
		else if (v2d->scroll & V2D_SCROLL_LEFT)
			sdrawline(vert.xmax, vert.ymin, vert.xmax, vert.ymax);
	}
}

/* free temporary memory used for drawing scrollers */
void UI_view2d_scrollers_free(View2DScrollers *scrollers)
{
	/* need to free grid as well... */
	if (scrollers->grid) MEM_freeN(scrollers->grid);
	MEM_freeN(scrollers);
}

/* *********************************************************************** */
/* Coordinate Conversions */

/* Convert from screen/region space to 2d-View space 
 *	
 *	- x,y 			= coordinates to convert
 *	- viewx,viewy		= resultant coordinates
 */
void UI_view2d_region_to_view(View2D *v2d, int x, int y, float *viewx, float *viewy)
{
	float div, ofs;

	if (viewx) {
		div= v2d->mask.xmax - v2d->mask.xmin;
		ofs= v2d->mask.xmin;
		
		*viewx= v2d->cur.xmin + (v2d->cur.xmax-v2d->cur.xmin) * ((float)x - ofs) / div;
	}

	if (viewy) {
		div= v2d->mask.ymax - v2d->mask.ymin;
		ofs= v2d->mask.ymin;
		
		*viewy= v2d->cur.ymin + (v2d->cur.ymax - v2d->cur.ymin) * ((float)y - ofs) / div;
	}
}

/* Convert from 2d-View space to screen/region space
 *	- Coordinates are clamped to lie within bounds of region
 *
 *	- x,y 				= coordinates to convert
 *	- regionx,regiony 	= resultant coordinates 
 */
void UI_view2d_view_to_region(View2D *v2d, float x, float y, short *regionx, short *regiony)
{
	/* set initial value in case coordinate lies outside of bounds */
	if (regionx)
		*regionx= V2D_IS_CLIPPED;
	if (regiony)
		*regiony= V2D_IS_CLIPPED;
	
	/* express given coordinates as proportional values */
	x= (x - v2d->cur.xmin) / (v2d->cur.xmax - v2d->cur.xmin);
	y= (y - v2d->cur.ymin) / (v2d->cur.ymax - v2d->cur.ymin);
	
	/* check if values are within bounds */
	if ((x>=0.0f) && (x<=1.0f) && (y>=0.0f) && (y<=1.0f)) {
		if (regionx)
			*regionx= v2d->mask.xmin + x*(v2d->mask.xmax-v2d->mask.xmin);
		if (regiony)
			*regiony= v2d->mask.ymin + y*(v2d->mask.ymax-v2d->mask.ymin);
	}
}

/* Convert from 2d-view space to screen/region space
 *	- Coordinates are NOT clamped to lie within bounds of region
 *
 *	- x,y 				= coordinates to convert
 *	- regionx,regiony 	= resultant coordinates 
 */
void UI_view2d_to_region_no_clip(View2D *v2d, float x, float y, short *regionx, short *regiony)
{
	/* step 1: express given coordinates as proportional values */
	x= (x - v2d->cur.xmin) / (v2d->cur.xmax - v2d->cur.xmin);
	y= (y - v2d->cur.ymin) / (v2d->cur.ymax - v2d->cur.ymin);
	
	/* step 2: convert proportional distances to screen coordinates  */
	x= v2d->mask.xmin + x*(v2d->mask.xmax - v2d->mask.xmin);
	y= v2d->mask.ymin + y*(v2d->mask.ymax - v2d->mask.ymin);
	
	/* although we don't clamp to lie within region bounds, we must avoid exceeding size of shorts */
	if (regionx) {
		if (x < -32760) *regionx= -32760;
		else if(x > 32760) *regionx= 32760;
		else *regionx= x;
	}
	if (regiony) {
		if (y < -32760) *regiony= -32760;
		else if(y > 32760) *regiony= 32760;
		else *regiony= y;
	}
}

/* *********************************************************************** */
/* Utilities */

/* View2D data by default resides in region, so get from region stored in context */
View2D *UI_view2d_fromcontext(const bContext *C)
{
	if (C->area == NULL) return NULL;
	if (C->region == NULL) return NULL;
	return &(C->region->v2d);
}

/* same as above, but it returns regionwindow. Utility for pulldowns or buttons */
View2D *UI_view2d_fromcontext_rwin(const bContext *C)
{
	if (C->area == NULL) return NULL;
	if (C->region == NULL) return NULL;
	if (C->region->regiontype!=RGN_TYPE_WINDOW) {
		ARegion *ar= C->area->regionbase.first;
		for(; ar; ar= ar->next)
			if(ar->regiontype==RGN_TYPE_WINDOW)
				return &(ar->v2d);
		return NULL;
	}
	return &(C->region->v2d);
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

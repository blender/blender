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

#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "WM_api.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_screen.h"

#include "UI_resources.h"
#include "UI_text.h"
#include "UI_view2d.h"

/* *********************************************************************** */
/* Refresh and Validation */

/* Adjust mask size in response to view size changes */
// XXX pre2.5 -> this used to be called  calc_scrollrcts()
void UI_view2d_update_size(View2D *v2d, int winx, int winy)
{
	/* mask - view frame */
	v2d->mask.xmin= v2d->mask.ymin= 0;
	v2d->mask.xmax= winx;
	v2d->mask.ymax= winy;
	
	/* scrollbars shrink mask area, but should be based off regionsize 
	 *	- they can only be on one edge of the region they define
	 */
	if (v2d->scroll) {
		/* vertical scrollbar */
		if (v2d->scroll & L_SCROLL) {
			/* on left-hand edge of region */
			v2d->vert= v2d->mask;
			v2d->vert.xmax= SCROLLB;
			v2d->mask.xmin= SCROLLB;
		}
		else if (v2d->scroll & R_SCROLL) {
			/* on right-hand edge of region */
			v2d->vert= v2d->mask;
			v2d->vert.xmin= v2d->vert.xmax-SCROLLB;
			v2d->mask.xmax= v2d->vert.xmin;
		}
		
		/* horizontal scrollbar */
		if (v2d->scroll & (B_SCROLL|B_SCROLLO)) {
			/* on bottom edge of region (B_SCROLLO is outliner, the ohter is for standard) */
			v2d->hor= v2d->mask;
			v2d->hor.ymax= SCROLLH;
			v2d->mask.ymin= SCROLLH;
		}
		else if (v2d->scroll & T_SCROLL) {
			/* on upper edge of region */
			v2d->hor= v2d->mask;
			v2d->hor.ymin= v2d->hor.ymax-SCROLLH;
			v2d->mask.ymax= v2d->hor.ymin;
		}
	}
}

/* Ensure View2D rects remain in a viable configuration 
 *	- cur is not allowed to be: larger than max, smaller than min, or outside of tot
 */
// XXX pre2.5 -> this used to be called  test_view2d()
// XXX FIXME - still need to go through this and figure out what it all parts of it do
void UI_view2d_enforce_status(View2D *v2d, int winx, int winy)
{
	/* cur is not allowed to be larger than max, smaller than min, or outside of tot */
	rctf *cur, *tot;
	float dx, dy, temp, fac, zoom;
	
	/* correct winx for scrollbars */
	if (v2d->scroll & L_SCROLL) winx-= SCROLLB;
	if (v2d->scroll & (B_SCROLL|B_SCROLLO)) winy-= SCROLLH;
	
	/* header completely closed window */
	if (winy <= 0) return;
	
	/* get pointers */
	cur= &v2d->cur;
	tot= &v2d->tot;
	
	/* dx, dy are width and height of v2d->cur, respectively */
	dx= cur->xmax - cur->xmin;
	dy= cur->ymax - cur->ymin;
	
	/* keepzoom - restore old zoom */
	if (v2d->keepzoom) {	
		/* keepzoom on x or y axis - reset size of current-viewable area to size of region (i.e. no zooming happened) */
		if (v2d->keepzoom & V2D_LOCKZOOM_Y)
			cur->ymax= cur->ymin + ((float)winy);
		if (v2d->keepzoom & V2D_LOCKZOOM_X)
			cur->xmax= cur->xmin + ((float)winx);
		
		/* calculate zoom-factor for x */
		zoom= ((float)winx)/dx;
		
		/* if zoom factor is excessive, normalise it and calculate new width */
		if ((zoom < v2d->minzoom) || (zoom > v2d->maxzoom)) {
			if (zoom < v2d->minzoom) fac= zoom / v2d->minzoom;
			else fac= zoom / v2d->maxzoom;
			
			dx *= fac;
			temp= 0.5f * (cur->xmax + cur->xmin);
			
			cur->xmin= temp - (0.5f * dx);
			cur->xmax= temp + (0.5f * dx);
		}
		
		/* calculate zoom-factor for y */
		zoom= ((float)winy)/dy;
		
		/* if zoom factor is excessive, normalise it and calculate new width */
		if ((zoom < v2d->minzoom) || (zoom > v2d->maxzoom)) {
			if (zoom < v2d->minzoom) fac= zoom / v2d->minzoom;
			else fac= zoom / v2d->maxzoom;
			
			dy *= fac;
			temp= 0.5f * (cur->ymax + cur->ymin);
			
			cur->ymin= temp - (0.5f * dy);
			cur->ymax= temp + (0.5f * dy);
		}
	}
	else {
		/* if extents of cur are below or above what's acceptable, interpolate extent to lie halfway */
		if (dx < v2d->min[0]) {
			dx= v2d->min[0];
			temp= 0.5f * (cur->xmax + cur->xmin);
			
			cur->xmin= temp - (0.5f * dx);
			cur->xmax= temp + (0.5f * dx);
		}
		else if (dx > v2d->max[0]) {
			dx= v2d->max[0];
			temp= 0.5f * (cur->xmax + cur->xmin);
			
			cur->xmin= temp - (0.5f * dx);
			cur->xmax= temp + (0.5f * dx);
		}
		
		if (dy < v2d->min[1]) {
			dy= v2d->min[1];
			temp= 0.5f * (cur->ymax + cur->ymin);
			
			cur->ymin= temp - (0.5f * dy);
			cur->ymax= temp + (0.5f * dy);
		}
		else if (dy > v2d->max[1]) {
			dy= v2d->max[1];
			temp= 0.5f * (cur->ymax + cur->ymin);
			cur->ymin= temp-0.5*dy;
			cur->ymax= temp+0.5*dy;
		}
	}
	
	/* keep aspect - maintain aspect ratio */
	if (v2d->keepaspect) {
		short do_x=0, do_y=0;
		
		/* when a window edge changes, the aspect ratio can't be used to
		 * find which is the best new 'cur' rect. thats why it stores 'old' 
		 */
		if (winx != v2d->oldwinx) do_x= 1;
		if (winy != v2d->oldwiny) do_y= 1;
		
		/* here dx is cur ratio, while dy is win ratio */
		dx= (cur->ymax - cur->ymin) / (cur->xmax - cur->xmin);
		dy= ((float)winy) / ((float)winx);
		
		/* both sizes change (area/region maximised)  */
		if (do_x == do_y) {
			if ((do_x==1) && (do_y==1)) {
				if (ABS(winx - v2d->oldwinx) > ABS(winy - v2d->oldwiny)) do_y= 0;
				else do_x= 0;
			}
			else if (dy > 1.0f) do_x= 0; 
			else do_x= 1;
		}
		
		if (do_x) {
			if ((v2d->keeptot == 2) && (winx < v2d->oldwinx)) {
				/* This is a special hack for the outliner, to ensure that the 
				 * outliner contents will not eventually get pushed out of view
				 * when shrinking the view. 
				 */
				cur->xmax -= cur->xmin;
				cur->xmin= 0.0f;
			}
			else {
				/* portrait window: correct for x */
				dx= cur->ymax - cur->ymin;
				temp= cur->xmax + cur->xmin;
				
				cur->xmin= (temp / 2.0f) - (0.5f * dx / dy);
				cur->xmax= (temp / 2.0f) + (0.5f * dx / dy);
			}
		}
		else {
			dx= cur->xmax - cur->xmin;
			temp= cur->ymax + cur->ymin;
			
			cur->ymin= (temp / 2.0f) - (0.5f * dy * dx);
			cur->ymax= (temp / 2.0f) + (0.5f * dy * dx);
		}
		
		/* store region size for next time */
		v2d->oldwinx= winx; 
		v2d->oldwiny= winy;
	}
	
	/* keeptot - make sure that size of cur doesn't exceed that of tot, otherwise, adjust! */
	if (v2d->keeptot) {
		/* calculate extents of cur */
		dx= cur->xmax - cur->xmin;
		dy= cur->ymax - cur->ymin;
		
		/* cur is wider than tot? */
		if (dx > (tot->xmax - tot->xmin)) {
			if (v2d->keepzoom == 0) {
				if (cur->xmin < tot->xmin) cur->xmin= tot->xmin;
				if (cur->xmax > tot->xmax) cur->xmax= tot->xmax;
			}
			else {
				/* maintaining zoom, so restore cur to tot size */
				if (cur->xmax < tot->xmax) {
					dx= tot->xmax - cur->xmax;
					
					cur->xmin+= dx;
					cur->xmax+= dx;
				}
				else if (cur->xmin > tot->xmin) {
					dx= cur->xmin - tot->xmin;
					
					cur->xmin-= dx;
					cur->xmax-= dx;
				}
			}
		}
		else {
			/* cur is smaller than tot, but cur cannot be outside of tot */
			if (cur->xmin < tot->xmin) {
				dx= tot->xmin - cur->xmin;
				
				cur->xmin += dx;
				cur->xmax += dx;
			}
			else if ((v2d->keeptot != 2) && (cur->xmax > tot->xmax)) {
				/* NOTE: keeptot is 2, as keeptot!=0 makes sure it does get
				 * 		too freely scrolled on x-axis, but keeptot=1 will result
				 *		in a snap-back when clicking on elements
				 */
				dx= cur->xmax - tot->xmax;
				cur->xmin -= dx;
				cur->xmax -= dx;
			}
		}
		
		if (dy > (tot->ymax - tot->ymin)) {
			if (v2d->keepzoom==0) {
				if (cur->ymin < tot->ymin) cur->ymin= tot->ymin;
				if (cur->ymax > tot->ymax) cur->ymax= tot->ymax;
			}
			else {
				if (cur->ymax < tot->ymax) {
					dy= tot->ymax - cur->ymax;
					cur->ymin+= dy;
					cur->ymax+= dy;
				}
				else if (cur->ymin > tot->ymin) {
					dy= cur->ymin - tot->ymin;
					cur->ymin -= dy;
					cur->ymax -= dy;
				}
			}
		}
		else {
			if (cur->ymin < tot->ymin) {
				dy= tot->ymin - cur->ymin;
				cur->ymin += dy;
				cur->ymax += dy;
			}
			else if (cur->ymax > tot->ymax) {
				dy= cur->ymax - tot->ymax;
				cur->ymin-= dy;
				cur->ymax-= dy;
			}
		}
	}
}

/* *********************************************************************** */
/* View Matrix Setup */

/* Set view matrices to use 'cur' rect as viewing frame for View2D drawing 
 *	- this assumes viewport/scissor been set for the region, taking scrollbars into account
 */
void UI_view2d_view_ortho(const bContext *C, View2D *v2d)
{
	/* set the matrix - pixel offsets (-0.375) for 1:1 correspondance are not applied, 
	 * as they were causing some unwanted offsets when drawing 
	 */
	wmOrtho2(C->window, v2d->cur.xmin, v2d->cur.xmax, v2d->cur.ymin, v2d->cur.ymax);
}

/* Set view matrices to only use one axis of 'cur' only
 *	- this assumes viewport/scissor been set for the region, taking scrollbars into account
 *
 *	- xaxis 	= if non-zero, only use cur x-axis, otherwise use cur-yaxis (mostly this will be used for x)
 */
void UI_view2d_view_orthospecial(const bContext *C, View2D *v2d, short xaxis)
{
	ARegion *region= C->region;
	int winx, winy;
	
	/* calculate extents of region */
	winx= region->winrct.xmax - region->winrct.xmin;
	winy= region->winrct.ymax - region->winrct.ymin;
	
	/* set the matrix - pixel offsets (-0.375) for 1:1 correspondance are not applied, 
	 * as they were causing some unwanted offsets when drawing 
	 */
	if (xaxis)
		wmOrtho2(C->window, v2d->cur.xmin, v2d->cur.xmax, 0, winy);
	else
		wmOrtho2(C->window, 0, winx, v2d->cur.ymin, v2d->cur.ymax);
} 


/* Restore view matrices after drawing */
void UI_view2d_view_restore(const bContext *C)
{
	ED_region_pixelspace(C, C->region);
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
 *	  be freed with UI_view2d_free_grid()
 *	- Is used for scrollbar drawing too (for units drawing)
 *	
 *	- unit	= V2D_UNIT_*  grid steps in seconds or frames 
 *	- clamp	= V2D_CLAMP_* only show whole-number intervals
 *	- winx	= width of region we're drawing to
 *	- winy	= height of region we're drawing into
 */
View2DGrid *UI_view2d_calc_grid(const bContext *C, View2D *v2d, short unit, short clamp, int winx, int winy)
{
	View2DGrid *grid;
	float space, pixels, seconddiv;
	int secondgrid;
	
	/* grid here is allocated... */
	grid= MEM_callocN(sizeof(View2DGrid), "View2DGrid");
	
	/* rule: gridstep is minimal GRIDSTEP pixels */
	if (unit == V2D_UNIT_FRAMES) {
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
	space= (v2d->cur.ymax - v2d->cur.ymin);
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
void UI_view2d_draw_grid(const bContext *C, View2D *v2d, View2DGrid *grid, int flag)
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
		/* only major gridlines */
		vec1[0]= grid->startx;
		vec1[1]= vec2[1]= grid->starty;
		vec2[0]= v2d->cur.xmax;
		
		step= (v2d->mask.ymax - v2d->mask.ymax + 1) / MINGRIDSTEP;
		
		UI_ThemeColor(TH_GRID);
		for (a=0; a<=step; a++) {
			glBegin(GL_LINE_STRIP);
				glVertex2fv(vec1); 
				glVertex2fv(vec2);
			glEnd();
			
			vec2[1]= vec1[1]+= grid->dy;
		}
		
		/* fine grid lines */
		// er... only in IPO-Editor it seems (how to expose this in nice way)?
		vec2[1]= vec1[1]-= 0.5f*grid->dy;
		step++;
		
#if 0
		if (curarea->spacetype==SPACE_IPO) { 
			UI_ThemeColorShade(TH_GRID, 16);
			for (a=0; a<step; a++) {
				glBegin(GL_LINE_STRIP);
					glVertex2fv(vec1); 
					glVertex2fv(vec2);
				glEnd();
				
				vec2[1]= vec1[1]-= grid->dy;
			}
		}
#endif
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
void UI_view2d_free_grid(View2DGrid *grid)
{
	MEM_freeN(grid);
}

/* *********************************************************************** */
/* Scrollbars */

/* View2DScrollers is typedef'd in UI_view2d.h */
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
View2DScrollers *UI_view2d_calc_scrollers(const bContext *C, View2D *v2d, short xunits, short xclamp, short yunits, short yclamp)
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
	if (v2d->scroll & (HOR_SCROLL|HOR_SCROLLO)) {
		/* slider 'button' extents */
		totsize= v2d->tot.xmax - v2d->tot.xmin;
		scrollsize= hor.xmax - hor.xmin;
		
		fac= (v2d->cur.xmin- v2d->tot.xmin) / totsize;
		//if (fac < 0.0f) fac= 0.0f;
		scrollers->hor_min= hor.xmin + (fac * scrollsize);
		
		fac= (v2d->cur.xmax - v2d->tot.xmin) / totsize;
		//if (fac > 1.0f) fac= 1.0f;
		scrollers->hor_max= hor.xmin + (fac * scrollsize);
		
		if (scrollers->hor_min > scrollers->hor_max) 
			scrollers->hor_min= scrollers->hor_max;
	}
	
	/* vertical scrollers */
	if (v2d->scroll & VERT_SCROLL) {
		/* slider 'button' extents */
		totsize= v2d->tot.ymax - v2d->tot.ymin;
		scrollsize= vert.ymax - vert.ymin;
		
		fac= (v2d->cur.ymin- v2d->tot.ymin) / totsize;
		//if (fac < 0.0f) fac= 0.0f;
		scrollers->vert_min= vert.ymin + (fac * scrollsize);
		
		fac= (v2d->cur.ymax - v2d->tot.ymin) / totsize;
		//if (fac > 1.0f) fac= 1.0f;
		scrollers->vert_max= vert.ymin + (fac * scrollsize);
		
		if (scrollers->vert_min > scrollers->vert_max) 
			scrollers->vert_min= scrollers->vert_max;
	}
	
	/* grid markings on scrollbars */
	if (v2d->scroll & (HOR_SCROLLGRID|VERT_SCROLLGRID)) {
		/* store clamping */
		scrollers->xclamp= xclamp;
		scrollers->xunits= xunits;
		scrollers->yclamp= yclamp;
		scrollers->yunits= yunits;
		
		/* calculate grid */
		if (v2d->scroll & HOR_SCROLLGRID)
			scrollers->grid= UI_view2d_calc_grid(C, v2d, xunits, xclamp, (hor.xmax - hor.xmin), (vert.ymax - vert.ymin));
		else if (v2d->scroll & VERT_SCROLLGRID)
			scrollers->grid= UI_view2d_calc_grid(C, v2d, yunits, yclamp, (hor.xmax - hor.xmin), (vert.ymax - vert.ymin));
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
	if (power <= 0) sprintf(str, "%.*f", 1-power, val);
	else sprintf(str, "%d", (int)floor(val + 0.375));
	
	/* get length of string, and adjust printing location to fit it into the horizontal scrollbar */
	len= strlen(str);
	if (dir == 'h') x-= 4*len;  // err... why not just half len?
	
	/* Add degree sympbol to end of string for vertical scrollbar? */
	if ((dir == 'v') && (unit == V2D_UNIT_DEGREES)) {
		str[len]= 186;
		str[len+1]= 0;
	}
	
	/* draw it */
	ui_rasterpos_safe(x, y, 1.0);
	UI_DrawString(G.fonts, str, 0); // XXX check this again when new text-drawing api is done
}


/* Draw scrollbars in the given 2d-region */
void UI_view2d_draw_scrollers(const bContext *C, View2D *v2d, View2DScrollers *scrollers, int flag)
{
	const int darker= -40, dark= 0, light= 20, lighter= 50;
	rcti vert, hor;
	
	vert= v2d->vert;
	hor= v2d->hor;
	
	/* horizontal scrollbar */
	if (v2d->scroll & (HOR_SCROLL|HOR_SCROLLO)) {
		/* scroller backdrop */
		UI_ThemeColorShade(TH_SHADE1, light);
		glRecti(hor.xmin,  hor.ymin,  hor.xmax,  hor.ymax);
		
		/* slider 'button' */
			// FIXME: implement fancy one... but only when we get this working first!
		{
			UI_ThemeColorShade(TH_SHADE1, dark);
			glRecti(scrollers->hor_min,  hor.ymin,  scrollers->hor_max,  hor.ymax);
			
				/* draw lines on either end of 'box' */
			glLineWidth(2.0);
				UI_ThemeColorShade(TH_SHADE1, darker);
				sdrawline(scrollers->hor_min, hor.ymin, scrollers->hor_min, hor.ymax);
				sdrawline(scrollers->hor_max, hor.ymin, scrollers->hor_max, hor.ymax);
			glLineWidth(1.0);
		}
		
		/* scale indicators */
		// XXX will need to update the font drawing when the new stuff comes in
		if (v2d->scroll & HOR_SCROLLGRID) {
			View2DGrid *grid= scrollers->grid;
			float fac, dfac, fac2;
			int val;
			
			/* the numbers: convert grid->startx and -dx to scroll coordinates 
			 *	- fac is x-coordinate to draw to
			 *	- dfac is gap between scale markings
			 */
			fac= (grid->startx- v2d->cur.xmin) / (v2d->cur.xmax - v2d->cur.xmin);
			fac= hor.xmin + fac*(hor.xmax - hor.xmin);
			
			dfac= (grid->dx) / (v2d->cur.xmax - v2d->cur.xmin);
			dfac= dfac*(hor.xmax-hor.xmin);
			
			UI_ThemeColor(TH_TEXT);
			val= grid->startx;
			
			/* if we're clamping to whole numbers only, make sure entries won't be repeated */
			if (scrollers->xclamp == V2D_GRID_CLAMP) {
				while (grid->dx < 0.9999f) {
					grid->dx *= 2.0f;
					dfac *= 2.0f;
				}
			}
			if (scrollers->xunits == V2D_UNIT_FRAMES)
				grid->powerx= 1;
			
			/* draw numbers in the appropriate range */
			if (dfac != 0.0f) {
				for (; fac < hor.xmax; fac+=dfac, val+=grid->dx) {
					switch (scrollers->xunits) {
						case V2D_UNIT_FRAMES:		/* frames (as whole numbers)*/
							scroll_printstr(scrollers, fac, 3.0+(float)(hor.ymin), val, grid->powerx, V2D_UNIT_FRAMES, 'h');
							break;
						
						case V2D_UNIT_SECONDS:		/* seconds */
							fac2= val/FPS;
							scroll_printstr(scrollers, fac, 3.0+(float)(hor.ymin), fac2, grid->powerx, V2D_UNIT_SECONDS, 'h');
							break;
							
						case V2D_UNIT_SECONDSSEQ:	/* seconds with special calculations (only used for sequencer only) */
						{
							float time;
							
							fac2= val/FPS;
							time= floor(fac2);
							fac2= fac2-time;
							
							scroll_printstr(scrollers, fac, 3.0+(float)(hor.ymin), time+FPS*fac2/100.0, grid->powerx, V2D_UNIT_SECONDSSEQ, 'h');
						}
							break;
							
						case V2D_UNIT_DEGREES:		/* IPO-Editor for rotation IPO-Drivers */
							/* HACK: although we're drawing horizontal, we make this draw as 'vertical', just to get degree signs */
							scroll_printstr(scrollers, fac, 3.0+(float)(hor.ymin), val, grid->powerx, V2D_UNIT_DEGREES, 'v');
							break;
					}
				}
			}
		}
		
		/* decoration outer bevel line */
		UI_ThemeColorShade(TH_SHADE1, lighter);
		if (v2d->scroll & B_SCROLL)
			sdrawline(hor.xmin, hor.ymax, hor.xmax, hor.ymax);
		else if (v2d->scroll & T_SCROLL)
			sdrawline(hor.xmin, hor.ymin, hor.xmax, hor.ymin);
	}
	
	/* vertical scrollbar */
	if (v2d->scroll & VERT_SCROLL) {
		/* scroller backdrop  */
		UI_ThemeColorShade(TH_SHADE1, light);
		glRecti(vert.xmin,  vert.ymin,  vert.xmax,  vert.ymax);
		
		/* slider 'button' */
			// FIXME: implement fancy one... but only when we get this working first!
		{
			UI_ThemeColorShade(TH_SHADE1, dark);
			glRecti(vert.xmin,  scrollers->vert_min,  vert.xmax,  scrollers->vert_max);
			
				/* draw lines on either end of 'box' */
			glLineWidth(2.0);
				UI_ThemeColorShade(TH_SHADE1, darker);
				sdrawline(vert.xmin, scrollers->vert_min, vert.xmax, scrollers->vert_min);
				sdrawline(vert.xmin, scrollers->vert_max, vert.xmax, scrollers->vert_max);
			glLineWidth(1.0);
		}
		
		/* scale indiators */
		// XXX will need to update the font drawing when the new stuff comes in
		if (v2d->scroll & VERT_SCROLLGRID) {
			
		}	
		
		/* decoration outer bevel line */
		UI_ThemeColorShade(TH_SHADE1, lighter);
		if (v2d->scroll & R_SCROLL)
			sdrawline(vert.xmin, vert.ymin, vert.xmin, vert.ymax);
		else 
			sdrawline(vert.xmax, vert.ymin, vert.xmax, vert.ymax);
	}
}

/* free temporary memory used for drawing scrollers */
void UI_view2d_free_scrollers(View2DScrollers *scrollers)
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

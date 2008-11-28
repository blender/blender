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

#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"

#include "BKE_global.h"

#include "WM_api.h"

#include "BIF_gl.h"

#include "UI_resources.h"
#include "UI_view2d.h"

/* *********************************************************************** */
/* Setup and Refresh Code */

/* Set view matrices to ortho for View2D drawing */
void UI_view2d_ortho(const bContext *C, View2D *v2d)
{
	wmOrtho2(C->window, v2d->cur.xmin, v2d->cur.xmax, v2d->cur.ymin, v2d->cur.ymax);
}

/* Adjust mask size in response to view size changes */
// XXX pre2.5 -> this used to be called  calc_scrollrcts()
void UI_view2d_update_size(View2D *v2d, int winx, int winy)
{
	/* mask - view frame */
	v2d->mask.xmin= v2d->mask.ymin= 0;
	v2d->mask.xmax= winx;
	v2d->mask.ymax= winy;
	
	/* scrollbars shrink mask area, but should be based off regionsize */
	// XXX scrollbars should become limited to one bottom lower edges of region like everyone else does!
	if(v2d->scroll) {
		/* vertical scrollbar */
		if (v2d->scroll & L_SCROLL) {
			/* on left-hand edge of region */
			v2d->vert= v2d->mask;
			v2d->vert.xmax= SCROLLB;
			v2d->mask.xmin= SCROLLB;
		}
		else if(v2d->scroll & R_SCROLL) {
			/* on right-hand edge of region */
			v2d->vert= v2d->mask;
			v2d->vert.xmin= v2d->vert.xmax-SCROLLB;
			v2d->mask.xmax= v2d->vert.xmin;
		}
		
		/* horizontal scrollbar */
		if ((v2d->scroll & B_SCROLL) || (v2d->scroll & B_SCROLLO)) {
			/* on bottom edge of region (B_SCROLLO is outliner, the ohter is for standard) */
			v2d->hor= v2d->mask;
			v2d->hor.ymax= SCROLLH;
			v2d->mask.ymin= SCROLLH;
		}
		else if(v2d->scroll & T_SCROLL) {
			/* on upper edge of region */
			v2d->hor= v2d->mask;
			v2d->hor.ymin= v2d->hor.ymax-SCROLLH;
			v2d->mask.ymax= v2d->hor.ymin;
		}
	}
}

/* *********************************************************************** */
/* Gridlines */

/* minimum pixels per gridstep */
#define MINGRIDSTEP 	35

/* View2DGrid is typedef'd in UI_view2d.h */
struct View2DGrid {
	float dx, dy;			/* stepsize (in pixels) between gridlines */
	float startx, starty;	/* */
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
	
	if (loga < 0.0f) {
		if (rem < 0.2f) rem= 0.2f;
		else if(rem < 0.5f) rem= 0.5f;
		else rem= 1.0f;
		
		*step= rem * pow(10.0, (double)(*power));
		
		/* for frames, we want 1.0 frame intervals only */
		if (unit == V2D_UNIT_FRAMES) {
			rem = 1.0f;
			*step = 1.0f;
		}
		
		/* prevents printing 1.0 2.0 3.0 etc */
		if (rem == 1.0f) (*power)++;	
	}
	else {
		if (rem < 2.0f) rem= 2.0f;
		else if(rem < 5.0f) rem= 5.0f;
		else rem= 10.0f;
		
		*step= rem * pow(10.0, (double)(*power));
		
		(*power)++;
		/* prevents printing 1.0, 2.0, 3.0, etc. */
		if (rem == 10.0f) (*power)++;	
	}
}

/* Intialise settings necessary for drawing gridlines in a 2d-view 
 *	- Currently, will return pointer to View2DGrid struct that needs to 
 *	  be freed with UI_view2d_free_grid()
 *	- Is used for scrollbar drawing too (for units drawing)  --> (XXX needs review)
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
		secondgrid= 0;
		seconddiv= 0.01f * FPS;
	}
	else {
		secondgrid= 1;
		seconddiv= 1.0f;
	}
	
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
	
	space= (v2d->cur.ymax - v2d->cur.ymin);
	pixels= winy;
	grid->dy= MINGRIDSTEP*space/pixels;
	step_to_grid(&grid->dy, &grid->powery, unit);
	
	if (clamp == V2D_GRID_CLAMP) {
		if (grid->dy < 1.0f) grid->dy= 1.0f;
		if (grid->powery < 1) grid->powery= 1;
	}
	
	grid->startx= seconddiv*(v2d->cur.xmin/seconddiv - fmod(v2d->cur.xmin/seconddiv, grid->dx/seconddiv));
	if (v2d->cur.xmin < 0.0f) grid->startx-= grid->dx;
	
	grid->starty= (v2d->cur.ymin-fmod(v2d->cur.ymin, grid->dy));
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
		
		vec2[1]= vec1[1]-= 0.5f*grid->dy;
		step++;
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
/* Coordinate Conversions */

/* Convert from screen/region space to 2d-View space 
 *	
 *	- x,y 			= coordinates to convert
 *	- viewx,viewy		= resultant coordinates
 */
void UI_view2d_region_to_view(View2D *v2d, short x, short y, float *viewx, float *viewy)
{
	float div, ofs;

	if (viewx) {
		div= v2d->mask.xmax - v2d->mask.xmin;
		ofs= v2d->mask.xmin;
		
		*viewx= v2d->cur.xmin + (v2d->cur.xmax-v2d->cur.xmin) * (x - ofs) / div;
	}

	if (viewy) {
		div= v2d->mask.ymax - v2d->mask.ymin;
		ofs= v2d->mask.ymin;
		
		*viewy= v2d->cur.ymin + (v2d->cur.ymax - v2d->cur.ymin) * (y - ofs) / div;
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
	y= (x - v2d->cur.ymin) / (v2d->cur.ymax - v2d->cur.ymin);
	
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
	y= (x - v2d->cur.ymin) / (v2d->cur.ymax - v2d->cur.ymin);
	
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


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

/* minimum pixels per gridstep */
#define IPOSTEP 35

struct View2DGrid {
	float dx, dy, startx, starty;
	int machtx, machty;
};

/* Setup */

void UI_view2d_ortho(const bContext *C, View2D *v2d)
{
	wmOrtho2(C->window, v2d->cur.xmin, v2d->cur.xmax, v2d->cur.ymin, v2d->cur.ymax);
}

void UI_view2d_update_size(View2D *v2d, int winx, int winy)
{
	v2d->mask.xmin= v2d->mask.ymin= 0;
	v2d->mask.xmax= winx;
	v2d->mask.ymax= winy;
	
#if 0
	if(sa->spacetype==SPACE_ACTION) {
		if(sa->winx > ACTWIDTH+50) { 
			v2d->mask.xmin+= ACTWIDTH;
			v2d->hor.xmin+=ACTWIDTH;
		}
	}
	else if(sa->spacetype==SPACE_NLA){
		if(sa->winx > NLAWIDTH+50) { 
			v2d->mask.xmin+= NLAWIDTH;
			v2d->hor.xmin+=NLAWIDTH;
		}
	}
	else if(sa->spacetype==SPACE_IPO) {
		int ipobutx = calc_ipobuttonswidth(sa);
		
		v2d->mask.xmax-= ipobutx;
		
		if(v2d->mask.xmax<ipobutx)
			v2d->mask.xmax= winx;
	}
#endif
	
	if(v2d->scroll) {
		if(v2d->scroll & L_SCROLL) {
			v2d->vert= v2d->mask;
			v2d->vert.xmax= SCROLLB;
			v2d->mask.xmin= SCROLLB;
		}
		else if(v2d->scroll & R_SCROLL) {
			v2d->vert= v2d->mask;
			v2d->vert.xmin= v2d->vert.xmax-SCROLLB;
			v2d->mask.xmax= v2d->vert.xmin;
		}
		
		if((v2d->scroll & B_SCROLL) || (v2d->scroll & B_SCROLLO)) {
			v2d->hor= v2d->mask;
			v2d->hor.ymax= SCROLLH;
			v2d->mask.ymin= SCROLLH;
		}
		else if(v2d->scroll & T_SCROLL) {
			v2d->hor= v2d->mask;
			v2d->hor.ymin= v2d->hor.ymax-SCROLLH;
			v2d->mask.ymax= v2d->hor.ymin;
		}
	}
}

/* Grid */

static void step_to_grid(float *step, int *macht, int unit)
{
	float loga, rem;
	
	/* try to write step as a power of 10 */
	
	loga= log10(*step);
	*macht= (int)(loga);

	rem= loga- *macht;
	rem= pow(10.0, rem);
	
	if(loga<0.0) {
		if(rem < 0.2) rem= 0.2;
		else if(rem < 0.5) rem= 0.5;
		else rem= 1.0;

		*step= rem*pow(10.0, (float)*macht);

		if(unit == V2D_UNIT_FRAMES) {
			rem = 1.0;
			*step = 1.0;
		}

		if(rem==1.0) (*macht)++;	// prevents printing 1.0 2.0 3.0 etc
	}
	else {
		if(rem < 2.0) rem= 2.0;
		else if(rem < 5.0) rem= 5.0;
		else rem= 10.0;
		
		*step= rem*pow(10.0, (float)*macht);
		
		(*macht)++;
		if(rem==10.0) (*macht)++;	// prevents printing 1.0 2.0 3.0 etc
	}
}

View2DGrid *UI_view2d_calc_grid(const bContext *C, View2D *v2d, int unit, int clamp, int winx, int winy)
{
	View2DGrid *grid;
	float space, pixels, seconddiv;
	int secondgrid;

	grid= MEM_callocN(sizeof(View2DGrid), "View2DGrid");

	/* rule: gridstep is minimal IPOSTEP pixels */
	/* how large is IPOSTEP pixels? */
	
	if(unit == V2D_UNIT_FRAMES) {
		secondgrid= 0;
		seconddiv= 0.01f * FPS;
	}
	else {
		secondgrid= 1;
		seconddiv= 1.0f;
	}

	space= v2d->cur.xmax - v2d->cur.xmin;
	pixels= v2d->mask.xmax - v2d->mask.xmin;
	
	grid->dx= IPOSTEP*space/(seconddiv*pixels);
	step_to_grid(&grid->dx, &grid->machtx, unit);
	grid->dx*= seconddiv;
	
	if(clamp == V2D_GRID_CLAMP) {
		if(grid->dx < 0.1) grid->dx= 0.1;
		grid->machtx-= 2;
		if(grid->machtx<-2) grid->machtx= -2;
	}
	
	space= (v2d->cur.ymax - v2d->cur.ymin);
	pixels= winy;
	grid->dy= IPOSTEP*space/pixels;
	step_to_grid(&grid->dy, &grid->machty, unit);
	
	if(clamp == V2D_GRID_CLAMP) {
		if(grid->dy < 1.0) grid->dy= 1.0;
		if(grid->machty<1) grid->machty= 1;
	}
	
	grid->startx= seconddiv*(v2d->cur.xmin/seconddiv - fmod(v2d->cur.xmin/seconddiv, grid->dx/seconddiv));
	if(v2d->cur.xmin<0.0) grid->startx-= grid->dx;
	
	grid->starty= (v2d->cur.ymin-fmod(v2d->cur.ymin, grid->dy));
	if(v2d->cur.ymin<0.0) grid->starty-= grid->dy;

	return grid;
}

void UI_view2d_draw_grid(const bContext *C, View2D *v2d, View2DGrid *grid, int flag)
{
	float vec1[2], vec2[2];
	int a, step;
	
	if(flag & V2D_VERTICAL_LINES) {
		/* vertical lines */
		vec1[0]= vec2[0]= grid->startx;
		vec1[1]= grid->starty;
		vec2[1]= v2d->cur.ymax;
		
		step= (v2d->mask.xmax - v2d->mask.xmin+1)/IPOSTEP;
		
		UI_ThemeColor(TH_GRID);
		
		for(a=0; a<step; a++) {
			glBegin(GL_LINE_STRIP);
			glVertex2fv(vec1); glVertex2fv(vec2);
			glEnd();
			vec2[0]= vec1[0]+= grid->dx;
		}
		
		vec2[0]= vec1[0]-= 0.5*grid->dx;
		
		UI_ThemeColorShade(TH_GRID, 16);
		
		step++;
		for(a=0; a<=step; a++) {
			glBegin(GL_LINE_STRIP);
			glVertex2fv(vec1); glVertex2fv(vec2);
			glEnd();
			vec2[0]= vec1[0]-= grid->dx;
		}
	}
	
	if(flag & V2D_HORIZONTAL_LINES) {
		/* horizontal lines */
		vec1[0]= grid->startx;
		vec1[1]= vec2[1]= grid->starty;
		vec2[0]= v2d->cur.xmax;
		
		step= (C->area->winy+1)/IPOSTEP;
		
		UI_ThemeColor(TH_GRID);
		for(a=0; a<=step; a++) {
			glBegin(GL_LINE_STRIP);
			glVertex2fv(vec1); glVertex2fv(vec2);
			glEnd();
			vec2[1]= vec1[1]+= grid->dy;
		}
		vec2[1]= vec1[1]-= 0.5*grid->dy;
		step++;
	}
	
	UI_ThemeColorShade(TH_GRID, -50);
	
	if(flag & V2D_HORIZONTAL_AXIS) {
		/* horizontal axis */
		vec1[0]= v2d->cur.xmin;
		vec2[0]= v2d->cur.xmax;
		vec1[1]= vec2[1]= 0.0;
		glBegin(GL_LINE_STRIP);
		
		glVertex2fv(vec1);
		glVertex2fv(vec2);
		
		glEnd();
	}
	
	if(flag & V2D_VERTICAL_AXIS) {
		/* vertical axis */
		vec1[1]= v2d->cur.ymin;
		vec2[1]= v2d->cur.ymax;
		vec1[0]= vec2[0]= 0.0;
		glBegin(GL_LINE_STRIP);
		glVertex2fv(vec1); glVertex2fv(vec2);
		glEnd();
	}
}

void UI_view2d_free_grid(View2DGrid *grid)
{
	MEM_freeN(grid);
}

/* Coordinate conversion */

void UI_view2d_region_to_view(View2D *v2d, short x, short y, float *viewx, float *viewy)
{
	float div, ofs;

	if(viewx) {
		div= v2d->mask.xmax-v2d->mask.xmin;
		ofs= v2d->mask.xmin;

		*viewx= v2d->cur.xmin+ (v2d->cur.xmax-v2d->cur.xmin)*(x-ofs)/div;
	}

	if(viewy) {
		div= v2d->mask.ymax-v2d->mask.ymin;
		ofs= v2d->mask.ymin;

		*viewy= v2d->cur.ymin+ (v2d->cur.ymax-v2d->cur.ymin)*(y-ofs)/div;
	}
}

void UI_view2d_view_to_region(View2D *v2d, float x, float y, short *regionx, short *regiony)
{
	*regionx= V2D_IS_CLIPPED;
	*regiony= V2D_IS_CLIPPED;

	x= (x - v2d->cur.xmin)/(v2d->cur.xmax-v2d->cur.xmin);
	y= (x - v2d->cur.ymin)/(v2d->cur.ymax-v2d->cur.ymin);

	if(x>=0.0 && x<=1.0) {
		if(y>=0.0 && y<=1.0) {
			if(regionx)
				*regionx= v2d->mask.xmin + x*(v2d->mask.xmax-v2d->mask.xmin);
			if(regiony)
				*regiony= v2d->mask.ymin + y*(v2d->mask.ymax-v2d->mask.ymin);
		}
	}
}

void UI_view2d_to_region_no_clip(View2D *v2d, float x, float y, short *regionx, short *regiony)
{
	x= (x - v2d->cur.xmin)/(v2d->cur.xmax-v2d->cur.xmin);
	y= (x - v2d->cur.ymin)/(v2d->cur.ymax-v2d->cur.ymin);

	x= v2d->mask.xmin + x*(v2d->mask.xmax-v2d->mask.xmin);
	y= v2d->mask.ymin + y*(v2d->mask.ymax-v2d->mask.ymin);

	if(regionx) {
		if(x<-32760) *regionx= -32760;
		else if(x>32760) *regionx= 32760;
		else *regionx= x;
	}

	if(regiony) {
		if(y<-32760) *regiony= -32760;
		else if(y>32760) *regiony= 32760;
		else *regiony= y;
	}
}



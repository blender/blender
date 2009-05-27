/**
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_arithb.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLF_api.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_util.h"
#include "ED_types.h"

#include "interface_intern.h"

/* ************** widget base functions ************** */
/*
     - in: roundbox codes for corner types and radius
     - return: array of [size][2][x,y] points, the edges of the roundbox, + UV coords
 
     - draw black box with alpha 0 on exact button boundbox
     - for ever AA step:
        - draw the inner part for a round filled box, with color blend codes or texture coords
        - draw outline in outline color
        - draw outer part, bottom half, extruded 1 pixel to bottom, for emboss shadow
        - draw extra decorations
     - draw background color box with alpha 1 on exact button boundbox
 
 */

/* fill this struct with polygon info to draw AA'ed */
/* it has outline, back, and two optional tria meshes */

typedef struct uiWidgetTrias {
	int tot;
	
	float vec[32][2];
	int (*index)[3];
	
} uiWidgetTrias;

typedef struct uiWidgetStateColors {
	char inner_anim[4];
	char inner_anim_sel[4];
	char inner_key[4];
	char inner_key_sel[4];
	char inner_driven[4];
	char inner_driven_sel[4];
} uiWidgetStateColors;

typedef struct uiWidgetBase {
	
	int totvert, halfwayvert;
	float outer_v[64][2];
	float inner_v[64][2];
	float inner_uv[64][2];
	
	short inner, outline, emboss; /* set on/off */
	
	uiWidgetTrias tria1;
	uiWidgetTrias tria2;
	
} uiWidgetBase;

/* uiWidgetType: for time being only for visual appearance,
   later, a handling callback can be added too 
*/
typedef struct uiWidgetType {
	
	/* pointer to theme color definition */
	uiWidgetColors *wcol_theme;
	
	/* converted colors for state */
	uiWidgetColors wcol;
	
	void (*state)(struct uiWidgetType *, int state);
	void (*draw)(uiWidgetColors *, rcti *, int state, int roundboxalign);
	void (*custom)(uiBut *, uiWidgetColors *, rcti *, int state, int roundboxalign);
	void (*text)(uiFontStyle *, uiWidgetColors *, uiBut *, rcti *);
	
} uiWidgetType;


/* *********************** draw data ************************** */

static float cornervec[9][2]= {{0.0, 0.0}, {0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, 
{0.707, 0.293}, {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}, {1.0, 1.0}};

static float jit[8][2]= {{0.468813 , -0.481430}, {-0.155755 , -0.352820}, 
{0.219306 , -0.238501},  {-0.393286 , -0.110949}, {-0.024699 , 0.013908}, 
{0.343805 , 0.147431}, {-0.272855 , 0.269918}, {0.095909 , 0.388710}};

static float num_tria_vert[19][2]= {
{0.382684, 0.923879}, {0.000001, 1.000000}, {-0.382683, 0.923880}, {-0.707107, 0.707107},
{-0.923879, 0.382684}, {-1.000000, 0.000000}, {-0.923880, -0.382684}, {-0.707107, -0.707107},
{-0.382683, -0.923880}, {0.000000, -1.000000}, {0.382684, -0.923880}, {0.707107, -0.707107},
{0.923880, -0.382684}, {1.000000, -0.000000}, {0.923880, 0.382683}, {0.707107, 0.707107}, 
{-0.352077, 0.532607}, {-0.352077, -0.549313}, {0.729843, -0.008353}};

static int num_tria_face[19][3]= {
{13, 14, 18}, {17, 5, 6}, {12, 13, 18}, {17, 6, 7}, {15, 18, 14}, {16, 4, 5}, {16, 5, 17}, {18, 11, 12}, 
{18, 17, 10}, {18, 10, 11}, {17, 9, 10}, {15, 0, 18}, {18, 0, 16}, {3, 4, 16}, {8, 9, 17}, {8, 17, 7}, 
{2, 3, 16}, {1, 2, 16}, {16, 0, 1}};

static float menu_tria_vert[6][2]= {
{-0.41, 0.16}, {0.41, 0.16}, {0, 0.82}, 
{0, -0.82}, {-0.41, -0.16}, {0.41, -0.16}};

static int menu_tria_face[2][3]= {{2, 0, 1}, {3, 5, 4}};

static float check_tria_vert[6][2]= {
{-0.578579, 0.253369}, 	{-0.392773, 0.412794}, 	{-0.004241, -0.328551}, 
{-0.003001, 0.034320}, 	{1.055313, 0.864744}, 	{0.866408, 1.026895}};

static int check_tria_face[4][3]= {
{3, 2, 4}, {3, 4, 5}, {1, 0, 3}, {0, 2, 3}};

/* ************************************************* */

void ui_draw_anti_tria(float x1, float y1, float x2, float y2, float x3, float y3)
{
	float color[4];
	int j;
	
	glEnable(GL_BLEND);
	glGetFloatv(GL_CURRENT_COLOR, color);
	color[3]= 0.125;
	glColor4fv(color);
	
	/* for each AA step */
	for(j=0; j<8; j++) {
		glTranslatef(1.0*jit[j][0], 1.0*jit[j][1], 0.0f);

		glBegin(GL_POLYGON);
		glVertex2f(x1, y1);
		glVertex2f(x2, y2);
		glVertex2f(x3, y3);
		glEnd();
		
		glTranslatef(-1.0*jit[j][0], -1.0*jit[j][1], 0.0f);
	}

	glDisable(GL_BLEND);
	
}

static void widget_init(uiWidgetBase *wtb)
{
	wtb->totvert= wtb->halfwayvert= 0;
	wtb->tria1.tot= 0;
	wtb->tria2.tot= 0;
	
	wtb->inner= 1;
	wtb->outline= 1;
	wtb->emboss= 1;
}

/* helper call, makes shadow rect, with 'sun' above menu, so only shadow to left/right/bottom */
/* return tot */
static int round_box_shadow_edges(float (*vert)[2], rcti *rect, float rad, int roundboxalign, float step)
{
	float vec[9][2];
	float minx, miny, maxx, maxy;
	int a, tot= 0;
	
	rad+= step;
	
	if(2.0f*rad > rect->ymax-rect->ymin)
		rad= 0.5f*(rect->ymax-rect->ymin);
	
	minx= rect->xmin-step;
	miny= rect->ymin-step;
	maxx= rect->xmax+step;
	maxy= rect->ymax+step;
	
	/* mult */
	for(a=0; a<9; a++) {
		vec[a][0]= rad*cornervec[a][0]; 
		vec[a][1]= rad*cornervec[a][1]; 
	}
	
	/* start with left-top, anti clockwise */
	if(roundboxalign & 1) {
		for(a=0; a<9; a++, tot++) {
			vert[tot][0]= minx+rad-vec[a][0];
			vert[tot][1]= maxy-vec[a][1];
		}
	}
	else {
		for(a=0; a<9; a++, tot++) {
			vert[tot][0]= minx;
			vert[tot][1]= maxy;
		}
	}
	
	if(roundboxalign & 8) {
		for(a=0; a<9; a++, tot++) {
			vert[tot][0]= minx+vec[a][1];
			vert[tot][1]= miny+rad-vec[a][0];
		}
	}
	else {
		for(a=0; a<9; a++, tot++) {
			vert[tot][0]= minx;
			vert[tot][1]= miny;
		}
	}
	
	if(roundboxalign & 4) {
		for(a=0; a<9; a++, tot++) {
			vert[tot][0]= maxx-rad+vec[a][0];
			vert[tot][1]= miny+vec[a][1];
		}
	}
	else {
		for(a=0; a<9; a++, tot++) {
			vert[tot][0]= maxx;
			vert[tot][1]= miny;
		}
	}
	
	if(roundboxalign & 2) {
		for(a=0; a<9; a++, tot++) {
			vert[tot][0]= maxx-vec[a][1];
			vert[tot][1]= maxy-rad+vec[a][0];
		}
	}
	else {
		for(a=0; a<9; a++, tot++) {
			vert[tot][0]= maxx;
			vert[tot][1]= maxy;
		}
	}
	return tot;
}



static void round_box_edges(uiWidgetBase *wt, int roundboxalign, rcti *rect, float rad)
{
	float vec[9][2], veci[9][2];
	float minx= rect->xmin, miny= rect->ymin, maxx= rect->xmax, maxy= rect->ymax;
	float radi;				  /* rad inner */
	float minxi= minx + 1.0f; /* boundbox inner */
	float maxxi= maxx - 1.0f;
	float minyi= miny + 1.0f;
	float maxyi= maxy - 1.0f;
	float facxi= 1.0f/(maxxi-minxi); /* for uv */
	float facyi= 1.0f/(maxyi-minyi);
	int a, tot= 0;
	
	if(2.0f*rad > rect->ymax-rect->ymin)
		rad= 0.5f*(rect->ymax-rect->ymin);

	radi= rad - 1.0f;
	
	/* mult */
	for(a=0; a<9; a++) {
		veci[a][0]= radi*cornervec[a][0]; 
		veci[a][1]= radi*cornervec[a][1]; 
		vec[a][0]= rad*cornervec[a][0]; 
		vec[a][1]= rad*cornervec[a][1]; 
	}
	
	/* corner left-bottom */
	if(roundboxalign & 8) {
		
		for(a=0; a<9; a++, tot++) {
			wt->inner_v[tot][0]= minxi+veci[a][1];
			wt->inner_v[tot][1]= minyi+radi-veci[a][0];
			
			wt->outer_v[tot][0]= minx+vec[a][1];
			wt->outer_v[tot][1]= miny+rad-vec[a][0];
			
			wt->inner_uv[tot][0]= facxi*(wt->inner_v[tot][0] - minxi);
			wt->inner_uv[tot][1]= facyi*(wt->inner_v[tot][1] - minyi);
		}
	}
	else {
		wt->inner_v[tot][0]= minxi;
		wt->inner_v[tot][1]= minyi;
		
		wt->outer_v[tot][0]= minx;
		wt->outer_v[tot][1]= miny;

		wt->inner_uv[tot][0]= 0.0f;
		wt->inner_uv[tot][1]= 0.0f;
		
		tot++;
	}
	
	/* corner right-bottom */
	if(roundboxalign & 4) {
		
		for(a=0; a<9; a++, tot++) {
			wt->inner_v[tot][0]= maxxi-radi+veci[a][0];
			wt->inner_v[tot][1]= minyi+veci[a][1];
			
			wt->outer_v[tot][0]= maxx-rad+vec[a][0];
			wt->outer_v[tot][1]= miny+vec[a][1];
			
			wt->inner_uv[tot][0]= facxi*(wt->inner_v[tot][0] - minxi);
			wt->inner_uv[tot][1]= facyi*(wt->inner_v[tot][1] - minyi);
		}
	}
	else {
		wt->inner_v[tot][0]= maxxi;
		wt->inner_v[tot][1]= minyi;
		
		wt->outer_v[tot][0]= maxx;
		wt->outer_v[tot][1]= miny;

		wt->inner_uv[tot][0]= 1.0f;
		wt->inner_uv[tot][1]= 0.0f;
		
		tot++;
	}
	
	wt->halfwayvert= tot;
	
	/* corner right-top */
	if(roundboxalign & 2) {
		
		for(a=0; a<9; a++, tot++) {
			wt->inner_v[tot][0]= maxxi-veci[a][1];
			wt->inner_v[tot][1]= maxyi-radi+veci[a][0];
			
			wt->outer_v[tot][0]= maxx-vec[a][1];
			wt->outer_v[tot][1]= maxy-rad+vec[a][0];
			
			wt->inner_uv[tot][0]= facxi*(wt->inner_v[tot][0] - minxi);
			wt->inner_uv[tot][1]= facyi*(wt->inner_v[tot][1] - minyi);
		}
	}
	else {
		wt->inner_v[tot][0]= maxxi;
		wt->inner_v[tot][1]= maxyi;
		
		wt->outer_v[tot][0]= maxx;
		wt->outer_v[tot][1]= maxy;
		
		wt->inner_uv[tot][0]= 1.0f;
		wt->inner_uv[tot][1]= 1.0f;
		
		tot++;
	}
	
	/* corner left-top */
	if(roundboxalign & 1) {
		
		for(a=0; a<9; a++, tot++) {
			wt->inner_v[tot][0]= minxi+radi-veci[a][0];
			wt->inner_v[tot][1]= maxyi-veci[a][1];
			
			wt->outer_v[tot][0]= minx+rad-vec[a][0];
			wt->outer_v[tot][1]= maxy-vec[a][1];
			
			wt->inner_uv[tot][0]= facxi*(wt->inner_v[tot][0] - minxi);
			wt->inner_uv[tot][1]= facyi*(wt->inner_v[tot][1] - minyi);
		}
		
	}
	else {
		
		wt->inner_v[tot][0]= minxi;
		wt->inner_v[tot][1]= maxyi;
		
		wt->outer_v[tot][0]= minx;
		wt->outer_v[tot][1]= maxy;
		
		wt->inner_uv[tot][0]= 0.0f;
		wt->inner_uv[tot][1]= 1.0f;
		
		tot++;
	}
		
	wt->totvert= tot;
}

/* based on button rect, return scaled array of triangles */
static void widget_num_tria(uiWidgetTrias *tria, rcti *rect, float triasize, char where)
{
	float centx, centy, size;
	int a;
	
	/* center position and size */
	centx= (float)rect->xmin + 0.5f*(rect->ymax-rect->ymin);
	centy= (float)rect->ymin + 0.5f*(rect->ymax-rect->ymin);
	size= -0.5f*triasize*(rect->ymax-rect->ymin);

	if(where=='r') {
		centx= (float)rect->xmax - 0.5f*(rect->ymax-rect->ymin);
		size= -size;
	}	
	
	for(a=0; a<19; a++) {
		tria->vec[a][0]= size*num_tria_vert[a][0] + centx;
		tria->vec[a][1]= size*num_tria_vert[a][1] + centy;
	}
	
	tria->tot= 19;
	tria->index= num_tria_face;
}

static void widget_trias_draw(uiWidgetTrias *tria)
{
	int a;
	
	glBegin(GL_TRIANGLES);
	for(a=0; a<tria->tot; a++) {
		glVertex2fv(tria->vec[ tria->index[a][0] ]);
		glVertex2fv(tria->vec[ tria->index[a][1] ]);
		glVertex2fv(tria->vec[ tria->index[a][2] ]);
	}
	glEnd();
	
}

static void widget_menu_trias(uiWidgetTrias *tria, rcti *rect)
{
	float centx, centy, size, asp;
	int a;
		
	/* center position and size */
	centx= rect->xmax - 0.5f*(rect->ymax-rect->ymin);
	centy= rect->ymin + 0.5f*(rect->ymax-rect->ymin);
	size= 0.4f*(rect->ymax-rect->ymin);
	
	/* XXX exception */
	asp= ((float)rect->xmax-rect->xmin)/((float)rect->ymax-rect->ymin);
	if(asp > 1.2f && asp < 2.6f)
		centx= rect->xmax - 0.3f*(rect->ymax-rect->ymin);
	
	for(a=0; a<6; a++) {
		tria->vec[a][0]= size*menu_tria_vert[a][0] + centx;
		tria->vec[a][1]= size*menu_tria_vert[a][1] + centy;
	}

	tria->tot= 2;
	tria->index= menu_tria_face;
}

static void widget_check_trias(uiWidgetTrias *tria, rcti *rect)
{
	float centx, centy, size;
	int a;
	
	/* center position and size */
	centx= rect->xmin + 0.5f*(rect->ymax-rect->ymin);
	centy= rect->ymin + 0.5f*(rect->ymax-rect->ymin);
	size= 0.5f*(rect->ymax-rect->ymin);
	
	for(a=0; a<6; a++) {
		tria->vec[a][0]= size*check_tria_vert[a][0] + centx;
		tria->vec[a][1]= size*check_tria_vert[a][1] + centy;
	}
	
	tria->tot= 4;
	tria->index= check_tria_face;
}


/* prepares shade colors */
static void shadecolors4(char *coltop, char *coldown, char *color, short shadetop, short shadedown)
{
	
	coltop[0]= CLAMPIS(color[0]+shadetop, 0, 255);
	coltop[1]= CLAMPIS(color[1]+shadetop, 0, 255);
	coltop[2]= CLAMPIS(color[2]+shadetop, 0, 255);
	coltop[3]= color[3];

	coldown[0]= CLAMPIS(color[0]+shadedown, 0, 255);
	coldown[1]= CLAMPIS(color[1]+shadedown, 0, 255);
	coldown[2]= CLAMPIS(color[2]+shadedown, 0, 255);
	coldown[3]= color[3];
}

static void round_box_shade_col4(char *col1, char *col2, float fac)
{
	int faci, facm;
	char col[4];
	
	faci= floor(255.1f*fac);
	facm= 255-faci;
	
	col[0]= (faci*col1[0] + facm*col2[0])>>8;
	col[1]= (faci*col1[1] + facm*col2[1])>>8;
	col[2]= (faci*col1[2] + facm*col2[2])>>8;
	col[3]= (faci*col1[3] + facm*col2[3])>>8;
	
	glColor4ubv(col);
}

static void widgetbase_draw(uiWidgetBase *wtb, uiWidgetColors *wcol)
{
	int j, a;
	
	glEnable(GL_BLEND);

	/* backdrop non AA */
	if(wtb->inner) {
		if(wcol->shaded==0) {
			/* filled center, solid */
			glColor4ubv(wcol->inner);
			glBegin(GL_POLYGON);
			for(a=0; a<wtb->totvert; a++)
				glVertex2fv(wtb->inner_v[a]);
			glEnd();
		}
		else {
			char col1[4], col2[4];
			
			shadecolors4(col1, col2, wcol->inner, wcol->shadetop, wcol->shadedown);
			
			glShadeModel(GL_SMOOTH);
			glBegin(GL_POLYGON);
			for(a=0; a<wtb->totvert; a++) {
				round_box_shade_col4(col1, col2, wtb->inner_uv[a][1]);
				glVertex2fv(wtb->inner_v[a]);
			}
			glEnd();
			glShadeModel(GL_FLAT);
		}
	}
	
	/* for each AA step */
	if(wtb->outline) {
		for(j=0; j<8; j++) {
			glTranslatef(1.0*jit[j][0], 1.0*jit[j][1], 0.0f);
			
			/* outline */
			glColor4ub(wcol->outline[0], wcol->outline[1], wcol->outline[2], 32);
			glBegin(GL_QUAD_STRIP);
			for(a=0; a<wtb->totvert; a++) {
				glVertex2fv(wtb->outer_v[a]);
				glVertex2fv(wtb->inner_v[a]);
			}
			glVertex2fv(wtb->outer_v[0]);
			glVertex2fv(wtb->inner_v[0]);
			glEnd();
		
			/* emboss bottom shadow */
			if(wtb->emboss) {
				glColor4f(1.0f, 1.0f, 1.0f, 0.02f);
				glBegin(GL_QUAD_STRIP);
				for(a=0; a<wtb->halfwayvert; a++) {
					glVertex2fv(wtb->outer_v[a]);
					glVertex2f(wtb->outer_v[a][0], wtb->outer_v[a][1]-1.0f);
				}
				glEnd();
			}
			
			glTranslatef(-1.0*jit[j][0], -1.0*jit[j][1], 0.0f);
		}
	}
	
	/* decoration */
	if(wtb->tria1.tot || wtb->tria2.tot) {
		/* for each AA step */
		for(j=0; j<8; j++) {
			glTranslatef(1.0*jit[j][0], 1.0*jit[j][1], 0.0f);

			if(wtb->tria1.tot) {
				glColor4ub(wcol->item[0], wcol->item[1], wcol->item[2], 32);
				widget_trias_draw(&wtb->tria1);
			}
			if(wtb->tria2.tot) {
				glColor4ub(wcol->item[0], wcol->item[1], wcol->item[2], 32);
				widget_trias_draw(&wtb->tria2);
			}
		
			glTranslatef(-1.0*jit[j][0], -1.0*jit[j][1], 0.0f);
		}
	}

	glDisable(GL_BLEND);
	
}

/* *********************** text/icon ************************************** */


/* icons have been standardized... and this call draws in untransformed coordinates */
#define ICON_HEIGHT		16.0f

static void widget_draw_icon(uiBut *but, BIFIconID icon, int blend, rcti *rect)
{
	float xs=0, ys=0, aspect, height;
	
	/* this icon doesn't need draw... */
	if(icon==ICON_BLANK1 && (but->flag & UI_ICON_SUBMENU)==0) return;
	
	/* we need aspect from block, for menus... these buttons are scaled in uiPositionBlock() */
	aspect= but->block->aspect;
	if(aspect != but->aspect) {
		/* prevent scaling up icon in pupmenu */
		if (aspect < 1.0f) {			
			height= ICON_HEIGHT;
			aspect = 1.0f;
			
		}
		else 
			height= ICON_HEIGHT/aspect;
	}
	else
		height= ICON_HEIGHT;
	
	/* calculate blend color */
	if ELEM3(but->type, TOG, ROW, TOGN) {
		if(but->flag & UI_SELECT);
		else if(but->flag & UI_ACTIVE);
		else blend= -60;
	}
	
	glEnable(GL_BLEND);
	
	if(icon && icon!=ICON_BLANK1) {
		if(but->flag & UI_ICON_LEFT) {
			if (but->type==BUT_TOGDUAL) {
				if (but->drawstr[0]) {
					xs= rect->xmin-1.0;
				} else {
					xs= (rect->xmin+rect->xmax- height)/2.0;
				}
			}
			else if (but->block->flag & UI_BLOCK_LOOP) {
				xs= rect->xmin+1.0;
			}
			else if ((but->type==ICONROW) || (but->type==ICONTEXTROW)) {
				xs= rect->xmin+3.0;
			}
			else {
				xs= rect->xmin+4.0;
			}
			ys= (rect->ymin+rect->ymax- height)/2.0;
		}
		else {
			xs= (rect->xmin+rect->xmax- height)/2.0;
			ys= (rect->ymin+rect->ymax- height)/2.0;
		}
	
		UI_icon_draw_aspect_blended(xs, ys, icon, aspect, blend);
	}
	
	if(but->flag & UI_ICON_SUBMENU) {
		xs= rect->xmax-17.0;
		ys= (rect->ymin+rect->ymax- height)/2.0;
		
		UI_icon_draw_aspect_blended(xs, ys, ICON_RIGHTARROW_THIN, aspect, blend);
	}
	
	glDisable(GL_BLEND);
}

/* sets but->ofs to make sure text is correctly visible */
static void ui_text_leftclip(uiFontStyle *fstyle, uiBut *but, rcti *rect)
{
	int okwidth= rect->xmax-rect->xmin;
	
	/* need to set this first */
	uiStyleFontSet(fstyle);

	but->strwidth= BLF_width(but->drawstr);
	but->ofs= 0;
	
	while(but->strwidth > okwidth ) {
		
		but->ofs++;
		but->strwidth= BLF_width(but->drawstr+but->ofs);
		
		/* textbut exception */
		if(but->editstr && but->pos != -1) {
			int pos= but->pos+strlen(but->str);
			
			if(pos-1 < but->ofs) {
				pos= but->ofs-pos+1;
				but->ofs -= pos;
				if(but->ofs<0) {
					but->ofs= 0;
					pos--;
				}
				but->drawstr[ strlen(but->drawstr)-pos ]= 0;
			}
		}
		
		if(but->strwidth < 10) break;
	}
}

static void widget_draw_text(uiFontStyle *fstyle, uiWidgetColors *wcol, uiBut *but, rcti *rect)
{
//	int transopts;
	char *cpoin = NULL;
	
	uiStyleFontSet(fstyle);
	
	if(but->editstr || (but->flag & UI_TEXT_LEFT))
		fstyle->align= UI_STYLE_TEXT_LEFT;
	else
		fstyle->align= UI_STYLE_TEXT_CENTER;			
		
	/* text button selection and cursor */
	if(but->editstr && but->pos != -1) {
		short t, pos, ch;
		short selsta_tmp, selend_tmp, selsta_draw, selwidth_draw;
		
		if ((but->selend - but->selsta) > 0) {
			/* XXX weak, why is this? (ton) */
			t= but->str[0]?1:-2;
			
			/* text button selection */
			selsta_tmp = but->selsta + strlen(but->str);
			selend_tmp = but->selend + strlen(but->str);
			
			if(but->drawstr[0]!=0) {
				ch= but->drawstr[selsta_tmp];
				but->drawstr[selsta_tmp]= 0;
				
				selsta_draw = BLF_width(but->drawstr+but->ofs) + t;
				
				but->drawstr[selsta_tmp]= ch;
				
				ch= but->drawstr[selend_tmp];
				but->drawstr[selend_tmp]= 0;
				
				selwidth_draw = BLF_width(but->drawstr+but->ofs) + t;
				
				but->drawstr[selend_tmp]= ch;
				
				glColor3ubv(wcol->item);
				glRects(rect->xmin+selsta_draw+1, rect->ymin+2, rect->xmin+selwidth_draw+1, rect->ymax-2);
			}
		} else {
			/* text cursor */
			pos= but->pos+strlen(but->str);
			if(pos >= but->ofs) {
				if(but->drawstr[0]!=0) {
					ch= but->drawstr[pos];
					but->drawstr[pos]= 0;
					
					t= BLF_width(but->drawstr+but->ofs) + 1;
					
					but->drawstr[pos]= ch;
				}
				else t= 1;
				
				glColor3ub(255,0,0);
				glRects(rect->xmin+t, rect->ymin+2, rect->xmin+t+2, rect->ymax-2);
			}
		}
	}
	//	ui_rasterpos_safe(x, y, but->aspect);
//	if(but->type==IDPOIN) transopts= 0;	// no translation, of course!
//	else transopts= ui_translate_buttons();
	
	/* cut string in 2 parts - only for menu entries */
	if(ELEM5(but->type, SLI, NUM, TEX, NUMSLI, NUMABS)==0) {
		cpoin= strchr(but->drawstr, '|');
		if(cpoin) *cpoin= 0;		
	}
	
	glColor3ubv(wcol->text);
	uiStyleFontDraw(fstyle, rect, but->drawstr+but->ofs);

	/* part text right aligned */
	if(cpoin) {
		fstyle->align= UI_STYLE_TEXT_RIGHT;
		rect->xmax-=5;
		uiStyleFontDraw(fstyle, rect, cpoin+1);
		*cpoin= '|';
	}
}

/* draws text and icons for buttons */
static void widget_draw_text_icon(uiFontStyle *fstyle, uiWidgetColors *wcol, uiBut *but, rcti *rect)
{
	
	if(but==NULL) return;
	
	/* cutting off from left part */
	if ELEM3(but->type, NUM, NUMABS, TEX) {	
		ui_text_leftclip(fstyle, but, rect);
	}
	else but->ofs= 0;
	
	/* check for button text label */
	if (but->type == ICONTEXTROW) {
		widget_draw_icon(but, (BIFIconID) (but->icon+but->iconadd), 0, rect);
	}
	else {
				
		if(but->type==BUT_TOGDUAL) {
			int dualset= 0;
			if(but->pointype==SHO)
				dualset= BTST( *(((short *)but->poin)+1), but->bitnr);
			else if(but->pointype==INT)
				dualset= BTST( *(((int *)but->poin)+1), but->bitnr);
			
			widget_draw_icon(but, ICON_DOT, dualset?0:-100, rect);
		}
		
		if(but->drawstr[0]!=0) {
			
			/* If there's an icon too (made with uiDefIconTextBut) then draw the icon
			and offset the text label to accomodate it */
			
			if (but->flag & UI_HAS_ICON) {
				widget_draw_icon(but, but->icon, 0, rect);
				
				rect->xmin += UI_icon_get_width(but->icon);
				
				if(but->editstr || (but->flag & UI_TEXT_LEFT)) 
					rect->xmin += 5;
			}
			else if(but->flag & UI_TEXT_LEFT)
				rect->xmin += 5;
			
			widget_draw_text(fstyle, wcol, but, rect);
			
		}
		/* if there's no text label, then check to see if there's an icon only and draw it */
		else if( but->flag & UI_HAS_ICON ) {
			widget_draw_icon(but, (BIFIconID) (but->icon+but->iconadd), 0, rect);
		}
	}
}



/* *********************** widget types ************************************* */


/*   uiWidgetStateColors
 char inner_anim[4];
 char inner_anim_sel[4];
 char inner_key[4];
 char inner_key_sel[4];
 char inner_driven[4];
 char inner_driven_sel[4];
 
*/

static struct uiWidgetStateColors wcol_state= {
	{115, 190, 76, 255},
	{90, 166, 51, 255},
	{240, 235, 100, 255},
	{148, 204, 76, 255},
	{180, 0, 255, 255},
	{153, 0, 230, 255}
};

/*  uiWidgetColors
 float outline[3];
 float inner[4];
 float inner_sel[4];
 float item[3];
 float text[3];
 float text_sel[3];

 short shaded;
 float shadetop, shadedown;
*/	

static struct uiWidgetColors wcol_num= {
	{25, 25, 25, 255},
	{180, 180, 180, 255},
	{153, 153, 153, 255},
	{90, 90, 90, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	-20, 0
};

static struct uiWidgetColors wcol_numslider= {
	{25, 25, 25, 255},
	{180, 180, 180, 255},
	{153, 153, 153, 255},
	{128, 128, 128, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	-20, 0
};

static struct uiWidgetColors wcol_text= {
	{25, 25, 25, 255},
	{153, 153, 153, 255},
	{153, 153, 153, 255},
	{90, 90, 90, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	0, 25
};

static struct uiWidgetColors wcol_option= {
	{0, 0, 0, 255},
	{70, 70, 70, 255},
	{70, 70, 70, 255},
	{255, 255, 255, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	15, -15
};

/* button that shows popup */
static struct uiWidgetColors wcol_menu= {
	{0, 0, 0, 255},
	{70, 70, 70, 255},
	{70, 70, 70, 255},
	{255, 255, 255, 255},
	
	{255, 255, 255, 255},
	{204, 204, 204, 255},
	
	1,
	15, -15
};

/* button that starts pulldown */
static struct uiWidgetColors wcol_pulldown= {
	{0, 0, 0, 255},
	{63, 63, 63, 255},
	{86, 128, 194, 255},
	{255, 255, 255, 255},
	
	{0, 0, 0, 255},
	{0, 0, 0, 255},
	
	0,
	25, -20
};

/* button inside menu */
static struct uiWidgetColors wcol_menu_item= {
	{0, 0, 0, 255},
	{0, 0, 0, 0},
	{86, 128, 194, 255},
	{255, 255, 255, 255},
	
	{255, 255, 255, 255},
	{0, 0, 0, 255},
	
	0,
	38, 0
};

/* backdrop menu + title text color */
static struct uiWidgetColors wcol_menu_back= {
	{0, 0, 0, 255},
	{25, 25, 25, 230},
	{46, 124, 217, 204},
	{255, 255, 255, 255},
	
	{255, 255, 255, 255},
	{0, 0, 0, 255},
	
	0,
	25, -20
};


static struct uiWidgetColors wcol_radio= {
	{0, 0, 0, 255},
	{70, 70, 70, 255},
	{86, 128, 194, 255},
	{255, 255, 255, 255},
	
	{255, 255, 255, 255},
	{0, 0, 0, 255},
	
	1,
	15, -15
};

static struct uiWidgetColors wcol_regular= {
	{25, 25, 25, 255},
	{153, 153, 153, 255},
	{100, 100, 100, 255},
	{25, 25, 25, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	0,
	0, 0
};

static struct uiWidgetColors wcol_tool= {
	{25, 25, 25, 255},
	{153, 153, 153, 255},
	{100, 100, 100, 255},
	{25, 25, 25, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	25, -25
};

/* called for theme init (new theme) and versions */
void ui_widget_color_init(ThemeUI *tui)
{

	tui->wcol_regular= wcol_regular;
	tui->wcol_tool= wcol_tool;
	tui->wcol_radio= wcol_radio;
	tui->wcol_text= wcol_text;
	tui->wcol_option= wcol_option;
	tui->wcol_num= wcol_num;
	tui->wcol_numslider= wcol_numslider;
	tui->wcol_menu= wcol_menu;
	tui->wcol_pulldown= wcol_pulldown;
	tui->wcol_menu_back= wcol_menu_back;
	tui->wcol_menu_item= wcol_menu_item;
	
	tui->iconfile[0]= 0;
}

/* ************ button callbacks, state ***************** */

/* copy colors from theme, and set changes in it based on state */
static void widget_state(uiWidgetType *wt, int state)
{
	wt->wcol= *(wt->wcol_theme);
	
	if(state & UI_SELECT) {
		if(state & UI_BUT_ANIMATED_KEY)
			QUATCOPY(wt->wcol.inner, wcol_state.inner_key_sel)
		else if(state & UI_BUT_ANIMATED)
			QUATCOPY(wt->wcol.inner, wcol_state.inner_anim_sel)
		else if(state & UI_BUT_DRIVEN)
			QUATCOPY(wt->wcol.inner, wcol_state.inner_driven_sel)
		else
			QUATCOPY(wt->wcol.inner, wt->wcol.inner_sel)

		VECCOPY(wt->wcol.text, wt->wcol.text_sel);
		
		/* only flip shade if it's not "pushed in" already */
		if(wt->wcol.shaded && wt->wcol.shadetop>wt->wcol.shadedown) {
			SWAP(short, wt->wcol.shadetop, wt->wcol.shadedown);
		}
	}
	else {
		if(state & UI_BUT_ANIMATED_KEY)
			QUATCOPY(wt->wcol.inner, wcol_state.inner_key)
		else if(state & UI_BUT_ANIMATED)
			QUATCOPY(wt->wcol.inner, wcol_state.inner_anim)
		else if(state & UI_BUT_DRIVEN)
			QUATCOPY(wt->wcol.inner, wcol_state.inner_driven)

		if(state & UI_ACTIVE) { /* mouse over? */
			wt->wcol.inner[0]= wt->wcol.inner[0]>=240? 255 : wt->wcol.inner[0]+15;
			wt->wcol.inner[1]= wt->wcol.inner[1]>=240? 255 : wt->wcol.inner[1]+15;
			wt->wcol.inner[2]= wt->wcol.inner[2]>=240? 255 : wt->wcol.inner[2]+15;
		}
	}
}

/* labels use theme colors for text */
static void widget_state_label(uiWidgetType *wt, int state)
{
	/* call this for option button */
	widget_state(wt, state);

	if(state & UI_SELECT)
		UI_GetThemeColor4ubv(TH_TEXT_HI, wt->wcol.text);
	else
		UI_GetThemeColor4ubv(TH_TEXT, wt->wcol.text);
	
}


/* special case, button that calls pulldown */
static void widget_state_pulldown(uiWidgetType *wt, int state)
{
	wt->wcol= *(wt->wcol_theme);
	
	QUATCOPY(wt->wcol.inner, wt->wcol.inner_sel);
	VECCOPY(wt->wcol.outline, wt->wcol.inner);

	if(state & UI_ACTIVE)
		VECCOPY(wt->wcol.text, wt->wcol.text_sel);
}

/* special case, menu items */
static void widget_state_menu_item(uiWidgetType *wt, int state)
{
	wt->wcol= *(wt->wcol_theme);
	
	if(state & UI_BUT_DISABLED) {
		wt->wcol.text[0]= 0.5f*(wt->wcol.text[0]+wt->wcol.text_sel[0]);
		wt->wcol.text[1]= 0.5f*(wt->wcol.text[1]+wt->wcol.text_sel[1]);
		wt->wcol.text[2]= 0.5f*(wt->wcol.text[2]+wt->wcol.text_sel[2]);
	}
	else if(state & UI_ACTIVE) {
		QUATCOPY(wt->wcol.inner, wt->wcol.inner_sel);
		VECCOPY(wt->wcol.text, wt->wcol.text_sel);
		
		wt->wcol.shaded= 1;
	}
}


/* ************ menu backdrop ************************* */

/* outside of rect, rad to left/bottom/right */
static void widget_softshadow(rcti *rect, int roundboxalign, float radin, float radout)
{
	uiWidgetBase wtb;
	rcti rect1= *rect;
	float alpha, alphastep;
	int step, tot, a;
	
	/* prevent tooltips to not show round shadow */
	if( 2.0f*radout > 0.2f*(rect1.ymax-rect1.ymin) )
		rect1.ymax -= 0.2f*(rect1.ymax-rect1.ymin);
	else
		rect1.ymax -= 2.0f*radout;
	
	/* inner part */
	tot= round_box_shadow_edges(wtb.inner_v, &rect1, radin, roundboxalign & 12, 0.0f);
	
	/* inverse linear shadow alpha */
	alpha= 0.15;
	alphastep= 0.67;
	
	for(step= 1; step<=radout; step++, alpha*=alphastep) {
		round_box_shadow_edges(wtb.outer_v, &rect1, radin, 15, (float)step);
		
		glColor4f(0.0f, 0.0f, 0.0f, alpha);
		
		glBegin(GL_QUAD_STRIP);
		for(a=0; a<tot; a++) {
			glVertex2fv(wtb.outer_v[a]);
			glVertex2fv(wtb.inner_v[a]);
		}
		glEnd();
	}
	
}

static void widget_menu_back(uiWidgetColors *wcol, rcti *rect, int flag, int direction)
{
	uiWidgetBase wtb;
	int roundboxalign= 15;
	
	widget_init(&wtb);
	
	/* menu is 2nd level or deeper */
	if (flag & UI_BLOCK_POPUP) {
		rect->ymin -= 4.0;
		rect->ymax += 4.0;
	}
	else if (direction == UI_DOWN) {
		roundboxalign= 12;
		rect->ymin -= 4.0;
	} 
	else if (direction == UI_TOP) {
		roundboxalign= 3;
		rect->ymax += 4.0;
	}
	
	glEnable(GL_BLEND);
	widget_softshadow(rect, roundboxalign, 5.0f, 8.0f);
	
	round_box_edges(&wtb, roundboxalign, rect, 5.0f);
	wtb.emboss= 0;
	widgetbase_draw(&wtb, wcol);
	
	glDisable(GL_BLEND);
}

/* ************ custom buttons, old stuff ************** */

/* draws in resolution of 20x4 colors */
static void ui_draw_but_HSVCUBE(uiBut *but, rcti *rect)
{
	int a;
	float h,s,v;
	float dx, dy, sx1, sx2, sy, x, y;
	float col0[4][3];	// left half, rect bottom to top
	float col1[4][3];	// right half, rect bottom to top
	
	h= but->hsv[0];
	s= but->hsv[1];
	v= but->hsv[2];
	
	/* draw series of gouraud rects */
	glShadeModel(GL_SMOOTH);
	
	if(but->a1==0) {	// H and V vary
		hsv_to_rgb(0.0, s, 0.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
		hsv_to_rgb(0.0, s, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
		hsv_to_rgb(0.0, s, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
		hsv_to_rgb(0.0, s, 1.0,   &col1[3][0], &col1[3][1], &col1[3][2]);
		x= h; y= v;
	}
	else if(but->a1==1) {	// H and S vary
		hsv_to_rgb(0.0, 0.0, v,   &col1[0][0], &col1[0][1], &col1[0][2]);
		hsv_to_rgb(0.0, 0.333, v, &col1[1][0], &col1[1][1], &col1[1][2]);
		hsv_to_rgb(0.0, 0.666, v, &col1[2][0], &col1[2][1], &col1[2][2]);
		hsv_to_rgb(0.0, 1.0, v,   &col1[3][0], &col1[3][1], &col1[3][2]);
		x= h; y= s;
	}
	else if(but->a1==2) {	// S and V vary
		hsv_to_rgb(h, 0.0, 0.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
		hsv_to_rgb(h, 0.333, 0.0, &col1[1][0], &col1[1][1], &col1[1][2]);
		hsv_to_rgb(h, 0.666, 0.0, &col1[2][0], &col1[2][1], &col1[2][2]);
		hsv_to_rgb(h, 1.0, 0.0,   &col1[3][0], &col1[3][1], &col1[3][2]);
		x= v; y= s;
	}
	else {		// only hue slider
		hsv_to_rgb(0.0, 1.0, 1.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
		VECCOPY(col1[1], col1[0]);
		VECCOPY(col1[2], col1[0]);
		VECCOPY(col1[3], col1[0]);
		x= h; y= 0.5;
	}
	
	for(dx=0.0; dx<1.0; dx+= 0.05) {
		// previous color
		VECCOPY(col0[0], col1[0]);
		VECCOPY(col0[1], col1[1]);
		VECCOPY(col0[2], col1[2]);
		VECCOPY(col0[3], col1[3]);
		
		// new color
		if(but->a1==0) {	// H and V vary
			hsv_to_rgb(dx, s, 0.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
			hsv_to_rgb(dx, s, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
			hsv_to_rgb(dx, s, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
			hsv_to_rgb(dx, s, 1.0,   &col1[3][0], &col1[3][1], &col1[3][2]);
		}
		else if(but->a1==1) {	// H and S vary
			hsv_to_rgb(dx, 0.0, v,   &col1[0][0], &col1[0][1], &col1[0][2]);
			hsv_to_rgb(dx, 0.333, v, &col1[1][0], &col1[1][1], &col1[1][2]);
			hsv_to_rgb(dx, 0.666, v, &col1[2][0], &col1[2][1], &col1[2][2]);
			hsv_to_rgb(dx, 1.0, v,   &col1[3][0], &col1[3][1], &col1[3][2]);
		}
		else if(but->a1==2) {	// S and V vary
			hsv_to_rgb(h, 0.0, dx,   &col1[0][0], &col1[0][1], &col1[0][2]);
			hsv_to_rgb(h, 0.333, dx, &col1[1][0], &col1[1][1], &col1[1][2]);
			hsv_to_rgb(h, 0.666, dx, &col1[2][0], &col1[2][1], &col1[2][2]);
			hsv_to_rgb(h, 1.0, dx,   &col1[3][0], &col1[3][1], &col1[3][2]);
		}
		else {	// only H
			hsv_to_rgb(dx, 1.0, 1.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
			VECCOPY(col1[1], col1[0]);
			VECCOPY(col1[2], col1[0]);
			VECCOPY(col1[3], col1[0]);
		}
		
		// rect
		sx1= rect->xmin + dx*(rect->xmax-rect->xmin);
		sx2= rect->xmin + (dx+0.05)*(rect->xmax-rect->xmin);
		sy= rect->ymin;
		dy= (rect->ymax-rect->ymin)/3.0;
		
		glBegin(GL_QUADS);
		for(a=0; a<3; a++, sy+=dy) {
			glColor3fv(col0[a]);
			glVertex2f(sx1, sy);
			
			glColor3fv(col1[a]);
			glVertex2f(sx2, sy);
			
			glColor3fv(col1[a+1]);
			glVertex2f(sx2, sy+dy);
			
			glColor3fv(col0[a+1]);
			glVertex2f(sx1, sy+dy);
		}
		glEnd();
	}
	
	glShadeModel(GL_FLAT);
	
	/* cursor */
	x= rect->xmin + x*(rect->xmax-rect->xmin);
	y= rect->ymin + y*(rect->ymax-rect->ymin);
	CLAMP(x, rect->xmin+3.0, rect->xmax-3.0);
	CLAMP(y, rect->ymin+3.0, rect->ymax-3.0);
	
	fdrawXORcirc(x, y, 3.1);
	
	/* outline */
	glColor3ub(0,  0,  0);
	fdrawbox((rect->xmin), (rect->ymin), (rect->xmax), (rect->ymax));
}


/* ************ button callbacks, draw ***************** */

static void widget_numbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	
	widget_init(&wtb);
	
	/* fully rounded */
	round_box_edges(&wtb, roundboxalign, rect, 0.5f*(rect->ymax - rect->ymin));
	
	/* decoration */
	if(!(state & UI_TEXTINPUT)) {
		widget_num_tria(&wtb.tria1, rect, 0.6f, 0);
		widget_num_tria(&wtb.tria2, rect, 0.6f, 'r');
	}	
	widgetbase_draw(&wtb, wcol);
	
	/* text space */
	rect->xmin += (rect->ymax-rect->ymin);
	rect->xmax -= (rect->ymax-rect->ymin);

}

static void widget_numslider(uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb, wtb1;
	rcti rect1;
	double value;
	float offs, fac;
	char outline[3];
	
	widget_init(&wtb);
	widget_init(&wtb1);
	
	/* backdrop first */
	
	/* fully rounded */
	offs= 0.5f*(rect->ymax - rect->ymin);
	round_box_edges(&wtb, roundboxalign, rect, offs);

	wtb.outline= 0;
	widgetbase_draw(&wtb, wcol);
	
	/* slider part */
	VECCOPY(outline, wcol->outline);
	VECCOPY(wcol->outline, wcol->item);
	VECCOPY(wcol->inner, wcol->item);
	SWAP(short, wcol->shadetop, wcol->shadedown);
	
	rect1= *rect;
	
	value= ui_get_but_val(but);
	fac= (value-but->softmin)*(rect1.xmax - rect1.xmin - offs)/(but->softmax - but->softmin);
	
	/* left part of slider, always rounded */
	rect1.xmax= rect1.xmin + ceil(offs+1.0f);
	round_box_edges(&wtb1, roundboxalign & ~6, &rect1, offs);
	wtb1.outline= 0;
	widgetbase_draw(&wtb1, wcol);
	
	/* right part of slider, interpolate roundness */
	rect1.xmax= rect1.xmin + fac + offs;
	rect1.xmin+=  floor(offs-1.0f);
	if(rect1.xmax + offs > rect->xmax)
		offs*= (rect1.xmax + offs - rect->xmax)/offs;
	else 
		offs= 0.0f;
	round_box_edges(&wtb1, roundboxalign & ~9, &rect1, offs);
	
	widgetbase_draw(&wtb1, wcol);
	VECCOPY(wcol->outline, outline);
	SWAP(short, wcol->shadetop, wcol->shadedown);
	
	/* outline */
	wtb.outline= 1;
	wtb.inner= 0;
	widgetbase_draw(&wtb, wcol);
	
	/* text space */
	rect->xmin += (rect->ymax-rect->ymin);
	rect->xmax -= (rect->ymax-rect->ymin);
	
}

static void widget_swatch(uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	float col[4];
	
	widget_init(&wtb);
	
	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, 4.0f);
		
	ui_get_but_vectorf(but, col);
	wcol->inner[0]= FTOCHAR(col[0]);
	wcol->inner[1]= FTOCHAR(col[1]);
	wcol->inner[2]= FTOCHAR(col[2]);
	
	widgetbase_draw(&wtb, wcol);
	
}


static void widget_textbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	
	widget_init(&wtb);
	
	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, 4.0f);
	
	widgetbase_draw(&wtb, wcol);

}


static void widget_menubut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	
	widget_init(&wtb);
	
	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, 4.0f);
	
	/* decoration */
	widget_menu_trias(&wtb.tria1, rect);
	
	widgetbase_draw(&wtb, wcol);
	
	/* text space */
	rect->xmax -= (rect->ymax-rect->ymin);
	
}

static void widget_pulldownbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	if(state & UI_ACTIVE) {
		uiWidgetBase wtb;
		
		widget_init(&wtb);
		
		/* fully rounded */
		round_box_edges(&wtb, roundboxalign, rect, 0.5f*(rect->ymax - rect->ymin));
		
		widgetbase_draw(&wtb, wcol);
	}
}

static void widget_menu_itembut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	
	widget_init(&wtb);
	
	/* not rounded, no outline */
	wtb.outline= 0;
	round_box_edges(&wtb, 0, rect, 0.0f);
	
	widgetbase_draw(&wtb, wcol);
}


static void widget_optionbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	rcti recttemp= *rect;
	int delta;
	
	widget_init(&wtb);
	
	/* square */
	recttemp.xmax= recttemp.xmin + (recttemp.ymax-recttemp.ymin);
	
	/* smaller */
	delta= 1 + (recttemp.ymax-recttemp.ymin)/8;
	recttemp.xmin+= delta;
	recttemp.ymin+= delta;
	recttemp.xmax-= delta;
	recttemp.ymax-= delta;
	
	/* half rounded */
	round_box_edges(&wtb, 15, &recttemp, 4.0f);
	
	/* decoration */
	if(state & UI_SELECT) {
		widget_check_trias(&wtb.tria1, &recttemp);
	}
	
	widgetbase_draw(&wtb, wcol);
	
	/* text space */
	rect->xmin += (rect->ymax-rect->ymin)*0.7 + delta;
}


static void widget_radiobut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	
	widget_init(&wtb);
	
	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, 4.0f);
	
	widgetbase_draw(&wtb, wcol);

}

static void widget_but(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	
	widget_init(&wtb);
	
	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, 4.0f);
		
	widgetbase_draw(&wtb, wcol);

}

static void widget_roundbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	
	widget_init(&wtb);
	
	/* fully rounded */
	round_box_edges(&wtb, roundboxalign, rect, 0.5f*(rect->ymax - rect->ymin));

	widgetbase_draw(&wtb, wcol);
}

static void widget_disabled(rcti *rect)
{
	float col[4];
	
	glEnable(GL_BLEND);
	
	/* can't use theme TH_BACK or TH_PANEL... undefined */
	glGetFloatv(GL_COLOR_CLEAR_VALUE, col);
	glColor4f(col[0], col[1], col[2], 0.5f);
	glRectf(rect->xmin, rect->ymin, rect->xmax, rect->ymax);

	glDisable(GL_BLEND);
}

static uiWidgetType *widget_type(uiWidgetTypeEnum type)
{
	bTheme *btheme= U.themes.first;
	static uiWidgetType wt;
	
	/* defaults */
	wt.wcol_theme= &btheme->tui.wcol_regular;
	wt.state= widget_state;
	wt.draw= widget_but;
	wt.custom= NULL;
	wt.text= widget_draw_text_icon;
	
	switch(type) {
		case UI_WTYPE_LABEL:
			wt.draw= NULL;
			wt.state= widget_state_label;
			break;
			
		case UI_WTYPE_TOGGLE:
			break;
			
		case UI_WTYPE_OPTION:
			wt.wcol_theme= &btheme->tui.wcol_option;
			wt.draw= widget_optionbut;
			wt.state= widget_state_label;
			break;
			
		case UI_WTYPE_RADIO:
			wt.wcol_theme= &btheme->tui.wcol_radio;
			wt.draw= widget_radiobut;
			break;
			
		case UI_WTYPE_NUMBER:
			wt.wcol_theme= &btheme->tui.wcol_num;
			wt.draw= widget_numbut;
			break;
			
		case UI_WTYPE_SLIDER:
			wt.wcol_theme= &btheme->tui.wcol_numslider;
			wt.custom= widget_numslider;
			break;
			
		case UI_WTYPE_EXEC:
			wt.wcol_theme= &btheme->tui.wcol_tool;
			wt.draw= widget_roundbut;
			break;
			
			
			/* strings */
		case UI_WTYPE_NAME:
			wt.wcol_theme= &btheme->tui.wcol_text;
			wt.draw= widget_textbut;
			break;
			
		case UI_WTYPE_NAME_LINK:
			break;
			
		case UI_WTYPE_POINTER_LINK:
			break;
			
		case UI_WTYPE_FILENAME:
			break;
			
			
			/* start menus */
		case UI_WTYPE_MENU_RADIO:
			wt.wcol_theme= &btheme->tui.wcol_menu;
			wt.draw= widget_menubut;
			break;
			
		case UI_WTYPE_MENU_POINTER_LINK:
			wt.wcol_theme= &btheme->tui.wcol_menu;
			wt.draw= widget_menubut;
			break;
			
			
		case UI_WTYPE_PULLDOWN:
			wt.wcol_theme= &btheme->tui.wcol_pulldown;
			wt.draw= widget_pulldownbut;
			wt.state= widget_state_pulldown;
			break;
			
			/* in menus */
		case UI_WTYPE_MENU_ITEM:
			wt.wcol_theme= &btheme->tui.wcol_menu_item;
			wt.draw= widget_menu_itembut;
			wt.state= widget_state_menu_item;
			break;
			
		case UI_WTYPE_MENU_BACK:
			wt.wcol_theme= &btheme->tui.wcol_menu_back;
			wt.draw= widget_menu_back;
			break;
			
			/* specials */
		case UI_WTYPE_ICON:
			wt.draw= NULL;
			break;
			
		case UI_WTYPE_SWATCH:
			wt.custom= widget_swatch;
			break;
			
		case UI_WTYPE_RGB_PICKER:
			break;
			
		case UI_WTYPE_NORMAL:
			break;
	}
	
	return &wt;
}


static int widget_roundbox_set(uiBut *but, rcti *rect)
{
	/* alignment */
	if(but->flag & UI_BUT_ALIGN) {
		
		if(but->flag & UI_BUT_ALIGN_TOP)
			rect->ymax+= 1;
		if(but->flag & UI_BUT_ALIGN_LEFT)
			rect->xmin-= 1;
		
		switch(but->flag & UI_BUT_ALIGN) {
			case UI_BUT_ALIGN_TOP:
				return (12);
				break;
			case UI_BUT_ALIGN_DOWN:
				return (3);
				break;
			case UI_BUT_ALIGN_LEFT:
				return (6);
				break;
			case UI_BUT_ALIGN_RIGHT:
				return (9);
				break;
				
			case UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_RIGHT:
				return (1);
				break;
			case UI_BUT_ALIGN_DOWN|UI_BUT_ALIGN_LEFT:
				return (2);
				break;
			case UI_BUT_ALIGN_TOP|UI_BUT_ALIGN_RIGHT:
				return (8);
				break;
			case UI_BUT_ALIGN_TOP|UI_BUT_ALIGN_LEFT:
				return (4);
				break;
				
			default:
				return (0);
				break;
		}
	} 
	return 15;
}

/* conversion from old to new buttons, so still messy */
void ui_draw_but(ARegion *ar, uiStyle *style, uiBut *but, rcti *rect)
{
	bTheme *btheme= U.themes.first;
	ThemeUI *tui= &btheme->tui;
	uiFontStyle *fstyle= &style->widget;
	uiWidgetType *wt= NULL;
	
	/* handle menus seperately */
	if(but->dt==UI_EMBOSSP) {
		switch (but->type) {
			case LABEL:
				widget_draw_text_icon(&style->widgetlabel, &tui->wcol_menu_back, but, rect);
				break;
			case SEPR:
				break;
				
			default:
				wt= widget_type(UI_WTYPE_MENU_ITEM);
		}
	}
	else if(but->dt==UI_EMBOSSN) {
		/* "nothing" */
		wt= widget_type(UI_WTYPE_ICON);
	}
	else {
		
		switch (but->type) {
			case LABEL:
				if(but->block->flag & UI_BLOCK_LOOP)
					widget_draw_text_icon(&style->widgetlabel, &tui->wcol_menu_back, but, rect);
				else {
					wt= widget_type(UI_WTYPE_LABEL);
					fstyle= &style->widgetlabel;
				}
				break;
			case SEPR:
				break;
			case BUT:
				wt= widget_type(UI_WTYPE_EXEC);
				break;
			case NUM:
				wt= widget_type(UI_WTYPE_NUMBER);
				break;
			case NUMSLI:
			case HSVSLI:
				wt= widget_type(UI_WTYPE_SLIDER);
				break;
			case ROW:
				wt= widget_type(UI_WTYPE_RADIO);
				break;
			case TEX:
				wt= widget_type(UI_WTYPE_NAME);
				break;
			case TOGBUT:
			case TOG:
			case TOGN:
			case TOG3:
				wt= widget_type(UI_WTYPE_TOGGLE);
				break;
			case OPTION:
			case OPTIONN:
				if (!(but->flag & UI_HAS_ICON)) {
					wt= widget_type(UI_WTYPE_OPTION);
					but->flag |= UI_TEXT_LEFT;
				}
				else
					wt= widget_type(UI_WTYPE_TOGGLE);
				break;
			case MENU:
			case BLOCK:
			case ICONTEXTROW:
				wt= widget_type(UI_WTYPE_MENU_RADIO);
				break;
				
			case PULLDOWN:
				wt= widget_type(UI_WTYPE_PULLDOWN);
				break;
			
			case BUTM:
				wt= widget_type(UI_WTYPE_MENU_ITEM);
				break;
				
			case COL:
				wt= widget_type(UI_WTYPE_SWATCH);
				break;
			
				 // XXX four old button types
			case HSVCUBE:
				ui_draw_but_HSVCUBE(but, rect);
				break;
			case BUT_COLORBAND:
				ui_draw_but_COLORBAND(but, &tui->wcol_regular, rect);
				break;
			case BUT_NORMAL:
				ui_draw_but_NORMAL(but, &tui->wcol_regular, rect);
				break;
			case BUT_CURVE:
				ui_draw_but_CURVE(ar, but, &tui->wcol_regular, rect);
				break;
				
			default:
				wt= widget_type(UI_WTYPE_TOGGLE);
		}
	}
	
	if(wt) {
		rcti disablerect= *rect; /* rect gets clipped smaller for text */
		int roundboxalign, state;
		
		roundboxalign= widget_roundbox_set(but, rect);
		state= but->flag;
		if(but->editstr) state |= UI_TEXTINPUT;
		
		wt->state(wt, state);
		if(wt->custom)
			wt->custom(but, &wt->wcol, rect, state, roundboxalign);
		else if(wt->draw)
			wt->draw(&wt->wcol, rect, state, roundboxalign);
		wt->text(fstyle, &wt->wcol, but, rect);
		
		if(state & UI_BUT_DISABLED)
			if(but->dt!=UI_EMBOSSP)
				widget_disabled(&disablerect);
	}
}

void ui_draw_menu_back(uiStyle *style, uiBlock *block, rcti *rect)
{
	uiWidgetType *wt= widget_type(UI_WTYPE_MENU_BACK);
	
	wt->state(wt, 0);
	if(block)
		wt->draw(&wt->wcol, rect, block->flag, block->direction);
	else
		wt->draw(&wt->wcol, rect, 0, 0);
	
}



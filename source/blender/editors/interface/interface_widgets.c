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
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_arithb.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"

#include "BIF_gl.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_text.h"
#include "UI_view2d.h"

#include "ED_util.h"
#include "ED_types.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BMF_Api.h"
#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

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

typedef struct uiWidgetColors {
	float outline[3];
	float inner[3];
	float inner_sel[3];
	float item[3];
	float text[3];
	float text_sel[3];
	short shaded;
	float shadetop, shadedown;
	
} uiWidgetColors;

typedef struct uiWidgetBase {
	
	int totvert, halfwayvert;
	float outer_v[64][2];
	float inner_v[64][2];
	float inner_uv[64][2];
	
	uiWidgetTrias tria1;
	uiWidgetTrias tria2;
	
} uiWidgetBase;


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



static void widget_init(uiWidgetBase *wt)
{
	wt->totvert= wt->halfwayvert= 0;
	wt->tria1.tot= 0;
	wt->tria2.tot= 0;
}


static void round_box_edges(uiWidgetBase *wt, int roundboxtype, rcti *rect, float rad)
{
	float vec[9][2], veci[9][2];
	float minx= rect->xmin, miny= rect->ymin, maxx= rect->xmax, maxy= rect->ymax;
	float radi= rad - 1.0f; /* rad inner */
	float minxi= minx + 1.0f; /* boundbox inner */
	float maxxi= maxx - 1.0f;
	float minyi= miny + 1.0f;
	float maxyi= maxy - 1.0f;
	float facxi= 1.0f/(maxxi-minxi); /* for uv */
	float facyi= 1.0f/(maxyi-minyi);
	int a, tot= 0;
	
	/* mult */
	for(a=0; a<9; a++) {
		veci[a][0]= radi*cornervec[a][0]; 
		veci[a][1]= radi*cornervec[a][1]; 
		vec[a][0]= rad*cornervec[a][0]; 
		vec[a][1]= rad*cornervec[a][1]; 
	}
	
	/* corner left-bottom */
	if(roundboxtype & 8) {
		
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
	if(roundboxtype & 4) {
		
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
	if(roundboxtype & 2) {
		
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
	if(roundboxtype & 1) {
		
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
	size= 0.5f*triasize*(rect->ymax-rect->ymin);

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
	float centx, centy, size;
	int a;
		
	/* center position and size */
	centx= rect->xmax - 0.5f*(rect->ymax-rect->ymin);
	centy= rect->ymin + 0.5f*(rect->ymax-rect->ymin);
	size= 0.4f*(rect->ymax-rect->ymin);
	
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
static void shadecolors(float *coltop, float *coldown, float *color, float shadetop, float shadedown)
{
	float hue, sat, val, valshade;
	
	rgb_to_hsv(color[0], color[1], color[2], &hue, &sat, &val);
	
	valshade= CLAMPIS(val+shadetop, 0.0f, 1.0f);
	hsv_to_rgb(hue, sat, valshade, coltop, coltop+1, coltop+2);

	valshade= CLAMPIS(val+shadedown, 0.0f, 1.0f);
	hsv_to_rgb(hue, sat, valshade, coldown, coldown+1, coldown+2);
}

static void round_box_shade_col(float *col1, float *col2, float fac)
{
	float col[4];
	
	col[0]= (fac*col1[0] + (1.0-fac)*col2[0]);
	col[1]= (fac*col1[1] + (1.0-fac)*col2[1]);
	col[2]= (fac*col1[2] + (1.0-fac)*col2[2]);
	col[3]= 1;
	
	glColor4fv(col);
}

static void widget_draw(uiWidgetBase *wt, uiWidgetColors *wcol, int state)
{
	float *inner= wcol->inner;
	int j, a;
	
	if(state & UI_SELECT)
		inner= wcol->inner_sel;
	
	glEnable(GL_BLEND);

	/* backdrop non AA */
	if(wcol->shaded==0) {
		/* filled center, solid */
		glColor3fv(inner);
		glBegin(GL_POLYGON);
		for(a=0; a<wt->totvert; a++)
			glVertex2fv(wt->inner_v[a]);
		glEnd();
	}
	else {
		float col1[3], col2[3];
		
		shadecolors(col1, col2, inner, wcol->shadetop, wcol->shadedown);
		
		glShadeModel(GL_SMOOTH);
		glBegin(GL_POLYGON);
		for(a=0; a<wt->totvert; a++) {
			round_box_shade_col(col1, col2, wt->inner_uv[a][1]);
			glVertex2fv(wt->inner_v[a]);
		}
		glEnd();
		glShadeModel(GL_FLAT);
	}
	
	/* for each AA step */
	for(j=0; j<8; j++) {
		glTranslatef(1.0*jit[j][0], 1.0*jit[j][1], 0.0f);
		
		/* outline */
		glColor4f(wcol->outline[0], wcol->outline[1], wcol->outline[0], 0.125);
		glBegin(GL_QUAD_STRIP);
		for(a=0; a<wt->totvert; a++) {
			glVertex2fv(wt->outer_v[a]);
			glVertex2fv(wt->inner_v[a]);
		}
		glVertex2fv(wt->outer_v[0]);
		glVertex2fv(wt->inner_v[0]);
		glEnd();
	
		/* emboss bottom shadow */
		glColor4f(1.0f, 1.0f, 1.0f, 0.02f);
		glBegin(GL_QUAD_STRIP);
		for(a=0; a<wt->halfwayvert; a++) {
			glVertex2fv(wt->outer_v[a]);
			glVertex2f(wt->outer_v[a][0], wt->outer_v[a][1]-1.0f);
		}
		glEnd();
		
		glTranslatef(-1.0*jit[j][0], -1.0*jit[j][1], 0.0f);
	}
	
	/* decoration */
	if(wt->tria1.tot || wt->tria2.tot) {
		/* for each AA step */
		for(j=0; j<8; j++) {
			glTranslatef(1.0*jit[j][0], 1.0*jit[j][1], 0.0f);

			if(wt->tria1.tot) {
				glColor4f(wcol->item[0], wcol->item[1], wcol->item[2], 0.125);
				widget_trias_draw(&wt->tria1);
			}
			if(wt->tria2.tot) {
				glColor4f(wcol->item[0], wcol->item[1], wcol->item[2], 0.125);
				widget_trias_draw(&wt->tria2);
			}
		
			glTranslatef(-1.0*jit[j][0], -1.0*jit[j][1], 0.0f);
		}
	}

	glDisable(GL_BLEND);
	
}

/* *********************** text/icon ************************************** */

static void widget_draw_text(uiBut *but, float x, float y)
{
	int transopts;
	int len;
	char *cpoin;
	
	ui_rasterpos_safe(x, y, but->aspect);
	if(but->type==IDPOIN) transopts= 0;	// no translation, of course!
	else transopts= ui_translate_buttons();
	
	/* cut string in 2 parts */
	cpoin= strchr(but->drawstr, '|');
	if(cpoin) *cpoin= 0;		
	
#ifdef INTERNATIONAL
	if (but->type == FTPREVIEW)
		FTF_DrawNewFontString (but->drawstr+but->ofs, FTF_INPUT_UTF8);
	else
		UI_DrawString(but->font, but->drawstr+but->ofs, transopts);
#else
	UI_DrawString(but->font, but->drawstr+but->ofs, transopts);
#endif
	
	/* part text right aligned */
	if(cpoin) {
		len= UI_GetStringWidth(but->font, cpoin+1, ui_translate_buttons());
		ui_rasterpos_safe( but->x2 - len*but->aspect-3, y, but->aspect);
		UI_DrawString(but->font, cpoin+1, ui_translate_buttons());
		*cpoin= '|';
	}
}

/* draws text and icons for buttons */
static void widget_draw_text_icon(uiBut *but, rcti *rect, float *col)
{
	float x, y;
	short t, pos, ch;
	short selsta_tmp, selend_tmp, selsta_draw, selwidth_draw;
	
	if(but==NULL) return;
	
	/* check for button text label */
	if (but->type == ICONTEXTROW) {
		ui_draw_icon(but, (BIFIconID) (but->icon+but->iconadd), 0);
	}
	else {
		
		/* text button selection and cursor */
		if(but->editstr && but->pos != -1) {
			
			if ((but->selend - but->selsta) > 0) {
				/* text button selection */
				selsta_tmp = but->selsta + strlen(but->str);
				selend_tmp = but->selend + strlen(but->str);
				
				if(but->drawstr[0]!=0) {
					ch= but->drawstr[selsta_tmp];
					but->drawstr[selsta_tmp]= 0;
					
					selsta_draw = but->aspect*UI_GetStringWidth(but->font, but->drawstr+but->ofs, ui_translate_buttons()) + 3;
					
					but->drawstr[selsta_tmp]= ch;
					
					
					ch= but->drawstr[selend_tmp];
					but->drawstr[selend_tmp]= 0;
					
					selwidth_draw = but->aspect*UI_GetStringWidth(but->font, but->drawstr+but->ofs, ui_translate_buttons()) + 3;
					
					but->drawstr[selend_tmp]= ch;
					
					UI_ThemeColor(TH_BUT_TEXTFIELD_HI);
					glRects(rect->xmin+selsta_draw+1, rect->ymin+2, rect->xmin+selwidth_draw+1, rect->ymax-2);
				}
			} else {
				/* text cursor */
				pos= but->pos+strlen(but->str);
				if(pos >= but->ofs) {
					if(but->drawstr[0]!=0) {
						ch= but->drawstr[pos];
						but->drawstr[pos]= 0;
						
						t= but->aspect*UI_GetStringWidth(but->font, but->drawstr+but->ofs, ui_translate_buttons()) + 3;
						
						but->drawstr[pos]= ch;
					}
					else t= 3;
					
					glColor3ub(255,0,0);
					glRects(rect->xmin+t, rect->ymin+2, rect->xmin+t+2, rect->ymax-2);
				}
			}
		}
		
		if(but->type==BUT_TOGDUAL) {
			int dualset= 0;
			if(but->pointype==SHO)
				dualset= BTST( *(((short *)but->poin)+1), but->bitnr);
			else if(but->pointype==INT)
				dualset= BTST( *(((int *)but->poin)+1), but->bitnr);
			
			ui_draw_icon(but, ICON_DOT, dualset?0:-100);
		}
		
		if(but->drawstr[0]!=0) {
			
			/* If there's an icon too (made with uiDefIconTextBut) then draw the icon
			and offset the text label to accomodate it */
			
			if ( (but->flag & UI_HAS_ICON) && (but->flag & UI_ICON_LEFT) ) 
			{
				ui_draw_icon(but, but->icon, 0);
				
				if(but->editstr || (but->flag & UI_TEXT_LEFT)) x= rect->xmin + but->aspect*UI_icon_get_width(but->icon)+5.0;
				else x= (rect->xmin+rect->xmax-but->strwidth+1)/2.0;
			}
			else
			{
				if(but->editstr || (but->flag & UI_TEXT_LEFT))
					x= rect->xmin+4.0;
				else if ELEM3(but->type, TOG, TOGN, TOG3)
					x= rect->xmin+28.0;	/* offset for checkmark */
				else
					x= (rect->xmin+rect->xmax-but->strwidth+1)/2.0;
			}
			
			/* position and draw */
			y = (rect->ymin+rect->ymax- 9.0)/2.0;
			
			glColor3fv(col);
			widget_draw_text(but, x, y);
			
		}
		/* if there's no text label, then check to see if there's an icon only and draw it */
		else if( but->flag & UI_HAS_ICON ) {
			ui_draw_icon(but, (BIFIconID) (but->icon+but->iconadd), 0);
		}
	}
}



/* *********************** widget types ************************************* */

/*		
 float outline[3];
 float inner[3];
 float select[3];
 float item[3];
 short shaded;
 float shadetop, shadedown;
*/	

static struct uiWidgetColors wcol_num= {
	{0.1f, 0.1f, 0.1f},
	{0.7f, 0.7f, 0.7f},
	{0.6f, 0.6f, 0.6f},
	{0.35f, 0.35f, 0.35f},
	
	{0.0f, 0.0f, 0.0f},
	{1.0f, 1.0f, 1.0f},
	
	1,
	-0.08f, 0.0f
};

static struct uiWidgetColors wcol_text= {
	{0.1f, 0.1f, 0.1f},
	{0.6f, 0.6f, 0.6f},
	{0.6f, 0.6f, 0.6f},
	{0.35f, 0.35f, 0.35f},
	
	{0.0f, 0.0f, 0.0f},
	{1.0f, 1.0f, 1.0f},
	
	1,
	0.0f, 0.1f
};

static struct uiWidgetColors wcol_menu= {
	{0.0f, 0.0f, 0.0f},
	{0.25f, 0.25f, 0.25f},
	{0.25f, 0.25f, 0.25f},
	{1.0f, 1.0f, 1.0f},
	
	{1.0f, 1.0f, 1.0f},
	{0.0f, 0.0f, 0.0f},
	
	1,
	0.1f, -0.08f
};

static struct uiWidgetColors wcol_row= {
	{0.0f, 0.0f, 0.0f},
	{0.25f, 0.25f, 0.25f},
	{0.34f, 0.5f, 0.76f},
	{1.0f, 1.0f, 1.0f},
	
	{1.0f, 1.0f, 1.0f},
	{0.0f, 0.0f, 0.0f},
	
	1,
	0.1f, -0.1f
};

static struct uiWidgetColors wcol_regular= {
	{0.1f, 0.1f, 0.1f},
	{0.6f, 0.6f, 0.6f},
	{0.4f, 0.4f, 0.4f},
	{0.1f, 0.1f, 0.1f},
	
	{0.0f, 0.0f, 0.0f},
	{1.0f, 1.0f, 1.0f},
	
	0,
	0.0f, 0.0f
};

static struct uiWidgetColors wcol_regular2= {
	{0.1f, 0.1f, 0.1f},
	{0.6f, 0.6f, 0.6f},
	{0.4f, 0.4f, 0.4f},
	{0.1f, 0.1f, 0.1f},
	
	{0.0f, 0.0f, 0.0f},
	{1.0f, 1.0f, 1.0f},
	
	1,
	0.1f, -0.1f
};


static void widget_numbut(uiBut *but, rcti *rect, int state, int roundboxtype)
{
	uiWidgetBase wt;
	
	widget_init(&wt);
	
	/* fully rounded */
	round_box_edges(&wt, roundboxtype, rect, 0.5f*(rect->ymax - rect->ymin));
	
	/* decoration */
	widget_num_tria(&wt.tria1, rect, 0.6f, 0);
	widget_num_tria(&wt.tria2, rect, 0.6f, 'r');
	
	widget_draw(&wt, &wcol_num, state);

	if(state & UI_SELECT)
		widget_draw_text_icon(but, rect, wcol_num.text_sel);
	else
		widget_draw_text_icon(but, rect, wcol_num.text);
}

static void widget_textbut(uiBut *but, rcti *rect, int state, int roundboxtype)
{
	uiWidgetBase wt;
	
	widget_init(&wt);
	
	/* half rounded */
	round_box_edges(&wt, roundboxtype, rect, 4.0f);
	
	/* XXX button state */
	widget_draw(&wt, &wcol_text, state);

	widget_draw_text_icon(but, rect, wcol_text.text);
}


static void widget_menubut(uiBut *but, rcti *rect, int state, int roundboxtype)
{
	uiWidgetBase wt;
	
	widget_init(&wt);
	
	/* half rounded */
	round_box_edges(&wt, roundboxtype, rect, 4.0f);
	
	/* XXX button state */
	
	/* decoration */
	widget_menu_trias(&wt.tria1, rect);
	
	widget_draw(&wt, &wcol_menu, state);

	widget_draw_text_icon(but, rect, wcol_menu.text);
}

static void widget_togbut(uiBut *but, rcti *rect, int state, int roundboxtype)
{
	uiWidgetBase wt;
	rcti recttemp= *rect;
	int delta;
	
	widget_init(&wt);
	
	/* square */
	recttemp.xmax= recttemp.xmin + (recttemp.ymax-recttemp.ymin);
	
	/* smaller */
	delta= 1 + (recttemp.ymax-recttemp.ymin)/8;
	recttemp.xmin+= delta;
	recttemp.ymin+= delta;
	recttemp.xmax-= delta;
	recttemp.ymax-= delta;
	
	/* half rounded */
	round_box_edges(&wt, roundboxtype, &recttemp, 4.0f);
	
	/* button state */
	
	/* decoration */
	if(state & UI_SELECT) {
		widget_check_trias(&wt.tria1, &recttemp);
	}
	
	widget_draw(&wt, &wcol_menu, state);

	if(state & UI_SELECT)
		widget_draw_text_icon(but, rect, wcol_menu.text);
	else
		widget_draw_text_icon(but, rect, wcol_menu.text_sel);
}


static void widget_rowbut(uiBut *but, rcti *rect, int state, int roundboxtype)
{
	uiWidgetBase wt;
	
	widget_init(&wt);
	
	/* half rounded */
	round_box_edges(&wt, roundboxtype, rect, 4.0f);
	
	widget_draw(&wt, &wcol_row, state);

	widget_draw_text_icon(but, rect, wcol_row.text);
}

static void widget_but(uiBut *but, rcti *rect, int state, int roundboxtype)
{
	uiWidgetBase wt;
	
	widget_init(&wt);
	
	/* half rounded */
	round_box_edges(&wt, roundboxtype, rect, 4.0f);
		
	widget_draw(&wt, &wcol_regular, state);

	widget_draw_text_icon(but, rect, wcol_regular.text);
}

static void widget_roundbut(uiBut *but, rcti *rect, int state, int roundboxtype)
{
	uiWidgetBase wt;
	
	widget_init(&wt);
	
	/* fully rounded */
	round_box_edges(&wt, roundboxtype, rect, 0.5f*(rect->ymax - rect->ymin));
	
	widget_num_tria(&wt.tria1, rect, 0.6f, 0);

	widget_draw(&wt, &wcol_regular2, state);
	
	widget_draw_text_icon(but, rect, wcol_regular2.text);

}

/* test function only */
void drawnewstuff()
{
	rcti rect;
	
	rect.xmin= 10; rect.xmax= 10+100;
	rect.ymin= -30; rect.ymax= -30+18;
	widget_numbut(NULL, &rect, 0, 15);
	
	rect.xmin= 120; rect.xmax= 120+100;
	rect.ymin= -30; rect.ymax= -30+20;
	widget_numbut(NULL, &rect, 0, 15);
	
	rect.xmin= 10; rect.xmax= 10+100;
	rect.ymin= -60; rect.ymax= -60+20;
	widget_menubut(NULL, &rect, 0, 15);

	rect.xmin= 120; rect.xmax= 120+100;
	widget_but(NULL, &rect, 0, 15);
	
	rect.xmin= 10; rect.xmax= 10+100;
	rect.ymin= -90; rect.ymax= -90+20;
	widget_rowbut(NULL, &rect, 1, 9);
	
	rect.xmin= 109; rect.xmax= 110+100;
	rect.ymin= -90; rect.ymax= -90+20;
	widget_rowbut(NULL, &rect, 0, 6);
	
	rect.xmin= 240; rect.xmax= 240+30;
	rect.ymin= -90; rect.ymax= -90+30;
	widget_roundbut(NULL, &rect, 0, 15);
}

/* ************ new color and style definition ********************* */
/*

- minimum width definition?

- Types
    * Icon toggle button
    * Row button (exclusive "enum" values)
    * Option button (also "bit flags")
    * Tool/Operator button
    * Number button
    * Number slider

    * Text string button (to rename data)
    * File name button (separate design?)
    * Linkage "Library" button (Object, Material, Parent, etc)
    * Linkage data name button (Bone, Vgroup)

    * Popup settings button, with optional text, icon or both.
    * Popup linkage button (Materials, Bones, etc)
    * Pulldown menu button (to invoke pulldown)
    * Pulldown menu item (and menu backdrop + title)

    * Button-less icons (open-close triangle, delete cross, ...)
    * Color picker Swatch
    * Color picker fields
    * Normal button (rotatable sphere) 

*/

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


/* widget classification

- state: 
	UI_MOUSE_OVER: on mouse over
	UI_ACTIVE: while using it
    UI_SELECT: internal state (toggle, row)

- drawtype
    CUSTOM: no widget class, entirely free within rect
    WIDGET: part of the standard widget set

- text placement, split?

- widget color style hint
   - outline
   - interior col
   - interior slider color?
   - shade factors
   - decoration color
   - text colors

- callbacks
    - widget_draw()
    - widget_text_icon()
    - 

*/


void ui_draw_but_new(ARegion *ar, uiBut *but)
{
	rcti rect;
	int roundboxtype, state;
	
	/* XXX project later */
	rect.xmin= but->x1;
	rect.xmax= but->x2;
	rect.ymin= but->y1;
	rect.ymax= but->y2;
	
	roundboxtype= widget_roundbox_set(but, &rect);
	state= but->flag;
	
	switch (but->type) {
		case LABEL:
			widget_draw_text_icon(but, &rect, wcol_regular2.text);
			break;
		case NUM:
			widget_numbut(but, &rect, state, roundboxtype);
			break;
		case ROW:
			widget_rowbut(but, &rect, state, roundboxtype);
			break;
		case TEX:
			widget_textbut(but, &rect, state, roundboxtype);
			break;
		case TOG:
		case TOGN:
		case TOG3:
			if (!(state & UI_HAS_ICON))
				widget_togbut(but, &rect, state, roundboxtype);
			else
				widget_but(but, &rect, state, roundboxtype);
			break;
		case MENU:
		case BLOCK:
			widget_menubut(but, &rect, state, roundboxtype);
			break;
			
		default:
			widget_but(but, &rect, state, roundboxtype);
	}
	
}





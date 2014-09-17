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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_widgets.c
 *  \ingroup edinterface
 */


#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "DNA_brush_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_curve.h"

#include "RNA_access.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLF_api.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"


#include "interface_intern.h"

/* icons are 80% of height of button (16 pixels inside 20 height) */
#define ICON_SIZE_FROM_BUTRECT(rect) (0.8f * BLI_rcti_size_y(rect))

/* ************** widget base functions ************** */
/*
 * - in: roundbox codes for corner types and radius
 * - return: array of [size][2][x,y] points, the edges of the roundbox, + UV coords
 *
 * - draw black box with alpha 0 on exact button boundbox
 * - for ever AA step:
 *    - draw the inner part for a round filled box, with color blend codes or texture coords
 *    - draw outline in outline color
 *    - draw outer part, bottom half, extruded 1 pixel to bottom, for emboss shadow
 *    - draw extra decorations
 * - draw background color box with alpha 1 on exact button boundbox
 */

/* fill this struct with polygon info to draw AA'ed */
/* it has outline, back, and two optional tria meshes */

typedef struct uiWidgetTrias {
	unsigned int tot;
	
	float vec[16][2];
	const unsigned int (*index)[3];
	
} uiWidgetTrias;

/* max as used by round_box__edges */
#define WIDGET_CURVE_RESOLU 9
#define WIDGET_SIZE_MAX (WIDGET_CURVE_RESOLU * 4)

typedef struct uiWidgetBase {
	
	int totvert, halfwayvert;
	float outer_v[WIDGET_SIZE_MAX][2];
	float inner_v[WIDGET_SIZE_MAX][2];
	float inner_uv[WIDGET_SIZE_MAX][2];
	
	bool inner, outline, emboss, shadedir;
	
	uiWidgetTrias tria1;
	uiWidgetTrias tria2;
	
} uiWidgetBase;

/* uiWidgetType: for time being only for visual appearance,
 * later, a handling callback can be added too 
 */
typedef struct uiWidgetType {
	
	/* pointer to theme color definition */
	uiWidgetColors *wcol_theme;
	uiWidgetStateColors *wcol_state;
	
	/* converted colors for state */
	uiWidgetColors wcol;
	
	void (*state)(struct uiWidgetType *, int state);
	void (*draw)(uiWidgetColors *, rcti *, int state, int roundboxalign);
	void (*custom)(uiBut *, uiWidgetColors *, rcti *, int state, int roundboxalign);
	void (*text)(uiFontStyle *, uiWidgetColors *, uiBut *, rcti *);
	
} uiWidgetType;


/* *********************** draw data ************************** */

static const float cornervec[WIDGET_CURVE_RESOLU][2] = {
	{0.0, 0.0}, {0.195, 0.02}, {0.383, 0.067},
	{0.55, 0.169}, {0.707, 0.293}, {0.831, 0.45},
	{0.924, 0.617}, {0.98, 0.805}, {1.0, 1.0}
};

#define WIDGET_AA_JITTER 8
static const float jit[WIDGET_AA_JITTER][2] = {
	{ 0.468813, -0.481430}, {-0.155755, -0.352820},
	{ 0.219306, -0.238501}, {-0.393286, -0.110949},
	{-0.024699,  0.013908}, { 0.343805,  0.147431},
	{-0.272855,  0.269918}, { 0.095909,  0.388710}
};

static const float num_tria_vert[3][2] = {
	{-0.352077, 0.532607}, {-0.352077, -0.549313}, {0.330000, -0.008353}
};

static const unsigned int num_tria_face[1][3] = {
	{0, 1, 2}
};

static const float scroll_circle_vert[16][2] = {
	{0.382684, 0.923879}, {0.000001, 1.000000}, {-0.382683, 0.923880}, {-0.707107, 0.707107},
	{-0.923879, 0.382684}, {-1.000000, 0.000000}, {-0.923880, -0.382684}, {-0.707107, -0.707107},
	{-0.382683, -0.923880}, {0.000000, -1.000000}, {0.382684, -0.923880}, {0.707107, -0.707107},
	{0.923880, -0.382684}, {1.000000, -0.000000}, {0.923880, 0.382683}, {0.707107, 0.707107}
};

static const unsigned int scroll_circle_face[14][3] = {
	{0, 1, 2}, {2, 0, 3}, {3, 0, 15}, {3, 15, 4}, {4, 15, 14}, {4, 14, 5}, {5, 14, 13}, {5, 13, 6},
	{6, 13, 12}, {6, 12, 7}, {7, 12, 11}, {7, 11, 8}, {8, 11, 10}, {8, 10, 9}
};


static const float menu_tria_vert[6][2] = {
	{-0.33, 0.16}, {0.33, 0.16}, {0, 0.82},
	{0, -0.82}, {-0.33, -0.16}, {0.33, -0.16}
};



static const unsigned int menu_tria_face[2][3] = {{2, 0, 1}, {3, 5, 4}};

static const float check_tria_vert[6][2] = {
	{-0.578579, 0.253369},  {-0.392773, 0.412794},  {-0.004241, -0.328551},
	{-0.003001, 0.034320},  {1.055313, 0.864744},   {0.866408, 1.026895}
};

static const unsigned int check_tria_face[4][3] = {
	{3, 2, 4}, {3, 4, 5}, {1, 0, 3}, {0, 2, 3}
};

/* ************************************************* */

void ui_draw_anti_tria(float x1, float y1, float x2, float y2, float x3, float y3)
{
	float tri_arr[3][2] = {{x1, y1}, {x2, y2}, {x3, y3}};
	float color[4];
	int j;
	
	glEnable(GL_BLEND);
	glGetFloatv(GL_CURRENT_COLOR, color);
	color[3] *= 0.125f;
	glColor4fv(color);

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, tri_arr);

	/* for each AA step */
	for (j = 0; j < WIDGET_AA_JITTER; j++) {
		glTranslatef(jit[j][0], jit[j][1], 0.0f);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glTranslatef(-jit[j][0], -jit[j][1], 0.0f);
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisable(GL_BLEND);
}

void ui_draw_anti_roundbox(int mode, float minx, float miny, float maxx, float maxy, float rad, bool use_alpha)
{
	float color[4];
	int j;
	
	glEnable(GL_BLEND);
	glGetFloatv(GL_CURRENT_COLOR, color);
	if (use_alpha) {
		color[3] = 0.5f;
	}
	color[3] *= 0.125f;
	glColor4fv(color);
	
	for (j = 0; j < WIDGET_AA_JITTER; j++) {
		glTranslatef(jit[j][0], jit[j][1], 0.0f);
		uiDrawBox(mode, minx, miny, maxx, maxy, rad);
		glTranslatef(-jit[j][0], -jit[j][1], 0.0f);
	}

	glDisable(GL_BLEND);
}

static void widget_init(uiWidgetBase *wtb)
{
	wtb->totvert = wtb->halfwayvert = 0;
	wtb->tria1.tot = 0;
	wtb->tria2.tot = 0;

	wtb->inner = 1;
	wtb->outline = 1;
	wtb->emboss = 1;
	wtb->shadedir = 1;
}

/* helper call, makes shadow rect, with 'sun' above menu, so only shadow to left/right/bottom */
/* return tot */
static int round_box_shadow_edges(float (*vert)[2], const rcti *rect, float rad, int roundboxalign, float step)
{
	float vec[WIDGET_CURVE_RESOLU][2];
	float minx, miny, maxx, maxy;
	int a, tot = 0;
	
	rad += step;
	
	if (2.0f * rad > BLI_rcti_size_y(rect))
		rad = 0.5f * BLI_rcti_size_y(rect);

	minx = rect->xmin - step;
	miny = rect->ymin - step;
	maxx = rect->xmax + step;
	maxy = rect->ymax + step;
	
	/* mult */
	for (a = 0; a < WIDGET_CURVE_RESOLU; a++) {
		vec[a][0] = rad * cornervec[a][0];
		vec[a][1] = rad * cornervec[a][1];
	}
	
	/* start with left-top, anti clockwise */
	if (roundboxalign & UI_CNR_TOP_LEFT) {
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			vert[tot][0] = minx + rad - vec[a][0];
			vert[tot][1] = maxy - vec[a][1];
		}
	}
	else {
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			vert[tot][0] = minx;
			vert[tot][1] = maxy;
		}
	}
	
	if (roundboxalign & UI_CNR_BOTTOM_LEFT) {
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			vert[tot][0] = minx + vec[a][1];
			vert[tot][1] = miny + rad - vec[a][0];
		}
	}
	else {
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			vert[tot][0] = minx;
			vert[tot][1] = miny;
		}
	}
	
	if (roundboxalign & UI_CNR_BOTTOM_RIGHT) {
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			vert[tot][0] = maxx - rad + vec[a][0];
			vert[tot][1] = miny + vec[a][1];
		}
	}
	else {
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			vert[tot][0] = maxx;
			vert[tot][1] = miny;
		}
	}
	
	if (roundboxalign & UI_CNR_TOP_RIGHT) {
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			vert[tot][0] = maxx - vec[a][1];
			vert[tot][1] = maxy - rad + vec[a][0];
		}
	}
	else {
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			vert[tot][0] = maxx;
			vert[tot][1] = maxy;
		}
	}
	return tot;
}

/* this call has 1 extra arg to allow mask outline */
static void round_box__edges(uiWidgetBase *wt, int roundboxalign, const rcti *rect, float rad, float radi)
{
	float vec[WIDGET_CURVE_RESOLU][2], veci[WIDGET_CURVE_RESOLU][2];
	float minx = rect->xmin, miny = rect->ymin, maxx = rect->xmax, maxy = rect->ymax;
	float minxi = minx + U.pixelsize; /* boundbox inner */
	float maxxi = maxx - U.pixelsize;
	float minyi = miny + U.pixelsize;
	float maxyi = maxy - U.pixelsize;
	float facxi = (maxxi != minxi) ? 1.0f / (maxxi - minxi) : 0.0f; /* for uv, can divide by zero */
	float facyi = (maxyi != minyi) ? 1.0f / (maxyi - minyi) : 0.0f;
	int a, tot = 0, minsize;
	const int hnum = ((roundboxalign & (UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT)) == (UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT) ||
	                  (roundboxalign & (UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT)) == (UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT)) ? 1 : 2;
	const int vnum = ((roundboxalign & (UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT)) == (UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT) ||
	                  (roundboxalign & (UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT)) == (UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT)) ? 1 : 2;

	minsize = min_ii(BLI_rcti_size_x(rect) * hnum,
	                 BLI_rcti_size_y(rect) * vnum);
	
	if (2.0f * rad > minsize)
		rad = 0.5f * minsize;

	if (2.0f * (radi + 1.0f) > minsize)
		radi = 0.5f * minsize - U.pixelsize;
	
	/* mult */
	for (a = 0; a < WIDGET_CURVE_RESOLU; a++) {
		veci[a][0] = radi * cornervec[a][0];
		veci[a][1] = radi * cornervec[a][1];
		vec[a][0] = rad * cornervec[a][0];
		vec[a][1] = rad * cornervec[a][1];
	}
	
	/* corner left-bottom */
	if (roundboxalign & UI_CNR_BOTTOM_LEFT) {
		
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			wt->inner_v[tot][0] = minxi + veci[a][1];
			wt->inner_v[tot][1] = minyi + radi - veci[a][0];
			
			wt->outer_v[tot][0] = minx + vec[a][1];
			wt->outer_v[tot][1] = miny + rad - vec[a][0];
			
			wt->inner_uv[tot][0] = facxi * (wt->inner_v[tot][0] - minxi);
			wt->inner_uv[tot][1] = facyi * (wt->inner_v[tot][1] - minyi);
		}
	}
	else {
		wt->inner_v[tot][0] = minxi;
		wt->inner_v[tot][1] = minyi;
		
		wt->outer_v[tot][0] = minx;
		wt->outer_v[tot][1] = miny;

		wt->inner_uv[tot][0] = 0.0f;
		wt->inner_uv[tot][1] = 0.0f;
		
		tot++;
	}
	
	/* corner right-bottom */
	if (roundboxalign & UI_CNR_BOTTOM_RIGHT) {
		
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			wt->inner_v[tot][0] = maxxi - radi + veci[a][0];
			wt->inner_v[tot][1] = minyi + veci[a][1];
			
			wt->outer_v[tot][0] = maxx - rad + vec[a][0];
			wt->outer_v[tot][1] = miny + vec[a][1];
			
			wt->inner_uv[tot][0] = facxi * (wt->inner_v[tot][0] - minxi);
			wt->inner_uv[tot][1] = facyi * (wt->inner_v[tot][1] - minyi);
		}
	}
	else {
		wt->inner_v[tot][0] = maxxi;
		wt->inner_v[tot][1] = minyi;
		
		wt->outer_v[tot][0] = maxx;
		wt->outer_v[tot][1] = miny;

		wt->inner_uv[tot][0] = 1.0f;
		wt->inner_uv[tot][1] = 0.0f;
		
		tot++;
	}
	
	wt->halfwayvert = tot;
	
	/* corner right-top */
	if (roundboxalign & UI_CNR_TOP_RIGHT) {
		
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			wt->inner_v[tot][0] = maxxi - veci[a][1];
			wt->inner_v[tot][1] = maxyi - radi + veci[a][0];
			
			wt->outer_v[tot][0] = maxx - vec[a][1];
			wt->outer_v[tot][1] = maxy - rad + vec[a][0];
			
			wt->inner_uv[tot][0] = facxi * (wt->inner_v[tot][0] - minxi);
			wt->inner_uv[tot][1] = facyi * (wt->inner_v[tot][1] - minyi);
		}
	}
	else {
		wt->inner_v[tot][0] = maxxi;
		wt->inner_v[tot][1] = maxyi;
		
		wt->outer_v[tot][0] = maxx;
		wt->outer_v[tot][1] = maxy;
		
		wt->inner_uv[tot][0] = 1.0f;
		wt->inner_uv[tot][1] = 1.0f;
		
		tot++;
	}
	
	/* corner left-top */
	if (roundboxalign & UI_CNR_TOP_LEFT) {
		
		for (a = 0; a < WIDGET_CURVE_RESOLU; a++, tot++) {
			wt->inner_v[tot][0] = minxi + radi - veci[a][0];
			wt->inner_v[tot][1] = maxyi - veci[a][1];
			
			wt->outer_v[tot][0] = minx + rad - vec[a][0];
			wt->outer_v[tot][1] = maxy - vec[a][1];
			
			wt->inner_uv[tot][0] = facxi * (wt->inner_v[tot][0] - minxi);
			wt->inner_uv[tot][1] = facyi * (wt->inner_v[tot][1] - minyi);
		}
		
	}
	else {
		
		wt->inner_v[tot][0] = minxi;
		wt->inner_v[tot][1] = maxyi;
		
		wt->outer_v[tot][0] = minx;
		wt->outer_v[tot][1] = maxy;
		
		wt->inner_uv[tot][0] = 0.0f;
		wt->inner_uv[tot][1] = 1.0f;
		
		tot++;
	}

	BLI_assert(tot <= WIDGET_SIZE_MAX);

	wt->totvert = tot;
}

static void round_box_edges(uiWidgetBase *wt, int roundboxalign, const rcti *rect, float rad)
{
	round_box__edges(wt, roundboxalign, rect, rad, rad - U.pixelsize);
}


/* based on button rect, return scaled array of triangles */
static void widget_draw_tria_ex(
        uiWidgetTrias *tria, const rcti *rect, float triasize, char where,
        /* input data */
        const float verts[][2], const int verts_tot,
        const unsigned int tris[][3], const int tris_tot)
{
	float centx, centy, sizex, sizey, minsize;
	int a, i1 = 0, i2 = 1;

	minsize = min_ii(BLI_rcti_size_x(rect), BLI_rcti_size_y(rect));

	/* center position and size */
	centx = (float)rect->xmin + 0.5f * minsize;
	centy = (float)rect->ymin + 0.5f * minsize;
	sizex = sizey = -0.5f * triasize * minsize;

	if (where == 'r') {
		centx = (float)rect->xmax - 0.5f * minsize;
		sizex = -sizex;
	}
	else if (where == 't') {
		centy = (float)rect->ymax - 0.5f * minsize;
		sizey = -sizey;
		i2 = 0; i1 = 1;
	}
	else if (where == 'b') {
		sizex = -sizex;
		i2 = 0; i1 = 1;
	}

	for (a = 0; a < verts_tot; a++) {
		tria->vec[a][0] = sizex * verts[a][i1] + centx;
		tria->vec[a][1] = sizey * verts[a][i2] + centy;
	}

	tria->tot = tris_tot;
	tria->index = tris;
}

static void widget_num_tria(uiWidgetTrias *tria, const rcti *rect, float triasize, char where)
{
	widget_draw_tria_ex(
	        tria, rect, triasize, where,
	        num_tria_vert, ARRAY_SIZE(num_tria_vert),
	        num_tria_face, ARRAY_SIZE(num_tria_face));
}

static void widget_scroll_circle(uiWidgetTrias *tria, const rcti *rect, float triasize, char where)
{
	widget_draw_tria_ex(
	        tria, rect, triasize, where,
	        scroll_circle_vert, ARRAY_SIZE(scroll_circle_vert),
	        scroll_circle_face, ARRAY_SIZE(scroll_circle_face));
}

static void widget_trias_draw(uiWidgetTrias *tria)
{
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, tria->vec);
	glDrawElements(GL_TRIANGLES, tria->tot * 3, GL_UNSIGNED_INT, tria->index);
	glDisableClientState(GL_VERTEX_ARRAY);
}

static void widget_menu_trias(uiWidgetTrias *tria, const rcti *rect)
{
	float centx, centy, size;
	int a;
		
	/* center position and size */
	centx = rect->xmax - 0.32f * BLI_rcti_size_y(rect);
	centy = rect->ymin + 0.50f * BLI_rcti_size_y(rect);
	size = 0.4f * (float)BLI_rcti_size_y(rect);
	
	for (a = 0; a < 6; a++) {
		tria->vec[a][0] = size * menu_tria_vert[a][0] + centx;
		tria->vec[a][1] = size * menu_tria_vert[a][1] + centy;
	}

	tria->tot = 2;
	tria->index = menu_tria_face;
}

static void widget_check_trias(uiWidgetTrias *tria, const rcti *rect)
{
	float centx, centy, size;
	int a;
	
	/* center position and size */
	centx = rect->xmin + 0.5f * BLI_rcti_size_y(rect);
	centy = rect->ymin + 0.5f * BLI_rcti_size_y(rect);
	size = 0.5f * BLI_rcti_size_y(rect);
	
	for (a = 0; a < 6; a++) {
		tria->vec[a][0] = size * check_tria_vert[a][0] + centx;
		tria->vec[a][1] = size * check_tria_vert[a][1] + centy;
	}
	
	tria->tot = 4;
	tria->index = check_tria_face;
}


/* prepares shade colors */
static void shadecolors4(char coltop[4], char coldown[4], const char *color, short shadetop, short shadedown)
{
	
	coltop[0] = CLAMPIS(color[0] + shadetop, 0, 255);
	coltop[1] = CLAMPIS(color[1] + shadetop, 0, 255);
	coltop[2] = CLAMPIS(color[2] + shadetop, 0, 255);
	coltop[3] = color[3];

	coldown[0] = CLAMPIS(color[0] + shadedown, 0, 255);
	coldown[1] = CLAMPIS(color[1] + shadedown, 0, 255);
	coldown[2] = CLAMPIS(color[2] + shadedown, 0, 255);
	coldown[3] = color[3];
}

static void round_box_shade_col4_r(unsigned char r_col[4], const char col1[4], const char col2[4], const float fac)
{
	const int faci = FTOCHAR(fac);
	const int facm = 255 - faci;

	r_col[0] = (faci * col1[0] + facm * col2[0]) >> 8;
	r_col[1] = (faci * col1[1] + facm * col2[1]) >> 8;
	r_col[2] = (faci * col1[2] + facm * col2[2]) >> 8;
	r_col[3] = (faci * col1[3] + facm * col2[3]) >> 8;
}

static void widget_verts_to_quad_strip(uiWidgetBase *wtb, const int totvert, float quad_strip[WIDGET_SIZE_MAX * 2 + 2][2])
{
	int a;
	for (a = 0; a < totvert; a++) {
		copy_v2_v2(quad_strip[a * 2], wtb->outer_v[a]);
		copy_v2_v2(quad_strip[a * 2 + 1], wtb->inner_v[a]);
	}
	copy_v2_v2(quad_strip[a * 2], wtb->outer_v[0]);
	copy_v2_v2(quad_strip[a * 2 + 1], wtb->inner_v[0]);
}

static void widget_verts_to_quad_strip_open(uiWidgetBase *wtb, const int totvert, float quad_strip[WIDGET_SIZE_MAX * 2][2])
{
	int a;
	for (a = 0; a < totvert; a++) {
		quad_strip[a * 2][0] = wtb->outer_v[a][0];
		quad_strip[a * 2][1] = wtb->outer_v[a][1];
		quad_strip[a * 2 + 1][0] = wtb->outer_v[a][0];
		quad_strip[a * 2 + 1][1] = wtb->outer_v[a][1] - 1.0f;
	}
}

static void widgetbase_outline(uiWidgetBase *wtb)
{
	float quad_strip[WIDGET_SIZE_MAX * 2 + 2][2]; /* + 2 because the last pair is wrapped */
	widget_verts_to_quad_strip(wtb, wtb->totvert, quad_strip);

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, quad_strip);
	glDrawArrays(GL_QUAD_STRIP, 0, wtb->totvert * 2 + 2);
	glDisableClientState(GL_VERTEX_ARRAY);
}

static void widgetbase_draw(uiWidgetBase *wtb, uiWidgetColors *wcol)
{
	int j, a;
	
	glEnable(GL_BLEND);

	/* backdrop non AA */
	if (wtb->inner) {
		if (wcol->shaded == 0) {
			if (wcol->alpha_check) {
				float inner_v_half[WIDGET_SIZE_MAX][2];
				float x_mid = 0.0f; /* used for dumb clamping of values */

				/* dark checkers */
				glColor4ub(UI_ALPHA_CHECKER_DARK, UI_ALPHA_CHECKER_DARK, UI_ALPHA_CHECKER_DARK, 255);
				glEnableClientState(GL_VERTEX_ARRAY);
				glVertexPointer(2, GL_FLOAT, 0, wtb->inner_v);
				glDrawArrays(GL_POLYGON, 0, wtb->totvert);

				/* light checkers */
				glEnable(GL_POLYGON_STIPPLE);
				glColor4ub(UI_ALPHA_CHECKER_LIGHT, UI_ALPHA_CHECKER_LIGHT, UI_ALPHA_CHECKER_LIGHT, 255);
				glPolygonStipple(stipple_checker_8px);

				glVertexPointer(2, GL_FLOAT, 0, wtb->inner_v);
				glDrawArrays(GL_POLYGON, 0, wtb->totvert);

				glDisable(GL_POLYGON_STIPPLE);

				/* alpha fill */
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

				glColor4ubv((unsigned char *)wcol->inner);

				for (a = 0; a < wtb->totvert; a++) {
					x_mid += wtb->inner_v[a][0];
				}
				x_mid /= wtb->totvert;

				glVertexPointer(2, GL_FLOAT, 0, wtb->inner_v);
				glDrawArrays(GL_POLYGON, 0, wtb->totvert);

				/* 1/2 solid color */
				glColor4ub(wcol->inner[0], wcol->inner[1], wcol->inner[2], 255);

				for (a = 0; a < wtb->totvert; a++) {
					inner_v_half[a][0] = MIN2(wtb->inner_v[a][0], x_mid);
					inner_v_half[a][1] = wtb->inner_v[a][1];
				}

				glVertexPointer(2, GL_FLOAT, 0, inner_v_half);
				glDrawArrays(GL_POLYGON, 0, wtb->totvert);
				glDisableClientState(GL_VERTEX_ARRAY);
			}
			else {
				/* simple fill */
				glColor4ubv((unsigned char *)wcol->inner);

				glEnableClientState(GL_VERTEX_ARRAY);
				glVertexPointer(2, GL_FLOAT, 0, wtb->inner_v);
				glDrawArrays(GL_POLYGON, 0, wtb->totvert);
				glDisableClientState(GL_VERTEX_ARRAY);
			}
		}
		else {
			char col1[4], col2[4];
			unsigned char col_array[WIDGET_SIZE_MAX * 4];
			unsigned char *col_pt = col_array;
			
			shadecolors4(col1, col2, wcol->inner, wcol->shadetop, wcol->shadedown);
			
			glShadeModel(GL_SMOOTH);
			for (a = 0; a < wtb->totvert; a++, col_pt += 4) {
				round_box_shade_col4_r(col_pt, col1, col2, wtb->inner_uv[a][wtb->shadedir]);
			}

			glEnableClientState(GL_VERTEX_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);
			glVertexPointer(2, GL_FLOAT, 0, wtb->inner_v);
			glColorPointer(4, GL_UNSIGNED_BYTE, 0, col_array);
			glDrawArrays(GL_POLYGON, 0, wtb->totvert);
			glDisableClientState(GL_VERTEX_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);

			glShadeModel(GL_FLAT);
		}
	}
	
	/* for each AA step */
	if (wtb->outline) {
		float quad_strip[WIDGET_SIZE_MAX * 2 + 2][2]; /* + 2 because the last pair is wrapped */
		float quad_strip_emboss[WIDGET_SIZE_MAX * 2][2]; /* only for emboss */

		const unsigned char tcol[4] = {wcol->outline[0],
		                               wcol->outline[1],
		                               wcol->outline[2],
		                               wcol->outline[3] / WIDGET_AA_JITTER};

		widget_verts_to_quad_strip(wtb, wtb->totvert, quad_strip);

		if (wtb->emboss) {
			widget_verts_to_quad_strip_open(wtb, wtb->halfwayvert, quad_strip_emboss);
		}

		glEnableClientState(GL_VERTEX_ARRAY);

		for (j = 0; j < WIDGET_AA_JITTER; j++) {
			glTranslatef(jit[j][0], jit[j][1], 0.0f);
			
			/* outline */
			glColor4ubv(tcol);

			glVertexPointer(2, GL_FLOAT, 0, quad_strip);
			glDrawArrays(GL_QUAD_STRIP, 0, wtb->totvert * 2 + 2);
		
			/* emboss bottom shadow */
			if (wtb->emboss) {
				glColor4f(1.0f, 1.0f, 1.0f, 0.02f);

				glVertexPointer(2, GL_FLOAT, 0, quad_strip_emboss);
				glDrawArrays(GL_QUAD_STRIP, 0, wtb->halfwayvert * 2);
			}
			
			glTranslatef(-jit[j][0], -jit[j][1], 0.0f);
		}

		glDisableClientState(GL_VERTEX_ARRAY);
	}
	
	/* decoration */
	if (wtb->tria1.tot || wtb->tria2.tot) {
		const unsigned char tcol[4] = {wcol->item[0],
		                               wcol->item[1],
		                               wcol->item[2],
		                               (unsigned char)((float)wcol->item[3] / WIDGET_AA_JITTER)};
		/* for each AA step */
		for (j = 0; j < WIDGET_AA_JITTER; j++) {
			glTranslatef(jit[j][0], jit[j][1], 0.0f);

			if (wtb->tria1.tot) {
				glColor4ubv(tcol);
				widget_trias_draw(&wtb->tria1);
			}
			if (wtb->tria2.tot) {
				glColor4ubv(tcol);
				widget_trias_draw(&wtb->tria2);
			}
		
			glTranslatef(-jit[j][0], -jit[j][1], 0.0f);
		}
	}

	glDisable(GL_BLEND);
	
}

/* *********************** text/icon ************************************** */

#define UI_TEXT_CLIP_MARGIN (0.25f * U.widget_unit / but->block->aspect)

#define PREVIEW_PAD 4

static void widget_draw_preview(BIFIconID icon, float UNUSED(alpha), const rcti *rect)
{
	int w, h, size;

	if (icon == ICON_NONE)
		return;

	w = BLI_rcti_size_x(rect);
	h = BLI_rcti_size_y(rect);
	size = MIN2(w, h);
	size -= PREVIEW_PAD * 2;  /* padding */

	if (size > 0) {
		int x = rect->xmin + w / 2 - size / 2;
		int y = rect->ymin + h / 2 - size / 2;

		UI_icon_draw_preview_aspect_size(x, y, icon, 1.0f, size);
	}
}


static int ui_but_draw_menu_icon(const uiBut *but)
{
	return (but->flag & UI_ICON_SUBMENU) && (but->dt == UI_EMBOSSP);
}

/* icons have been standardized... and this call draws in untransformed coordinates */

static void widget_draw_icon(const uiBut *but, BIFIconID icon, float alpha, const rcti *rect,
                             const bool show_menu_icon)
{
	float xs = 0.0f, ys = 0.0f;
	float aspect, height;
	
	if (but->flag & UI_ICON_PREVIEW) {
		glEnable(GL_BLEND);
		widget_draw_preview(icon, alpha, rect);
		glDisable(GL_BLEND);
		return;
	}
	
	/* this icon doesn't need draw... */
	if (icon == ICON_BLANK1 && (but->flag & UI_ICON_SUBMENU) == 0) return;
	
	aspect = but->block->aspect / UI_DPI_FAC;
	height = ICON_DEFAULT_HEIGHT / aspect;

	/* calculate blend color */
	if (ELEM(but->type, TOG, ROW, TOGN, LISTROW)) {
		if (but->flag & UI_SELECT) {}
		else if (but->flag & UI_ACTIVE) {}
		else alpha = 0.5f;
	}
	
	/* extra feature allows more alpha blending */
	if ((but->type == LABEL) && but->a1 == 1.0f)
		alpha *= but->a2;
	
	glEnable(GL_BLEND);
	
	if (icon && icon != ICON_BLANK1) {
		float ofs = 1.0f / aspect;
		
		if (but->drawflag & UI_BUT_ICON_LEFT) {
			if (but->block->flag & UI_BLOCK_LOOP) {
				if (ELEM(but->type, SEARCH_MENU, SEARCH_MENU_UNLINK))
					xs = rect->xmin + 4.0f * ofs;
				else
					xs = rect->xmin + ofs;
			}
			else {
				xs = rect->xmin + 4.0f * ofs;
			}
			ys = (rect->ymin + rect->ymax - height) / 2.0f;
		}
		else {
			xs = (rect->xmin + rect->xmax - height) / 2.0f;
			ys = (rect->ymin + rect->ymax - height) / 2.0f;
		}

		/* force positions to integers, for zoom levels near 1. draws icons crisp. */
		if (aspect > 0.95f && aspect < 1.05f) {
			xs = (int)(xs + 0.1f);
			ys = (int)(ys + 0.1f);
		}
		
		/* to indicate draggable */
		if (but->dragpoin && (but->flag & UI_ACTIVE)) {
			float rgb[3] = {1.25f, 1.25f, 1.25f};
			UI_icon_draw_aspect_color(xs, ys, icon, aspect, rgb);
		}
		else
			UI_icon_draw_aspect(xs, ys, icon, aspect, alpha);
	}

	if (show_menu_icon) {
		xs = rect->xmax - UI_DPI_ICON_SIZE - aspect;
		ys = (rect->ymin + rect->ymax - height) / 2.0f;
		
		UI_icon_draw_aspect(xs, ys, ICON_RIGHTARROW_THIN, aspect, alpha);
	}
	
	glDisable(GL_BLEND);
}

static void ui_text_clip_give_prev_off(uiBut *but, const char *str)
{
	const char *prev_utf8 = BLI_str_find_prev_char_utf8(str, str + but->ofs);
	int bytes = str + but->ofs - prev_utf8;

	but->ofs -= bytes;
}

static void ui_text_clip_give_next_off(uiBut *but, const char *str)
{
	const char *next_utf8 = BLI_str_find_next_char_utf8(str + but->ofs, NULL);
	int bytes = next_utf8 - (str + but->ofs);

	but->ofs += bytes;
}

/* Helper.
 * This func assumes things like kerning handling have already been handled!
 * Return the length of modified (right-clipped + ellipsis) string.
 */
static void ui_text_clip_right_ex(uiFontStyle *fstyle, char *str, const size_t max_len, const float okwidth,
                                  const char *sep, const int sep_len, const float sep_strwidth, size_t *r_final_len)
{
	float tmp;
	int l_end;

	BLI_assert(str[0]);

	/* If the trailing ellipsis takes more than 20% of all available width, just cut the string
	 * (as using the ellipsis would remove even more useful chars, and we cannot show much already!).
	 */
	if (sep_strwidth / okwidth > 0.2f) {
		l_end = BLF_width_to_strlen(fstyle->uifont_id, str, max_len, okwidth, &tmp);
		str[l_end] = '\0';
		if (r_final_len) {
			*r_final_len = (size_t)l_end;
		}
	}
	else {
		l_end = BLF_width_to_strlen(fstyle->uifont_id, str, max_len, okwidth - sep_strwidth, &tmp);
		memcpy(str + l_end, sep, sep_len + 1);  /* +1 for trailing '\0'. */
		if (r_final_len) {
			*r_final_len = (size_t)(l_end + sep_len);
		}
	}
}

/**
 * Cut off the middle of the text to fit into the given width.
 * Note in case this middle clipping would just remove a few chars, it rather clips right, which is more readable.
 * If rpart_sep is not Null, the part of str starting to first occurrence of rpart_sep is preserved at all cost (useful
 * for strings with shortcuts, like 'AVeryLongFooBarLabelForMenuEntry|Ctrl O' -> 'AVeryLong...MenuEntry|Ctrl O').
 */
static float ui_text_clip_middle_ex(uiFontStyle *fstyle, char *str, float okwidth, const float minwidth,
                                    const size_t max_len, const char *rpart_sep)
{
	float strwidth;

	BLI_assert(str[0]);

	/* need to set this first */
	uiStyleFontSet(fstyle);

	if (fstyle->kerning == 1) {  /* for BLF_width */
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	}

	strwidth = BLF_width(fstyle->uifont_id, str, max_len);

	if ((okwidth > 0.0f) && (strwidth > okwidth)) {
		/* utf8 ellipsis '...', some compilers complain */
		const char sep[] = {0xe2, 0x80, 0xa6, 0x0};
		const int sep_len = sizeof(sep) - 1;
		const float sep_strwidth = BLF_width(fstyle->uifont_id, sep, sep_len + 1);
		float parts_strwidth;
		size_t l_end;

		char *rpart = NULL, rpart_buf[UI_MAX_DRAW_STR];
		float rpart_width = 0.0f;
		size_t rpart_len = 0;
		size_t final_lpart_len;

		if (rpart_sep) {
			rpart = strstr(str, rpart_sep);

			if (rpart) {
				rpart_len = strlen(rpart);
				rpart_width = BLF_width(fstyle->uifont_id, rpart, rpart_len);
				okwidth -= rpart_width;
				strwidth -= rpart_width;

				if (okwidth < 0.0f) {
					/* Not enough place for actual label, just display protected right part.
					 * Here just for safety, should never happen in real life! */
					memmove(str, rpart, rpart_len + 1);
					rpart = NULL;
					okwidth += rpart_width;
					strwidth = rpart_width;
				}
			}
		}

		parts_strwidth = (okwidth - sep_strwidth) / 2.0f;

		if (rpart) {
			strcpy(rpart_buf, rpart);
			*rpart = '\0';
			rpart = rpart_buf;
		}

		l_end = BLF_width_to_strlen(fstyle->uifont_id, str, max_len, parts_strwidth, &rpart_width);
		if (l_end < 10 || min_ff(parts_strwidth, strwidth - okwidth) < minwidth) {
			/* If we really have no place, or we would clip a very small piece of string in the middle,
			 * only show start of string.
			 */
			ui_text_clip_right_ex(fstyle, str, max_len, okwidth, sep, sep_len, sep_strwidth, &final_lpart_len);
		}
		else {
			size_t r_offset, r_len;

			r_offset = BLF_width_to_rstrlen(fstyle->uifont_id, str, max_len, parts_strwidth, &rpart_width);
			r_len = strlen(str + r_offset) + 1;  /* +1 for the trailing '\0'. */

			if (l_end + sep_len + r_len + rpart_len > max_len) {
				/* Corner case, the str already takes all available mem, and the ellipsis chars would actually
				 * add more chars...
				 * Better to just trim one or two letters to the right in this case...
				 * Note: with a single-char ellipsis, this should never happen! But better be safe here...
				 */
				ui_text_clip_right_ex(fstyle, str, max_len, okwidth, sep, sep_len, sep_strwidth, &final_lpart_len);
			}
			else {
				memmove(str + l_end + sep_len, str + r_offset, r_len);
				memcpy(str + l_end, sep, sep_len);
				final_lpart_len = (size_t)(l_end + sep_len + r_len - 1);  /* -1 to remove trailing '\0'! */
			}
		}

		if (rpart) {
			/* Add back preserved right part to our shorten str. */
			memcpy(str + final_lpart_len, rpart, rpart_len + 1);  /* +1 for trailing '\0'. */
		}

		strwidth = BLF_width(fstyle->uifont_id, str, max_len);
	}

	if (fstyle->kerning == 1) {
		BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	}

	return strwidth;
}

/**
 * Wrapper around ui_text_clip_middle_ex.
 */
static void ui_text_clip_middle(uiFontStyle *fstyle, uiBut *but, const rcti *rect)
{
	/* No margin for labels! */
	const int border = ELEM(but->type, LABEL, MENU) ? 0 : (int)(UI_TEXT_CLIP_MARGIN + 0.5f);
	const float okwidth = (float)max_ii(BLI_rcti_size_x(rect) - border, 0);
	const size_t max_len = sizeof(but->drawstr);
	const float minwidth = (float)(UI_DPI_ICON_SIZE) / but->block->aspect * 2.0f;

	but->ofs = 0;
	but->strwidth = ui_text_clip_middle_ex(fstyle, but->drawstr, okwidth, minwidth, max_len, NULL);
}

/**
 * Like ui_text_clip_middle(), but protect/preserve at all cost the right part of the string after sep.
 * Useful for strings with shortcuts (like 'AVeryLongFooBarLabelForMenuEntry|Ctrl O' -> 'AVeryLong...MenuEntry|Ctrl O').
 */
static void ui_text_clip_middle_protect_right(uiFontStyle *fstyle, uiBut *but, const rcti *rect, const char *rsep)
{
	/* No margin for labels! */
	const int border = ELEM(but->type, LABEL, MENU) ? 0 : (int)(UI_TEXT_CLIP_MARGIN + 0.5f);
	const float okwidth = (float)max_ii(BLI_rcti_size_x(rect) - border, 0);
	const size_t max_len = sizeof(but->drawstr);
	const float minwidth = (float)(UI_DPI_ICON_SIZE) / but->block->aspect * 2.0f;

	but->ofs = 0;
	but->strwidth = ui_text_clip_middle_ex(fstyle, but->drawstr, okwidth, minwidth, max_len, rsep);
}

/**
 * Cut off the text, taking into account the cursor location (text display while editing).
 */
static void ui_text_clip_cursor(uiFontStyle *fstyle, uiBut *but, const rcti *rect)
{
	const int border = (int)(UI_TEXT_CLIP_MARGIN + 0.5f);
	const int okwidth = max_ii(BLI_rcti_size_x(rect) - border, 0);

	BLI_assert(but->editstr && but->pos >= 0);

	/* need to set this first */
	uiStyleFontSet(fstyle);

	if (fstyle->kerning == 1) /* for BLF_width */
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);

	/* define ofs dynamically */
	if (but->ofs > but->pos)
		but->ofs = but->pos;

	if (BLF_width(fstyle->uifont_id, but->editstr, INT_MAX) <= okwidth)
		but->ofs = 0;

	but->strwidth = BLF_width(fstyle->uifont_id, but->editstr + but->ofs, INT_MAX);

	if (but->strwidth > okwidth) {
		int len = strlen(but->editstr);

		while (but->strwidth > okwidth) {
			float width;

			/* string position of cursor */
			width = BLF_width(fstyle->uifont_id, but->editstr + but->ofs, (but->pos - but->ofs));

			/* if cursor is at 20 pixels of right side button we clip left */
			if (width > okwidth - 20) {
				ui_text_clip_give_next_off(but, but->editstr);
			}
			else {
				int bytes;
				/* shift string to the left */
				if (width < 20 && but->ofs > 0)
					ui_text_clip_give_prev_off(but, but->editstr);
				bytes = BLI_str_utf8_size(BLI_str_find_prev_char_utf8(but->editstr, but->editstr + len));
				if (bytes == -1)
					bytes = 1;
				len -= bytes;
			}

			but->strwidth = BLF_width(fstyle->uifont_id, but->editstr + but->ofs, len - but->ofs);

			if (but->strwidth < 10) break;
		}
	}

	if (fstyle->kerning == 1) {
		BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	}
}

/**
 * Cut off the end of text to fit into the width of \a rect.
 *
 * \note deals with ': ' especially for number buttons
 */
static void ui_text_clip_right_label(uiFontStyle *fstyle, uiBut *but, const rcti *rect)
{
	const int border = UI_TEXT_CLIP_MARGIN + 1;
	const int okwidth = max_ii(BLI_rcti_size_x(rect) - border, 0);
	char *cpoin = NULL;
	int drawstr_len = strlen(but->drawstr);
	const char *cpend = but->drawstr + drawstr_len;
	
	/* need to set this first */
	uiStyleFontSet(fstyle);
	
	if (fstyle->kerning == 1) /* for BLF_width */
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	
	but->strwidth = BLF_width(fstyle->uifont_id, but->drawstr, sizeof(but->drawstr));
	but->ofs = 0;
	

	/* First shorten num-buttons eg,
	 *   Translucency: 0.000
	 * becomes
	 *   Trans: 0.000
	 */

	/* find the space after ':' separator */
	cpoin = strrchr(but->drawstr, ':');
	
	if (cpoin && (cpoin < cpend - 2)) {
		char *cp2 = cpoin;
		
		/* chop off the leading text, starting from the right */
		while (but->strwidth > okwidth && cp2 > but->drawstr) {
			const char *prev_utf8 = BLI_str_find_prev_char_utf8(but->drawstr, cp2);
			int bytes = cp2 - prev_utf8;

			/* shift the text after and including cp2 back by 1 char, +1 to include null terminator */
			memmove(cp2 - bytes, cp2, drawstr_len + 1);
			cp2 -= bytes;

			drawstr_len -= bytes;
			// BLI_assert(strlen(but->drawstr) == drawstr_len);
			
			but->strwidth = BLF_width(fstyle->uifont_id, but->drawstr + but->ofs, sizeof(but->drawstr) - but->ofs);
			if (but->strwidth < 10) break;
		}
	
	
		/* after the leading text is gone, chop off the : and following space, with ofs */
		while ((but->strwidth > okwidth) && (but->ofs < 2)) {
			ui_text_clip_give_next_off(but, but->drawstr);
			but->strwidth = BLF_width(fstyle->uifont_id, but->drawstr + but->ofs, sizeof(but->drawstr) - but->ofs);
			if (but->strwidth < 10) break;
		}
		
	}


	/* Now just remove trailing chars */
	/* once the label's gone, chop off the least significant digits */
	if (but->strwidth > okwidth) {
		float strwidth;
		drawstr_len = BLF_width_to_strlen(fstyle->uifont_id, but->drawstr + but->ofs,
		                                  drawstr_len - but->ofs, okwidth, &strwidth) + but->ofs;
		but->strwidth = strwidth;
		but->drawstr[drawstr_len] = 0;
	}
	
	if (fstyle->kerning == 1)
		BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
}

static void widget_draw_text(uiFontStyle *fstyle, uiWidgetColors *wcol, uiBut *but, rcti *rect)
{
	int drawstr_left_len = UI_MAX_DRAW_STR;
	const char *drawstr = but->drawstr;
	const char *drawstr_right = NULL;
	bool use_right_only = false;

	uiStyleFontSet(fstyle);
	
	if (but->editstr || (but->drawflag & UI_BUT_TEXT_LEFT))
		fstyle->align = UI_STYLE_TEXT_LEFT;
	else if (but->drawflag & UI_BUT_TEXT_RIGHT)
		fstyle->align = UI_STYLE_TEXT_RIGHT;
	else
		fstyle->align = UI_STYLE_TEXT_CENTER;
	
	if (fstyle->kerning == 1) /* for BLF_width */
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	

	/* Special case: when we're entering text for multiple buttons,
	 * don't draw the text for any of the multi-editing buttons */
	if (UNLIKELY(but->flag & UI_BUT_DRAG_MULTI)) {
		uiBut *but_edit = ui_get_but_drag_multi_edit(but);
		if (but_edit) {
			drawstr = but_edit->editstr;
			fstyle->align = UI_STYLE_TEXT_LEFT;
		}
	}
	else {
		if (but->editstr) {
			/* max length isn't used in this case,
			 * we rely on string being NULL terminated. */
			drawstr_left_len = INT_MAX;
			drawstr = but->editstr;
		}
	}


	/* text button selection and cursor */
	if (but->editstr && but->pos != -1) {

		/* text button selection */
		if ((but->selend - but->selsta) > 0) {
			int selsta_draw, selwidth_draw;
			
			if (drawstr[0] != 0) {

				if (but->selsta >= but->ofs) {
					selsta_draw = BLF_width(fstyle->uifont_id, drawstr + but->ofs, but->selsta - but->ofs);
				}
				else {
					selsta_draw = 0;
				}

				selwidth_draw = BLF_width(fstyle->uifont_id, drawstr + but->ofs, but->selend - but->ofs);

				glColor4ubv((unsigned char *)wcol->item);
				glRecti(rect->xmin + selsta_draw,
				        rect->ymin + 2,
				        min_ii(rect->xmin + selwidth_draw, rect->xmax - 2),
				        rect->ymax - 2);
			}
		}

		/* text cursor */
		if (but->pos >= but->ofs) {
			int t;
			if (drawstr[0] != 0) {
				t = BLF_width(fstyle->uifont_id, drawstr + but->ofs, but->pos - but->ofs);
			}
			else {
				t = 0;
			}

			glColor3f(0.20, 0.6, 0.9);
			glRecti(rect->xmin + t, rect->ymin + 2, rect->xmin + t + 2, rect->ymax - 2);
		}
	}
	
	if (fstyle->kerning == 1)
		BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);

#if 0
	ui_rasterpos_safe(x, y, but->aspect);
	transopts = ui_translate_buttons();
#endif

	/* cut string in 2 parts - only for menu entries */
	if ((but->block->flag & UI_BLOCK_LOOP) &&
	    (but->editstr == NULL))
	{
		if (but->flag & UI_BUT_HAS_SEP_CHAR) {
			drawstr_right = strrchr(drawstr, UI_SEP_CHAR);
			if (drawstr_right) {
				drawstr_left_len = (drawstr_right - drawstr);
				drawstr_right++;
			}
		}
	}
	
#ifdef USE_NUMBUTS_LR_ALIGN
	if (!drawstr_right && ELEM(but->type, NUM, NUMSLI) &&
	    /* if we're editing or multi-drag (fake editing), then use left alignment */
	    (but->editstr == NULL) && (drawstr == but->drawstr))
	{
		drawstr_right = strchr(drawstr + but->ofs, ':');
		if (drawstr_right) {
			drawstr_right++;
			drawstr_left_len = (drawstr_right - drawstr);

			while (*drawstr_right == ' ') {
				drawstr_right++;
			}
		}
		else {
			/* no prefix, even so use only cpoin */
			drawstr_right = drawstr + but->ofs;
			use_right_only = true;
		}
	}
#endif

	glColor4ubv((unsigned char *)wcol->text);

	if (!use_right_only) {
		/* for underline drawing */
		float font_xofs, font_yofs;

		uiStyleFontDrawExt(fstyle, rect, drawstr + but->ofs,
		                   drawstr_left_len - but->ofs, &font_xofs, &font_yofs);

		if (but->menu_key != '\0') {
			char fixedbuf[128];
			const char *str;

			BLI_strncpy(fixedbuf, drawstr + but->ofs, min_ii(sizeof(fixedbuf), drawstr_left_len));

			str = strchr(fixedbuf, but->menu_key - 32); /* upper case */
			if (str == NULL)
				str = strchr(fixedbuf, but->menu_key);

			if (str) {
				int ul_index = -1;
				float ul_advance;

				ul_index = (int)(str - fixedbuf);

				if (fstyle->kerning == 1) {
					BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
				}

				fixedbuf[ul_index] = '\0';
				ul_advance = BLF_width(fstyle->uifont_id, fixedbuf, ul_index);

				BLF_position(fstyle->uifont_id, rect->xmin + font_xofs + ul_advance, rect->ymin + font_yofs, 0.0f);
				BLF_draw(fstyle->uifont_id, "_", 2);

				if (fstyle->kerning == 1) {
					BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
				}
			}
		}
	}

	/* part text right aligned */
	if (drawstr_right) {
		fstyle->align = UI_STYLE_TEXT_RIGHT;
		rect->xmax -= UI_TEXT_CLIP_MARGIN;
		uiStyleFontDraw(fstyle, rect, drawstr_right);
	}
}

/* draws text and icons for buttons */
static void widget_draw_text_icon(uiFontStyle *fstyle, uiWidgetColors *wcol, uiBut *but, rcti *rect)
{
	const bool show_menu_icon = ui_but_draw_menu_icon(but);
	float alpha = (float)wcol->text[3] / 255.0f;
	char password_str[UI_MAX_DRAW_STR];

	ui_button_text_password_hide(password_str, but, false);

	/* check for button text label */
	if (but->type == MENU && (but->flag & UI_BUT_NODE_LINK)) {
		rcti temp = *rect;
		temp.xmin = rect->xmax - BLI_rcti_size_y(rect) - 1;
		widget_draw_icon(but, ICON_LAYER_USED, alpha, &temp, false);
	}

	/* If there's an icon too (made with uiDefIconTextBut) then draw the icon
	 * and offset the text label to accommodate it */

	if (but->flag & UI_HAS_ICON || show_menu_icon) {
		const BIFIconID icon = (but->flag & UI_HAS_ICON) ? but->icon + but->iconadd : ICON_NONE;
		const float icon_size = ICON_SIZE_FROM_BUTRECT(rect);

		/* menu item - add some more padding so menus don't feel cramped. it must
		 * be part of the button so that this area is still clickable */
		if (ui_block_is_menu(but->block))
			rect->xmin += 0.3f * U.widget_unit;

		widget_draw_icon(but, icon, alpha, rect, show_menu_icon);

		rect->xmin += icon_size;
		/* without this menu keybindings will overlap the arrow icon [#38083] */
		if (show_menu_icon) {
			rect->xmax -= icon_size / 2.0f;
		}
	}

	if (but->editstr || (but->drawflag & UI_BUT_TEXT_LEFT)) {
		rect->xmin += (UI_TEXT_MARGIN_X * U.widget_unit) / but->block->aspect;
	}
	else if ((but->drawflag & UI_BUT_TEXT_RIGHT)) {
		rect->xmax -= (UI_TEXT_MARGIN_X * U.widget_unit) / but->block->aspect;
	}

	/* unlink icon for this button type */
	if ((but->type == SEARCH_MENU_UNLINK) && ui_is_but_search_unlink_visible(but)) {
		rcti temp = *rect;

		temp.xmin = temp.xmax - (BLI_rcti_size_y(rect) * 1.08f);
		widget_draw_icon(but, ICON_X, alpha, &temp, false);
		rect->xmax -= ICON_SIZE_FROM_BUTRECT(rect);
	}

	/* clip but->drawstr to fit in available space */
	if (but->editstr && but->pos >= 0) {
		ui_text_clip_cursor(fstyle, but, rect);
	}
	else if (but->drawstr[0] == '\0') {
		/* bypass text clipping on icon buttons */
		but->ofs = 0;
		but->strwidth = 0;
	}
	else if (ELEM(but->type, NUM, NUMSLI)) {
		ui_text_clip_right_label(fstyle, but, rect);
	}
	else if ((but->block->flag & UI_BLOCK_LOOP) && (but->type == BUT)) {
		/* Clip middle, but protect in all case right part containing the shortcut, if any. */
		ui_text_clip_middle_protect_right(fstyle, but, rect, "|");
	}
	else {
		ui_text_clip_middle(fstyle, but, rect);
	}

	/* always draw text for textbutton cursor */
	widget_draw_text(fstyle, wcol, but, rect);

	ui_button_text_password_hide(password_str, but, true);
}

#undef UI_TEXT_CLIP_MARGIN


/* *********************** widget types ************************************* */


/* uiWidgetStateColors
 *     char inner_anim[4];
 *     char inner_anim_sel[4];
 *     char inner_key[4];
 *     char inner_key_sel[4];
 *     char inner_driven[4];
 *     char inner_driven_sel[4];
 *     float blend;
 */

static struct uiWidgetStateColors wcol_state_colors = {
	{115, 190, 76, 255},
	{90, 166, 51, 255},
	{240, 235, 100, 255},
	{215, 211, 75, 255},
	{180, 0, 255, 255},
	{153, 0, 230, 255},
	0.5f, 0.0f
};

/* uiWidgetColors
 *     char outline[3];
 *     char inner[4];
 *     char inner_sel[4];
 *     char item[3];
 *     char text[3];
 *     char text_sel[3];
 *     
 *     short shaded;
 *     float shadetop, shadedown;
 */

static struct uiWidgetColors wcol_num = {
	{25, 25, 25, 255},
	{180, 180, 180, 255},
	{153, 153, 153, 255},
	{90, 90, 90, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	-20, 0
};

static struct uiWidgetColors wcol_numslider = {
	{25, 25, 25, 255},
	{180, 180, 180, 255},
	{153, 153, 153, 255},
	{128, 128, 128, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	-20, 0
};

static struct uiWidgetColors wcol_text = {
	{25, 25, 25, 255},
	{153, 153, 153, 255},
	{153, 153, 153, 255},
	{90, 90, 90, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	0, 25
};

static struct uiWidgetColors wcol_option = {
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
static struct uiWidgetColors wcol_menu = {
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
static struct uiWidgetColors wcol_pulldown = {
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
static struct uiWidgetColors wcol_menu_item = {
	{0, 0, 0, 255},
	{0, 0, 0, 0},
	{86, 128, 194, 255},
	{172, 172, 172, 128},
	
	{255, 255, 255, 255},
	{0, 0, 0, 255},
	
	1,
	38, 0
};

/* backdrop menu + title text color */
static struct uiWidgetColors wcol_menu_back = {
	{0, 0, 0, 255},
	{25, 25, 25, 230},
	{45, 45, 45, 230},
	{100, 100, 100, 255},
	
	{160, 160, 160, 255},
	{255, 255, 255, 255},
	
	0,
	25, -20
};

/* pie menus */
static struct uiWidgetColors wcol_pie_menu = {
	{10, 10, 10, 200},
	{25, 25, 25, 230},
	{140, 140, 140, 255},
	{45, 45, 45, 230},

	{160, 160, 160, 255},
	{255, 255, 255, 255},

	1,
	10, -10
};


/* tooltip color */
static struct uiWidgetColors wcol_tooltip = {
	{0, 0, 0, 255},
	{25, 25, 25, 230},
	{45, 45, 45, 230},
	{100, 100, 100, 255},

	{160, 160, 160, 255},
	{255, 255, 255, 255},

	0,
	25, -20
};

static struct uiWidgetColors wcol_radio = {
	{0, 0, 0, 255},
	{70, 70, 70, 255},
	{86, 128, 194, 255},
	{255, 255, 255, 255},
	
	{255, 255, 255, 255},
	{0, 0, 0, 255},
	
	1,
	15, -15
};

static struct uiWidgetColors wcol_regular = {
	{25, 25, 25, 255},
	{153, 153, 153, 255},
	{100, 100, 100, 255},
	{25, 25, 25, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	0,
	0, 0
};

static struct uiWidgetColors wcol_tool = {
	{25, 25, 25, 255},
	{153, 153, 153, 255},
	{100, 100, 100, 255},
	{25, 25, 25, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	15, -15
};

static struct uiWidgetColors wcol_box = {
	{25, 25, 25, 255},
	{128, 128, 128, 255},
	{100, 100, 100, 255},
	{25, 25, 25, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	0,
	0, 0
};

static struct uiWidgetColors wcol_toggle = {
	{25, 25, 25, 255},
	{153, 153, 153, 255},
	{100, 100, 100, 255},
	{25, 25, 25, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	0,
	0, 0
};

static struct uiWidgetColors wcol_scroll = {
	{50, 50, 50, 180},
	{80, 80, 80, 180},
	{100, 100, 100, 180},
	{128, 128, 128, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	1,
	5, -5
};

static struct uiWidgetColors wcol_progress = {
	{0, 0, 0, 255},
	{190, 190, 190, 255},
	{100, 100, 100, 180},
	{68, 68, 68, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	0,
	0, 0
};

static struct uiWidgetColors wcol_list_item = {
	{0, 0, 0, 255},
	{0, 0, 0, 0},
	{86, 128, 194, 255},
	{0, 0, 0, 255},
	
	{0, 0, 0, 255},
	{0, 0, 0, 255},
	
	0,
	0, 0
};

/* free wcol struct to play with */
static struct uiWidgetColors wcol_tmp = {
	{0, 0, 0, 255},
	{128, 128, 128, 255},
	{100, 100, 100, 255},
	{25, 25, 25, 255},
	
	{0, 0, 0, 255},
	{255, 255, 255, 255},
	
	0,
	0, 0
};


/* called for theme init (new theme) and versions */
void ui_widget_color_init(ThemeUI *tui)
{
	tui->wcol_regular = wcol_regular;
	tui->wcol_tool = wcol_tool;
	tui->wcol_text = wcol_text;
	tui->wcol_radio = wcol_radio;
	tui->wcol_option = wcol_option;
	tui->wcol_toggle = wcol_toggle;
	tui->wcol_num = wcol_num;
	tui->wcol_numslider = wcol_numslider;
	tui->wcol_menu = wcol_menu;
	tui->wcol_pulldown = wcol_pulldown;
	tui->wcol_menu_back = wcol_menu_back;
	tui->wcol_pie_menu = wcol_pie_menu;
	tui->wcol_tooltip = wcol_tooltip;
	tui->wcol_menu_item = wcol_menu_item;
	tui->wcol_box = wcol_box;
	tui->wcol_scroll = wcol_scroll;
	tui->wcol_list_item = wcol_list_item;
	tui->wcol_progress = wcol_progress;

	tui->wcol_state = wcol_state_colors;
}

/* ************ button callbacks, state ***************** */

static void widget_state_blend(char cp[3], const char cpstate[3], const float fac)
{
	if (fac != 0.0f) {
		cp[0] = (int)((1.0f - fac) * cp[0] + fac * cpstate[0]);
		cp[1] = (int)((1.0f - fac) * cp[1] + fac * cpstate[1]);
		cp[2] = (int)((1.0f - fac) * cp[2] + fac * cpstate[2]);
	}
}

/* copy colors from theme, and set changes in it based on state */
static void widget_state(uiWidgetType *wt, int state)
{
	uiWidgetStateColors *wcol_state = wt->wcol_state;

	if ((state & UI_BUT_LIST_ITEM) && !(state & UI_TEXTINPUT)) {
		/* Override default widget's colors. */
		bTheme *btheme = UI_GetTheme();
		wt->wcol_theme = &btheme->tui.wcol_list_item;
	}

	wt->wcol = *(wt->wcol_theme);

	if (state & UI_SELECT) {
		copy_v4_v4_char(wt->wcol.inner, wt->wcol.inner_sel);

		if (state & UI_BUT_ANIMATED_KEY)
			widget_state_blend(wt->wcol.inner, wcol_state->inner_key_sel, wcol_state->blend);
		else if (state & UI_BUT_ANIMATED)
			widget_state_blend(wt->wcol.inner, wcol_state->inner_anim_sel, wcol_state->blend);
		else if (state & UI_BUT_DRIVEN)
			widget_state_blend(wt->wcol.inner, wcol_state->inner_driven_sel, wcol_state->blend);

		copy_v3_v3_char(wt->wcol.text, wt->wcol.text_sel);
		
		if (state & UI_SELECT)
			SWAP(short, wt->wcol.shadetop, wt->wcol.shadedown);
	}
	else {
		if (state & UI_BUT_ANIMATED_KEY)
			widget_state_blend(wt->wcol.inner, wcol_state->inner_key, wcol_state->blend);
		else if (state & UI_BUT_ANIMATED)
			widget_state_blend(wt->wcol.inner, wcol_state->inner_anim, wcol_state->blend);
		else if (state & UI_BUT_DRIVEN)
			widget_state_blend(wt->wcol.inner, wcol_state->inner_driven, wcol_state->blend);

		if (state & UI_ACTIVE) { /* mouse over? */
			wt->wcol.inner[0] = wt->wcol.inner[0] >= 240 ? 255 : wt->wcol.inner[0] + 15;
			wt->wcol.inner[1] = wt->wcol.inner[1] >= 240 ? 255 : wt->wcol.inner[1] + 15;
			wt->wcol.inner[2] = wt->wcol.inner[2] >= 240 ? 255 : wt->wcol.inner[2] + 15;
		}
	}

	if (state & UI_BUT_REDALERT) {
		char red[4] = {255, 0, 0};
		widget_state_blend(wt->wcol.inner, red, 0.4f);
	}

	if (state & UI_BUT_DRAG_MULTI) {
		/* the button isn't SELECT but we're editing this so draw with sel color */
		copy_v4_v4_char(wt->wcol.inner, wt->wcol.inner_sel);
		SWAP(short, wt->wcol.shadetop, wt->wcol.shadedown);
		widget_state_blend(wt->wcol.text, wt->wcol.text_sel, 0.85f);
	}

	if (state & UI_BUT_NODE_ACTIVE) {
		char blue[4] = {86, 128, 194};
		widget_state_blend(wt->wcol.inner, blue, 0.3f);
	}
}

/* sliders use special hack which sets 'item' as inner when drawing filling */
static void widget_state_numslider(uiWidgetType *wt, int state)
{
	uiWidgetStateColors *wcol_state = wt->wcol_state;
	float blend = wcol_state->blend - 0.2f; /* XXX special tweak to make sure that bar will still be visible */

	/* call this for option button */
	widget_state(wt, state);
	
	/* now, set the inner-part so that it reflects state settings too */
	/* TODO: maybe we should have separate settings for the blending colors used for this case? */
	if (state & UI_SELECT) {
		
		if (state & UI_BUT_ANIMATED_KEY)
			widget_state_blend(wt->wcol.item, wcol_state->inner_key_sel, blend);
		else if (state & UI_BUT_ANIMATED)
			widget_state_blend(wt->wcol.item, wcol_state->inner_anim_sel, blend);
		else if (state & UI_BUT_DRIVEN)
			widget_state_blend(wt->wcol.item, wcol_state->inner_driven_sel, blend);
		
		if (state & UI_SELECT)
			SWAP(short, wt->wcol.shadetop, wt->wcol.shadedown);
	}
	else {
		if (state & UI_BUT_ANIMATED_KEY)
			widget_state_blend(wt->wcol.item, wcol_state->inner_key, blend);
		else if (state & UI_BUT_ANIMATED)
			widget_state_blend(wt->wcol.item, wcol_state->inner_anim, blend);
		else if (state & UI_BUT_DRIVEN)
			widget_state_blend(wt->wcol.item, wcol_state->inner_driven, blend);
	}
}

/* labels use theme colors for text */
static void widget_state_option_menu(uiWidgetType *wt, int state)
{
	bTheme *btheme = UI_GetTheme(); /* XXX */
	
	/* call this for option button */
	widget_state(wt, state);
	
	/* if not selected we get theme from menu back */
	if (state & UI_SELECT)
		copy_v3_v3_char(wt->wcol.text, btheme->tui.wcol_menu_back.text_sel);
	else
		copy_v3_v3_char(wt->wcol.text, btheme->tui.wcol_menu_back.text);
}


static void widget_state_nothing(uiWidgetType *wt, int UNUSED(state))
{
	wt->wcol = *(wt->wcol_theme);
}	

/* special case, button that calls pulldown */
static void widget_state_pulldown(uiWidgetType *wt, int state)
{
	wt->wcol = *(wt->wcol_theme);
	
	copy_v4_v4_char(wt->wcol.inner, wt->wcol.inner_sel);
	copy_v3_v3_char(wt->wcol.outline, wt->wcol.inner);

	if (state & UI_ACTIVE)
		copy_v3_v3_char(wt->wcol.text, wt->wcol.text_sel);
}

/* special case, pie menu items */
static void widget_state_pie_menu_item(uiWidgetType *wt, int state)
{
	wt->wcol = *(wt->wcol_theme);

	/* active and disabled (not so common) */
	if ((state & UI_BUT_DISABLED) && (state & UI_ACTIVE)) {
		widget_state_blend(wt->wcol.text, wt->wcol.text_sel, 0.5f);
		/* draw the backdrop at low alpha, helps navigating with keys
		 * when disabled items are active */
		copy_v4_v4_char(wt->wcol.inner, wt->wcol.item);
		wt->wcol.inner[3] = 64;
	}
	/* regular disabled */
	else if (state & (UI_BUT_DISABLED | UI_BUT_INACTIVE)) {
		widget_state_blend(wt->wcol.text, wt->wcol.inner, 0.5f);
	}
	/* regular active */
	else if (state & UI_SELECT) {
		copy_v4_v4_char(wt->wcol.outline, wt->wcol.inner_sel);
		copy_v3_v3_char(wt->wcol.text, wt->wcol.text_sel);
	}
	else if (state & UI_ACTIVE) {
		copy_v4_v4_char(wt->wcol.inner, wt->wcol.item);
		copy_v3_v3_char(wt->wcol.text, wt->wcol.text_sel);
	}
}

/* special case, menu items */
static void widget_state_menu_item(uiWidgetType *wt, int state)
{
	wt->wcol = *(wt->wcol_theme);
	
	/* active and disabled (not so common) */
	if ((state & UI_BUT_DISABLED) && (state & UI_ACTIVE)) {
		widget_state_blend(wt->wcol.text, wt->wcol.text_sel, 0.5f);
		/* draw the backdrop at low alpha, helps navigating with keys
		 * when disabled items are active */
		copy_v4_v4_char(wt->wcol.inner, wt->wcol.inner_sel);
		wt->wcol.inner[3] = 64;
	}
	/* regular disabled */
	else if (state & (UI_BUT_DISABLED | UI_BUT_INACTIVE)) {
		widget_state_blend(wt->wcol.text, wt->wcol.inner, 0.5f);
	}
	/* regular active */
	else if (state & UI_ACTIVE) {
		copy_v4_v4_char(wt->wcol.inner, wt->wcol.inner_sel);
		copy_v3_v3_char(wt->wcol.text, wt->wcol.text_sel);
	}
}


/* ************ menu backdrop ************************* */

/* outside of rect, rad to left/bottom/right */
static void widget_softshadow(const rcti *rect, int roundboxalign, const float radin)
{
	bTheme *btheme = UI_GetTheme();
	uiWidgetBase wtb;
	rcti rect1 = *rect;
	float alphastep;
	int step, totvert;
	float quad_strip[WIDGET_SIZE_MAX * 2 + 2][2];
	const float radout = UI_ThemeMenuShadowWidth();
	
	/* disabled shadow */
	if (radout == 0.0f)
		return;
	
	/* prevent tooltips to not show round shadow */
	if (radout > 0.2f * BLI_rcti_size_y(&rect1))
		rect1.ymax -= 0.2f * BLI_rcti_size_y(&rect1);
	else
		rect1.ymax -= radout;
	
	/* inner part */
	totvert = round_box_shadow_edges(wtb.inner_v, &rect1, radin, roundboxalign & (UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT), 0.0f);

	/* we draw a number of increasing size alpha quad strips */
	alphastep = 3.0f * btheme->tui.menu_shadow_fac / radout;
	
	glEnableClientState(GL_VERTEX_ARRAY);

	for (step = 1; step <= (int)radout; step++) {
		float expfac = sqrtf(step / radout);
		
		round_box_shadow_edges(wtb.outer_v, &rect1, radin, UI_CNR_ALL, (float)step);
		
		glColor4f(0.0f, 0.0f, 0.0f, alphastep * (1.0f - expfac));

		widget_verts_to_quad_strip(&wtb, totvert, quad_strip);

		glVertexPointer(2, GL_FLOAT, 0, quad_strip);
		glDrawArrays(GL_QUAD_STRIP, 0, totvert * 2); /* add + 2 for getting a complete soft rect. Now it skips top edge to allow transparent menus */
	}

	glDisableClientState(GL_VERTEX_ARRAY);
}

static void widget_menu_back(uiWidgetColors *wcol, rcti *rect, int flag, int direction)
{
	uiWidgetBase wtb;
	int roundboxalign = UI_CNR_ALL;
	
	widget_init(&wtb);
	
	/* menu is 2nd level or deeper */
	if (flag & UI_BLOCK_POPUP) {
		//rect->ymin -= 4.0;
		//rect->ymax += 4.0;
	}
	else if (direction == UI_DOWN) {
		roundboxalign = (UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT);
		rect->ymin -= 0.1f * U.widget_unit;
	}
	else if (direction == UI_TOP) {
		roundboxalign = UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT;
		rect->ymax += 0.1f * U.widget_unit;
	}
	
	glEnable(GL_BLEND);
	widget_softshadow(rect, roundboxalign, 0.25f * U.widget_unit);
	
	round_box_edges(&wtb, roundboxalign, rect, 0.25f * U.widget_unit);
	wtb.emboss = 0;
	widgetbase_draw(&wtb, wcol);
	
	glDisable(GL_BLEND);
}


static void ui_hsv_cursor(float x, float y)
{
	
	glPushMatrix();
	glTranslatef(x, y, 0.0f);
	
	glColor3f(1.0f, 1.0f, 1.0f);
	glutil_draw_filled_arc(0.0f, M_PI * 2.0, 3.0f * U.pixelsize, 8);
	
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	glColor3f(0.0f, 0.0f, 0.0f);
	glutil_draw_lined_arc(0.0f, M_PI * 2.0, 3.0f * U.pixelsize, 12);
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
	
	glPopMatrix();
	
}

void ui_hsvcircle_vals_from_pos(float *val_rad, float *val_dist, const rcti *rect,
                                const float mx, const float my)
{
	/* duplication of code... well, simple is better now */
	const float centx = BLI_rcti_cent_x_fl(rect);
	const float centy = BLI_rcti_cent_y_fl(rect);
	const float radius = (float)min_ii(BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)) / 2.0f;
	const float m_delta[2] = {mx - centx, my - centy};
	const float dist_sq = len_squared_v2(m_delta);

	*val_dist = (dist_sq < (radius * radius)) ? sqrtf(dist_sq) / radius : 1.0f;
	*val_rad = atan2f(m_delta[0], m_delta[1]) / (2.0f * (float)M_PI) + 0.5f;
}

/* cursor in hsv circle, in float units -1 to 1, to map on radius */
void ui_hsvcircle_pos_from_vals(uiBut *but, const rcti *rect, float *hsv, float *xpos, float *ypos)
{
	/* duplication of code... well, simple is better now */
	const float centx = BLI_rcti_cent_x_fl(rect);
	const float centy = BLI_rcti_cent_y_fl(rect);
	float radius = (float)min_ii(BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)) / 2.0f;
	float ang, radius_t;
	
	ang = 2.0f * (float)M_PI * hsv[0] + 0.5f * (float)M_PI;
	
	if ((but->flag & UI_BUT_COLOR_CUBIC) && (U.color_picker_type == USER_CP_CIRCLE_HSV))
		radius_t = (1.0f - powf(1.0f - hsv[1], 3.0f));
	else
		radius_t = hsv[1];
	
	radius = CLAMPIS(radius_t, 0.0f, 1.0f) * radius;
	*xpos = centx + cosf(-ang) * radius;
	*ypos = centy + sinf(-ang) * radius;
}

static void ui_draw_but_HSVCIRCLE(uiBut *but, uiWidgetColors *wcol, const rcti *rect)
{
	const int tot = 64;
	const float radstep = 2.0f * (float)M_PI / (float)tot;
	const float centx = BLI_rcti_cent_x_fl(rect);
	const float centy = BLI_rcti_cent_y_fl(rect);
	float radius = (float)min_ii(BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)) / 2.0f;

	/* gouraud triangle fan */
	const float *hsv_ptr = ui_block_hsv_get(but->block);
	float xpos, ypos, ang = 0.0f;
	float rgb[3], hsvo[3], hsv[3], col[3], colcent[3];
	int a;
	bool color_profile = ui_color_picker_use_display_colorspace(but);
		
	/* color */
	ui_get_but_vectorf(but, rgb);

	/* since we use compat functions on both 'hsv' and 'hsvo', they need to be initialized */
	hsvo[0] = hsv[0] = hsv_ptr[0];
	hsvo[1] = hsv[1] = hsv_ptr[1];
	hsvo[2] = hsv[2] = hsv_ptr[2];

	if (color_profile)
		ui_block_to_display_space_v3(but->block, rgb);

	ui_rgb_to_color_picker_compat_v(rgb, hsv);
	copy_v3_v3(hsvo, hsv);

	CLAMP(hsv[2], 0.0f, 1.0f); /* for display only */

	/* exception: if 'lock' is set
	 * lock the value of the color wheel to 1.
	 * Useful for color correction tools where you're only interested in hue. */
	if (but->flag & UI_BUT_COLOR_LOCK) {
		if (U.color_picker_type == USER_CP_CIRCLE_HSV)
			hsv[2] = 1.f;
		else
			hsv[2] = 0.5f;
	}
	
	ui_color_picker_to_rgb(0.f, 0.f, hsv[2], colcent, colcent + 1, colcent + 2);

	glShadeModel(GL_SMOOTH);

	glBegin(GL_TRIANGLE_FAN);
	glColor3fv(colcent);
	glVertex2f(centx, centy);
	
	for (a = 0; a <= tot; a++, ang += radstep) {
		float si = sinf(ang);
		float co = cosf(ang);
		
		ui_hsvcircle_vals_from_pos(hsv, hsv + 1, rect, centx + co * radius, centy + si * radius);

		ui_color_picker_to_rgb_v(hsv, col);

		glColor3fv(col);
		glVertex2f(centx + co * radius, centy + si * radius);
	}
	glEnd();
	
	glShadeModel(GL_FLAT);
	
	/* fully rounded outline */
	glPushMatrix();
	glTranslatef(centx, centy, 0.0f);
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	glColor3ubv((unsigned char *)wcol->outline);
	glutil_draw_lined_arc(0.0f, M_PI * 2.0, radius, tot + 1);
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
	glPopMatrix();

	/* cursor */
	ui_hsvcircle_pos_from_vals(but, rect, hsvo, &xpos, &ypos);

	ui_hsv_cursor(xpos, ypos);
}

/* ************ custom buttons, old stuff ************** */

/* draws in resolution of 48x4 colors */
void ui_draw_gradient(const rcti *rect, const float hsv[3], const int type, const float alpha)
{
	/* allows for 4 steps (red->yellow) */
	const float color_step = (1.0 / 48.0);
	int a;
	float h = hsv[0], s = hsv[1], v = hsv[2];
	float dx, dy, sx1, sx2, sy;
	float col0[4][3];   /* left half, rect bottom to top */
	float col1[4][3];   /* right half, rect bottom to top */

	/* draw series of gouraud rects */
	glShadeModel(GL_SMOOTH);
	
	switch (type) {
		case UI_GRAD_SV:
			hsv_to_rgb(h, 0.0, 0.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
			hsv_to_rgb(h, 0.333, 0.0, &col1[1][0], &col1[1][1], &col1[1][2]);
			hsv_to_rgb(h, 0.666, 0.0, &col1[2][0], &col1[2][1], &col1[2][2]);
			hsv_to_rgb(h, 1.0, 0.0,   &col1[3][0], &col1[3][1], &col1[3][2]);
			break;
		case UI_GRAD_HV:
			hsv_to_rgb(0.0, s, 0.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
			hsv_to_rgb(0.0, s, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
			hsv_to_rgb(0.0, s, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
			hsv_to_rgb(0.0, s, 1.0,   &col1[3][0], &col1[3][1], &col1[3][2]);
			break;
		case UI_GRAD_HS:
			hsv_to_rgb(0.0, 0.0, v,   &col1[0][0], &col1[0][1], &col1[0][2]);
			hsv_to_rgb(0.0, 0.333, v, &col1[1][0], &col1[1][1], &col1[1][2]);
			hsv_to_rgb(0.0, 0.666, v, &col1[2][0], &col1[2][1], &col1[2][2]);
			hsv_to_rgb(0.0, 1.0, v,   &col1[3][0], &col1[3][1], &col1[3][2]);
			break;
		case UI_GRAD_H:
			hsv_to_rgb(0.0, 1.0, 1.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
			copy_v3_v3(col1[1], col1[0]);
			copy_v3_v3(col1[2], col1[0]);
			copy_v3_v3(col1[3], col1[0]);
			break;
		case UI_GRAD_S:
			hsv_to_rgb(1.0, 0.0, 1.0,   &col1[1][0], &col1[1][1], &col1[1][2]);
			copy_v3_v3(col1[0], col1[1]);
			copy_v3_v3(col1[2], col1[1]);
			copy_v3_v3(col1[3], col1[1]);
			break;
		case UI_GRAD_V:
			hsv_to_rgb(1.0, 1.0, 0.0,   &col1[2][0], &col1[2][1], &col1[2][2]);
			copy_v3_v3(col1[0], col1[2]);
			copy_v3_v3(col1[1], col1[2]);
			copy_v3_v3(col1[3], col1[2]);
			break;
		default:
			assert(!"invalid 'type' argument");
			hsv_to_rgb(1.0, 1.0, 1.0,   &col1[2][0], &col1[2][1], &col1[2][2]);
			copy_v3_v3(col1[0], col1[2]);
			copy_v3_v3(col1[1], col1[2]);
			copy_v3_v3(col1[3], col1[2]);
			break;
	}
	
	/* old below */
	
	for (dx = 0.0f; dx < 0.999f; dx += color_step) { /* 0.999 = prevent float inaccuracy for steps */
		const float dx_next = dx + color_step;

		/* previous color */
		copy_v3_v3(col0[0], col1[0]);
		copy_v3_v3(col0[1], col1[1]);
		copy_v3_v3(col0[2], col1[2]);
		copy_v3_v3(col0[3], col1[3]);
		
		/* new color */
		switch (type) {
			case UI_GRAD_SV:
				hsv_to_rgb(h, 0.0, dx,   &col1[0][0], &col1[0][1], &col1[0][2]);
				hsv_to_rgb(h, 0.333, dx, &col1[1][0], &col1[1][1], &col1[1][2]);
				hsv_to_rgb(h, 0.666, dx, &col1[2][0], &col1[2][1], &col1[2][2]);
				hsv_to_rgb(h, 1.0, dx,   &col1[3][0], &col1[3][1], &col1[3][2]);
				break;
			case UI_GRAD_HV:
				hsv_to_rgb(dx_next, s, 0.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
				hsv_to_rgb(dx_next, s, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
				hsv_to_rgb(dx_next, s, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
				hsv_to_rgb(dx_next, s, 1.0,   &col1[3][0], &col1[3][1], &col1[3][2]);
				break;
			case UI_GRAD_HS:
				hsv_to_rgb(dx_next, 0.0, v,   &col1[0][0], &col1[0][1], &col1[0][2]);
				hsv_to_rgb(dx_next, 0.333, v, &col1[1][0], &col1[1][1], &col1[1][2]);
				hsv_to_rgb(dx_next, 0.666, v, &col1[2][0], &col1[2][1], &col1[2][2]);
				hsv_to_rgb(dx_next, 1.0, v,   &col1[3][0], &col1[3][1], &col1[3][2]);
				break;
			case UI_GRAD_H:
			{
				/* annoying but without this the color shifts - could be solved some other way
				 * - campbell */
				hsv_to_rgb(dx_next, 1.0, 1.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
				copy_v3_v3(col1[1], col1[0]);
				copy_v3_v3(col1[2], col1[0]);
				copy_v3_v3(col1[3], col1[0]);
				break;
			}
			case UI_GRAD_S:
				hsv_to_rgb(h, dx, 1.0,   &col1[1][0], &col1[1][1], &col1[1][2]);
				copy_v3_v3(col1[0], col1[1]);
				copy_v3_v3(col1[2], col1[1]);
				copy_v3_v3(col1[3], col1[1]);
				break;
			case UI_GRAD_V:
				hsv_to_rgb(h, 1.0, dx,   &col1[2][0], &col1[2][1], &col1[2][2]);
				copy_v3_v3(col1[0], col1[2]);
				copy_v3_v3(col1[1], col1[2]);
				copy_v3_v3(col1[3], col1[2]);
				break;
		}
		
		/* rect */
		sx1 = rect->xmin + dx      * BLI_rcti_size_x(rect);
		sx2 = rect->xmin + dx_next * BLI_rcti_size_x(rect);
		sy = rect->ymin;
		dy = (float)BLI_rcti_size_y(rect) / 3.0f;
		
		glBegin(GL_QUADS);
		for (a = 0; a < 3; a++, sy += dy) {
			glColor4f(col0[a][0], col0[a][1], col0[a][2], alpha);
			glVertex2f(sx1, sy);
			
			glColor4f(col1[a][0], col1[a][1], col1[a][2], alpha);
			glVertex2f(sx2, sy);

			glColor4f(col1[a + 1][0], col1[a + 1][1], col1[a + 1][2], alpha);
			glVertex2f(sx2, sy + dy);
			
			glColor4f(col0[a + 1][0], col0[a + 1][1], col0[a + 1][2], alpha);
			glVertex2f(sx1, sy + dy);
		}
		glEnd();
	}
	
	glShadeModel(GL_FLAT);
	
}

bool ui_color_picker_use_display_colorspace(uiBut *but)
{
	bool color_profile = but->block->color_profile;

	if (but->rnaprop) {
		if (RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA)
			color_profile = false;
	}

	return color_profile;
}

void ui_hsvcube_pos_from_vals(uiBut *but, const rcti *rect, float *hsv, float *xp, float *yp)
{
	float x = 0.0f, y = 0.0f;

	switch ((int)but->a1) {
		case UI_GRAD_SV:
			x = hsv[2]; y = hsv[1]; break;
		case UI_GRAD_HV:
			x = hsv[0]; y = hsv[2]; break;
		case UI_GRAD_HS:
			x = hsv[0]; y = hsv[1]; break;
		case UI_GRAD_H:
			x = hsv[0]; y = 0.5; break;
		case UI_GRAD_S:
			x = hsv[1]; y = 0.5; break;
		case UI_GRAD_V:
			x = hsv[2]; y = 0.5; break;
		case UI_GRAD_L_ALT:
			x = 0.5f;
			/* exception only for value strip - use the range set in but->min/max */
			y = hsv[2];
			break;
		case UI_GRAD_V_ALT:
			x = 0.5f;
			/* exception only for value strip - use the range set in but->min/max */
			y = (hsv[2] - but->softmin) / (but->softmax - but->softmin);
			break;
	}
	
	/* cursor */
	*xp = rect->xmin + x * BLI_rcti_size_x(rect);
	*yp = rect->ymin + y * BLI_rcti_size_y(rect);

}

static void ui_draw_but_HSVCUBE(uiBut *but, const rcti *rect)
{
	float rgb[3];
	float x = 0.0f, y = 0.0f;
	const float *hsv = ui_block_hsv_get(but->block);
	float hsv_n[3];
	bool use_display_colorspace = ui_color_picker_use_display_colorspace(but);
	
	copy_v3_v3(hsv_n, hsv);
	
	ui_get_but_vectorf(but, rgb);
	
	if (use_display_colorspace)
		ui_block_to_display_space_v3(but->block, rgb);
	
	rgb_to_hsv_compat_v(rgb, hsv_n);
	
	ui_draw_gradient(rect, hsv_n, but->a1, 1.0f);

	ui_hsvcube_pos_from_vals(but, rect, hsv_n, &x, &y);
	CLAMP(x, rect->xmin + 3.0f, rect->xmax - 3.0f);
	CLAMP(y, rect->ymin + 3.0f, rect->ymax - 3.0f);
	
	ui_hsv_cursor(x, y);
	
	/* outline */
	glColor3ub(0,  0,  0);
	fdrawbox((rect->xmin), (rect->ymin), (rect->xmax), (rect->ymax));
}

/* vertical 'value' slider, using new widget code */
static void ui_draw_but_HSV_v(uiBut *but, const rcti *rect)
{
	uiWidgetBase wtb;
	const float rad = 0.5f * BLI_rcti_size_x(rect);
	float x, y;
	float rgb[3], hsv[3], v;
	bool color_profile = but->block->color_profile;
	
	if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA)
		color_profile = false;

	ui_get_but_vectorf(but, rgb);

	if (color_profile)
		ui_block_to_display_space_v3(but->block, rgb);

	if (but->a1 == UI_GRAD_L_ALT)
		rgb_to_hsl_v(rgb, hsv);
	else
		rgb_to_hsv_v(rgb, hsv);
	v = hsv[2];
	
	/* map v from property range to [0,1] */
	if (but->a1 == UI_GRAD_V_ALT) {
		float range = but->softmax - but->softmin;
		v = (v - but->softmin) / range;
	}

	widget_init(&wtb);
	
	/* fully rounded */
	round_box_edges(&wtb, UI_CNR_ALL, rect, rad);
	
	/* setup temp colors */
	wcol_tmp.outline[0] = wcol_tmp.outline[1] = wcol_tmp.outline[2] = 0;
	wcol_tmp.inner[0] = wcol_tmp.inner[1] = wcol_tmp.inner[2] = 128;
	wcol_tmp.shadetop = 127;
	wcol_tmp.shadedown = -128;
	wcol_tmp.shaded = 1;
	
	widgetbase_draw(&wtb, &wcol_tmp);

	/* cursor */
	x = rect->xmin + 0.5f * BLI_rcti_size_x(rect);
	y = rect->ymin + v    * BLI_rcti_size_y(rect);
	CLAMP(y, rect->ymin + 3.0f, rect->ymax - 3.0f);
	
	ui_hsv_cursor(x, y);
	
}


/* ************ separator, for menus etc ***************** */
static void ui_draw_separator(const rcti *rect,  uiWidgetColors *wcol)
{
	int y = rect->ymin + BLI_rcti_size_y(rect) / 2 - 1;
	unsigned char col[4];
	
	col[0] = wcol->text[0];
	col[1] = wcol->text[1];
	col[2] = wcol->text[2];
	col[3] = 30;
	
	glEnable(GL_BLEND);
	glColor4ubv(col);
	sdrawline(rect->xmin, y, rect->xmax, y);
	glDisable(GL_BLEND);
}

/* ************ button callbacks, draw ***************** */
static void widget_numbut_draw(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign, bool emboss)
{
	uiWidgetBase wtb;
	const float rad = 0.5f * BLI_rcti_size_y(rect);
	float textofs = rad * 0.85f;

	if (state & UI_SELECT)
		SWAP(short, wcol->shadetop, wcol->shadedown);
	
	widget_init(&wtb);
	
	if (!emboss) {
		round_box_edges(&wtb, roundboxalign, rect, rad);
	}

	/* decoration */
	if (!(state & UI_TEXTINPUT)) {
		widget_num_tria(&wtb.tria1, rect, 0.6f, 'l');
		widget_num_tria(&wtb.tria2, rect, 0.6f, 'r');
	}

	widgetbase_draw(&wtb, wcol);
	
	if (!(state & UI_TEXTINPUT)) {
		/* text space */
		rect->xmin += textofs;
		rect->xmax -= textofs;
	}
}

static void widget_numbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	widget_numbut_draw(wcol, rect, state, roundboxalign, false);
}

/**
 * Draw number buttons still with triangles when field is not embossed
 */
static void widget_numbut_embossn(uiBut *UNUSED(but), uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	widget_numbut_draw(wcol, rect, state, roundboxalign, true);
}

bool ui_link_bezier_points(const rcti *rect, float coord_array[][2], int resol)
{
	float dist, vec[4][2];

	vec[0][0] = rect->xmin;
	vec[0][1] = rect->ymin;
	vec[3][0] = rect->xmax;
	vec[3][1] = rect->ymax;
	
	dist = 0.5f * fabsf(vec[0][0] - vec[3][0]);
	
	vec[1][0] = vec[0][0] + dist;
	vec[1][1] = vec[0][1];
	
	vec[2][0] = vec[3][0] - dist;
	vec[2][1] = vec[3][1];
	
	BKE_curve_forward_diff_bezier(vec[0][0], vec[1][0], vec[2][0], vec[3][0], &coord_array[0][0], resol, sizeof(float[2]));
	BKE_curve_forward_diff_bezier(vec[0][1], vec[1][1], vec[2][1], vec[3][1], &coord_array[0][1], resol, sizeof(float[2]));

	return 1;
}

#define LINK_RESOL  24
void ui_draw_link_bezier(const rcti *rect)
{
	float coord_array[LINK_RESOL + 1][2];

	if (ui_link_bezier_points(rect, coord_array, LINK_RESOL)) {
		/* we can reuse the dist variable here to increment the GL curve eval amount*/
		// const float dist = 1.0f / (float)LINK_RESOL; // UNUSED

		glEnable(GL_BLEND);
		glEnable(GL_LINE_SMOOTH);

		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(2, GL_FLOAT, 0, coord_array);
		glDrawArrays(GL_LINE_STRIP, 0, LINK_RESOL + 1);
		glDisableClientState(GL_VERTEX_ARRAY);

		glDisable(GL_BLEND);
		glDisable(GL_LINE_SMOOTH);

	}
}

/* function in use for buttons and for view2d sliders */
void uiWidgetScrollDraw(uiWidgetColors *wcol, const rcti *rect, const rcti *slider, int state)
{
	uiWidgetBase wtb;
	int horizontal;
	float rad;
	bool outline = false;

	widget_init(&wtb);

	/* determine horizontal/vertical */
	horizontal = (BLI_rcti_size_x(rect) > BLI_rcti_size_y(rect));

	if (horizontal)
		rad = 0.5f * BLI_rcti_size_y(rect);
	else
		rad = 0.5f * BLI_rcti_size_x(rect);
	
	wtb.shadedir = (horizontal) ? 1 : 0;
	
	/* draw back part, colors swapped and shading inverted */
	if (horizontal)
		SWAP(short, wcol->shadetop, wcol->shadedown);
	
	round_box_edges(&wtb, UI_CNR_ALL, rect, rad);
	widgetbase_draw(&wtb, wcol);
	
	/* slider */
	if ((BLI_rcti_size_x(slider) < 2) || (BLI_rcti_size_y(slider) < 2)) {
		/* pass */
	}
	else {
		SWAP(short, wcol->shadetop, wcol->shadedown);
		
		copy_v4_v4_char(wcol->inner, wcol->item);
		
		if (wcol->shadetop > wcol->shadedown)
			wcol->shadetop += 20;   /* XXX violates themes... */
		else wcol->shadedown += 20;
		
		if (state & UI_SCROLL_PRESSED) {
			wcol->inner[0] = wcol->inner[0] >= 250 ? 255 : wcol->inner[0] + 5;
			wcol->inner[1] = wcol->inner[1] >= 250 ? 255 : wcol->inner[1] + 5;
			wcol->inner[2] = wcol->inner[2] >= 250 ? 255 : wcol->inner[2] + 5;
		}

		/* draw */
		wtb.emboss = 0; /* only emboss once */
		
		/* exception for progress bar */
		if (state & UI_SCROLL_NO_OUTLINE) {
			SWAP(bool, outline, wtb.outline);
		}
		
		round_box_edges(&wtb, UI_CNR_ALL, slider, rad);
		
		if (state & UI_SCROLL_ARROWS) {
			if (wcol->item[0] > 48) wcol->item[0] -= 48;
			if (wcol->item[1] > 48) wcol->item[1] -= 48;
			if (wcol->item[2] > 48) wcol->item[2] -= 48;
			wcol->item[3] = 255;
			
			if (horizontal) {
				widget_scroll_circle(&wtb.tria1, slider, 0.6f, 'l');
				widget_scroll_circle(&wtb.tria2, slider, 0.6f, 'r');
			}
			else {
				widget_scroll_circle(&wtb.tria1, slider, 0.6f, 'b');
				widget_scroll_circle(&wtb.tria2, slider, 0.6f, 't');
			}
		}
		widgetbase_draw(&wtb, wcol);
		
		if (state & UI_SCROLL_NO_OUTLINE) {
			SWAP(bool, outline, wtb.outline);
		}
	}
}

static void widget_scroll(uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int UNUSED(roundboxalign))
{
	rcti rect1;
	double value;
	float fac, size, min;
	int horizontal;

	/* calculate slider part */
	value = ui_get_but_val(but);

	size = (but->softmax + but->a1 - but->softmin);
	size = max_ff(size, 2.0f);
	
	/* position */
	rect1 = *rect;

	/* determine horizontal/vertical */
	horizontal = (BLI_rcti_size_x(rect) > BLI_rcti_size_y(rect));
	
	if (horizontal) {
		fac = BLI_rcti_size_x(rect) / size;
		rect1.xmin = rect1.xmin + ceilf(fac * ((float)value - but->softmin));
		rect1.xmax = rect1.xmin + ceilf(fac * (but->a1 - but->softmin));

		/* ensure minimium size */
		min = BLI_rcti_size_y(rect);

		if (BLI_rcti_size_x(&rect1) < min) {
			rect1.xmax = rect1.xmin + min;

			if (rect1.xmax > rect->xmax) {
				rect1.xmax = rect->xmax;
				rect1.xmin = max_ii(rect1.xmax - min, rect->xmin);
			}
		}
	}
	else {
		fac = BLI_rcti_size_y(rect) / size;
		rect1.ymax = rect1.ymax - ceilf(fac * ((float)value - but->softmin));
		rect1.ymin = rect1.ymax - ceilf(fac * (but->a1 - but->softmin));

		/* ensure minimium size */
		min = BLI_rcti_size_x(rect);

		if (BLI_rcti_size_y(&rect1) < min) {
			rect1.ymax = rect1.ymin + min;

			if (rect1.ymax > rect->ymax) {
				rect1.ymax = rect->ymax;
				rect1.ymin = max_ii(rect1.ymax - min, rect->ymin);
			}
		}
	}

	if (state & UI_SELECT)
		state = UI_SCROLL_PRESSED;
	else
		state = 0;
	uiWidgetScrollDraw(wcol, rect, &rect1, state);
}

static void widget_progressbar(uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	rcti rect_prog = *rect, rect_bar = *rect;
	float value = but->a1;
	float w, min;
	
	/* make the progress bar a proportion of the original height */
	/* hardcoded 4px high for now */
	rect_prog.ymax = rect_prog.ymin + 4 * UI_DPI_FAC;
	rect_bar.ymax = rect_bar.ymin + 4 * UI_DPI_FAC;
	
	w = value * BLI_rcti_size_x(&rect_prog);
	
	/* ensure minimium size */
	min = BLI_rcti_size_y(&rect_prog);
	w = MAX2(w, min);
	
	rect_bar.xmax = rect_bar.xmin + w;
		
	uiWidgetScrollDraw(wcol, &rect_prog, &rect_bar, UI_SCROLL_NO_OUTLINE);
	
	/* raise text a bit */
	rect->ymin += 6 * UI_DPI_FAC;
	rect->xmin -= 6 * UI_DPI_FAC;
}

static void widget_link(uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	
	if (but->flag & UI_SELECT) {
		rcti rectlink;
		
		UI_ThemeColor(TH_TEXT_HI);
		
		rectlink.xmin = BLI_rcti_cent_x(rect);
		rectlink.ymin = BLI_rcti_cent_y(rect);
		rectlink.xmax = but->linkto[0];
		rectlink.ymax = but->linkto[1];
		
		ui_draw_link_bezier(&rectlink);
	}
}

static void widget_numslider(uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb, wtb1;
	rcti rect1;
	double value;
	float offs, toffs, fac;
	char outline[3];

	widget_init(&wtb);
	widget_init(&wtb1);
	
	/* backdrop first */
	
	/* fully rounded */
	offs = 0.5f * BLI_rcti_size_y(rect);
	toffs = offs * 0.75f;
	round_box_edges(&wtb, roundboxalign, rect, offs);

	wtb.outline = 0;
	widgetbase_draw(&wtb, wcol);
	
	/* draw left/right parts only when not in text editing */
	if (!(state & UI_TEXTINPUT)) {
		int roundboxalign_slider;
		
		/* slider part */
		copy_v3_v3_char(outline, wcol->outline);
		copy_v3_v3_char(wcol->outline, wcol->item);
		copy_v3_v3_char(wcol->inner, wcol->item);

		if (!(state & UI_SELECT))
			SWAP(short, wcol->shadetop, wcol->shadedown);
		
		rect1 = *rect;
		
		value = ui_get_but_val(but);
		fac = ((float)value - but->softmin) * (BLI_rcti_size_x(&rect1) - offs) / (but->softmax - but->softmin);
		
		/* left part of slider, always rounded */
		rect1.xmax = rect1.xmin + ceil(offs + U.pixelsize);
		round_box_edges(&wtb1, roundboxalign & ~(UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT), &rect1, offs);
		wtb1.outline = 0;
		widgetbase_draw(&wtb1, wcol);
		
		/* right part of slider, interpolate roundness */
		rect1.xmax = rect1.xmin + fac + offs;
		rect1.xmin +=  floor(offs - U.pixelsize);
		
		if (rect1.xmax + offs > rect->xmax) {
			roundboxalign_slider = roundboxalign & ~(UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT);
			offs *= (rect1.xmax + offs - rect->xmax) / offs;
		}
		else {
			roundboxalign_slider = 0;
			offs = 0.0f;
		}
		round_box_edges(&wtb1, roundboxalign_slider, &rect1, offs);
		
		widgetbase_draw(&wtb1, wcol);
		copy_v3_v3_char(wcol->outline, outline);
		
		if (!(state & UI_SELECT))
			SWAP(short, wcol->shadetop, wcol->shadedown);
	}
	
	/* outline */
	wtb.outline = 1;
	wtb.inner = 0;
	widgetbase_draw(&wtb, wcol);

	/* add space at either side of the button so text aligns with numbuttons (which have arrow icons) */
	if (!(state & UI_TEXTINPUT)) {
		rect->xmax -= toffs;
		rect->xmin += toffs;
	}

}

/* I think 3 is sufficient border to indicate keyed status */
#define SWATCH_KEYED_BORDER 3

static void widget_swatch(uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	float rad, col[4];
	bool color_profile = but->block->color_profile;
	
	col[3] = 1.0f;

	if (but->rnaprop) {
		BLI_assert(but->rnaindex == -1);

		if (RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA)
			color_profile = false;

		if (RNA_property_array_length(&but->rnapoin, but->rnaprop) == 4) {
			col[3] = RNA_property_float_get_index(&but->rnapoin, but->rnaprop, 3);
		}
	}
	
	widget_init(&wtb);
	
	/* half rounded */
	rad = 0.25f * U.widget_unit;
	round_box_edges(&wtb, roundboxalign, rect, rad);
		
	ui_get_but_vectorf(but, col);

	if (state & (UI_BUT_ANIMATED | UI_BUT_ANIMATED_KEY | UI_BUT_DRIVEN | UI_BUT_REDALERT)) {
		/* draw based on state - color for keyed etc */
		widgetbase_draw(&wtb, wcol);

		/* inset to draw swatch color */
		rect->xmin += SWATCH_KEYED_BORDER;
		rect->xmax -= SWATCH_KEYED_BORDER;
		rect->ymin += SWATCH_KEYED_BORDER;
		rect->ymax -= SWATCH_KEYED_BORDER;
		
		round_box_edges(&wtb, roundboxalign, rect, rad);
	}
	
	if (color_profile)
		ui_block_to_display_space_v3(but->block, col);
	
	rgba_float_to_uchar((unsigned char *)wcol->inner, col);

	wcol->shaded = 0;
	wcol->alpha_check = (wcol->inner[3] < 255);

	widgetbase_draw(&wtb, wcol);
	
	if (but->a1 == UI_PALETTE_COLOR && ((Palette *)but->rnapoin.id.data)->active_color == (int)but->a2) {
		float width = rect->xmax - rect->xmin;
		float height = rect->ymax - rect->ymin;
		/* find color luminance and change it slightly */
		float bw = rgb_to_bw(col);
		
		if (bw > 0.5)
			bw -= 0.5;
		else
			bw += 0.5;
		
		glColor4f(bw, bw, bw, 1.0);
		glBegin(GL_TRIANGLES);
		glVertex2f(rect->xmin + 0.1f * width, rect->ymin + 0.9f * height);
		glVertex2f(rect->xmin + 0.1f * width, rect->ymin + 0.5f * height);
		glVertex2f(rect->xmin + 0.5f * width, rect->ymin + 0.9f * height);
		glEnd();
	}
}

static void widget_normal(uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	ui_draw_but_NORMAL(but, wcol, rect);
}

static void widget_icon_has_anim(uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	if (state & (UI_BUT_ANIMATED | UI_BUT_ANIMATED_KEY | UI_BUT_DRIVEN | UI_BUT_REDALERT)) {
		uiWidgetBase wtb;
		float rad;
		
		widget_init(&wtb);
		wtb.outline = 0;
		
		/* rounded */
		rad = 0.5f * BLI_rcti_size_y(rect);
		round_box_edges(&wtb, UI_CNR_ALL, rect, rad);
		widgetbase_draw(&wtb, wcol);
	}
	else if (but->type == NUM) {
		/* Draw number buttons still with left/right 
		 * triangles when field is not embossed */
		widget_numbut_embossn(but, wcol, rect, state, roundboxalign);
	}
}


static void widget_textbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	float rad;
	
	if (state & UI_SELECT)
		SWAP(short, wcol->shadetop, wcol->shadedown);
	
	widget_init(&wtb);
	
	/* half rounded */
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, roundboxalign, rect, rad);
	
	widgetbase_draw(&wtb, wcol);

}


static void widget_menubut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	float rad;
	
	widget_init(&wtb);
	
	/* half rounded */
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, roundboxalign, rect, rad);
	
	/* decoration */
	widget_menu_trias(&wtb.tria1, rect);
	
	widgetbase_draw(&wtb, wcol);
	
	/* text space, arrows are about 0.6 height of button */
	rect->xmax -= (6 * BLI_rcti_size_y(rect)) / 10;
}

static void widget_menuiconbut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	float rad;
	
	widget_init(&wtb);
	
	/* half rounded */
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, roundboxalign, rect, rad);
	
	/* decoration */
	widgetbase_draw(&wtb, wcol);
}

static void widget_menunodebut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	/* silly node link button hacks */
	uiWidgetBase wtb;
	uiWidgetColors wcol_backup = *wcol;
	float rad;
	
	widget_init(&wtb);
	
	/* half rounded */
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, roundboxalign, rect, rad);

	wcol->inner[0] = min_ii(wcol->inner[0] + 15, 255);
	wcol->inner[1] = min_ii(wcol->inner[1] + 15, 255);
	wcol->inner[2] = min_ii(wcol->inner[2] + 15, 255);
	wcol->outline[0] = min_ii(wcol->outline[0] + 15, 255);
	wcol->outline[1] = min_ii(wcol->outline[1] + 15, 255);
	wcol->outline[2] = min_ii(wcol->outline[2] + 15, 255);
	
	/* decoration */
	widgetbase_draw(&wtb, wcol);
	*wcol = wcol_backup;
}

static void widget_pulldownbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	if (state & UI_ACTIVE) {
		uiWidgetBase wtb;
		const float rad = 0.2f * U.widget_unit;

		widget_init(&wtb);

		/* half rounded */
		round_box_edges(&wtb, roundboxalign, rect, rad);
		
		widgetbase_draw(&wtb, wcol);
	}
}

static void widget_menu_itembut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	uiWidgetBase wtb;
	
	widget_init(&wtb);
	
	/* not rounded, no outline */
	wtb.outline = 0;
	round_box_edges(&wtb, 0, rect, 0.0f);
	
	widgetbase_draw(&wtb, wcol);
}

static void widget_menu_radial_itembut(uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	uiWidgetBase wtb;
	float rad;
	float fac = but->block->pie_data.alphafac;

	widget_init(&wtb);

	wtb.emboss = 0;

	rad = 0.5f * BLI_rcti_size_y(rect);
	round_box_edges(&wtb, UI_CNR_ALL, rect, rad);

	wcol->inner[3] *= fac;
	wcol->inner_sel[3] *= fac;
	wcol->item[3] *= fac;
	wcol->text[3] *= fac;
	wcol->text_sel[3] *= fac;
	wcol->outline[3] *= fac;

	widgetbase_draw(&wtb, wcol);
}

static void widget_list_itembut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	uiWidgetBase wtb;
	float rad;
	
	widget_init(&wtb);
	
	/* rounded, but no outline */
	wtb.outline = 0;
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, UI_CNR_ALL, rect, rad);
	
	widgetbase_draw(&wtb, wcol);
}

static void widget_optionbut(uiWidgetColors *wcol, rcti *rect, int state, int UNUSED(roundboxalign))
{
	uiWidgetBase wtb;
	rcti recttemp = *rect;
	float rad;
	int delta;
	
	widget_init(&wtb);
	
	/* square */
	recttemp.xmax = recttemp.xmin + BLI_rcti_size_y(&recttemp);
	
	/* smaller */
	delta = 1 + BLI_rcti_size_y(&recttemp) / 8;
	recttemp.xmin += delta;
	recttemp.ymin += delta;
	recttemp.xmax -= delta;
	recttemp.ymax -= delta;
	
	/* half rounded */
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, UI_CNR_ALL, &recttemp, rad);
	
	/* decoration */
	if (state & UI_SELECT) {
		widget_check_trias(&wtb.tria1, &recttemp);
	}
	
	widgetbase_draw(&wtb, wcol);
	
	/* text space */
	rect->xmin += BLI_rcti_size_y(rect) * 0.7 + delta;
}

/* labels use Editor theme colors for text */
static void widget_state_label(uiWidgetType *wt, int state)
{
	if (state & UI_BUT_LIST_ITEM) {
		/* Override default label theme's colors. */
		bTheme *btheme = UI_GetTheme();
		wt->wcol_theme = &btheme->tui.wcol_list_item;
		/* call this for option button */
		widget_state(wt, state);
	}
	else {
		/* call this for option button */
		widget_state(wt, state);
		if (state & UI_SELECT)
			UI_GetThemeColor3ubv(TH_TEXT_HI, (unsigned char *)wt->wcol.text);
		else
			UI_GetThemeColor3ubv(TH_TEXT, (unsigned char *)wt->wcol.text);
	}
}

static void widget_radiobut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	float rad;
	
	widget_init(&wtb);
	
	/* half rounded */
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, roundboxalign, rect, rad);
	
	widgetbase_draw(&wtb, wcol);

}

static void widget_box(uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	float rad;
	char old_col[3];
	
	widget_init(&wtb);
	
	copy_v3_v3_char(old_col, wcol->inner);
	
	/* abuse but->hsv - if it's non-zero, use this color as the box's background */
	if (but->col[3]) {
		wcol->inner[0] = but->col[0];
		wcol->inner[1] = but->col[1];
		wcol->inner[2] = but->col[2];
	}
	
	/* half rounded */
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, roundboxalign, rect, rad);
	
	widgetbase_draw(&wtb, wcol);
		
	copy_v3_v3_char(wcol->inner, old_col);
}

static void widget_but(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	float rad;
	
	widget_init(&wtb);
	
	/* half rounded */
	rad = 0.2f * U.widget_unit;
	round_box_edges(&wtb, roundboxalign, rect, rad);
	
	widgetbase_draw(&wtb, wcol);

}

static void widget_roundbut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	const float rad = 0.25f * U.widget_unit;
	
	widget_init(&wtb);
	
	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, rad);

	widgetbase_draw(&wtb, wcol);
}

static void widget_draw_extra_mask(const bContext *C, uiBut *but, uiWidgetType *wt, rcti *rect)
{
	uiWidgetBase wtb;
	const float rad = 0.25f * U.widget_unit;
	unsigned char col[4];
	
	/* state copy! */
	wt->wcol = *(wt->wcol_theme);
	
	widget_init(&wtb);
	
	if (but->block->drawextra) {
		/* note: drawextra can change rect +1 or -1, to match round errors of existing previews */
		but->block->drawextra(C, but->poin, but->block->drawextra_arg1, but->block->drawextra_arg2, rect);
		
		/* make mask to draw over image */
		UI_GetThemeColor3ubv(TH_BACK, col);
		glColor3ubv(col);
		
		round_box__edges(&wtb, UI_CNR_ALL, rect, 0.0f, rad);
		widgetbase_outline(&wtb);
	}
	
	/* outline */
	round_box_edges(&wtb, UI_CNR_ALL, rect, rad);
	wtb.outline = 1;
	wtb.inner = 0;
	widgetbase_draw(&wtb, &wt->wcol);
	
}

static uiWidgetType *widget_type(uiWidgetTypeEnum type)
{
	bTheme *btheme = UI_GetTheme();
	static uiWidgetType wt;
	
	/* defaults */
	wt.wcol_theme = &btheme->tui.wcol_regular;
	wt.wcol_state = &btheme->tui.wcol_state;
	wt.state = widget_state;
	wt.draw = widget_but;
	wt.custom = NULL;
	wt.text = widget_draw_text_icon;
	
	switch (type) {
		case UI_WTYPE_REGULAR:
			break;

		case UI_WTYPE_LABEL:
			wt.draw = NULL;
			wt.state = widget_state_label;
			break;
			
		case UI_WTYPE_TOGGLE:
			wt.wcol_theme = &btheme->tui.wcol_toggle;
			break;
			
		case UI_WTYPE_OPTION:
			wt.wcol_theme = &btheme->tui.wcol_option;
			wt.draw = widget_optionbut;
			break;
			
		case UI_WTYPE_RADIO:
			wt.wcol_theme = &btheme->tui.wcol_radio;
			wt.draw = widget_radiobut;
			break;

		case UI_WTYPE_NUMBER:
			wt.wcol_theme = &btheme->tui.wcol_num;
			wt.draw = widget_numbut;
			break;
			
		case UI_WTYPE_SLIDER:
			wt.wcol_theme = &btheme->tui.wcol_numslider;
			wt.custom = widget_numslider;
			wt.state = widget_state_numslider;
			break;
			
		case UI_WTYPE_EXEC:
			wt.wcol_theme = &btheme->tui.wcol_tool;
			wt.draw = widget_roundbut;
			break;

		case UI_WTYPE_TOOLTIP:
			wt.wcol_theme = &btheme->tui.wcol_tooltip;
			wt.draw = widget_menu_back;
			break;
			
			
		/* strings */
		case UI_WTYPE_NAME:
			wt.wcol_theme = &btheme->tui.wcol_text;
			wt.draw = widget_textbut;
			break;
			
		case UI_WTYPE_NAME_LINK:
			break;
			
		case UI_WTYPE_POINTER_LINK:
			break;
			
		case UI_WTYPE_FILENAME:
			break;
			
			
		/* start menus */
		case UI_WTYPE_MENU_RADIO:
			wt.wcol_theme = &btheme->tui.wcol_menu;
			wt.draw = widget_menubut;
			break;

		case UI_WTYPE_MENU_ICON_RADIO:
			wt.wcol_theme = &btheme->tui.wcol_menu;
			wt.draw = widget_menuiconbut;
			break;
			
		case UI_WTYPE_MENU_POINTER_LINK:
			wt.wcol_theme = &btheme->tui.wcol_menu;
			wt.draw = widget_menubut;
			break;

		case UI_WTYPE_MENU_NODE_LINK:
			wt.wcol_theme = &btheme->tui.wcol_menu;
			wt.draw = widget_menunodebut;
			break;
			
		case UI_WTYPE_PULLDOWN:
			wt.wcol_theme = &btheme->tui.wcol_pulldown;
			wt.draw = widget_pulldownbut;
			wt.state = widget_state_pulldown;
			break;
			
		/* in menus */
		case UI_WTYPE_MENU_ITEM:
			wt.wcol_theme = &btheme->tui.wcol_menu_item;
			wt.draw = widget_menu_itembut;
			wt.state = widget_state_menu_item;
			break;
			
		case UI_WTYPE_MENU_BACK:
			wt.wcol_theme = &btheme->tui.wcol_menu_back;
			wt.draw = widget_menu_back;
			break;
			
		/* specials */
		case UI_WTYPE_ICON:
			wt.custom = widget_icon_has_anim;
			break;
			
		case UI_WTYPE_SWATCH:
			wt.custom = widget_swatch;
			break;
			
		case UI_WTYPE_BOX:
			wt.custom = widget_box;
			wt.wcol_theme = &btheme->tui.wcol_box;
			break;
			
		case UI_WTYPE_RGB_PICKER:
			break;
			
		case UI_WTYPE_NORMAL:
			wt.custom = widget_normal;
			break;

		case UI_WTYPE_SCROLL:
			wt.wcol_theme = &btheme->tui.wcol_scroll;
			wt.state = widget_state_nothing;
			wt.custom = widget_scroll;
			break;

		case UI_WTYPE_LISTITEM:
			wt.wcol_theme = &btheme->tui.wcol_list_item;
			wt.draw = widget_list_itembut;
			break;
			
		case UI_WTYPE_PROGRESSBAR:
			wt.wcol_theme = &btheme->tui.wcol_progress;
			wt.custom = widget_progressbar;
			break;

		case UI_WTYPE_MENU_ITEM_RADIAL:
			wt.wcol_theme = &btheme->tui.wcol_pie_menu;
			wt.custom = widget_menu_radial_itembut;
			wt.state = widget_state_pie_menu_item;
			break;
	}
	
	return &wt;
}


static int widget_roundbox_set(uiBut *but, rcti *rect)
{
	int roundbox = UI_CNR_ALL;

	/* alignment */
	if ((but->drawflag & UI_BUT_ALIGN) && but->type != PULLDOWN) {
		
		/* ui_block_position has this correction too, keep in sync */
		if (but->drawflag & UI_BUT_ALIGN_TOP)
			rect->ymax += U.pixelsize;
		if (but->drawflag & UI_BUT_ALIGN_LEFT)
			rect->xmin -= U.pixelsize;
		
		switch (but->drawflag & UI_BUT_ALIGN) {
			case UI_BUT_ALIGN_TOP:
				roundbox = UI_CNR_BOTTOM_LEFT | UI_CNR_BOTTOM_RIGHT;
				break;
			case UI_BUT_ALIGN_DOWN:
				roundbox = UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT;
				break;
			case UI_BUT_ALIGN_LEFT:
				roundbox = UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT;
				break;
			case UI_BUT_ALIGN_RIGHT:
				roundbox = UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT;
				break;
			case UI_BUT_ALIGN_DOWN | UI_BUT_ALIGN_RIGHT:
				roundbox = UI_CNR_TOP_LEFT;
				break;
			case UI_BUT_ALIGN_DOWN | UI_BUT_ALIGN_LEFT:
				roundbox = UI_CNR_TOP_RIGHT;
				break;
			case UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_RIGHT:
				roundbox = UI_CNR_BOTTOM_LEFT;
				break;
			case UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_LEFT:
				roundbox = UI_CNR_BOTTOM_RIGHT;
				break;
			default:
				roundbox = 0;
				break;
		}
	}

	/* align with open menu */
	if (but->active) {
		int direction = ui_button_open_menu_direction(but);

		if      (direction == UI_TOP)   roundbox &= ~(UI_CNR_TOP_RIGHT | UI_CNR_TOP_LEFT);
		else if (direction == UI_DOWN)  roundbox &= ~(UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT);
		else if (direction == UI_LEFT)  roundbox &= ~(UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT);
		else if (direction == UI_RIGHT) roundbox &= ~(UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT);
	}

	return roundbox;
}

/* put all widget colors on half alpha, use local storage */
static void ui_widget_color_disabled(uiWidgetType *wt)
{
	static uiWidgetColors wcol_theme_s;
	
	wcol_theme_s = *wt->wcol_theme;
	
	wcol_theme_s.outline[3] *= 0.5;
	wcol_theme_s.inner[3] *= 0.5;
	wcol_theme_s.inner_sel[3] *= 0.5;
	wcol_theme_s.item[3] *= 0.5;
	wcol_theme_s.text[3] *= 0.5;
	wcol_theme_s.text_sel[3] *= 0.5;

	wt->wcol_theme = &wcol_theme_s;
}

/* conversion from old to new buttons, so still messy */
void ui_draw_but(const bContext *C, ARegion *ar, uiStyle *style, uiBut *but, rcti *rect)
{
	bTheme *btheme = UI_GetTheme();
	ThemeUI *tui = &btheme->tui;
	uiFontStyle *fstyle = &style->widget;
	uiWidgetType *wt = NULL;

	/* handle menus separately */
	if (but->dt == UI_EMBOSSP) {
		switch (but->type) {
			case LABEL:
				widget_draw_text_icon(&style->widgetlabel, &tui->wcol_menu_back, but, rect);
				break;
			case SEPRLINE:
				ui_draw_separator(rect, &tui->wcol_menu_item);
				break;
			default:
				wt = widget_type(UI_WTYPE_MENU_ITEM);
				break;
		}
	}
	else if (but->dt == UI_EMBOSSN) {
		/* "nothing" */
		wt = widget_type(UI_WTYPE_ICON);
	}
	else if (but->dt == UI_EMBOSSR) {
		wt = widget_type(UI_WTYPE_MENU_ITEM_RADIAL);
	}
	else {
		
		switch (but->type) {
			case LABEL:
				if (but->block->flag & UI_BLOCK_LOOP)
					widget_draw_text_icon(&style->widgetlabel, &tui->wcol_menu_back, but, rect);
				else {
					wt = widget_type(UI_WTYPE_LABEL);
					fstyle = &style->widgetlabel;
				}
				break;

			case SEPR:
			case SEPRLINE:
				break;
				
			case BUT:
				wt = widget_type(UI_WTYPE_EXEC);
				break;

			case NUM:
				wt = widget_type(UI_WTYPE_NUMBER);
				break;
				
			case NUMSLI:
				wt = widget_type(UI_WTYPE_SLIDER);
				break;
				
			case ROW:
				wt = widget_type(UI_WTYPE_RADIO);
				break;

			case LISTROW:
				wt = widget_type(UI_WTYPE_LISTITEM);
				break;
				
			case TEX:
				wt = widget_type(UI_WTYPE_NAME);
				break;
			
			case SEARCH_MENU_UNLINK:
			case SEARCH_MENU:
				wt = widget_type(UI_WTYPE_NAME);
				if (but->block->flag & UI_BLOCK_LOOP)
					wt->wcol_theme = &btheme->tui.wcol_menu_back;
				break;
				
			case TOGBUT:
			case TOG:
			case TOGN:
				wt = widget_type(UI_WTYPE_TOGGLE);
				break;
				
			case OPTION:
			case OPTIONN:
				if (!(but->flag & UI_HAS_ICON)) {
					wt = widget_type(UI_WTYPE_OPTION);
					but->drawflag |= UI_BUT_TEXT_LEFT;
				}
				else
					wt = widget_type(UI_WTYPE_TOGGLE);
				
				/* option buttons have strings outside, on menus use different colors */
				if (but->block->flag & UI_BLOCK_LOOP)
					wt->state = widget_state_option_menu;
				
				break;
				
			case MENU:
			case BLOCK:
				if (but->flag & UI_BUT_NODE_LINK) {
					/* new node-link button, not active yet XXX */
					wt = widget_type(UI_WTYPE_MENU_NODE_LINK);
				}
				else {
					/* with menu arrows */

					/* we could use a flag for this, but for now just check size,
					 * add updown arrows if there is room. */
					if ((!but->str[0] && but->icon && (BLI_rcti_size_x(rect) < BLI_rcti_size_y(rect) + 2)) ||
					    /* disable for brushes also */
					    (but->flag & UI_ICON_PREVIEW))
					{
						/* no arrows */
						wt = widget_type(UI_WTYPE_MENU_ICON_RADIO);
					}
					else {
						wt = widget_type(UI_WTYPE_MENU_RADIO);
					}
				}
				break;
				
			case PULLDOWN:
				wt = widget_type(UI_WTYPE_PULLDOWN);
				break;
			
			case BUTM:
				wt = widget_type(UI_WTYPE_MENU_ITEM);
				break;
				
			case COLOR:
				wt = widget_type(UI_WTYPE_SWATCH);
				break;
				
			case ROUNDBOX:
			case LISTBOX:
				wt = widget_type(UI_WTYPE_BOX);
				break;
				
			case LINK:
			case INLINK:
				wt = widget_type(UI_WTYPE_ICON);
				wt->custom = widget_link;
				
				break;
			
			case BUT_EXTRA:
				widget_draw_extra_mask(C, but, widget_type(UI_WTYPE_BOX), rect);
				break;
				
			case HSVCUBE:
				if (ELEM(but->a1, UI_GRAD_V_ALT, UI_GRAD_L_ALT)) {  /* vertical V slider, uses new widget draw now */
					ui_draw_but_HSV_v(but, rect);
				}
				else {  /* other HSV pickers... */
					ui_draw_but_HSVCUBE(but, rect);
				}
				break;
				
			case HSVCIRCLE:
				ui_draw_but_HSVCIRCLE(but, &tui->wcol_regular, rect);
				break;
				
			case BUT_COLORBAND:
				ui_draw_but_COLORBAND(but, &tui->wcol_regular, rect);
				break;
				
			case BUT_NORMAL:
				wt = widget_type(UI_WTYPE_NORMAL);
				break;
				
			case BUT_IMAGE:
				ui_draw_but_IMAGE(ar, but, &tui->wcol_regular, rect);
				break;
			
			case HISTOGRAM:
				ui_draw_but_HISTOGRAM(ar, but, &tui->wcol_regular, rect);
				break;
				
			case WAVEFORM:
				ui_draw_but_WAVEFORM(ar, but, &tui->wcol_regular, rect);
				break;
				
			case VECTORSCOPE:
				ui_draw_but_VECTORSCOPE(ar, but, &tui->wcol_regular, rect);
				break;
					
			case BUT_CURVE:
				ui_draw_but_CURVE(ar, but, &tui->wcol_regular, rect);
				break;
				
			case PROGRESSBAR:
				wt = widget_type(UI_WTYPE_PROGRESSBAR);
				fstyle = &style->widgetlabel;
				break;

			case SCROLL:
				wt = widget_type(UI_WTYPE_SCROLL);
				break;

			case GRIP:
				wt = widget_type(UI_WTYPE_ICON);
				break;

			case TRACKPREVIEW:
				ui_draw_but_TRACKPREVIEW(ar, but, &tui->wcol_regular, rect);
				break;

			case NODESOCKET:
				ui_draw_but_NODESOCKET(ar, but, &tui->wcol_regular, rect);
				break;

			default:
				wt = widget_type(UI_WTYPE_REGULAR);
				break;
		}
	}
	
	if (wt) {
		//rcti disablerect = *rect; /* rect gets clipped smaller for text */
		int roundboxalign, state;
		bool disabled = false;
		
		roundboxalign = widget_roundbox_set(but, rect);

		state = but->flag;

		if ((but->editstr) ||
		    (UNLIKELY(but->flag & UI_BUT_DRAG_MULTI) && ui_get_but_drag_multi_edit(but)))
		{
			state |= UI_TEXTINPUT;
		}

		if (state & (UI_BUT_DISABLED | UI_BUT_INACTIVE))
			if (but->dt != UI_EMBOSSP)
				disabled = true;
		
		if (disabled)
			ui_widget_color_disabled(wt);
		
		wt->state(wt, state);
		if (wt->custom)
			wt->custom(but, &wt->wcol, rect, state, roundboxalign);
		else if (wt->draw)
			wt->draw(&wt->wcol, rect, state, roundboxalign);
		
		if (disabled)
			glEnable(GL_BLEND);
		wt->text(fstyle, &wt->wcol, but, rect);
		if (disabled)
			glDisable(GL_BLEND);
		
//		if (state & (UI_BUT_DISABLED | UI_BUT_INACTIVE))
//			if (but->dt != UI_EMBOSSP)
//				widget_disabled(&disablerect);
	}
}

void ui_draw_menu_back(uiStyle *UNUSED(style), uiBlock *block, rcti *rect)
{
	uiWidgetType *wt = widget_type(UI_WTYPE_MENU_BACK);
	
	wt->state(wt, 0);
	if (block)
		wt->draw(&wt->wcol, rect, block->flag, block->direction);
	else
		wt->draw(&wt->wcol, rect, 0, 0);
	
	if (block) {
		if (block->flag & UI_BLOCK_CLIPTOP) {
			/* XXX no scaling for UI here yet */
			glColor3ubv((unsigned char *)wt->wcol.text);
			UI_DrawTriIcon(BLI_rcti_cent_x(rect), rect->ymax - 8, 't');
		}
		if (block->flag & UI_BLOCK_CLIPBOTTOM) {
			/* XXX no scaling for UI here yet */
			glColor3ubv((unsigned char *)wt->wcol.text);
			UI_DrawTriIcon(BLI_rcti_cent_x(rect), rect->ymin + 10, 'v');
		}
	}
}

static void draw_disk_shaded(
        float start, float angle,
        float radius_int, float radius_ext, int subd,
        const char col1[4], const char col2[4],
        bool shaded)
{
	const float radius_ext_scale = (0.5f / radius_ext);  /* 1 / (2 * radius_ext) */
	int i;

	float s, c;
	float y1, y2;
	float fac;
	unsigned char r_col[4];

	glBegin(GL_TRIANGLE_STRIP);

	s = sinf(start);
	c = cosf(start);

	y1 = s * radius_int;
	y2 = s * radius_ext;

	if (shaded) {
		fac = (y1 + radius_ext) * radius_ext_scale;
		round_box_shade_col4_r(r_col, col1, col2, fac);

		glColor4ubv(r_col);
	}

	glVertex2f(c * radius_int, s * radius_int);

	if (shaded) {
		fac = (y2 + radius_ext) * radius_ext_scale;
		round_box_shade_col4_r(r_col, col1, col2, fac);

		glColor4ubv(r_col);
	}
	glVertex2f(c * radius_ext, s * radius_ext);

	for (i = 1; i < subd; i++) {
		float a;

		a = start + ((i) / (float)(subd - 1)) * angle;
		s = sinf(a);
		c = cosf(a);
		y1 = s * radius_int;
		y2 = s * radius_ext;

		if (shaded) {
			fac = (y1 + radius_ext) * radius_ext_scale;
			round_box_shade_col4_r(r_col, col1, col2, fac);

			glColor4ubv(r_col);
		}
		glVertex2f(c * radius_int, s * radius_int);

		if (shaded) {
			fac = (y2 + radius_ext) * radius_ext_scale;
			round_box_shade_col4_r(r_col, col1, col2, fac);

			glColor4ubv(r_col);
		}
		glVertex2f(c * radius_ext, s * radius_ext);
	}
	glEnd();

}

void ui_draw_pie_center(uiBlock *block)
{
	bTheme *btheme = UI_GetTheme();
	float cx = block->pie_data.pie_center_spawned[0];
	float cy = block->pie_data.pie_center_spawned[1];

	float *pie_dir = block->pie_data.pie_dir;

	float pie_radius_internal = U.pixelsize * U.pie_menu_threshold;
	float pie_radius_external = U.pixelsize * (U.pie_menu_threshold + 7.0f);

	int subd = 40;

	float angle = atan2f(pie_dir[1], pie_dir[0]);
	float range = (block->pie_data.flags & UI_PIE_DEGREES_RANGE_LARGE) ? ((float)M_PI / 2.0f) : ((float)M_PI / 4.0f);

	glPushMatrix();
	glTranslatef(cx, cy, 0.0f);

	glEnable(GL_BLEND);
	if (btheme->tui.wcol_pie_menu.shaded) {
		char col1[4], col2[4];
		shadecolors4(col1, col2, btheme->tui.wcol_pie_menu.inner, btheme->tui.wcol_pie_menu.shadetop, btheme->tui.wcol_pie_menu.shadedown);
		draw_disk_shaded(0.0f, (float)(M_PI * 2.0), pie_radius_internal, pie_radius_external, subd, col1, col2, true);
	}
	else {
		glColor4ubv((GLubyte *)btheme->tui.wcol_pie_menu.inner);
		draw_disk_shaded(0.0f, (float)(M_PI * 2.0), pie_radius_internal, pie_radius_external, subd, NULL, NULL, false);
	}

	if (!(block->pie_data.flags & UI_PIE_INVALID_DIR)) {
		if (btheme->tui.wcol_pie_menu.shaded) {
			char col1[4], col2[4];
			shadecolors4(col1, col2, btheme->tui.wcol_pie_menu.inner_sel, btheme->tui.wcol_pie_menu.shadetop, btheme->tui.wcol_pie_menu.shadedown);
			draw_disk_shaded(angle - range / 2.0f, range, pie_radius_internal, pie_radius_external, subd, col1, col2, true);
		}
		else {
			glColor4ubv((GLubyte *)btheme->tui.wcol_pie_menu.inner_sel);
			draw_disk_shaded(angle - range / 2.0f, range, pie_radius_internal, pie_radius_external, subd, NULL, NULL, false);
		}
	}

	glColor4ubv((GLubyte *)btheme->tui.wcol_pie_menu.outline);
	glutil_draw_lined_arc(0.0f, (float)M_PI * 2.0f, pie_radius_internal, subd);
	glutil_draw_lined_arc(0.0f, (float)M_PI * 2.0f, pie_radius_external, subd);

	glDisable(GL_BLEND);
	glPopMatrix();
}


uiWidgetColors *ui_tooltip_get_theme(void)
{
	uiWidgetType *wt = widget_type(UI_WTYPE_TOOLTIP);
	return wt->wcol_theme;
}

void ui_draw_tooltip_background(uiStyle *UNUSED(style), uiBlock *UNUSED(block), rcti *rect)
{
	uiWidgetType *wt = widget_type(UI_WTYPE_TOOLTIP);
	wt->state(wt, 0);
	/* wt->draw ends up using same function to draw the tooltip as menu_back */
	wt->draw(&wt->wcol, rect, 0, 0);
}

void ui_draw_search_back(uiStyle *UNUSED(style), uiBlock *block, rcti *rect)
{
	uiWidgetType *wt = widget_type(UI_WTYPE_BOX);
	
	glEnable(GL_BLEND);
	widget_softshadow(rect, UI_CNR_ALL, 0.25f * U.widget_unit);
	glDisable(GL_BLEND);

	wt->state(wt, 0);
	if (block)
		wt->draw(&wt->wcol, rect, block->flag, UI_CNR_ALL);
	else
		wt->draw(&wt->wcol, rect, 0, UI_CNR_ALL);
	
}


/* helper call to draw a menu item without button */
/* state: UI_ACTIVE or 0 */
void ui_draw_menu_item(uiFontStyle *fstyle, rcti *rect, const char *name, int iconid, int state, bool use_sep)
{
	uiWidgetType *wt = widget_type(UI_WTYPE_MENU_ITEM);
	rcti _rect = *rect;
	char *cpoin = NULL;

	wt->state(wt, state);
	wt->draw(&wt->wcol, rect, 0, 0);
	
	uiStyleFontSet(fstyle);
	fstyle->align = UI_STYLE_TEXT_LEFT;
	
	/* text location offset */
	rect->xmin += 0.25f * UI_UNIT_X;
	if (iconid) rect->xmin += UI_DPI_ICON_SIZE;

	/* cut string in 2 parts? */
	if (use_sep) {
		cpoin = strchr(name, UI_SEP_CHAR);
		if (cpoin) {
			*cpoin = 0;

			/* need to set this first */
			uiStyleFontSet(fstyle);

			if (fstyle->kerning == 1) { /* for BLF_width */
				BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
			}

			rect->xmax -= BLF_width(fstyle->uifont_id, cpoin + 1, INT_MAX) + UI_DPI_ICON_SIZE;

			if (fstyle->kerning == 1) {
				BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
			}
		}
	}

	{
		char drawstr[UI_MAX_DRAW_STR];
		const float okwidth = (float)BLI_rcti_size_x(rect);
		const size_t max_len = sizeof(drawstr);
		const float minwidth = (float)(UI_DPI_ICON_SIZE);

		BLI_strncpy(drawstr, name, sizeof(drawstr));
		ui_text_clip_middle_ex(fstyle, drawstr, okwidth, minwidth, max_len, NULL);

		glColor4ubv((unsigned char *)wt->wcol.text);
		uiStyleFontDraw(fstyle, rect, drawstr);
	}

	/* part text right aligned */
	if (use_sep) {
		if (cpoin) {
			fstyle->align = UI_STYLE_TEXT_RIGHT;
			rect->xmax = _rect.xmax - 5;
			uiStyleFontDraw(fstyle, rect, cpoin + 1);
			*cpoin = UI_SEP_CHAR;
		}
	}
	
	/* restore rect, was messed with */
	*rect = _rect;

	if (iconid) {
		float height, aspect;
		int xs = rect->xmin + 0.2f * UI_UNIT_X;
		int ys = rect->ymin + 0.1f * BLI_rcti_size_y(rect);

		height = ICON_SIZE_FROM_BUTRECT(rect);
		aspect = ICON_DEFAULT_HEIGHT / height;
		
		glEnable(GL_BLEND);
		UI_icon_draw_aspect(xs, ys, iconid, aspect, 1.0f); /* XXX scale weak get from fstyle? */
		glDisable(GL_BLEND);
	}
}

void ui_draw_preview_item(uiFontStyle *fstyle, rcti *rect, const char *name, int iconid, int state)
{
	rcti trect = *rect, bg_rect;
	float font_dims[2] = {0.0f, 0.0f};
	uiWidgetType *wt = widget_type(UI_WTYPE_MENU_ITEM);
	
	wt->state(wt, state);
	wt->draw(&wt->wcol, rect, 0, 0);
	
	glEnable(GL_BLEND);
	widget_draw_preview(iconid, 1.0f, rect);
	
	BLF_width_and_height(fstyle->uifont_id, name, BLF_DRAW_STR_DUMMY_MAX, &font_dims[0], &font_dims[1]);

	/* text rect */
	trect.xmin += 0;
	trect.xmax = trect.xmin + font_dims[0] + 10;
	trect.ymin += 10;
	trect.ymax = trect.ymin + font_dims[1];
	if (trect.xmax > rect->xmax - PREVIEW_PAD)
		trect.xmax = rect->xmax - PREVIEW_PAD;

	bg_rect = trect;
	bg_rect.xmin = rect->xmin + PREVIEW_PAD;
	bg_rect.ymin = rect->ymin + PREVIEW_PAD;
	bg_rect.xmax = rect->xmax - PREVIEW_PAD;
	bg_rect.ymax += PREVIEW_PAD / 2;
	
	if (bg_rect.xmax > rect->xmax - PREVIEW_PAD)
		bg_rect.xmax = rect->xmax - PREVIEW_PAD;

	glColor4ubv((unsigned char *)wt->wcol_theme->inner_sel);
	glRecti(bg_rect.xmin, bg_rect.ymin, bg_rect.xmax, bg_rect.ymax);
	glDisable(GL_BLEND);

	{
		char drawstr[UI_MAX_DRAW_STR];
		const float okwidth = (float)BLI_rcti_size_x(&trect);
		const size_t max_len = sizeof(drawstr);
		const float minwidth = (float)(UI_DPI_ICON_SIZE);

		BLI_strncpy(drawstr, name, sizeof(drawstr));
		ui_text_clip_middle_ex(fstyle, drawstr, okwidth, minwidth, max_len, NULL);

		glColor4ubv((unsigned char *)wt->wcol.text);
		uiStyleFontDraw(fstyle, &trect, drawstr);
	}
}

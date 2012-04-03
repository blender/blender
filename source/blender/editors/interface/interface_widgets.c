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


#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLF_api.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"


#include "interface_intern.h"

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
	
	float vec[32][2];
	unsigned int (*index)[3];
	
} uiWidgetTrias;

/* max as used by round_box__edges */
#define WIDGET_CURVE_RESOLU 9
#define WIDGET_SIZE_MAX (WIDGET_CURVE_RESOLU * 4)

typedef struct uiWidgetBase {
	
	int totvert, halfwayvert;
	float outer_v[WIDGET_SIZE_MAX][2];
	float inner_v[WIDGET_SIZE_MAX][2];
	float inner_uv[WIDGET_SIZE_MAX][2];
	
	short inner, outline, emboss; /* set on/off */
	short shadedir;
	
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

static float cornervec[WIDGET_CURVE_RESOLU][2] = {{0.0, 0.0}, {0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169},
												  {0.707, 0.293}, {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}, {1.0, 1.0}};

#define WIDGET_AA_JITTER 8
static float jit[WIDGET_AA_JITTER][2] = {
	{ 0.468813, -0.481430}, {-0.155755, -0.352820},
	{ 0.219306, -0.238501}, {-0.393286, -0.110949},
	{-0.024699,  0.013908}, { 0.343805,  0.147431},
	{-0.272855,  0.269918}, { 0.095909,  0.388710}
};

static float num_tria_vert[3][2] = {
	{-0.352077, 0.532607}, {-0.352077, -0.549313}, {0.330000, -0.008353}
};

static unsigned int num_tria_face[1][3] = {
	{0, 1, 2}
};

static float scroll_circle_vert[16][2] = {
	{0.382684, 0.923879}, {0.000001, 1.000000}, {-0.382683, 0.923880}, {-0.707107, 0.707107},
	{-0.923879, 0.382684}, {-1.000000, 0.000000}, {-0.923880, -0.382684}, {-0.707107, -0.707107},
	{-0.382683, -0.923880}, {0.000000, -1.000000}, {0.382684, -0.923880}, {0.707107, -0.707107},
	{0.923880, -0.382684}, {1.000000, -0.000000}, {0.923880, 0.382683}, {0.707107, 0.707107}
};

static unsigned int scroll_circle_face[14][3] = {
	{0, 1, 2}, {2, 0, 3}, {3, 0, 15}, {3, 15, 4}, {4, 15, 14}, {4, 14, 5}, {5, 14, 13}, {5, 13, 6},
	{6, 13, 12}, {6, 12, 7}, {7, 12, 11}, {7, 11, 8}, {8, 11, 10}, {8, 10, 9}
};

static float menu_tria_vert[6][2] = {
	{-0.41, 0.16}, {0.41, 0.16}, {0, 0.82},
	{0, -0.82}, {-0.41, -0.16}, {0.41, -0.16}
};

static unsigned int menu_tria_face[2][3] = {{2, 0, 1}, {3, 5, 4}};

static float check_tria_vert[6][2] = {
	{-0.578579, 0.253369},  {-0.392773, 0.412794},  {-0.004241, -0.328551},
	{-0.003001, 0.034320},  {1.055313, 0.864744},   {0.866408, 1.026895}
};

static unsigned int check_tria_face[4][3] = {
	{3, 2, 4}, {3, 4, 5}, {1, 0, 3}, {0, 2, 3}
};

GLubyte checker_stipple_sml[32 * 32 / 8] =
{
	255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
	255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
	0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
	0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
	255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
	255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
	0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
	0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
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
		glTranslatef(1.0f * jit[j][0], 1.0f * jit[j][1], 0.0f);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glTranslatef(-1.0f * jit[j][0], -1.0f * jit[j][1], 0.0f);
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisable(GL_BLEND);
}

void ui_draw_anti_roundbox(int mode, float minx, float miny, float maxx, float maxy, float rad)
{
	float color[4];
	int j;
	
	glEnable(GL_BLEND);
	glGetFloatv(GL_CURRENT_COLOR, color);
	color[3] *= 0.125f;
	glColor4fv(color);
	
	for (j = 0; j < WIDGET_AA_JITTER; j++) {
		glTranslatef(1.0f * jit[j][0], 1.0f * jit[j][1], 0.0f);
		uiDrawBox(mode, minx, miny, maxx, maxy, rad);
		glTranslatef(-1.0f * jit[j][0], -1.0f * jit[j][1], 0.0f);
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
static int round_box_shadow_edges(float (*vert)[2], rcti *rect, float rad, int roundboxalign, float step)
{
	float vec[WIDGET_CURVE_RESOLU][2];
	float minx, miny, maxx, maxy;
	int a, tot = 0;
	
	rad += step;
	
	if (2.0f * rad > rect->ymax - rect->ymin)
		rad = 0.5f * (rect->ymax - rect->ymin);
	
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
static void round_box__edges(uiWidgetBase *wt, int roundboxalign, rcti *rect, float rad, float radi)
{
	float vec[WIDGET_CURVE_RESOLU][2], veci[WIDGET_CURVE_RESOLU][2];
	float minx = rect->xmin, miny = rect->ymin, maxx = rect->xmax, maxy = rect->ymax;
	float minxi = minx + 1.0f; /* boundbox inner */
	float maxxi = maxx - 1.0f;
	float minyi = miny + 1.0f;
	float maxyi = maxy - 1.0f;
	float facxi = (maxxi != minxi) ? 1.0f / (maxxi - minxi) : 0.0f; /* for uv, can divide by zero */
	float facyi = (maxyi != minyi) ? 1.0f / (maxyi - minyi) : 0.0f;
	int a, tot = 0, minsize;
	const int hnum = ((roundboxalign & (UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT)) == (UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT) ||
	                  (roundboxalign & (UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT)) == (UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT)) ? 1 : 2;
	const int vnum = ((roundboxalign & (UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT)) == (UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT) ||
	                  (roundboxalign & (UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT)) == (UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT)) ? 1 : 2;

	minsize = MIN2((rect->xmax - rect->xmin) * hnum, (rect->ymax - rect->ymin) * vnum);
	
	if (2.0f * rad > minsize)
		rad = 0.5f * minsize;

	if (2.0f * (radi + 1.0f) > minsize)
		radi = 0.5f * minsize - 1.0f;
	
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

static void round_box_edges(uiWidgetBase *wt, int roundboxalign, rcti *rect, float rad)
{
	round_box__edges(wt, roundboxalign, rect, rad, rad - 1.0f);
}


/* based on button rect, return scaled array of triangles */
static void widget_num_tria(uiWidgetTrias *tria, rcti *rect, float triasize, char where)
{
	float centx, centy, sizex, sizey, minsize;
	int a, i1 = 0, i2 = 1;
	
	minsize = MIN2(rect->xmax - rect->xmin, rect->ymax - rect->ymin);
	
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
	
	for (a = 0; a < 3; a++) {
		tria->vec[a][0] = sizex * num_tria_vert[a][i1] + centx;
		tria->vec[a][1] = sizey * num_tria_vert[a][i2] + centy;
	}
	
	tria->tot = 1;
	tria->index = num_tria_face;
}

static void widget_scroll_circle(uiWidgetTrias *tria, rcti *rect, float triasize, char where)
{
	float centx, centy, sizex, sizey, minsize;
	int a, i1 = 0, i2 = 1;
	
	minsize = MIN2(rect->xmax - rect->xmin, rect->ymax - rect->ymin);
	
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
	
	for (a = 0; a < 16; a++) {
		tria->vec[a][0] = sizex * scroll_circle_vert[a][i1] + centx;
		tria->vec[a][1] = sizey * scroll_circle_vert[a][i2] + centy;
	}
	
	tria->tot = 14;
	tria->index = scroll_circle_face;
}

static void widget_trias_draw(uiWidgetTrias *tria)
{
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, tria->vec);
	glDrawElements(GL_TRIANGLES, tria->tot * 3, GL_UNSIGNED_INT, tria->index);
	glDisableClientState(GL_VERTEX_ARRAY);
}

static void widget_menu_trias(uiWidgetTrias *tria, rcti *rect)
{
	float centx, centy, size, asp;
	int a;
		
	/* center position and size */
	centx = rect->xmax - 0.5f * (rect->ymax - rect->ymin);
	centy = rect->ymin + 0.5f * (rect->ymax - rect->ymin);
	size = 0.4f * (rect->ymax - rect->ymin);
	
	/* XXX exception */
	asp = ((float)rect->xmax - rect->xmin) / ((float)rect->ymax - rect->ymin);
	if (asp > 1.2f && asp < 2.6f)
		centx = rect->xmax - 0.3f * (rect->ymax - rect->ymin);
	
	for (a = 0; a < 6; a++) {
		tria->vec[a][0] = size * menu_tria_vert[a][0] + centx;
		tria->vec[a][1] = size * menu_tria_vert[a][1] + centy;
	}

	tria->tot = 2;
	tria->index = menu_tria_face;
}

static void widget_check_trias(uiWidgetTrias *tria, rcti *rect)
{
	float centx, centy, size;
	int a;
	
	/* center position and size */
	centx = rect->xmin + 0.5f * (rect->ymax - rect->ymin);
	centy = rect->ymin + 0.5f * (rect->ymax - rect->ymin);
	size = 0.5f * (rect->ymax - rect->ymin);
	
	for (a = 0; a < 6; a++) {
		tria->vec[a][0] = size * check_tria_vert[a][0] + centx;
		tria->vec[a][1] = size * check_tria_vert[a][1] + centy;
	}
	
	tria->tot = 4;
	tria->index = check_tria_face;
}


/* prepares shade colors */
static void shadecolors4(char coltop[4], char *coldown, const char *color, short shadetop, short shadedown)
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

static void round_box_shade_col4_r(unsigned char col_r[4], const char col1[4], const char col2[4], const float fac)
{
	const int faci = FTOCHAR(fac);
	const int facm = 255 - faci;

	col_r[0] = (faci * col1[0] + facm * col2[0]) >> 8;
	col_r[1] = (faci * col1[1] + facm * col2[1]) >> 8;
	col_r[2] = (faci * col1[2] + facm * col2[2]) >> 8;
	col_r[3] = (faci * col1[3] + facm * col2[3]) >> 8;
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
				glColor4ub(UI_TRANSP_DARK, UI_TRANSP_DARK, UI_TRANSP_DARK, 255);
				glEnableClientState(GL_VERTEX_ARRAY);
				glVertexPointer(2, GL_FLOAT, 0, wtb->inner_v);
				glDrawArrays(GL_POLYGON, 0, wtb->totvert);
				glDisableClientState(GL_VERTEX_ARRAY);

				/* light checkers */
				glEnable(GL_POLYGON_STIPPLE);
				glColor4ub(UI_TRANSP_LIGHT, UI_TRANSP_LIGHT, UI_TRANSP_LIGHT, 255);
				glPolygonStipple(checker_stipple_sml);

				glEnableClientState(GL_VERTEX_ARRAY);
				glVertexPointer(2, GL_FLOAT, 0, wtb->inner_v);
				glDrawArrays(GL_POLYGON, 0, wtb->totvert);
				glDisableClientState(GL_VERTEX_ARRAY);

				glDisable(GL_POLYGON_STIPPLE);

				/* alpha fill */
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

				glColor4ubv((unsigned char *)wcol->inner);
				glEnableClientState(GL_VERTEX_ARRAY);

				for (a = 0; a < wtb->totvert; a++) {
					x_mid += wtb->inner_v[a][0];
				}
				x_mid /= wtb->totvert;

				glVertexPointer(2, GL_FLOAT, 0, wtb->inner_v);
				glDrawArrays(GL_POLYGON, 0, wtb->totvert);
				glDisableClientState(GL_VERTEX_ARRAY);

				/* 1/2 solid color */
				glColor4ub(wcol->inner[0], wcol->inner[1], wcol->inner[2], 255);

				for (a = 0; a < wtb->totvert; a++) {
					inner_v_half[a][0] = MIN2(wtb->inner_v[a][0], x_mid);
					inner_v_half[a][1] = wtb->inner_v[a][1];
				}

				glEnableClientState(GL_VERTEX_ARRAY);
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
		                               UCHAR_MAX / WIDGET_AA_JITTER};

		widget_verts_to_quad_strip(wtb, wtb->totvert, quad_strip);

		if (wtb->emboss) {
			widget_verts_to_quad_strip_open(wtb, wtb->halfwayvert, quad_strip_emboss);
		}

		glEnableClientState(GL_VERTEX_ARRAY);

		for (j = 0; j < WIDGET_AA_JITTER; j++) {
			glTranslatef(1.0f * jit[j][0], 1.0f * jit[j][1], 0.0f);
			
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
			
			glTranslatef(-1.0f * jit[j][0], -1.0f * jit[j][1], 0.0f);
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
			glTranslatef(1.0f * jit[j][0], 1.0f * jit[j][1], 0.0f);

			if (wtb->tria1.tot) {
				glColor4ubv(tcol);
				widget_trias_draw(&wtb->tria1);
			}
			if (wtb->tria2.tot) {
				glColor4ubv(tcol);
				widget_trias_draw(&wtb->tria2);
			}
		
			glTranslatef(-1.0f * jit[j][0], -1.0f * jit[j][1], 0.0f);
		}
	}

	glDisable(GL_BLEND);
	
}

/* *********************** text/icon ************************************** */

#define PREVIEW_PAD 4

static void widget_draw_preview(BIFIconID icon, float UNUSED(alpha), rcti *rect)
{
	int w, h, size;

	if (icon == ICON_NONE)
		return;

	w = rect->xmax - rect->xmin;
	h = rect->ymax - rect->ymin;
	size = MIN2(w, h);
	size -= PREVIEW_PAD * 2;  /* padding */

	if (size > 0) {
		int x = rect->xmin + w / 2 - size / 2;
		int y = rect->ymin + h / 2 - size / 2;

		UI_icon_draw_preview_aspect_size(x, y, icon, 1.0f, size);
	}
}


static int ui_but_draw_menu_icon(uiBut *but)
{
	return (but->flag & UI_ICON_SUBMENU) && (but->dt == UI_EMBOSSP);
}

/* icons have been standardized... and this call draws in untransformed coordinates */

static void widget_draw_icon(uiBut *but, BIFIconID icon, float alpha, rcti *rect)
{
	int xs = 0, ys = 0;
	float aspect, height;
	
	if (but->flag & UI_ICON_PREVIEW) {
		widget_draw_preview(icon, alpha, rect);
		return;
	}
	
	/* this icon doesn't need draw... */
	if (icon == ICON_BLANK1 && (but->flag & UI_ICON_SUBMENU) == 0) return;
	
	/* we need aspect from block, for menus... these buttons are scaled in uiPositionBlock() */
	aspect = but->block->aspect;
	if (aspect != but->aspect) {
		/* prevent scaling up icon in pupmenu */
		if (aspect < 1.0f) {			
			height = UI_DPI_ICON_SIZE;
			aspect = 1.0f;
			
		}
		else 
			height = UI_DPI_ICON_SIZE / aspect;
	}
	else
		height = UI_DPI_ICON_SIZE;
	
	/* calculate blend color */
	if (ELEM4(but->type, TOG, ROW, TOGN, LISTROW)) {
		if (but->flag & UI_SELECT) ;
		else if (but->flag & UI_ACTIVE) ;
		else alpha = 0.5f;
	}
	
	/* extra feature allows more alpha blending */
	if (but->type == LABEL && but->a1 == 1.0f) alpha *= but->a2;
	
	glEnable(GL_BLEND);
	
	if (icon && icon != ICON_BLANK1) {
		if (but->flag & UI_ICON_LEFT) {
			if (but->type == BUT_TOGDUAL) {
				if (but->drawstr[0]) {
					xs = rect->xmin - 1;
				}
				else {
					xs = (rect->xmin + rect->xmax - height) / 2;
				}
			}
			else if (but->block->flag & UI_BLOCK_LOOP) {
				if (but->type == SEARCH_MENU)
					xs = rect->xmin + 4;
				else
					xs = rect->xmin + 1;
			}
			else if ((but->type == ICONROW) || (but->type == ICONTEXTROW)) {
				xs = rect->xmin + 3;
			}
			else {
				xs = rect->xmin + 4;
			}
			ys = (rect->ymin + rect->ymax - height) / 2;
		}
		else {
			xs = (rect->xmin + rect->xmax - height) / 2;
			ys = (rect->ymin + rect->ymax - height) / 2;
		}
		
		/* to indicate draggable */
		if (but->dragpoin && (but->flag & UI_ACTIVE)) {
			float rgb[3] = {1.25f, 1.25f, 1.25f};
			UI_icon_draw_aspect_color(xs, ys, icon, aspect, rgb);
		}
		else
			UI_icon_draw_aspect(xs, ys, icon, aspect, alpha);
	}

	if (ui_but_draw_menu_icon(but)) {
		xs = rect->xmax - 17;
		ys = (rect->ymin + rect->ymax - height) / 2;
		
		UI_icon_draw_aspect(xs, ys, ICON_RIGHTARROW_THIN, aspect, alpha);
	}
	
	glDisable(GL_BLEND);
}

static void ui_text_clip_give_prev_off(uiBut *but)
{
	char *prev_utf8 = BLI_str_find_prev_char_utf8(but->drawstr, but->drawstr + but->ofs);
	int bytes = but->drawstr + but->ofs - prev_utf8;

	but->ofs -= bytes;
}

static void ui_text_clip_give_next_off(uiBut *but)
{
	char *next_utf8 = BLI_str_find_next_char_utf8(but->drawstr + but->ofs, NULL);
	int bytes = next_utf8 - (but->drawstr + but->ofs);

	but->ofs += bytes;
}

/* sets but->ofs to make sure text is correctly visible */
static void ui_text_leftclip(uiFontStyle *fstyle, uiBut *but, rcti *rect)
{
	int border = (but->flag & UI_BUT_ALIGN_RIGHT) ? 8 : 10;
	int okwidth = rect->xmax - rect->xmin - border;
	
	if (but->flag & UI_HAS_ICON) okwidth -= UI_DPI_ICON_SIZE;
	
	/* need to set this first */
	uiStyleFontSet(fstyle);
	
	if (fstyle->kerning == 1) /* for BLF_width */
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);

	/* if text editing we define ofs dynamically */
	if (but->editstr && but->pos >= 0) {
		if (but->ofs > but->pos)
			but->ofs = but->pos;

		if (BLF_width(fstyle->uifont_id, but->drawstr) <= okwidth)
			but->ofs = 0;
	}
	else but->ofs = 0;
	
	but->strwidth = BLF_width(fstyle->uifont_id, but->drawstr + but->ofs);
	
	while (but->strwidth > okwidth) {
		
		/* textbut exception, clip right when... */
		if (but->editstr && but->pos >= 0) {
			float width;
			char buf[UI_MAX_DRAW_STR];
			
			/* copy draw string */
			BLI_strncpy_utf8(buf, but->drawstr, sizeof(buf));
			/* string position of cursor */
			buf[but->pos] = 0;
			width = BLF_width(fstyle->uifont_id, buf + but->ofs);
			
			/* if cursor is at 20 pixels of right side button we clip left */
			if (width > okwidth - 20)
				ui_text_clip_give_next_off(but);
			else {
				int len, bytes;
				/* shift string to the left */
				if (width < 20 && but->ofs > 0)
					ui_text_clip_give_prev_off(but);
				len = strlen(but->drawstr);
				bytes = BLI_str_utf8_size(BLI_str_find_prev_char_utf8(but->drawstr, but->drawstr + len));
				but->drawstr[len - bytes] = 0;
			}
		}
		else
			ui_text_clip_give_next_off(but);

		but->strwidth = BLF_width(fstyle->uifont_id, but->drawstr + but->ofs);
		
		if (but->strwidth < 10) break;
	}
	
	if (fstyle->kerning == 1)
		BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
}

static void ui_text_label_rightclip(uiFontStyle *fstyle, uiBut *but, rcti *rect)
{
	int border = (but->flag & UI_BUT_ALIGN_RIGHT) ? 8 : 10;
	int okwidth = rect->xmax - rect->xmin - border;
	char *cpoin = NULL;
	char *cpend = but->drawstr + strlen(but->drawstr);
	
	/* need to set this first */
	uiStyleFontSet(fstyle);
	
	if (fstyle->kerning == 1) /* for BLF_width */
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	
	but->strwidth = BLF_width(fstyle->uifont_id, but->drawstr);
	but->ofs = 0;
	
	/* find the space after ':' separator */
	cpoin = strrchr(but->drawstr, ':');
	
	if (cpoin && (cpoin < cpend - 2)) {
		char *cp2 = cpoin;
		
		/* chop off the leading text, starting from the right */
		while (but->strwidth > okwidth && cp2 > but->drawstr) {
			int bytes = BLI_str_utf8_size(cp2);
			if (bytes < 0)
				bytes = 1;

			/* shift the text after and including cp2 back by 1 char, +1 to include null terminator */
			memmove(cp2 - bytes, cp2, strlen(cp2) + 1);
			cp2 -= bytes;
			
			but->strwidth = BLF_width(fstyle->uifont_id, but->drawstr + but->ofs);
			if (but->strwidth < 10) break;
		}
	
	
		/* after the leading text is gone, chop off the : and following space, with ofs */
		while ((but->strwidth > okwidth) && (but->ofs < 2))
		{
			ui_text_clip_give_next_off(but);
			but->strwidth = BLF_width(fstyle->uifont_id, but->drawstr + but->ofs);
			if (but->strwidth < 10) break;
		}
		
	}

	/* once the label's gone, chop off the least significant digits */
	while (but->strwidth > okwidth) {
		int len = strlen(but->drawstr);
		int bytes = BLI_str_utf8_size(BLI_str_find_prev_char_utf8(but->drawstr, but->drawstr + len));
		if (bytes < 0)
			bytes = 1;

		but->drawstr[len - bytes] = 0;
		
		but->strwidth = BLF_width(fstyle->uifont_id, but->drawstr + but->ofs);
		if (but->strwidth < 10) break;
	}
	
	if (fstyle->kerning == 1)
		BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
}


static void widget_draw_text(uiFontStyle *fstyle, uiWidgetColors *wcol, uiBut *but, rcti *rect)
{
//	int transopts;
	char *cpoin = NULL;
	
	/* for underline drawing */
	float font_xofs, font_yofs;

	uiStyleFontSet(fstyle);
	
	if (but->editstr || (but->flag & UI_TEXT_LEFT))
		fstyle->align = UI_STYLE_TEXT_LEFT;
	else
		fstyle->align = UI_STYLE_TEXT_CENTER;
	
	if (fstyle->kerning == 1) /* for BLF_width */
		BLF_enable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	
	/* text button selection and cursor */
	if (but->editstr && but->pos != -1) {
		short t = 0, pos = 0, ch;
		short selsta_tmp, selend_tmp, selsta_draw, selwidth_draw;

		if ((but->selend - but->selsta) > 0) {
			/* text button selection */
			selsta_tmp = but->selsta;
			selend_tmp = but->selend;
			
			if (but->drawstr[0] != 0) {

				if (but->selsta >= but->ofs) {
					ch = but->drawstr[selsta_tmp];
					but->drawstr[selsta_tmp] = 0;
					
					selsta_draw = BLF_width(fstyle->uifont_id, but->drawstr + but->ofs);
					
					but->drawstr[selsta_tmp] = ch;
				}
				else {
					selsta_draw = 0;
				}
				
				ch = but->drawstr[selend_tmp];
				but->drawstr[selend_tmp] = 0;
				
				selwidth_draw = BLF_width(fstyle->uifont_id, but->drawstr + but->ofs);
				
				but->drawstr[selend_tmp] = ch;

				glColor3ubv((unsigned char *)wcol->item);
				glRects(rect->xmin + selsta_draw, rect->ymin + 2, rect->xmin + selwidth_draw, rect->ymax - 2);
			}
		}
		else {
			/* text cursor */
			pos = but->pos;
			if (pos >= but->ofs) {
				if (but->drawstr[0] != 0) {
					ch = but->drawstr[pos];
					but->drawstr[pos] = 0;
					
					t = BLF_width(fstyle->uifont_id, but->drawstr + but->ofs) / but->aspect;
					
					but->drawstr[pos] = ch;
				}

				glColor3f(0.20, 0.6, 0.9);
				glRects(rect->xmin + t, rect->ymin + 2, rect->xmin + t + 2, rect->ymax - 2);
			}
		}
	}
	
	if (fstyle->kerning == 1)
		BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
	
	//	ui_rasterpos_safe(x, y, but->aspect);
//	if (but->type==IDPOIN) transopts= 0;	// no translation, of course!
//	else transopts= ui_translate_buttons();
	
	/* cut string in 2 parts - only for menu entries */
	if ((but->block->flag & UI_BLOCK_LOOP)) {
		if (ELEM5(but->type, SLI, NUM, TEX, NUMSLI, NUMABS) == 0) {
			cpoin = strchr(but->drawstr, '|');
			if (cpoin) *cpoin = 0;
		}
	}
	
	glColor3ubv((unsigned char *)wcol->text);

	uiStyleFontDrawExt(fstyle, rect, but->drawstr + but->ofs, &font_xofs, &font_yofs);

	if (but->menu_key != '\0') {
		char fixedbuf[128];
		char *str;

		BLI_strncpy(fixedbuf, but->drawstr + but->ofs, sizeof(fixedbuf));

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
			ul_advance = BLF_width(fstyle->uifont_id, fixedbuf);

			BLF_position(fstyle->uifont_id, rect->xmin + font_xofs + ul_advance, rect->ymin + font_yofs, 0.0f);
			BLF_draw(fstyle->uifont_id, "_", 2);

			if (fstyle->kerning == 1) {
				BLF_disable(fstyle->uifont_id, BLF_KERNING_DEFAULT);
			}
		}
	}

	/* part text right aligned */
	if (cpoin) {
		fstyle->align = UI_STYLE_TEXT_RIGHT;
		rect->xmax -= ui_but_draw_menu_icon(but) ? UI_DPI_ICON_SIZE : 5;
		uiStyleFontDraw(fstyle, rect, cpoin + 1);
		*cpoin = '|';
	}
}

/* draws text and icons for buttons */
static void widget_draw_text_icon(uiFontStyle *fstyle, uiWidgetColors *wcol, uiBut *but, rcti *rect)
{
	
	if (but == NULL) return;

	/* clip but->drawstr to fit in available space */
	if (but->editstr && but->pos >= 0) {
		ui_text_leftclip(fstyle, but, rect);
	}
	else if (ELEM4(but->type, NUM, NUMABS, NUMSLI, SLI)) {
		ui_text_label_rightclip(fstyle, but, rect);
	}
	else if (ELEM(but->type, TEX, SEARCH_MENU)) {
		ui_text_leftclip(fstyle, but, rect);
	}
	else if ((but->block->flag & UI_BLOCK_LOOP) && (but->type == BUT)) {
		ui_text_leftclip(fstyle, but, rect);
	}
	else but->ofs = 0;
	
	/* check for button text label */
	if (but->type == ICONTEXTROW) {
		widget_draw_icon(but, (BIFIconID) (but->icon + but->iconadd), 1.0f, rect);
	}
	else {
				
		if (but->type == BUT_TOGDUAL) {
			int dualset = 0;
			if (but->pointype == SHO)
				dualset = BTST(*(((short *)but->poin) + 1), but->bitnr);
			else if (but->pointype == INT)
				dualset = BTST(*(((int *)but->poin) + 1), but->bitnr);
			
			widget_draw_icon(but, ICON_DOT, dualset ? 1.0f : 0.25f, rect);
		}
		else if (but->type == MENU && (but->flag & UI_BUT_NODE_LINK)) {
			int tmp = rect->xmin;
			rect->xmin = rect->xmax - (rect->ymax - rect->ymin) - 1;
			widget_draw_icon(but, ICON_LAYER_USED, 1.0f, rect);
			rect->xmin = tmp;
		}
		
		/* If there's an icon too (made with uiDefIconTextBut) then draw the icon
		 * and offset the text label to accommodate it */
		
		if (but->flag & UI_HAS_ICON) {
			widget_draw_icon(but, but->icon + but->iconadd, 1.0f, rect);
			
			rect->xmin += (int)((float)UI_icon_get_width(but->icon + but->iconadd) * UI_DPI_ICON_FAC);
			
			if (but->editstr || (but->flag & UI_TEXT_LEFT)) 
				rect->xmin += 5;
		}
		else if ((but->flag & UI_TEXT_LEFT)) 
			rect->xmin += 5;
		
		/* always draw text for textbutton cursor */
		widget_draw_text(fstyle, wcol, but, rect);

	}
}



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
 *     float outline[3];
 *     float inner[4];
 *     float inner_sel[4];
 *     float item[3];
 *     float text[3];
 *     float text_sel[3];
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

/* tooltip colour */
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
	if (state & UI_BUT_NODE_ACTIVE) {
		char blue[4] = {86, 128, 194};
		widget_state_blend(wt->wcol.inner, blue, 0.3f);
	}
}

/* sliders use special hack which sets 'item' as inner when drawing filling */
static void widget_state_numslider(uiWidgetType *wt, int state)
{
	uiWidgetStateColors *wcol_state = wt->wcol_state;
	float blend = wcol_state->blend - 0.2f; // XXX special tweak to make sure that bar will still be visible

	/* call this for option button */
	widget_state(wt, state);
	
	/* now, set the inner-part so that it reflects state settings too */
	// TODO: maybe we should have separate settings for the blending colors used for this case?
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
static void widget_state_label(uiWidgetType *wt, int state)
{
	/* call this for option button */
	widget_state(wt, state);

	if (state & UI_SELECT)
		UI_GetThemeColor4ubv(TH_TEXT_HI, (unsigned char *)wt->wcol.text);
	else
		UI_GetThemeColor4ubv(TH_TEXT, (unsigned char *)wt->wcol.text);
	
}

/* labels use theme colors for text */
static void widget_state_option_menu(uiWidgetType *wt, int state)
{
	
	/* call this for option button */
	widget_state(wt, state);
	
	/* if not selected we get theme from menu back */
	if (state & UI_SELECT)
		UI_GetThemeColor4ubv(TH_TEXT_HI, (unsigned char *)wt->wcol.text);
	else {
		bTheme *btheme = UI_GetTheme(); /* XXX */

		copy_v3_v3_char(wt->wcol.text, btheme->tui.wcol_menu_back.text);
	}
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

/* special case, menu items */
static void widget_state_menu_item(uiWidgetType *wt, int state)
{
	wt->wcol = *(wt->wcol_theme);
	
	if (state & (UI_BUT_DISABLED | UI_BUT_INACTIVE)) {
		wt->wcol.text[0] = 0.5f * (wt->wcol.text[0] + wt->wcol.text_sel[0]);
		wt->wcol.text[1] = 0.5f * (wt->wcol.text[1] + wt->wcol.text_sel[1]);
		wt->wcol.text[2] = 0.5f * (wt->wcol.text[2] + wt->wcol.text_sel[2]);
	}
	else if (state & UI_ACTIVE) {
		copy_v4_v4_char(wt->wcol.inner, wt->wcol.inner_sel);
		copy_v3_v3_char(wt->wcol.text, wt->wcol.text_sel);
	}
}


/* ************ menu backdrop ************************* */

/* outside of rect, rad to left/bottom/right */
static void widget_softshadow(rcti *rect, int roundboxalign, float radin, float radout)
{
	uiWidgetBase wtb;
	rcti rect1 = *rect;
	float alpha, alphastep;
	int step, totvert;
	float quad_strip[WIDGET_SIZE_MAX * 2][2];
	
	/* prevent tooltips to not show round shadow */
	if (2.0f * radout > 0.2f * (rect1.ymax - rect1.ymin) )
		rect1.ymax -= 0.2f * (rect1.ymax - rect1.ymin);
	else
		rect1.ymax -= 2.0f * radout;
	
	/* inner part */
	totvert = round_box_shadow_edges(wtb.inner_v, &rect1, radin, roundboxalign & (UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT), 0.0f);

	/* inverse linear shadow alpha */
	alpha = 0.15;
	alphastep = 0.67;
	
	glEnableClientState(GL_VERTEX_ARRAY);

	for (step = 1; step <= radout; step++, alpha *= alphastep) {
		round_box_shadow_edges(wtb.outer_v, &rect1, radin, UI_CNR_ALL, (float)step);
		
		glColor4f(0.0f, 0.0f, 0.0f, alpha);

		widget_verts_to_quad_strip_open(&wtb, totvert, quad_strip);

		glVertexPointer(2, GL_FLOAT, 0, quad_strip);
		glDrawArrays(GL_QUAD_STRIP, 0, totvert * 2);
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
		rect->ymin -= 4.0;
	} 
	else if (direction == UI_TOP) {
		roundboxalign = UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT;
		rect->ymax += 4.0;
	}
	
	glEnable(GL_BLEND);
	widget_softshadow(rect, roundboxalign, 5.0f, 8.0f);
	
	round_box_edges(&wtb, roundboxalign, rect, 5.0f);
	wtb.emboss = 0;
	widgetbase_draw(&wtb, wcol);
	
	glDisable(GL_BLEND);
}


static void ui_hsv_cursor(float x, float y)
{
	
	glPushMatrix();
	glTranslatef(x, y, 0.0f);
	
	glColor3f(1.0f, 1.0f, 1.0f);
	glutil_draw_filled_arc(0.0f, M_PI * 2.0, 3.0f, 8);
	
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	glColor3f(0.0f, 0.0f, 0.0f);
	glutil_draw_lined_arc(0.0f, M_PI * 2.0, 3.0f, 12);
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
	
	glPopMatrix();
	
}

void ui_hsvcircle_vals_from_pos(float *valrad, float *valdist, rcti *rect, float mx, float my)
{
	/* duplication of code... well, simple is better now */
	float centx = (float)(rect->xmin + rect->xmax) / 2;
	float centy = (float)(rect->ymin + rect->ymax) / 2;
	float radius, dist;
	
	if (rect->xmax - rect->xmin > rect->ymax - rect->ymin)
		radius = (float)(rect->ymax - rect->ymin) / 2;
	else
		radius = (float)(rect->xmax - rect->xmin) / 2;

	mx -= centx;
	my -= centy;
	dist = sqrt(mx * mx + my * my);
	if (dist < radius)
		*valdist = dist / radius;
	else
		*valdist = 1.0f;
	
	*valrad = atan2f(mx, my) / (2.0f * (float)M_PI) + 0.5f;
}

static void ui_draw_but_HSVCIRCLE(uiBut *but, uiWidgetColors *wcol, rcti *rect)
{
	/* gouraud triangle fan */
	float radstep, ang = 0.0f;
	float centx, centy, radius, cursor_radius;
	float rgb[3], hsvo[3], hsv[3], col[3], colcent[3];
	int a, tot = 32;
	int color_profile = but->block->color_profile;
	
	if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA)
		color_profile = BLI_PR_NONE;
	
	radstep = 2.0f * (float)M_PI / (float)tot;
	centx = (float)(rect->xmin + rect->xmax) / 2;
	centy = (float)(rect->ymin + rect->ymax) / 2;
	
	if (rect->xmax - rect->xmin > rect->ymax - rect->ymin)
		radius = (float)(rect->ymax - rect->ymin) / 2;
	else
		radius = (float)(rect->xmax - rect->xmin) / 2;
	
	/* color */
	ui_get_but_vectorf(but, rgb);
	copy_v3_v3(hsv, ui_block_hsv_get(but->block));
	rgb_to_hsv_compat(rgb[0], rgb[1], rgb[2], hsv, hsv + 1, hsv + 2);
	copy_v3_v3(hsvo, hsv);
	
	/* exception: if 'lock' is set
	 * lock the value of the color wheel to 1.
	 * Useful for color correction tools where you're only interested in hue. */
	if (but->flag & UI_BUT_COLOR_LOCK)
		hsv[2] = 1.f;
	else if (color_profile)
		hsv[2] = linearrgb_to_srgb(hsv[2]);
	
	hsv_to_rgb(0.f, 0.f, hsv[2], colcent, colcent + 1, colcent + 2);
	
	glShadeModel(GL_SMOOTH);

	glBegin(GL_TRIANGLE_FAN);
	glColor3fv(colcent);
	glVertex2f(centx, centy);
	
	for (a = 0; a <= tot; a++, ang += radstep) {
		float si = sin(ang);
		float co = cos(ang);
		
		ui_hsvcircle_vals_from_pos(hsv, hsv + 1, rect, centx + co * radius, centy + si * radius);
		CLAMP(hsv[2], 0.0f, 1.0f); /* for display only */

		hsv_to_rgb(hsv[0], hsv[1], hsv[2], col, col + 1, col + 2);
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
	ang = 2.0f * (float)M_PI * hsvo[0] + 0.5f * (float)M_PI;

	if (but->flag & UI_BUT_COLOR_CUBIC)
		cursor_radius = (1.0f - powf(1.0f - hsvo[1], 3.0f));
	else
		cursor_radius = hsvo[1];

	radius = CLAMPIS(cursor_radius, 0.0f, 1.0f) * radius;
	ui_hsv_cursor(centx + cosf(-ang) * radius, centy + sinf(-ang) * radius);
}

/* ************ custom buttons, old stuff ************** */

/* draws in resolution of 20x4 colors */
void ui_draw_gradient(rcti *rect, const float hsv[3], int type, float alpha)
{
	int a;
	float h = hsv[0], s = hsv[1], v = hsv[2];
	float dx, dy, sx1, sx2, sy;
	float col0[4][3];   // left half, rect bottom to top
	float col1[4][3];   // right half, rect bottom to top

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
	}
	
	/* old below */
	
	for (dx = 0.0f; dx < 1.0f; dx += 0.05f) {
		// previous color
		copy_v3_v3(col0[0], col1[0]);
		copy_v3_v3(col0[1], col1[1]);
		copy_v3_v3(col0[2], col1[2]);
		copy_v3_v3(col0[3], col1[3]);
		
		// new color
		switch (type) {
			case UI_GRAD_SV:
				hsv_to_rgb(h, 0.0, dx,   &col1[0][0], &col1[0][1], &col1[0][2]);
				hsv_to_rgb(h, 0.333, dx, &col1[1][0], &col1[1][1], &col1[1][2]);
				hsv_to_rgb(h, 0.666, dx, &col1[2][0], &col1[2][1], &col1[2][2]);
				hsv_to_rgb(h, 1.0, dx,   &col1[3][0], &col1[3][1], &col1[3][2]);
				break;
			case UI_GRAD_HV:
				hsv_to_rgb(dx, s, 0.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
				hsv_to_rgb(dx, s, 0.333, &col1[1][0], &col1[1][1], &col1[1][2]);
				hsv_to_rgb(dx, s, 0.666, &col1[2][0], &col1[2][1], &col1[2][2]);
				hsv_to_rgb(dx, s, 1.0,   &col1[3][0], &col1[3][1], &col1[3][2]);
				break;
			case UI_GRAD_HS:
				hsv_to_rgb(dx, 0.0, v,   &col1[0][0], &col1[0][1], &col1[0][2]);
				hsv_to_rgb(dx, 0.333, v, &col1[1][0], &col1[1][1], &col1[1][2]);
				hsv_to_rgb(dx, 0.666, v, &col1[2][0], &col1[2][1], &col1[2][2]);
				hsv_to_rgb(dx, 1.0, v,   &col1[3][0], &col1[3][1], &col1[3][2]);
				break;
			case UI_GRAD_H:
				hsv_to_rgb(dx, 1.0, 1.0,   &col1[0][0], &col1[0][1], &col1[0][2]);
				copy_v3_v3(col1[1], col1[0]);
				copy_v3_v3(col1[2], col1[0]);
				copy_v3_v3(col1[3], col1[0]);
				break;
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
		
		// rect
		sx1 = rect->xmin + dx * (rect->xmax - rect->xmin);
		sx2 = rect->xmin + (dx + 0.05f) * (rect->xmax - rect->xmin);
		sy = rect->ymin;
		dy = (rect->ymax - rect->ymin) / 3.0;
		
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



static void ui_draw_but_HSVCUBE(uiBut *but, rcti *rect)
{
	float rgb[3], h, s, v;
	float x = 0.0f, y = 0.0f;
	float *hsv = ui_block_hsv_get(but->block);
	float hsvn[3];
	
	h = hsv[0];
	s = hsv[1];
	v = hsv[2];
	
	ui_get_but_vectorf(but, rgb);
	rgb_to_hsv_compat(rgb[0], rgb[1], rgb[2], &h, &s, &v);

	hsvn[0] = h;
	hsvn[1] = s;
	hsvn[2] = v;
	
	ui_draw_gradient(rect, hsvn, but->a1, 1.f);
	
	switch ((int)but->a1) {
		case UI_GRAD_SV:
			x = v; y = s; break;
		case UI_GRAD_HV:
			x = h; y = v; break;
		case UI_GRAD_HS:
			x = h; y = s; break;
		case UI_GRAD_H:
			x = h; y = 0.5; break;
		case UI_GRAD_S:
			x = s; y = 0.5; break;
		case UI_GRAD_V:
			x = v; y = 0.5; break;
	}
	
	/* cursor */
	x = rect->xmin + x * (rect->xmax - rect->xmin);
	y = rect->ymin + y * (rect->ymax - rect->ymin);
	CLAMP(x, rect->xmin + 3.0f, rect->xmax - 3.0f);
	CLAMP(y, rect->ymin + 3.0f, rect->ymax - 3.0f);
	
	ui_hsv_cursor(x, y);
	
	/* outline */
	glColor3ub(0,  0,  0);
	fdrawbox((rect->xmin), (rect->ymin), (rect->xmax), (rect->ymax));
}

/* vertical 'value' slider, using new widget code */
static void ui_draw_but_HSV_v(uiBut *but, rcti *rect)
{
	uiWidgetBase wtb;
	float rad = 0.5f * (rect->xmax - rect->xmin);
	float x, y;
	float rgb[3], hsv[3], v, range;
	int color_profile = but->block->color_profile;
	
	if (but->rnaprop && RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA)
		color_profile = BLI_PR_NONE;

	ui_get_but_vectorf(but, rgb);
	rgb_to_hsv(rgb[0], rgb[1], rgb[2], hsv, hsv + 1, hsv + 2);
	v = hsv[2];
	
	if (color_profile)
		v = linearrgb_to_srgb(v);

	/* map v from property range to [0,1] */
	range = but->softmax - but->softmin;
	v = (v - but->softmin) / range;
	
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
	x = rect->xmin + 0.5f * (rect->xmax - rect->xmin);
	y = rect->ymin + v * (rect->ymax - rect->ymin);
	CLAMP(y, rect->ymin + 3.0f, rect->ymax - 3.0f);
	
	ui_hsv_cursor(x, y);
	
}


/* ************ separator, for menus etc ***************** */
static void ui_draw_separator(rcti *rect,  uiWidgetColors *wcol)
{
	int y = rect->ymin + (rect->ymax - rect->ymin) / 2 - 1;
	unsigned char col[4];
	
	col[0] = wcol->text[0];
	col[1] = wcol->text[1];
	col[2] = wcol->text[2];
	col[3] = 7;
	
	glEnable(GL_BLEND);
	glColor4ubv(col);
	sdrawline(rect->xmin, y, rect->xmax, y);
	glDisable(GL_BLEND);
}

/* ************ button callbacks, draw ***************** */

static void widget_numbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	float rad = 0.5f * (rect->ymax - rect->ymin);
	float textofs = rad * 0.75f;

	if (state & UI_SELECT)
		SWAP(short, wcol->shadetop, wcol->shadedown);
	
	widget_init(&wtb);
	
	/* fully rounded */
	round_box_edges(&wtb, roundboxalign, rect, rad);
	
	/* decoration */
	if (!(state & UI_TEXTINPUT)) {
		widget_num_tria(&wtb.tria1, rect, 0.6f, 'l');
		widget_num_tria(&wtb.tria2, rect, 0.6f, 'r');
	}

	widgetbase_draw(&wtb, wcol);
	
	/* text space */
	rect->xmin += textofs;
	rect->xmax -= textofs;
}

//static int ui_link_bezier_points(rcti *rect, float coord_array[][2], int resol)
int ui_link_bezier_points(rcti *rect, float coord_array[][2], int resol)
{
	float dist, vec[4][2];

	vec[0][0] = rect->xmin;
	vec[0][1] = rect->ymin;
	vec[3][0] = rect->xmax;
	vec[3][1] = rect->ymax;
	
	dist = 0.5f * ABS(vec[0][0] - vec[3][0]);
	
	vec[1][0] = vec[0][0] + dist;
	vec[1][1] = vec[0][1];
	
	vec[2][0] = vec[3][0] - dist;
	vec[2][1] = vec[3][1];
	
	forward_diff_bezier(vec[0][0], vec[1][0], vec[2][0], vec[3][0], coord_array[0], resol, sizeof(float) * 2);
	forward_diff_bezier(vec[0][1], vec[1][1], vec[2][1], vec[3][1], coord_array[0] + 1, resol, sizeof(float) * 2);
	
	return 1;
}

#define LINK_RESOL  24
void ui_draw_link_bezier(rcti *rect)
{
	float coord_array[LINK_RESOL + 1][2];
	
	if (ui_link_bezier_points(rect, coord_array, LINK_RESOL)) {
		/* we can reuse the dist variable here to increment the GL curve eval amount*/
		// const float dist= 1.0f/(float)LINK_RESOL; // UNUSED

		glEnable(GL_BLEND);
		glEnable(GL_LINE_SMOOTH);

		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(2, GL_FLOAT, 0, coord_array);
		glDrawArrays(GL_LINE_STRIP, 0, LINK_RESOL);
		glDisableClientState(GL_VERTEX_ARRAY);

		glDisable(GL_BLEND);
		glDisable(GL_LINE_SMOOTH);

	}
}

/* function in use for buttons and for view2d sliders */
void uiWidgetScrollDraw(uiWidgetColors *wcol, rcti *rect, rcti *slider, int state)
{
	uiWidgetBase wtb;
	float rad;
	int horizontal;
	short outline = 0;

	widget_init(&wtb);

	/* determine horizontal/vertical */
	horizontal = (rect->xmax - rect->xmin > rect->ymax - rect->ymin);
	
	if (horizontal)
		rad = 0.5f * (rect->ymax - rect->ymin);
	else
		rad = 0.5f * (rect->xmax - rect->xmin);
	
	wtb.shadedir = (horizontal) ? 1 : 0;
	
	/* draw back part, colors swapped and shading inverted */
	if (horizontal)
		SWAP(short, wcol->shadetop, wcol->shadedown);
	
	round_box_edges(&wtb, UI_CNR_ALL, rect, rad);
	widgetbase_draw(&wtb, wcol);
	
	/* slider */
	if (slider->xmax - slider->xmin < 2 || slider->ymax - slider->ymin < 2) ;
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
		if (state & UI_SCROLL_NO_OUTLINE)	
			SWAP(short, outline, wtb.outline);
		
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
		
		if (state & UI_SCROLL_NO_OUTLINE)
			SWAP(short, outline, wtb.outline);
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
	size = MAX2(size, 2);
	
	/* position */
	rect1 = *rect;

	/* determine horizontal/vertical */
	horizontal = (rect->xmax - rect->xmin > rect->ymax - rect->ymin);
	
	if (horizontal) {
		fac = (rect->xmax - rect->xmin) / (size);
		rect1.xmin = rect1.xmin + ceilf(fac * ((float)value - but->softmin));
		rect1.xmax = rect1.xmin + ceilf(fac * (but->a1 - but->softmin));

		/* ensure minimium size */
		min = rect->ymax - rect->ymin;

		if (rect1.xmax - rect1.xmin < min) {
			rect1.xmax = rect1.xmin + min;

			if (rect1.xmax > rect->xmax) {
				rect1.xmax = rect->xmax;
				rect1.xmin = MAX2(rect1.xmax - min, rect->xmin);
			}
		}
	}
	else {
		fac = (rect->ymax - rect->ymin) / (size);
		rect1.ymax = rect1.ymax - ceilf(fac * ((float)value - but->softmin));
		rect1.ymin = rect1.ymax - ceilf(fac * (but->a1 - but->softmin));

		/* ensure minimium size */
		min = rect->xmax - rect->xmin;

		if (rect1.ymax - rect1.ymin < min) {
			rect1.ymax = rect1.ymin + min;

			if (rect1.ymax > rect->ymax) {
				rect1.ymax = rect->ymax;
				rect1.ymin = MAX2(rect1.ymax - min, rect->ymin);
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
	rect_prog.ymax = rect_prog.ymin + 4;
	rect_bar.ymax = rect_bar.ymin + 4;
	
	w = value * (rect_prog.xmax - rect_prog.xmin);
	
	/* ensure minimium size */
	min = rect_prog.ymax - rect_prog.ymin;
	w = MAX2(w, min);
	
	rect_bar.xmax = rect_bar.xmin + w;
		
	uiWidgetScrollDraw(wcol, &rect_prog, &rect_bar, UI_SCROLL_NO_OUTLINE);
	
	/* raise text a bit */
	rect->ymin += 6;
	rect->xmin -= 6;
}

static void widget_link(uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	
	if (but->flag & UI_SELECT) {
		rcti rectlink;
		
		UI_ThemeColor(TH_TEXT_HI);
		
		rectlink.xmin = (rect->xmin + rect->xmax) / 2;
		rectlink.ymin = (rect->ymin + rect->ymax) / 2;
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
	offs = 0.5f * (rect->ymax - rect->ymin);
	toffs = offs * 0.75f;
	round_box_edges(&wtb, roundboxalign, rect, offs);

	wtb.outline = 0;
	widgetbase_draw(&wtb, wcol);
	
	/* draw left/right parts only when not in text editing */
	if (!(state & UI_TEXTINPUT)) {
		
		/* slider part */
		copy_v3_v3_char(outline, wcol->outline);
		copy_v3_v3_char(wcol->outline, wcol->item);
		copy_v3_v3_char(wcol->inner, wcol->item);

		if (!(state & UI_SELECT))
			SWAP(short, wcol->shadetop, wcol->shadedown);
		
		rect1 = *rect;
		
		value = ui_get_but_val(but);
		fac = ((float)value - but->softmin) * (rect1.xmax - rect1.xmin - offs) / (but->softmax - but->softmin);
		
		/* left part of slider, always rounded */
		rect1.xmax = rect1.xmin + ceil(offs + 1.0f);
		round_box_edges(&wtb1, roundboxalign & ~(UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT), &rect1, offs);
		wtb1.outline = 0;
		widgetbase_draw(&wtb1, wcol);
		
		/* right part of slider, interpolate roundness */
		rect1.xmax = rect1.xmin + fac + offs;
		rect1.xmin +=  floor(offs - 1.0f);
		if (rect1.xmax + offs > rect->xmax)
			offs *= (rect1.xmax + offs - rect->xmax) / offs;
		else 
			offs = 0.0f;
		round_box_edges(&wtb1, roundboxalign & ~(UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT), &rect1, offs);
		
		widgetbase_draw(&wtb1, wcol);
		copy_v3_v3_char(wcol->outline, outline);
		
		if (!(state & UI_SELECT))
			SWAP(short, wcol->shadetop, wcol->shadedown);
	}
	
	/* outline */
	wtb.outline = 1;
	wtb.inner = 0;
	widgetbase_draw(&wtb, wcol);
	
	/* text space */
	rect->xmin += toffs;
	rect->xmax -= toffs;
}

/* I think 3 is sufficient border to indicate keyed status */
#define SWATCH_KEYED_BORDER 3

static void widget_swatch(uiBut *but, uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	float col[4];
	int color_profile = but->block->color_profile;
	
	col[3] = 1.0f;

	if (but->rnaprop) {
		if (RNA_property_subtype(but->rnaprop) == PROP_COLOR_GAMMA)
			color_profile = BLI_PR_NONE;

		if (RNA_property_array_length(&but->rnapoin, but->rnaprop) == 4) {
			col[3] = RNA_property_float_get_index(&but->rnapoin, but->rnaprop, 3);
		}
	}
	
	widget_init(&wtb);
	
	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, 5.0f);
		
	ui_get_but_vectorf(but, col);
	
	if (state & (UI_BUT_ANIMATED | UI_BUT_ANIMATED_KEY | UI_BUT_DRIVEN | UI_BUT_REDALERT)) {
		// draw based on state - color for keyed etc
		widgetbase_draw(&wtb, wcol);
		
		// inset to draw swatch color
		rect->xmin += SWATCH_KEYED_BORDER;
		rect->xmax -= SWATCH_KEYED_BORDER;
		rect->ymin += SWATCH_KEYED_BORDER;
		rect->ymax -= SWATCH_KEYED_BORDER;
		
		round_box_edges(&wtb, roundboxalign, rect, 5.0f);
	}
	
	if (color_profile)
		linearrgb_to_srgb_v3_v3(col, col);
	
	rgba_float_to_uchar((unsigned char *)wcol->inner, col);

	wcol->shaded = 0;
	wcol->alpha_check = (wcol->inner[3] < 255);

	widgetbase_draw(&wtb, wcol);
	
}

static void widget_icon_has_anim(uiBut *UNUSED(but), uiWidgetColors *wcol, rcti *rect, int state, int UNUSED(roundboxalign))
{
	if (state & (UI_BUT_ANIMATED | UI_BUT_ANIMATED_KEY | UI_BUT_DRIVEN | UI_BUT_REDALERT)) {
		uiWidgetBase wtb;
	
		widget_init(&wtb);
		wtb.outline = 0;
		
		/* rounded */
		round_box_edges(&wtb, UI_CNR_ALL, rect, 10.0f);
		widgetbase_draw(&wtb, wcol);
	}	
}


static void widget_textbut(uiWidgetColors *wcol, rcti *rect, int state, int roundboxalign)
{
	uiWidgetBase wtb;
	
	if (state & UI_SELECT)
		SWAP(short, wcol->shadetop, wcol->shadedown);
	
	widget_init(&wtb);
	
	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, 4.0f);
	
	widgetbase_draw(&wtb, wcol);

}


static void widget_menubut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	
	widget_init(&wtb);
	
	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, 4.0f);
	
	/* decoration */
	widget_menu_trias(&wtb.tria1, rect);
	
	widgetbase_draw(&wtb, wcol);
	
	/* text space */
	rect->xmax -= (rect->ymax - rect->ymin);
}

static void widget_menuiconbut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	
	widget_init(&wtb);
	
	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, 4.0f);
	
	/* decoration */
	widgetbase_draw(&wtb, wcol);
}

static void widget_menunodebut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	/* silly node link button hacks */
	uiWidgetBase wtb;
	uiWidgetColors wcol_backup = *wcol;
	
	widget_init(&wtb);
	
	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, 4.0f);

	wcol->inner[0] += 15;
	wcol->inner[1] += 15;
	wcol->inner[2] += 15;
	wcol->outline[0] += 15;
	wcol->outline[1] += 15;
	wcol->outline[2] += 15;
	
	/* decoration */
	widgetbase_draw(&wtb, wcol);
	*wcol = wcol_backup;
}

static void widget_pulldownbut(uiWidgetColors *wcol, rcti *rect, int state, int UNUSED(roundboxalign))
{
	if (state & UI_ACTIVE) {
		uiWidgetBase wtb;
		float rad = 0.5f * (rect->ymax - rect->ymin); // 4.0f
		
		widget_init(&wtb);
		
		/* half rounded */
		round_box_edges(&wtb, UI_CNR_ALL, rect, rad);
		
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

static void widget_list_itembut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int UNUSED(roundboxalign))
{
	uiWidgetBase wtb;
	
	widget_init(&wtb);
	
	/* rounded, but no outline */
	wtb.outline = 0;
	round_box_edges(&wtb, UI_CNR_ALL, rect, 4.0f);
	
	widgetbase_draw(&wtb, wcol);
}

static void widget_optionbut(uiWidgetColors *wcol, rcti *rect, int state, int UNUSED(roundboxalign))
{
	uiWidgetBase wtb;
	rcti recttemp = *rect;
	int delta;
	
	widget_init(&wtb);
	
	/* square */
	recttemp.xmax = recttemp.xmin + (recttemp.ymax - recttemp.ymin);
	
	/* smaller */
	delta = 1 + (recttemp.ymax - recttemp.ymin) / 8;
	recttemp.xmin += delta;
	recttemp.ymin += delta;
	recttemp.xmax -= delta;
	recttemp.ymax -= delta;
	
	/* half rounded */
	round_box_edges(&wtb, UI_CNR_ALL, &recttemp, 4.0f);
	
	/* decoration */
	if (state & UI_SELECT) {
		widget_check_trias(&wtb.tria1, &recttemp);
	}
	
	widgetbase_draw(&wtb, wcol);
	
	/* text space */
	rect->xmin += (rect->ymax - rect->ymin) * 0.7 + delta;
}


static void widget_radiobut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	
	widget_init(&wtb);
	
	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, 4.0f);
	
	widgetbase_draw(&wtb, wcol);

}

static void widget_box(uiBut *but, uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
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
	round_box_edges(&wtb, roundboxalign, rect, 4.0f);
	
	widgetbase_draw(&wtb, wcol);
	
	/* store the box bg as gl clearcolor, to retrieve later when drawing semi-transparent rects
	 * over the top to indicate disabled buttons */
	/* XXX, this doesnt work right since the color applies to buttons outside the box too. */
	glClearColor(wcol->inner[0] / 255.0, wcol->inner[1] / 255.0, wcol->inner[2] / 255.0, 1.0);
	
	copy_v3_v3_char(wcol->inner, old_col);
}

static void widget_but(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	
	widget_init(&wtb);
	
	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, 4.0f);
		
	widgetbase_draw(&wtb, wcol);

}

static void widget_roundbut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	float rad = 5.0f; //0.5f*(rect->ymax - rect->ymin);
	
	widget_init(&wtb);
	
	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, rad);

	widgetbase_draw(&wtb, wcol);
}

static void widget_draw_extra_mask(const bContext *C, uiBut *but, uiWidgetType *wt, rcti *rect)
{
	uiWidgetBase wtb;
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
		
		round_box__edges(&wtb, UI_CNR_ALL, rect, 0.0f, 4.0);
		widgetbase_outline(&wtb);
	}
	
	/* outline */
	round_box_edges(&wtb, UI_CNR_ALL, rect, 5.0f);
	wtb.outline = 1;
	wtb.inner = 0;
	widgetbase_draw(&wtb, &wt->wcol);
	
}


static void widget_disabled(rcti *rect)
{
	float col[4];
	
	glEnable(GL_BLEND);
	
	/* can't use theme TH_BACK or TH_PANEL... undefined */
	glGetFloatv(GL_COLOR_CLEAR_VALUE, col);
	glColor4f(col[0], col[1], col[2], 0.5f);

	/* need -1 and +1 to make it work right for aligned buttons,
	 * but problem may be somewhere else? */
	glRectf(rect->xmin - 1, rect->ymin - 1, rect->xmax, rect->ymax + 1);
	
	glDisable(GL_BLEND);
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
	}
	
	return &wt;
}


static int widget_roundbox_set(uiBut *but, rcti *rect)
{
	/* alignment */
	if (but->flag & UI_BUT_ALIGN) {
		
		if (but->flag & UI_BUT_ALIGN_TOP)
			rect->ymax += 1;
		if (but->flag & UI_BUT_ALIGN_LEFT)
			rect->xmin -= 1;
		
		switch (but->flag & UI_BUT_ALIGN) {
			case UI_BUT_ALIGN_TOP:
				return UI_CNR_BOTTOM_LEFT | UI_CNR_BOTTOM_RIGHT;
			case UI_BUT_ALIGN_DOWN:
				return UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT;
			case UI_BUT_ALIGN_LEFT:
				return UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT;
			case UI_BUT_ALIGN_RIGHT:
				return UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT;
			case UI_BUT_ALIGN_DOWN | UI_BUT_ALIGN_RIGHT:
				return UI_CNR_TOP_LEFT;
			case UI_BUT_ALIGN_DOWN | UI_BUT_ALIGN_LEFT:
				return UI_CNR_TOP_RIGHT;
			case UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_RIGHT:
				return UI_CNR_BOTTOM_LEFT;
			case UI_BUT_ALIGN_TOP | UI_BUT_ALIGN_LEFT:
				return UI_CNR_BOTTOM_RIGHT;
			default:
				return 0;
		}
	}

	return UI_CNR_ALL;
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
			case SEPR:
				ui_draw_separator(rect, &tui->wcol_menu_item);
				break;
				
			default:
				wt = widget_type(UI_WTYPE_MENU_ITEM);
		}
	}
	else if (but->dt == UI_EMBOSSN) {
		/* "nothing" */
		wt = widget_type(UI_WTYPE_ICON);
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
				break;
				
			case BUT:
				wt = widget_type(UI_WTYPE_EXEC);
				break;

			case NUM:
				wt = widget_type(UI_WTYPE_NUMBER);
				break;
				
			case NUMSLI:
			case HSVSLI:
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
				
			case SEARCH_MENU:
				wt = widget_type(UI_WTYPE_NAME);
				if (but->block->flag & UI_BLOCK_LOOP)
					wt->wcol_theme = &btheme->tui.wcol_menu_back;
				break;
				
			case TOGBUT:
			case TOG:
			case TOGN:
			case TOG3:
				wt = widget_type(UI_WTYPE_TOGGLE);
				break;
				
			case OPTION:
			case OPTIONN:
				if (!(but->flag & UI_HAS_ICON)) {
					wt = widget_type(UI_WTYPE_OPTION);
					but->flag |= UI_TEXT_LEFT;
				}
				else
					wt = widget_type(UI_WTYPE_TOGGLE);
				
				/* option buttons have strings outside, on menus use different colors */
				if (but->block->flag & UI_BLOCK_LOOP)
					wt->state = widget_state_option_menu;
				
				break;
				
			case MENU:
			case BLOCK:
			case ICONTEXTROW:
				if (but->flag & UI_BUT_NODE_LINK)
					wt = widget_type(UI_WTYPE_MENU_NODE_LINK);
				else if (!but->str[0] && but->icon)
					wt = widget_type(UI_WTYPE_MENU_ICON_RADIO);
				else
					wt = widget_type(UI_WTYPE_MENU_RADIO);
				break;
				
			case PULLDOWN:
				wt = widget_type(UI_WTYPE_PULLDOWN);
				break;
			
			case BUTM:
				wt = widget_type(UI_WTYPE_MENU_ITEM);
				break;
				
			case COL:
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
				if (but->a1 == UI_GRAD_V_ALT) // vertical V slider, uses new widget draw now
					ui_draw_but_HSV_v(but, rect);
				else  // other HSV pickers...
					ui_draw_but_HSVCUBE(but, rect);
				break;
				
			case HSVCIRCLE:
				ui_draw_but_HSVCIRCLE(but, &tui->wcol_regular, rect);
				break;
				
			case BUT_COLORBAND:
				ui_draw_but_COLORBAND(but, &tui->wcol_regular, rect);
				break;
				
			case BUT_NORMAL:
				ui_draw_but_NORMAL(but, &tui->wcol_regular, rect);
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

			case TRACKPREVIEW:
				ui_draw_but_TRACKPREVIEW(ar, but, &tui->wcol_regular, rect);
				break;

			default:
				wt = widget_type(UI_WTYPE_REGULAR);
		}
	}
	
	if (wt) {
		rcti disablerect = *rect; /* rect gets clipped smaller for text */
		int roundboxalign, state;
		
		roundboxalign = widget_roundbox_set(but, rect);

		state = but->flag;
		if (but->editstr) state |= UI_TEXTINPUT;
		
		wt->state(wt, state);
		if (wt->custom)
			wt->custom(but, &wt->wcol, rect, state, roundboxalign);
		else if (wt->draw)
			wt->draw(&wt->wcol, rect, state, roundboxalign);
		wt->text(fstyle, &wt->wcol, but, rect);
		
		if (state & (UI_BUT_DISABLED | UI_BUT_INACTIVE))
			if (but->dt != UI_EMBOSSP)
				widget_disabled(&disablerect);
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
			UI_DrawTriIcon((rect->xmax + rect->xmin) / 2, rect->ymax - 8, 't');
		}
		if (block->flag & UI_BLOCK_CLIPBOTTOM) {
			/* XXX no scaling for UI here yet */
			glColor3ubv((unsigned char *)wt->wcol.text);
			UI_DrawTriIcon((rect->xmax + rect->xmin) / 2, rect->ymin + 10, 'v');
		}
	}	
}

uiWidgetColors *ui_tooltip_get_theme(void) {
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
	widget_softshadow(rect, UI_CNR_ALL, 5.0f, 8.0f);
	glDisable(GL_BLEND);

	wt->state(wt, 0);
	if (block)
		wt->draw(&wt->wcol, rect, block->flag, UI_CNR_ALL);
	else
		wt->draw(&wt->wcol, rect, 0, UI_CNR_ALL);
	
}


/* helper call to draw a menu item without button */
/* state: UI_ACTIVE or 0 */
void ui_draw_menu_item(uiFontStyle *fstyle, rcti *rect, const char *name, int iconid, int state)
{
	uiWidgetType *wt = widget_type(UI_WTYPE_MENU_ITEM);
	rcti _rect = *rect;
	char *cpoin;
	
	wt->state(wt, state);
	wt->draw(&wt->wcol, rect, 0, 0);
	
	uiStyleFontSet(fstyle);
	fstyle->align = UI_STYLE_TEXT_LEFT;
	
	/* text location offset */
	rect->xmin += 5;
	if (iconid) rect->xmin += UI_DPI_ICON_SIZE;

	/* cut string in 2 parts? */
	cpoin = strchr(name, '|');
	if (cpoin) {
		*cpoin = 0;
		rect->xmax -= BLF_width(fstyle->uifont_id, cpoin + 1) + 10;
	}
	
	glColor3ubv((unsigned char *)wt->wcol.text);
	uiStyleFontDraw(fstyle, rect, name);
	
	/* part text right aligned */
	if (cpoin) {
		fstyle->align = UI_STYLE_TEXT_RIGHT;
		rect->xmax = _rect.xmax - 5;
		uiStyleFontDraw(fstyle, rect, cpoin + 1);
		*cpoin = '|';
	}
	
	/* restore rect, was messed with */
	*rect = _rect;

	if (iconid) {
		int xs = rect->xmin + 4;
		int ys = 1 + (rect->ymin + rect->ymax - UI_DPI_ICON_SIZE) / 2;
		glEnable(GL_BLEND);
		UI_icon_draw_aspect(xs, ys, iconid, 1.2f, 0.5f); /* XXX scale weak get from fstyle? */
		glDisable(GL_BLEND);
	}
}

void ui_draw_preview_item(uiFontStyle *fstyle, rcti *rect, const char *name, int iconid, int state)
{
	rcti trect = *rect, bg_rect;
	float font_dims[2] = {0.0f, 0.0f};
	uiWidgetType *wt = widget_type(UI_WTYPE_MENU_ITEM);
	unsigned char bg_col[3];
	
	wt->state(wt, state);
	wt->draw(&wt->wcol, rect, 0, 0);
	
	widget_draw_preview(iconid, 1.0f, rect);
	
	BLF_width_and_height(fstyle->uifont_id, name, &font_dims[0], &font_dims[1]);

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

	UI_GetThemeColor3ubv(TH_BUTBACK, bg_col);
	glColor4ubv((unsigned char *)wt->wcol.item);
	glEnable(GL_BLEND);
	glRecti(bg_rect.xmin, bg_rect.ymin, bg_rect.xmax, bg_rect.ymax);
	glDisable(GL_BLEND);
	
	if (state == UI_ACTIVE)
		glColor3ubv((unsigned char *)wt->wcol.text);
	else
		glColor3ubv((unsigned char *)wt->wcol.text_sel);

	uiStyleFontDraw(fstyle, &trect, name);
}

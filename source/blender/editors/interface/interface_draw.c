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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_draw.c
 *  \ingroup edinterface
 */


#include <math.h>
#include <string.h>

#include "DNA_color_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_movieclip_types.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_colortools.h"
#include "BKE_node.h"
#include "BKE_texture.h"
#include "BKE_tracking.h"


#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLF_api.h"

#include "UI_interface.h"

/* own include */
#include "interface_intern.h"

static int roundboxtype = UI_CNR_ALL;

void uiSetRoundBox(int type)
{
	/* Not sure the roundbox function is the best place to change this
	 * if this is undone, its not that big a deal, only makes curves edges
	 * square for the  */
	roundboxtype = type;
	
}

int uiGetRoundBox(void)
{
	return roundboxtype;
}

void uiDrawBox(int mode, float minx, float miny, float maxx, float maxy, float rad)
{
	float vec[7][2] = {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
	                   {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	int a;
	
	/* mult */
	for (a = 0; a < 7; a++) {
		mul_v2_fl(vec[a], rad);
	}

	glBegin(mode);

	/* start with corner right-bottom */
	if (roundboxtype & UI_CNR_BOTTOM_RIGHT) {
		glVertex2f(maxx - rad, miny);
		for (a = 0; a < 7; a++) {
			glVertex2f(maxx - rad + vec[a][0], miny + vec[a][1]);
		}
		glVertex2f(maxx, miny + rad);
	}
	else {
		glVertex2f(maxx, miny);
	}
	
	/* corner right-top */
	if (roundboxtype & UI_CNR_TOP_RIGHT) {
		glVertex2f(maxx, maxy - rad);
		for (a = 0; a < 7; a++) {
			glVertex2f(maxx - vec[a][1], maxy - rad + vec[a][0]);
		}
		glVertex2f(maxx - rad, maxy);
	}
	else {
		glVertex2f(maxx, maxy);
	}
	
	/* corner left-top */
	if (roundboxtype & UI_CNR_TOP_LEFT) {
		glVertex2f(minx + rad, maxy);
		for (a = 0; a < 7; a++) {
			glVertex2f(minx + rad - vec[a][0], maxy - vec[a][1]);
		}
		glVertex2f(minx, maxy - rad);
	}
	else {
		glVertex2f(minx, maxy);
	}
	
	/* corner left-bottom */
	if (roundboxtype & UI_CNR_BOTTOM_LEFT) {
		glVertex2f(minx, miny + rad);
		for (a = 0; a < 7; a++) {
			glVertex2f(minx + vec[a][1], miny + rad - vec[a][0]);
		}
		glVertex2f(minx + rad, miny);
	}
	else {
		glVertex2f(minx, miny);
	}
	
	glEnd();
}

static void round_box_shade_col(const float col1[3], float const col2[3], const float fac)
{
	float col[3];

	col[0] = (fac * col1[0] + (1.0f - fac) * col2[0]);
	col[1] = (fac * col1[1] + (1.0f - fac) * col2[1]);
	col[2] = (fac * col1[2] + (1.0f - fac) * col2[2]);
	glColor3fv(col);
}

/* linear horizontal shade within button or in outline */
/* view2d scrollers use it */
void uiDrawBoxShade(int mode, float minx, float miny, float maxx, float maxy, float rad, float shadetop, float shadedown)
{
	float vec[7][2] = {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
	                   {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	const float div = maxy - miny;
	const float idiv = 1.0f / div;
	float coltop[3], coldown[3], color[4];
	int a;
	
	/* mult */
	for (a = 0; a < 7; a++) {
		mul_v2_fl(vec[a], rad);
	}
	/* get current color, needs to be outside of glBegin/End */
	glGetFloatv(GL_CURRENT_COLOR, color);

	/* 'shade' defines strength of shading */
	coltop[0]  = min_ff(1.0f, color[0] + shadetop);
	coltop[1]  = min_ff(1.0f, color[1] + shadetop);
	coltop[2]  = min_ff(1.0f, color[2] + shadetop);
	coldown[0] = max_ff(0.0f, color[0] + shadedown);
	coldown[1] = max_ff(0.0f, color[1] + shadedown);
	coldown[2] = max_ff(0.0f, color[2] + shadedown);

	glShadeModel(GL_SMOOTH);
	glBegin(mode);

	/* start with corner right-bottom */
	if (roundboxtype & UI_CNR_BOTTOM_RIGHT) {
		
		round_box_shade_col(coltop, coldown, 0.0);
		glVertex2f(maxx - rad, miny);
		
		for (a = 0; a < 7; a++) {
			round_box_shade_col(coltop, coldown, vec[a][1] * idiv);
			glVertex2f(maxx - rad + vec[a][0], miny + vec[a][1]);
		}
		
		round_box_shade_col(coltop, coldown, rad * idiv);
		glVertex2f(maxx, miny + rad);
	}
	else {
		round_box_shade_col(coltop, coldown, 0.0);
		glVertex2f(maxx, miny);
	}
	
	/* corner right-top */
	if (roundboxtype & UI_CNR_TOP_RIGHT) {
		
		round_box_shade_col(coltop, coldown, (div - rad) * idiv);
		glVertex2f(maxx, maxy - rad);
		
		for (a = 0; a < 7; a++) {
			round_box_shade_col(coltop, coldown, (div - rad + vec[a][1]) * idiv);
			glVertex2f(maxx - vec[a][1], maxy - rad + vec[a][0]);
		}
		round_box_shade_col(coltop, coldown, 1.0);
		glVertex2f(maxx - rad, maxy);
	}
	else {
		round_box_shade_col(coltop, coldown, 1.0);
		glVertex2f(maxx, maxy);
	}
	
	/* corner left-top */
	if (roundboxtype & UI_CNR_TOP_LEFT) {
		
		round_box_shade_col(coltop, coldown, 1.0);
		glVertex2f(minx + rad, maxy);
		
		for (a = 0; a < 7; a++) {
			round_box_shade_col(coltop, coldown, (div - vec[a][1]) * idiv);
			glVertex2f(minx + rad - vec[a][0], maxy - vec[a][1]);
		}
		
		round_box_shade_col(coltop, coldown, (div - rad) * idiv);
		glVertex2f(minx, maxy - rad);
	}
	else {
		round_box_shade_col(coltop, coldown, 1.0);
		glVertex2f(minx, maxy);
	}
	
	/* corner left-bottom */
	if (roundboxtype & UI_CNR_BOTTOM_LEFT) {
		
		round_box_shade_col(coltop, coldown, rad * idiv);
		glVertex2f(minx, miny + rad);
		
		for (a = 0; a < 7; a++) {
			round_box_shade_col(coltop, coldown, (rad - vec[a][1]) * idiv);
			glVertex2f(minx + vec[a][1], miny + rad - vec[a][0]);
		}
		
		round_box_shade_col(coltop, coldown, 0.0);
		glVertex2f(minx + rad, miny);
	}
	else {
		round_box_shade_col(coltop, coldown, 0.0);
		glVertex2f(minx, miny);
	}
	
	glEnd();
	glShadeModel(GL_FLAT);
}

/* linear vertical shade within button or in outline */
/* view2d scrollers use it */
void uiDrawBoxVerticalShade(int mode, float minx, float miny, float maxx, float maxy,
                            float rad, float shadeLeft, float shadeRight)
{
	float vec[7][2] = {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
	                   {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	const float div = maxx - minx;
	const float idiv = 1.0f / div;
	float colLeft[3], colRight[3], color[4];
	int a;
	
	/* mult */
	for (a = 0; a < 7; a++) {
		mul_v2_fl(vec[a], rad);
	}
	/* get current color, needs to be outside of glBegin/End */
	glGetFloatv(GL_CURRENT_COLOR, color);

	/* 'shade' defines strength of shading */
	colLeft[0]  = min_ff(1.0f, color[0] + shadeLeft);
	colLeft[1]  = min_ff(1.0f, color[1] + shadeLeft);
	colLeft[2]  = min_ff(1.0f, color[2] + shadeLeft);
	colRight[0] = max_ff(0.0f, color[0] + shadeRight);
	colRight[1] = max_ff(0.0f, color[1] + shadeRight);
	colRight[2] = max_ff(0.0f, color[2] + shadeRight);

	glShadeModel(GL_SMOOTH);
	glBegin(mode);

	/* start with corner right-bottom */
	if (roundboxtype & UI_CNR_BOTTOM_RIGHT) {
		round_box_shade_col(colLeft, colRight, 0.0);
		glVertex2f(maxx - rad, miny);
		
		for (a = 0; a < 7; a++) {
			round_box_shade_col(colLeft, colRight, vec[a][0] * idiv);
			glVertex2f(maxx - rad + vec[a][0], miny + vec[a][1]);
		}
		
		round_box_shade_col(colLeft, colRight, rad * idiv);
		glVertex2f(maxx, miny + rad);
	}
	else {
		round_box_shade_col(colLeft, colRight, 0.0);
		glVertex2f(maxx, miny);
	}
	
	/* corner right-top */
	if (roundboxtype & UI_CNR_TOP_RIGHT) {
		round_box_shade_col(colLeft, colRight, 0.0);
		glVertex2f(maxx, maxy - rad);
		
		for (a = 0; a < 7; a++) {
			
			round_box_shade_col(colLeft, colRight, (div - rad - vec[a][0]) * idiv);
			glVertex2f(maxx - vec[a][1], maxy - rad + vec[a][0]);
		}
		round_box_shade_col(colLeft, colRight, (div - rad) * idiv);
		glVertex2f(maxx - rad, maxy);
	}
	else {
		round_box_shade_col(colLeft, colRight, 0.0);
		glVertex2f(maxx, maxy);
	}
	
	/* corner left-top */
	if (roundboxtype & UI_CNR_TOP_LEFT) {
		round_box_shade_col(colLeft, colRight, (div - rad) * idiv);
		glVertex2f(minx + rad, maxy);
		
		for (a = 0; a < 7; a++) {
			round_box_shade_col(colLeft, colRight, (div - rad + vec[a][0]) * idiv);
			glVertex2f(minx + rad - vec[a][0], maxy - vec[a][1]);
		}
		
		round_box_shade_col(colLeft, colRight, 1.0);
		glVertex2f(minx, maxy - rad);
	}
	else {
		round_box_shade_col(colLeft, colRight, 1.0);
		glVertex2f(minx, maxy);
	}
	
	/* corner left-bottom */
	if (roundboxtype & UI_CNR_BOTTOM_LEFT) {
		round_box_shade_col(colLeft, colRight, 1.0);
		glVertex2f(minx, miny + rad);
		
		for (a = 0; a < 7; a++) {
			round_box_shade_col(colLeft, colRight, (vec[a][0]) * idiv);
			glVertex2f(minx + vec[a][1], miny + rad - vec[a][0]);
		}
		
		round_box_shade_col(colLeft, colRight, 1.0);
		glVertex2f(minx + rad, miny);
	}
	else {
		round_box_shade_col(colLeft, colRight, 1.0);
		glVertex2f(minx, miny);
	}
	
	glEnd();
	glShadeModel(GL_FLAT);
}

/* plain antialiased unfilled rectangle */
void uiRoundRect(float minx, float miny, float maxx, float maxy, float rad)
{
	float color[4];
	
	if (roundboxtype & UI_RB_ALPHA) {
		glGetFloatv(GL_CURRENT_COLOR, color);
		color[3] = 0.5;
		glColor4fv(color);
		glEnable(GL_BLEND);
	}
	
	/* set antialias line */
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);

	uiDrawBox(GL_LINE_LOOP, minx, miny, maxx, maxy, rad);

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

/* (old, used in outliner) plain antialiased filled box */
void uiRoundBox(float minx, float miny, float maxx, float maxy, float rad)
{
	ui_draw_anti_roundbox(GL_POLYGON, minx, miny, maxx, maxy, rad, roundboxtype & UI_RB_ALPHA);
}

/* ************** SPECIAL BUTTON DRAWING FUNCTIONS ************* */

void ui_draw_but_IMAGE(ARegion *UNUSED(ar), uiBut *but, uiWidgetColors *UNUSED(wcol), const rcti *rect)
{
#ifdef WITH_HEADLESS
	(void)rect;
	(void)but;
#else
	ImBuf *ibuf = (ImBuf *)but->poin;
	//GLint scissor[4];
	int w, h;

	if (!ibuf) return;
	
	w = BLI_rcti_size_x(rect);
	h = BLI_rcti_size_y(rect);
	
	/* scissor doesn't seem to be doing the right thing...? */
#if 0
	//glColor4f(1.0, 0.f, 0.f, 1.f);
	//fdrawbox(rect->xmin, rect->ymin, rect->xmax, rect->ymax)

	/* prevent drawing outside widget area */
	glGetIntegerv(GL_SCISSOR_BOX, scissor);
	glScissor(ar->winrct.xmin + rect->xmin, ar->winrct.ymin + rect->ymin, w, h);
#endif
	
	glEnable(GL_BLEND);
	glColor4f(0.0, 0.0, 0.0, 0.0);
	
	if (w != ibuf->x || h != ibuf->y) {
		float facx = (float)w / (float)ibuf->x;
		float facy = (float)h / (float)ibuf->y;
		glPixelZoom(facx, facy);
	}
	glaDrawPixelsAuto((float)rect->xmin, (float)rect->ymin, ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE, GL_NEAREST, ibuf->rect);
	
	glPixelZoom(1.0f, 1.0f);
	
	glDisable(GL_BLEND);
	
#if 0
	// restore scissortest
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
#endif
	
#endif
}

static void draw_scope_end(const rctf *rect, GLint *scissor)
{
	float scaler_x1, scaler_x2;
	
	/* restore scissortest */
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	/* scale widget */
	scaler_x1 = rect->xmin + BLI_rctf_size_x(rect) / 2 - SCOPE_RESIZE_PAD;
	scaler_x2 = rect->xmin + BLI_rctf_size_x(rect) / 2 + SCOPE_RESIZE_PAD;
	
	glColor4f(0.f, 0.f, 0.f, 0.25f);
	fdrawline(scaler_x1, rect->ymin - 4, scaler_x2, rect->ymin - 4);
	fdrawline(scaler_x1, rect->ymin - 7, scaler_x2, rect->ymin - 7);
	glColor4f(1.f, 1.f, 1.f, 0.25f);
	fdrawline(scaler_x1, rect->ymin - 5, scaler_x2, rect->ymin - 5);
	fdrawline(scaler_x1, rect->ymin - 8, scaler_x2, rect->ymin - 8);
	
	/* outline */
	glColor4f(0.f, 0.f, 0.f, 0.5f);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_LINE_LOOP, rect->xmin - 1, rect->ymin, rect->xmax + 1, rect->ymax + 1, 3.0f);
}

static void histogram_draw_one(float r, float g, float b, float alpha,
                               float x, float y, float w, float h, float *data, int res, const bool is_line)
{
	int i;
	
	if (is_line) {

		glLineWidth(1.5);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glColor4f(r, g, b, alpha);

		/* curve outline */

		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glEnable(GL_LINE_SMOOTH);
		glBegin(GL_LINE_STRIP);
		for (i = 0; i < res; i++) {
			float x2 = x + i * (w / (float)res);
			glVertex2f(x2, y + (data[i] * h));
		}
		glEnd();
		glDisable(GL_LINE_SMOOTH);

		glLineWidth(1.0);
	}
	else {
		/* under the curve */
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glColor4f(r, g, b, alpha);

		glShadeModel(GL_FLAT);
		glBegin(GL_QUAD_STRIP);
		glVertex2f(x, y);
		glVertex2f(x, y + (data[0] * h));
		for (i = 1; i < res; i++) {
			float x2 = x + i * (w / (float)res);
			glVertex2f(x2, y + (data[i] * h));
			glVertex2f(x2, y);
		}
		glEnd();

		/* curve outline */
		glColor4f(0.f, 0.f, 0.f, 0.25f);

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_LINE_SMOOTH);
		glBegin(GL_LINE_STRIP);
		for (i = 0; i < res; i++) {
			float x2 = x + i * (w / (float)res);
			glVertex2f(x2, y + (data[i] * h));
		}
		glEnd();
		glDisable(GL_LINE_SMOOTH);
	}
}

#define HISTOGRAM_TOT_GRID_LINES 4

void ui_draw_but_HISTOGRAM(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), const rcti *recti)
{
	Histogram *hist = (Histogram *)but->poin;
	int res = hist->x_resolution;
	rctf rect;
	int i;
	float w, h;
	const bool is_line = (hist->flag & HISTO_FLAG_LINE) != 0;
	//float alpha;
	GLint scissor[4];
	
	rect.xmin = (float)recti->xmin + 1;
	rect.xmax = (float)recti->xmax - 1;
	rect.ymin = (float)recti->ymin + SCOPE_RESIZE_PAD + 2;
	rect.ymax = (float)recti->ymax - 1;
	
	w = BLI_rctf_size_x(&rect);
	h = BLI_rctf_size_y(&rect) * hist->ymax;
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	glColor4f(0.f, 0.f, 0.f, 0.3f);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_POLYGON, rect.xmin - 1, rect.ymin - 1, rect.xmax + 1, rect.ymax + 1, 3.0f);

	/* need scissor test, histogram can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin - 1),
	          ar->winrct.ymin + (rect.ymin - 1),
	          (rect.xmax + 1) - (rect.xmin - 1),
	          (rect.ymax + 1) - (rect.ymin - 1));

	glColor4f(1.f, 1.f, 1.f, 0.08f);
	/* draw grid lines here */
	for (i = 1; i <= HISTOGRAM_TOT_GRID_LINES; i++) {
		const float fac = (float)i / (float)HISTOGRAM_TOT_GRID_LINES;

		/* so we can tell the 1.0 color point */
		if (i == HISTOGRAM_TOT_GRID_LINES) {
			glColor4f(1.0f, 1.0f, 1.0f, 0.5f);
		}

		fdrawline(rect.xmin, rect.ymin + fac * h, rect.xmax, rect.ymin + fac * h);
		fdrawline(rect.xmin + fac * w, rect.ymin, rect.xmin + fac * w, rect.ymax);
	}
	
	if (hist->mode == HISTO_MODE_LUMA) {
		histogram_draw_one(1.0, 1.0, 1.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_luma, res, is_line);
	}
	else if (hist->mode == HISTO_MODE_ALPHA) {
		histogram_draw_one(1.0, 1.0, 1.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_a, res, is_line);
	}
	else {
		if (hist->mode == HISTO_MODE_RGB || hist->mode == HISTO_MODE_R)
			histogram_draw_one(1.0, 0.0, 0.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_r, res, is_line);
		if (hist->mode == HISTO_MODE_RGB || hist->mode == HISTO_MODE_G)
			histogram_draw_one(0.0, 1.0, 0.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_g, res, is_line);
		if (hist->mode == HISTO_MODE_RGB || hist->mode == HISTO_MODE_B)
			histogram_draw_one(0.0, 0.0, 1.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_b, res, is_line);
	}
	
	/* outline, scale gripper */
	draw_scope_end(&rect, scissor);
}

#undef HISTOGRAM_TOT_GRID_LINES

void ui_draw_but_WAVEFORM(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), const rcti *recti)
{
	Scopes *scopes = (Scopes *)but->poin;
	rctf rect;
	int i, c;
	float w, w3, h, alpha, yofs;
	GLint scissor[4];
	float colors[3][3] = MAT3_UNITY;
	float colorsycc[3][3] = {{1, 0, 1}, {1, 1, 0}, {0, 1, 1}};
	float colors_alpha[3][3], colorsycc_alpha[3][3]; /* colors  pre multiplied by alpha for speed up */
	float min, max;
	
	if (scopes == NULL) return;
	
	rect.xmin = (float)recti->xmin + 1;
	rect.xmax = (float)recti->xmax - 1;
	rect.ymin = (float)recti->ymin + SCOPE_RESIZE_PAD + 2;
	rect.ymax = (float)recti->ymax - 1;

	if (scopes->wavefrm_yfac < 0.5f)
		scopes->wavefrm_yfac = 0.98f;
	w = BLI_rctf_size_x(&rect) - 7;
	h = BLI_rctf_size_y(&rect) * scopes->wavefrm_yfac;
	yofs = rect.ymin + (BLI_rctf_size_y(&rect) - h) / 2.0f;
	w3 = w / 3.0f;
	
	/* log scale for alpha */
	alpha = scopes->wavefrm_alpha * scopes->wavefrm_alpha;
	
	for (c = 0; c < 3; c++) {
		for (i = 0; i < 3; i++) {
			colors_alpha[c][i] = colors[c][i] * alpha;
			colorsycc_alpha[c][i] = colorsycc[c][i] * alpha;
		}
	}
			
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	glColor4f(0.f, 0.f, 0.f, 0.3f);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_POLYGON, rect.xmin - 1, rect.ymin - 1, rect.xmax + 1, rect.ymax + 1, 3.0f);
	

	/* need scissor test, waveform can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin - 1),
	          ar->winrct.ymin + (rect.ymin - 1),
	          (rect.xmax + 1) - (rect.xmin - 1),
	          (rect.ymax + 1) - (rect.ymin - 1));

	glColor4f(1.f, 1.f, 1.f, 0.08f);
	/* draw grid lines here */
	for (i = 0; i < 6; i++) {
		char str[4];
		BLI_snprintf(str, sizeof(str), "%-3d", i * 20);
		str[3] = '\0';
		fdrawline(rect.xmin + 22, yofs + (i / 5.f) * h, rect.xmax + 1, yofs + (i / 5.f) * h);
		BLF_draw_default(rect.xmin + 1, yofs - 5 + (i / 5.f) * h, 0, str, sizeof(str) - 1);
		/* in the loop because blf_draw reset it */
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	/* 3 vertical separation */
	if (scopes->wavefrm_mode != SCOPES_WAVEFRM_LUMA) {
		for (i = 1; i < 3; i++) {
			fdrawline(rect.xmin + i * w3, rect.ymin, rect.xmin + i * w3, rect.ymax);
		}
	}
	
	/* separate min max zone on the right */
	fdrawline(rect.xmin + w, rect.ymin, rect.xmin + w, rect.ymax);
	/* 16-235-240 level in case of ITU-R BT601/709 */
	glColor4f(1.f, 0.4f, 0.f, 0.2f);
	if (ELEM(scopes->wavefrm_mode, SCOPES_WAVEFRM_YCC_601, SCOPES_WAVEFRM_YCC_709)) {
		fdrawline(rect.xmin + 22, yofs + h * 16.0f / 255.0f, rect.xmax + 1, yofs + h * 16.0f / 255.0f);
		fdrawline(rect.xmin + 22, yofs + h * 235.0f / 255.0f, rect.xmin + w3, yofs + h * 235.0f / 255.0f);
		fdrawline(rect.xmin + 3 * w3, yofs + h * 235.0f / 255.0f, rect.xmax + 1, yofs + h * 235.0f / 255.0f);
		fdrawline(rect.xmin + w3, yofs + h * 240.0f / 255.0f, rect.xmax + 1, yofs + h * 240.0f / 255.0f);
	}
	/* 7.5 IRE black point level for NTSC */
	if (scopes->wavefrm_mode == SCOPES_WAVEFRM_LUMA)
		fdrawline(rect.xmin, yofs + h * 0.075f, rect.xmax + 1, yofs + h * 0.075f);

	if (scopes->ok && scopes->waveform_1 != NULL) {
		
		/* LUMA (1 channel) */
		glBlendFunc(GL_ONE, GL_ONE);
		glColor3f(alpha, alpha, alpha);
		if (scopes->wavefrm_mode == SCOPES_WAVEFRM_LUMA) {

			glBlendFunc(GL_ONE, GL_ONE);
			
			glPushMatrix();
			glEnableClientState(GL_VERTEX_ARRAY);
			
			glTranslatef(rect.xmin, yofs, 0.f);
			glScalef(w, h, 0.f);
			glVertexPointer(2, GL_FLOAT, 0, scopes->waveform_1);
			glDrawArrays(GL_POINTS, 0, scopes->waveform_tot);
					
			glDisableClientState(GL_VERTEX_ARRAY);
			glPopMatrix();

			/* min max */
			glColor3f(0.5f, 0.5f, 0.5f);
			min = yofs + scopes->minmax[0][0] * h;
			max = yofs + scopes->minmax[0][1] * h;
			CLAMP(min, rect.ymin, rect.ymax);
			CLAMP(max, rect.ymin, rect.ymax);
			fdrawline(rect.xmax - 3, min, rect.xmax - 3, max);
		}

		/* RGB / YCC (3 channels) */
		else if (ELEM4(scopes->wavefrm_mode,
		               SCOPES_WAVEFRM_RGB,
		               SCOPES_WAVEFRM_YCC_601,
		               SCOPES_WAVEFRM_YCC_709,
		               SCOPES_WAVEFRM_YCC_JPEG))
		{
			int rgb = (scopes->wavefrm_mode == SCOPES_WAVEFRM_RGB);
			
			glBlendFunc(GL_ONE, GL_ONE);
			
			glPushMatrix();
			glEnableClientState(GL_VERTEX_ARRAY);
			
			glTranslatef(rect.xmin, yofs, 0.f);
			glScalef(w3, h, 0.f);
			
			glColor3fv((rgb) ? colors_alpha[0] : colorsycc_alpha[0]);
			glVertexPointer(2, GL_FLOAT, 0, scopes->waveform_1);
			glDrawArrays(GL_POINTS, 0, scopes->waveform_tot);

			glTranslatef(1.f, 0.f, 0.f);
			glColor3fv((rgb) ? colors_alpha[1] : colorsycc_alpha[1]);
			glVertexPointer(2, GL_FLOAT, 0, scopes->waveform_2);
			glDrawArrays(GL_POINTS, 0, scopes->waveform_tot);
			
			glTranslatef(1.f, 0.f, 0.f);
			glColor3fv((rgb) ? colors_alpha[2] : colorsycc_alpha[2]);
			glVertexPointer(2, GL_FLOAT, 0, scopes->waveform_3);
			glDrawArrays(GL_POINTS, 0, scopes->waveform_tot);
			
			glDisableClientState(GL_VERTEX_ARRAY);
			glPopMatrix();

			
			/* min max */
			for (c = 0; c < 3; c++) {
				if (scopes->wavefrm_mode == SCOPES_WAVEFRM_RGB)
					glColor3f(colors[c][0] * 0.75f, colors[c][1] * 0.75f, colors[c][2] * 0.75f);
				else
					glColor3f(colorsycc[c][0] * 0.75f, colorsycc[c][1] * 0.75f, colorsycc[c][2] * 0.75f);
				min = yofs + scopes->minmax[c][0] * h;
				max = yofs + scopes->minmax[c][1] * h;
				CLAMP(min, rect.ymin, rect.ymax);
				CLAMP(max, rect.ymin, rect.ymax);
				fdrawline(rect.xmin + w + 2 + c * 2, min, rect.xmin + w + 2 + c * 2, max);
			}
		}
		
	}
	
	/* outline, scale gripper */
	draw_scope_end(&rect, scissor);
}

static float polar_to_x(float center, float diam, float ampli, float angle)
{
	return center + diam *ampli * cosf(angle);
}

static float polar_to_y(float center, float diam, float ampli, float angle)
{
	return center + diam *ampli * sinf(angle);
}

static void vectorscope_draw_target(float centerx, float centery, float diam, const float colf[3])
{
	float y, u, v;
	float tangle = 0.f, tampli;
	float dangle, dampli, dangle2, dampli2;

	rgb_to_yuv(colf[0], colf[1], colf[2], &y, &u, &v);
	if (u > 0 && v >= 0) tangle = atanf(v / u);
	else if (u > 0 && v < 0) tangle = atanf(v / u) + 2.0f * (float)M_PI;
	else if (u < 0) tangle = atanf(v / u) + (float)M_PI;
	else if (u == 0 && v > 0.0f) tangle = (float)M_PI / 2.0f;
	else if (u == 0 && v < 0.0f) tangle = -(float)M_PI / 2.0f;
	tampli = sqrtf(u * u + v * v);

	/* small target vary by 2.5 degree and 2.5 IRE unit */
	glColor4f(1.0f, 1.0f, 1.0, 0.12f);
	dangle = DEG2RADF(2.5f);
	dampli = 2.5f / 200.0f;
	glBegin(GL_LINE_STRIP);
	glVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle + dangle), polar_to_y(centery, diam, tampli + dampli, tangle + dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli - dampli, tangle + dangle), polar_to_y(centery, diam, tampli - dampli, tangle + dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli - dampli, tangle - dangle), polar_to_y(centery, diam, tampli - dampli, tangle - dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle - dangle), polar_to_y(centery, diam, tampli + dampli, tangle - dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle + dangle), polar_to_y(centery, diam, tampli + dampli, tangle + dangle));
	glEnd();
	/* big target vary by 10 degree and 20% amplitude */
	glColor4f(1.0f, 1.0f, 1.0, 0.12f);
	dangle = DEG2RADF(10.0f);
	dampli = 0.2f * tampli;
	dangle2 = DEG2RADF(5.0f);
	dampli2 = 0.5f * dampli;
	glBegin(GL_LINE_STRIP);
	glVertex2f(polar_to_x(centerx, diam, tampli + dampli - dampli2, tangle + dangle), polar_to_y(centery, diam, tampli + dampli - dampli2, tangle + dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle + dangle), polar_to_y(centery, diam, tampli + dampli, tangle + dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle + dangle - dangle2), polar_to_y(centery, diam, tampli + dampli, tangle + dangle - dangle2));
	glEnd();
	glBegin(GL_LINE_STRIP);
	glVertex2f(polar_to_x(centerx, diam, tampli - dampli + dampli2, tangle + dangle), polar_to_y(centery, diam, tampli - dampli + dampli2, tangle + dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli - dampli, tangle + dangle), polar_to_y(centery, diam, tampli - dampli, tangle + dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli - dampli, tangle + dangle - dangle2), polar_to_y(centery, diam, tampli - dampli, tangle + dangle - dangle2));
	glEnd();
	glBegin(GL_LINE_STRIP);
	glVertex2f(polar_to_x(centerx, diam, tampli - dampli + dampli2, tangle - dangle), polar_to_y(centery, diam, tampli - dampli + dampli2, tangle - dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli - dampli, tangle - dangle), polar_to_y(centery, diam, tampli - dampli, tangle - dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli - dampli, tangle - dangle + dangle2), polar_to_y(centery, diam, tampli - dampli, tangle - dangle + dangle2));
	glEnd();
	glBegin(GL_LINE_STRIP);
	glVertex2f(polar_to_x(centerx, diam, tampli + dampli - dampli2, tangle - dangle), polar_to_y(centery, diam, tampli + dampli - dampli2, tangle - dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle - dangle), polar_to_y(centery, diam, tampli + dampli, tangle - dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle - dangle + dangle2), polar_to_y(centery, diam, tampli + dampli, tangle - dangle + dangle2));
	glEnd();
}

void ui_draw_but_VECTORSCOPE(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), const rcti *recti)
{
	const float skin_rad = DEG2RADF(123.0f); /* angle in radians of the skin tone line */
	Scopes *scopes = (Scopes *)but->poin;
	rctf rect;
	int i, j;
	float w, h, centerx, centery, diam;
	float alpha;
	const float colors[6][3] = {
	    {0.75, 0.0, 0.0},  {0.75, 0.75, 0.0}, {0.0, 0.75, 0.0},
	    {0.0, 0.75, 0.75}, {0.0, 0.0, 0.75},  {0.75, 0.0, 0.75}};
	GLint scissor[4];
	
	rect.xmin = (float)recti->xmin + 1;
	rect.xmax = (float)recti->xmax - 1;
	rect.ymin = (float)recti->ymin + SCOPE_RESIZE_PAD + 2;
	rect.ymax = (float)recti->ymax - 1;
	
	w = BLI_rctf_size_x(&rect);
	h = BLI_rctf_size_y(&rect);
	centerx = rect.xmin + w / 2;
	centery = rect.ymin + h / 2;
	diam = (w < h) ? w : h;
	
	alpha = scopes->vecscope_alpha * scopes->vecscope_alpha * scopes->vecscope_alpha;
			
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	glColor4f(0.f, 0.f, 0.f, 0.3f);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_POLYGON, rect.xmin - 1, rect.ymin - 1, rect.xmax + 1, rect.ymax + 1, 3.0f);

	/* need scissor test, hvectorscope can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin - 1),
	          ar->winrct.ymin + (rect.ymin - 1),
	          (rect.xmax + 1) - (rect.xmin - 1),
	          (rect.ymax + 1) - (rect.ymin - 1));
	
	glColor4f(1.f, 1.f, 1.f, 0.08f);
	/* draw grid elements */
	/* cross */
	fdrawline(centerx - (diam / 2) - 5, centery, centerx + (diam / 2) + 5, centery);
	fdrawline(centerx, centery - (diam / 2) - 5, centerx, centery + (diam / 2) + 5);
	/* circles */
	for (j = 0; j < 5; j++) {
		glBegin(GL_LINE_STRIP);
		for (i = 0; i <= 360; i = i + 15) {
			const float a = DEG2RADF((float)i);
			const float r = (j + 1) / 10.0f;
			glVertex2f(polar_to_x(centerx, diam, r, a), polar_to_y(centery, diam, r, a));
		}
		glEnd();
	}
	/* skin tone line */
	glColor4f(1.f, 0.4f, 0.f, 0.2f);
	fdrawline(polar_to_x(centerx, diam, 0.5f, skin_rad), polar_to_y(centery, diam, 0.5, skin_rad),
	          polar_to_x(centerx, diam, 0.1f, skin_rad), polar_to_y(centery, diam, 0.1, skin_rad));
	/* saturation points */
	for (i = 0; i < 6; i++)
		vectorscope_draw_target(centerx, centery, diam, colors[i]);
	
	if (scopes->ok && scopes->vecscope != NULL) {
		/* pixel point cloud */
		glBlendFunc(GL_ONE, GL_ONE);
		glColor3f(alpha, alpha, alpha);

		glPushMatrix();
		glEnableClientState(GL_VERTEX_ARRAY);

		glTranslatef(centerx, centery, 0.f);
		glScalef(diam, diam, 0.f);

		glVertexPointer(2, GL_FLOAT, 0, scopes->vecscope);
		glDrawArrays(GL_POINTS, 0, scopes->waveform_tot);
		
		glDisableClientState(GL_VERTEX_ARRAY);
		glPopMatrix();
	}

	/* outline, scale gripper */
	draw_scope_end(&rect, scissor);
		
	glDisable(GL_BLEND);
}

void ui_draw_but_COLORBAND(uiBut *but, uiWidgetColors *UNUSED(wcol), const rcti *rect)
{
	ColorBand *coba;
	CBData *cbd;
	float x1, y1, sizex, sizey;
	float v3[2], v1[2], v2[2], v1a[2], v2a[2];
	int a;
	float pos, colf[4] = {0, 0, 0, 0}; /* initialize in case the colorband isn't valid */
	struct ColorManagedDisplay *display = NULL;

	coba = (ColorBand *)(but->editcoba ? but->editcoba : but->poin);
	if (coba == NULL) return;

	if (but->block->color_profile)
		display = ui_block_display_get(but->block);

	x1 = rect->xmin;
	y1 = rect->ymin;
	sizex = rect->xmax - x1;
	sizey = rect->ymax - y1;

	/* first background, to show tranparency */

	glColor4ub(UI_ALPHA_CHECKER_DARK, UI_ALPHA_CHECKER_DARK, UI_ALPHA_CHECKER_DARK, 255);
	glRectf(x1, y1, x1 + sizex, y1 + sizey);
	glEnable(GL_POLYGON_STIPPLE);
	glColor4ub(UI_ALPHA_CHECKER_LIGHT, UI_ALPHA_CHECKER_LIGHT, UI_ALPHA_CHECKER_LIGHT, 255);
	glPolygonStipple(stipple_checker_8px);
	glRectf(x1, y1, x1 + sizex, y1 + sizey);
	glDisable(GL_POLYGON_STIPPLE);

	glShadeModel(GL_FLAT);
	glEnable(GL_BLEND);
	
	cbd = coba->data;
	
	v1[0] = v2[0] = x1;
	v1[1] = y1;
	v2[1] = y1 + sizey;
	
	glBegin(GL_QUAD_STRIP);
	
	glColor4fv(&cbd->r);
	glVertex2fv(v1);
	glVertex2fv(v2);

	for (a = 1; a <= sizex; a++) {
		pos = ((float)a) / (sizex - 1);
		do_colorband(coba, pos, colf);
		if (display)
			IMB_colormanagement_scene_linear_to_display_v3(colf, display);
		
		v1[0] = v2[0] = x1 + a;
		
		glColor4fv(colf);
		glVertex2fv(v1);
		glVertex2fv(v2);
	}
	
	glEnd();
	glShadeModel(GL_FLAT);
	glDisable(GL_BLEND);
	
	/* outline */
	glColor4f(0.0, 0.0, 0.0, 1.0);
	fdrawbox(x1, y1, x1 + sizex, y1 + sizey);
	
	/* help lines */
	v1[0] = v2[0] = v3[0] = x1;
	v1[1] = y1;
	v1a[1] = y1 + 0.25f * sizey;
	v2[1] = y1 + 0.5f * sizey;
	v2a[1] = y1 + 0.75f * sizey;
	v3[1] = y1 + sizey;
	
	
	cbd = coba->data;
	glBegin(GL_LINES);
	for (a = 0; a < coba->tot; a++, cbd++) {
		v1[0] = v2[0] = v3[0] = v1a[0] = v2a[0] = x1 + cbd->pos * sizex;
		
		if (a == coba->cur) {
			glColor3ub(0, 0, 0);
			glVertex2fv(v1);
			glVertex2fv(v3);
			glEnd();
			
			setlinestyle(2);
			glBegin(GL_LINES);
			glColor3ub(255, 255, 255);
			glVertex2fv(v1);
			glVertex2fv(v3);
			glEnd();
			setlinestyle(0);
			glBegin(GL_LINES);
			
#if 0
			glColor3ub(0, 0, 0);
			glVertex2fv(v1);
			glVertex2fv(v1a);
			glColor3ub(255, 255, 255);
			glVertex2fv(v1a);
			glVertex2fv(v2);
			glColor3ub(0, 0, 0);
			glVertex2fv(v2);
			glVertex2fv(v2a);
			glColor3ub(255, 255, 255);
			glVertex2fv(v2a);
			glVertex2fv(v3);
#endif
		}
		else {
			glColor3ub(0, 0, 0);
			glVertex2fv(v1);
			glVertex2fv(v2);
			
			glColor3ub(255, 255, 255);
			glVertex2fv(v2);
			glVertex2fv(v3);
		}
	}
	glEnd();

}

void ui_draw_but_NORMAL(uiBut *but, uiWidgetColors *wcol, const rcti *rect)
{
	static GLuint displist = 0;
	int a, old[8];
	GLfloat diff[4], diffn[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	float vec0[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float dir[4], size;
	
	/* store stuff */
	glGetMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
		
	/* backdrop */
	glColor3ubv((unsigned char *)wcol->inner);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_POLYGON, rect->xmin, rect->ymin, rect->xmax, rect->ymax, 5.0f);
	
	/* sphere color */
	glMaterialfv(GL_FRONT, GL_DIFFUSE, diffn);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
	
	/* disable blender light */
	for (a = 0; a < 8; a++) {
		old[a] = glIsEnabled(GL_LIGHT0 + a);
		glDisable(GL_LIGHT0 + a);
	}
	
	/* own light */
	glEnable(GL_LIGHT7);
	glEnable(GL_LIGHTING);
	
	ui_get_but_vectorf(but, dir);

	dir[3] = 0.0f;   /* glLightfv needs 4 args, 0.0 is sun */
	glLightfv(GL_LIGHT7, GL_POSITION, dir); 
	glLightfv(GL_LIGHT7, GL_DIFFUSE, diffn); 
	glLightfv(GL_LIGHT7, GL_SPECULAR, vec0); 
	glLightf(GL_LIGHT7, GL_CONSTANT_ATTENUATION, 1.0f);
	glLightf(GL_LIGHT7, GL_LINEAR_ATTENUATION, 0.0f);
	
	/* transform to button */
	glPushMatrix();
	glTranslatef(rect->xmin + 0.5f * BLI_rcti_size_x(rect), rect->ymin + 0.5f * BLI_rcti_size_y(rect), 0.0f);
	
	if (BLI_rcti_size_x(rect) < BLI_rcti_size_y(rect))
		size = BLI_rcti_size_x(rect) / 200.f;
	else
		size = BLI_rcti_size_y(rect) / 200.f;
	
	glScalef(size, size, size);

	if (displist == 0) {
		GLUquadricObj *qobj;

		displist = glGenLists(1);
		glNewList(displist, GL_COMPILE);
		
		qobj = gluNewQuadric();
		gluQuadricDrawStyle(qobj, GLU_FILL);
		glShadeModel(GL_SMOOTH);
		gluSphere(qobj, 100.0, 32, 24);
		glShadeModel(GL_FLAT);
		gluDeleteQuadric(qobj);
		
		glEndList();
	}

	glCallList(displist);

	/* restore */
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, diff); 
	glDisable(GL_LIGHT7);
	
	/* AA circle */
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	glColor3ubv((unsigned char *)wcol->inner);
	glutil_draw_lined_arc(0.0f, M_PI * 2.0, 100.0f, 32);
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);

	/* matrix after circle */
	glPopMatrix();

	/* enable blender light */
	for (a = 0; a < 8; a++) {
		if (old[a])
			glEnable(GL_LIGHT0 + a);
	}
}

static void ui_draw_but_curve_grid(const rcti *rect, float zoomx, float zoomy, float offsx, float offsy, float step)
{
	float dx, dy, fx, fy;
	
	glBegin(GL_LINES);
	dx = step * zoomx;
	fx = rect->xmin + zoomx * (-offsx);
	if (fx > rect->xmin) fx -= dx * (floorf(fx - rect->xmin));
	while (fx < rect->xmax) {
		glVertex2f(fx, rect->ymin);
		glVertex2f(fx, rect->ymax);
		fx += dx;
	}
	
	dy = step * zoomy;
	fy = rect->ymin + zoomy * (-offsy);
	if (fy > rect->ymin) fy -= dy * (floorf(fy - rect->ymin));
	while (fy < rect->ymax) {
		glVertex2f(rect->xmin, fy);
		glVertex2f(rect->xmax, fy);
		fy += dy;
	}
	glEnd();
	
}

static void gl_shaded_color(unsigned char *col, int shade)
{
	glColor3ub(col[0] - shade > 0 ? col[0] - shade : 0,
	           col[1] - shade > 0 ? col[1] - shade : 0,
	           col[2] - shade > 0 ? col[2] - shade : 0);
}

void ui_draw_but_CURVE(ARegion *ar, uiBut *but, uiWidgetColors *wcol, const rcti *rect)
{
	CurveMapping *cumap;
	CurveMap *cuma;
	CurveMapPoint *cmp;
	float fx, fy, fac[2], zoomx, zoomy, offsx, offsy;
	GLint scissor[4];
	rcti scissor_new;
	int a;

	if (but->editcumap) {
		cumap = but->editcumap;
	}
	else {
		cumap = (CurveMapping *)but->poin;
	}

	cuma = &cumap->cm[cumap->cur];

	/* need scissor test, curve can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	scissor_new.xmin = ar->winrct.xmin + rect->xmin;
	scissor_new.ymin = ar->winrct.ymin + rect->ymin;
	scissor_new.xmax = ar->winrct.xmin + rect->xmax;
	scissor_new.ymax = ar->winrct.ymin + rect->ymax;
	BLI_rcti_isect(&scissor_new, &ar->winrct, &scissor_new);
	glScissor(scissor_new.xmin,
	          scissor_new.ymin,
	          BLI_rcti_size_x(&scissor_new),
	          BLI_rcti_size_y(&scissor_new));

	/* calculate offset and zoom */
	zoomx = (BLI_rcti_size_x(rect) - 2.0f * but->aspect) / BLI_rctf_size_x(&cumap->curr);
	zoomy = (BLI_rcti_size_y(rect) - 2.0f * but->aspect) / BLI_rctf_size_y(&cumap->curr);
	offsx = cumap->curr.xmin - but->aspect / zoomx;
	offsy = cumap->curr.ymin - but->aspect / zoomy;
	
	/* backdrop */
	if (but->a1 == UI_GRAD_H) {
		/* magic trigger for curve backgrounds */
		rcti grid;
		float col[3] = {0.0f, 0.0f, 0.0f}; /* dummy arg */

		grid.xmin = rect->xmin + zoomx * (-offsx);
		grid.xmax = rect->xmax + zoomx * (-offsx);
		grid.ymin = rect->ymin + zoomy * (-offsy);
		grid.ymax = rect->ymax + zoomy * (-offsy);

		ui_draw_gradient(&grid, col, UI_GRAD_H, 1.0f);

		/* grid, hsv uses different grid */
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4ub(0, 0, 0, 48);
		ui_draw_but_curve_grid(rect, zoomx, zoomy, offsx, offsy, 0.1666666f);
		glDisable(GL_BLEND);
	}
	else {
		if (cumap->flag & CUMA_DO_CLIP) {
			gl_shaded_color((unsigned char *)wcol->inner, -20);
			glRectf(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
			glColor3ubv((unsigned char *)wcol->inner);
			glRectf(rect->xmin + zoomx * (cumap->clipr.xmin - offsx),
			        rect->ymin + zoomy * (cumap->clipr.ymin - offsy),
			        rect->xmin + zoomx * (cumap->clipr.xmax - offsx),
			        rect->ymin + zoomy * (cumap->clipr.ymax - offsy));
		}
		else {
			glColor3ubv((unsigned char *)wcol->inner);
			glRectf(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
		}

		/* grid, every 0.25 step */
		gl_shaded_color((unsigned char *)wcol->inner, -16);
		ui_draw_but_curve_grid(rect, zoomx, zoomy, offsx, offsy, 0.25f);
		/* grid, every 1.0 step */
		gl_shaded_color((unsigned char *)wcol->inner, -24);
		ui_draw_but_curve_grid(rect, zoomx, zoomy, offsx, offsy, 1.0f);
		/* axes */
		gl_shaded_color((unsigned char *)wcol->inner, -50);
		glBegin(GL_LINES);
		glVertex2f(rect->xmin, rect->ymin + zoomy * (-offsy));
		glVertex2f(rect->xmax, rect->ymin + zoomy * (-offsy));
		glVertex2f(rect->xmin + zoomx * (-offsx), rect->ymin);
		glVertex2f(rect->xmin + zoomx * (-offsx), rect->ymax);
		glEnd();
	}

	/* cfra option */
	/* XXX 2.48 */
#if 0
	if (cumap->flag & CUMA_DRAW_CFRA) {
		glColor3ub(0x60, 0xc0, 0x40);
		glBegin(GL_LINES);
		glVertex2f(rect->xmin + zoomx * (cumap->sample[0] - offsx), rect->ymin);
		glVertex2f(rect->xmin + zoomx * (cumap->sample[0] - offsx), rect->ymax);
		glEnd();
	}
#endif
	/* sample option */

	if (cumap->flag & CUMA_DRAW_SAMPLE) {
		if (but->a1 == UI_GRAD_H) {
			float tsample[3];
			float hsv[3];
			linearrgb_to_srgb_v3_v3(tsample, cumap->sample);
			rgb_to_hsv_v(tsample, hsv);
			glColor3ub(240, 240, 240);

			glBegin(GL_LINES);
			glVertex2f(rect->xmin + zoomx * (hsv[0] - offsx), rect->ymin);
			glVertex2f(rect->xmin + zoomx * (hsv[0] - offsx), rect->ymax);
			glEnd();
		}
		else if (cumap->cur == 3) {
			float lum = rgb_to_bw(cumap->sample);
			glColor3ub(240, 240, 240);
			
			glBegin(GL_LINES);
			glVertex2f(rect->xmin + zoomx * (lum - offsx), rect->ymin);
			glVertex2f(rect->xmin + zoomx * (lum - offsx), rect->ymax);
			glEnd();
		}
		else {
			if (cumap->cur == 0)
				glColor3ub(240, 100, 100);
			else if (cumap->cur == 1)
				glColor3ub(100, 240, 100);
			else
				glColor3ub(100, 100, 240);
			
			glBegin(GL_LINES);
			glVertex2f(rect->xmin + zoomx * (cumap->sample[cumap->cur] - offsx), rect->ymin);
			glVertex2f(rect->xmin + zoomx * (cumap->sample[cumap->cur] - offsx), rect->ymax);
			glEnd();
		}
	}

	/* the curve */
	glColor3ubv((unsigned char *)wcol->item);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	glBegin(GL_LINE_STRIP);
	
	if (cuma->table == NULL)
		curvemapping_changed(cumap, false);
	cmp = cuma->table;
	
	/* first point */
	if ((cuma->flag & CUMA_EXTEND_EXTRAPOLATE) == 0) {
		glVertex2f(rect->xmin, rect->ymin + zoomy * (cmp[0].y - offsy));
	}
	else {
		fx = rect->xmin + zoomx * (cmp[0].x - offsx + cuma->ext_in[0]);
		fy = rect->ymin + zoomy * (cmp[0].y - offsy + cuma->ext_in[1]);
		glVertex2f(fx, fy);
	}
	for (a = 0; a <= CM_TABLE; a++) {
		fx = rect->xmin + zoomx * (cmp[a].x - offsx);
		fy = rect->ymin + zoomy * (cmp[a].y - offsy);
		glVertex2f(fx, fy);
	}
	/* last point */
	if ((cuma->flag & CUMA_EXTEND_EXTRAPOLATE) == 0) {
		glVertex2f(rect->xmax, rect->ymin + zoomy * (cmp[CM_TABLE].y - offsy));
	}
	else {
		fx = rect->xmin + zoomx * (cmp[CM_TABLE].x - offsx - cuma->ext_out[0]);
		fy = rect->ymin + zoomy * (cmp[CM_TABLE].y - offsy - cuma->ext_out[1]);
		glVertex2f(fx, fy);
	}
	glEnd();
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);

	/* the points, use aspect to make them visible on edges */
	cmp = cuma->curve;
	glPointSize(3.0f);
	bglBegin(GL_POINTS);
	for (a = 0; a < cuma->totpoint; a++) {
		if (cmp[a].flag & CUMA_SELECT)
			UI_ThemeColor(TH_TEXT_HI);
		else
			UI_ThemeColor(TH_TEXT);
		fac[0] = rect->xmin + zoomx * (cmp[a].x - offsx);
		fac[1] = rect->ymin + zoomy * (cmp[a].y - offsy);
		bglVertex2fv(fac);
	}
	bglEnd();
	glPointSize(1.0f);
	
	/* restore scissortest */
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);

	/* outline */
	glColor3ubv((unsigned char *)wcol->outline);
	fdrawbox(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
}

void ui_draw_but_TRACKPREVIEW(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), const rcti *recti)
{
	rctf rect;
	int ok = 0, width, height;
	GLint scissor[4];
	MovieClipScopes *scopes = (MovieClipScopes *)but->poin;

	rect.xmin = (float)recti->xmin + 1;
	rect.xmax = (float)recti->xmax - 1;
	rect.ymin = (float)recti->ymin + SCOPE_RESIZE_PAD + 2;
	rect.ymax = (float)recti->ymax - 1;

	width  = BLI_rctf_size_x(&rect) + 1;
	height = BLI_rctf_size_y(&rect);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* need scissor test, preview image can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin - 1),
	          ar->winrct.ymin + (rect.ymin - 1),
	          (rect.xmax + 1) - (rect.xmin - 1),
	          (rect.ymax + 1) - (rect.ymin - 1));

	if (scopes->track_disabled) {
		glColor4f(0.7f, 0.3f, 0.3f, 0.3f);
		uiSetRoundBox(UI_CNR_ALL);
		uiDrawBox(GL_POLYGON, rect.xmin - 1, rect.ymin, rect.xmax + 1, rect.ymax + 1, 3.0f);

		ok = 1;
	}
	else if ((scopes->track_search) &&
	         ((!scopes->track_preview) ||
	          (scopes->track_preview->x != width || scopes->track_preview->y != height)))
	{
		ImBuf *tmpibuf;

		if (scopes->track_preview)
			IMB_freeImBuf(scopes->track_preview);

		tmpibuf = BKE_tracking_sample_pattern(scopes->frame_width, scopes->frame_height,
		                                            scopes->track_search, scopes->track,
		                                            &scopes->undist_marker, true, scopes->use_track_mask,
		                                            width, height, scopes->track_pos);

		if (tmpibuf) {
			if (tmpibuf->rect_float)
				IMB_rect_from_float(tmpibuf);

			if (tmpibuf->rect)
				scopes->track_preview = tmpibuf;
			else
				IMB_freeImBuf(tmpibuf);
		}
	}

	if (!ok && scopes->track_preview) {
		float track_pos[2];
		int a;
		ImBuf *drawibuf;

		glPushMatrix();

		track_pos[0] = scopes->track_pos[0];
		track_pos[1] = scopes->track_pos[1];

		/* draw content of pattern area */
		glScissor(ar->winrct.xmin + rect.xmin, ar->winrct.ymin + rect.ymin, scissor[2], scissor[3]);

		if (width > 0 && height > 0) {
			drawibuf = scopes->track_preview;

			if (scopes->use_track_mask) {
				glColor4f(0.0f, 0.0f, 0.0f, 0.3f);
				uiSetRoundBox(UI_CNR_ALL);
				uiDrawBox(GL_POLYGON, rect.xmin - 1, rect.ymin, rect.xmax + 1, rect.ymax + 1, 3.0f);
			}

			glaDrawPixelsSafe(rect.xmin, rect.ymin + 1, drawibuf->x, drawibuf->y,
			                  drawibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, drawibuf->rect);

			/* draw cross for pizel position */
			glTranslatef(rect.xmin + track_pos[0], rect.ymin + track_pos[1], 0.f);
			glScissor(ar->winrct.xmin + rect.xmin,
			          ar->winrct.ymin + rect.ymin,
			          BLI_rctf_size_x(&rect),
			          BLI_rctf_size_y(&rect));

			for (a = 0; a < 2; a++) {
				if (a == 1) {
					glLineStipple(3, 0xaaaa);
					glEnable(GL_LINE_STIPPLE);
					UI_ThemeColor(TH_SEL_MARKER);
				}
				else {
					UI_ThemeColor(TH_MARKER_OUTLINE);
				}

				glBegin(GL_LINES);
				glVertex2f(-10.0f, 0.0f);
				glVertex2f(10.0f, 0.0f);
				glVertex2f(0.0f, -10.0f);
				glVertex2f(0.0f, 10.0f);
				glEnd();
			}
		}

		glDisable(GL_LINE_STIPPLE);
		glPopMatrix();

		ok = 1;
	}

	if (!ok) {
		glColor4f(0.f, 0.f, 0.f, 0.3f);
		uiSetRoundBox(UI_CNR_ALL);
		uiDrawBox(GL_POLYGON, rect.xmin - 1, rect.ymin, rect.xmax + 1, rect.ymax + 1, 3.0f);
	}

	/* outline, scale gripper */
	draw_scope_end(&rect, scissor);

	glDisable(GL_BLEND);
}

void ui_draw_but_NODESOCKET(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), const rcti *recti)
{
	static const float size = 5.0f;
	
	/* 16 values of sin function */
	static float si[16] = {
	    0.00000000f, 0.39435585f, 0.72479278f, 0.93775213f,
	    0.99871650f, 0.89780453f, 0.65137248f, 0.29936312f,
	    -0.10116832f, -0.48530196f, -0.79077573f, -0.96807711f,
	    -0.98846832f, -0.84864425f, -0.57126821f, -0.20129852f
	};
	/* 16 values of cos function */
	static float co[16] = {
	    1.00000000f, 0.91895781f, 0.68896691f, 0.34730525f,
	    -0.05064916f, -0.44039415f, -0.75875812f, -0.95413925f,
	    -0.99486932f, -0.87434661f, -0.61210598f, -0.25065253f,
	    0.15142777f, 0.52896401f, 0.82076344f, 0.97952994f,
	};
	
	unsigned char *col = but->col;
	int a;
	GLint scissor[4];
	rcti scissor_new;
	float x, y;
	
	x = 0.5f * (recti->xmin + recti->xmax);
	y = 0.5f * (recti->ymin + recti->ymax);
	
	/* need scissor test, can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	scissor_new.xmin = ar->winrct.xmin + recti->xmin;
	scissor_new.ymin = ar->winrct.ymin + recti->ymin;
	scissor_new.xmax = ar->winrct.xmin + recti->xmax;
	scissor_new.ymax = ar->winrct.ymin + recti->ymax;
	BLI_rcti_isect(&scissor_new, &ar->winrct, &scissor_new);
	glScissor(scissor_new.xmin,
	          scissor_new.ymin,
	          BLI_rcti_size_x(&scissor_new),
	          BLI_rcti_size_y(&scissor_new));
	
	glColor4ubv(col);
	
	glEnable(GL_BLEND);
	glBegin(GL_POLYGON);
	for (a = 0; a < 16; a++)
		glVertex2f(x + size * si[a], y + size * co[a]);
	glEnd();
	glDisable(GL_BLEND);
	
	glColor4ub(0, 0, 0, 150);
	
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	glBegin(GL_LINE_LOOP);
	for (a = 0; a < 16; a++)
		glVertex2f(x + size * si[a], y + size * co[a]);
	glEnd();
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
	glLineWidth(1.0f);
	
	/* restore scissortest */
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
}

/* ****************************************************** */


static void ui_shadowbox(float minx, float miny, float maxx, float maxy, float shadsize, unsigned char alpha)
{
	glEnable(GL_BLEND);
	glShadeModel(GL_SMOOTH);
	
	/* right quad */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(maxx, miny);
	glVertex2f(maxx, maxy - 0.3f * shadsize);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx + shadsize, maxy - 0.75f * shadsize);
	glVertex2f(maxx + shadsize, miny);
	glEnd();
	
	/* corner shape */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(maxx, miny);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx + shadsize, miny);
	glVertex2f(maxx + 0.7f * shadsize, miny - 0.7f * shadsize);
	glVertex2f(maxx, miny - shadsize);
	glEnd();
	
	/* bottom quad */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(minx + 0.3f * shadsize, miny);
	glVertex2f(maxx, miny);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx, miny - shadsize);
	glVertex2f(minx + 0.5f * shadsize, miny - shadsize);
	glEnd();
	
	glDisable(GL_BLEND);
	glShadeModel(GL_FLAT);
}

void uiDrawBoxShadow(unsigned char alpha, float minx, float miny, float maxx, float maxy)
{
	/* accumulated outline boxes to make shade not linear, is more pleasant */
	ui_shadowbox(minx, miny, maxx, maxy, 11.0, (20 * alpha) >> 8);
	ui_shadowbox(minx, miny, maxx, maxy, 7.0, (40 * alpha) >> 8);
	ui_shadowbox(minx, miny, maxx, maxy, 5.0, (80 * alpha) >> 8);
	
}


void ui_dropshadow(const rctf *rct, float radius, float aspect, float alpha, int UNUSED(select))
{
	int i;
	float rad;
	float a;
	float dalpha = alpha * 2.0f / 255.0f, calpha;
	
	glEnable(GL_BLEND);
	
	if (radius > (BLI_rctf_size_y(rct) - 10.0f) / 2.0f)
		rad = (BLI_rctf_size_y(rct) - 10.0f) / 2.0f;
	else
		rad = radius;

	i = 12;
#if 0
	if (select) {
		a = i * aspect; /* same as below */
	}
	else
#endif
	{
		a = i * aspect;
	}

	calpha = dalpha;
	for (; i--; a -= aspect) {
		/* alpha ranges from 2 to 20 or so */
		glColor4f(0.0f, 0.0f, 0.0f, calpha);
		calpha += dalpha;
		
		uiDrawBox(GL_POLYGON, rct->xmin - a, rct->ymin - a, rct->xmax + a, rct->ymax - 10.0f + a, rad + a);
	}
	
	/* outline emphasis */
	glEnable(GL_LINE_SMOOTH);
	glColor4ub(0, 0, 0, 100);
	uiDrawBox(GL_LINE_LOOP, rct->xmin - 0.5f, rct->ymin - 0.5f, rct->xmax + 0.5f, rct->ymax + 0.5f, radius + 0.5f);
	glDisable(GL_LINE_SMOOTH);
	
	glDisable(GL_BLEND);
}


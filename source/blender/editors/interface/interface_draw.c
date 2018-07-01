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
#include "DNA_screen_types.h"
#include "DNA_movieclip_types.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_colorband.h"
#include "BKE_colortools.h"
#include "BKE_node.h"
#include "BKE_tracking.h"


#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLF_api.h"

#include "GPU_draw.h"
#include "GPU_basic_shader.h"

#include "UI_interface.h"

/* own include */
#include "interface_intern.h"

static int roundboxtype = UI_CNR_ALL;

void UI_draw_roundbox_corner_set(int type)
{
	/* Not sure the roundbox function is the best place to change this
	 * if this is undone, it's not that big a deal, only makes curves edges
	 * square for the  */
	roundboxtype = type;

}

int UI_draw_roundbox_corner_get(void)
{
	return roundboxtype;
}

void UI_draw_roundbox_gl_mode(int mode, float minx, float miny, float maxx, float maxy, float rad)
{
	float vec[7][2] = {
		{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
		{0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805},
	};
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
	float col[3] = {
		fac * col1[0] + (1.0f - fac) * col2[0],
		fac * col1[1] + (1.0f - fac) * col2[1],
		fac * col1[2] + (1.0f - fac) * col2[2]
	};
	glColor3fv(col);
}

/* linear horizontal shade within button or in outline */
/* view2d scrollers use it */
void UI_draw_roundbox_shade_x(
        int mode, float minx, float miny, float maxx, float maxy,
        float rad, float shadetop, float shadedown)
{
	float vec[7][2] = {
		{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
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
}

/* linear vertical shade within button or in outline */
/* view2d scrollers use it */
void UI_draw_roundbox_shade_y(
        int mode, float minx, float miny, float maxx, float maxy,
        float rad, float shadeLeft, float shadeRight)
{
	float vec[7][2] = {
		{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
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
}

/* plain antialiased unfilled rectangle */
void UI_draw_roundbox_unfilled(float minx, float miny, float maxx, float maxy, float rad)
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

	UI_draw_roundbox_gl_mode(GL_LINE_LOOP, minx, miny, maxx, maxy, rad);

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

/* (old, used in outliner) plain antialiased filled box */
void UI_draw_roundbox(float minx, float miny, float maxx, float maxy, float rad)
{
	ui_draw_anti_roundbox(GL_POLYGON, minx, miny, maxx, maxy, rad, roundboxtype & UI_RB_ALPHA);
}

void UI_draw_text_underline(int pos_x, int pos_y, int len, int height)
{
	int ofs_y = 4 * U.pixelsize;
	glRecti(pos_x, pos_y - ofs_y, pos_x + len, pos_y - ofs_y + (height * U.pixelsize));
}

/* ************** SPECIAL BUTTON DRAWING FUNCTIONS ************* */

void ui_draw_but_IMAGE(ARegion *UNUSED(ar), uiBut *but, uiWidgetColors *UNUSED(wcol), const rcti *rect)
{
#ifdef WITH_HEADLESS
	(void)rect;
	(void)but;
#else
	ImBuf *ibuf = (ImBuf *)but->poin;

	if (!ibuf) return;

	int w = BLI_rcti_size_x(rect);
	int h = BLI_rcti_size_y(rect);

	/* scissor doesn't seem to be doing the right thing...? */
#if 0
	//glColor4f(1.0, 0.f, 0.f, 1.f);
	//fdrawbox(rect->xmin, rect->ymin, rect->xmax, rect->ymax)

	/* prevent drawing outside widget area */
	GLint scissor[4];
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

/**
 * Draw title and text safe areas.
 *
 * The first 4 parameters are the offsets for the view, not the zones.
 */
void UI_draw_safe_areas(
        float x1, float x2, float y1, float y2,
        const float title_aspect[2], const float action_aspect[2])
{
	const float size_x_half = (x2 - x1) * 0.5f;
	const float size_y_half = (y2 - y1) * 0.5f;

	const float *safe_areas[] = {title_aspect, action_aspect};
	int safe_len = ARRAY_SIZE(safe_areas);
	bool is_first = true;

	for (int i = 0; i < safe_len; i++) {
		if (safe_areas[i][0] || safe_areas[i][1]) {
			float margin_x, margin_y;
			float minx, miny, maxx, maxy;

			if (is_first) {
				UI_ThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25f, 0);
				is_first = false;
			}

			margin_x = safe_areas[i][0] * size_x_half;
			margin_y = safe_areas[i][1] * size_y_half;

			minx = x1 + margin_x;
			miny = y1 + margin_y;
			maxx = x2 - margin_x;
			maxy = y2 - margin_y;

			glBegin(GL_LINE_LOOP);
			glVertex2f(maxx, miny);
			glVertex2f(maxx, maxy);
			glVertex2f(minx, maxy);
			glVertex2f(minx, miny);
			glEnd();
		}
	}
}


static void draw_scope_end(const rctf *rect, GLint *scissor)
{
	/* restore scissortest */
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* outline */
	glColor4f(0.f, 0.f, 0.f, 0.5f);
	UI_draw_roundbox_corner_set(UI_CNR_ALL);
	UI_draw_roundbox_gl_mode(GL_LINE_LOOP, rect->xmin - 1, rect->ymin, rect->xmax + 1, rect->ymax + 1, 3.0f);
}

static void histogram_draw_one(
        float r, float g, float b, float alpha,
        float x, float y, float w, float h, const float *data, int res, const bool is_line)
{
	glEnable(GL_LINE_SMOOTH);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glColor4f(r, g, b, alpha);

	if (is_line) {
		/* curve outline */
		glLineWidth(1.5);

		glBegin(GL_LINE_STRIP);
		for (int i = 0; i < res; i++) {
			float x2 = x + i * (w / (float)res);
			glVertex2f(x2, y + (data[i] * h));
		}
		glEnd();
	}
	else {
		/* under the curve */
		glBegin(GL_TRIANGLE_STRIP);
		glVertex2f(x, y);
		glVertex2f(x, y + (data[0] * h));
		for (int i = 1; i < res; i++) {
			float x2 = x + i * (w / (float)res);
			glVertex2f(x2, y + (data[i] * h));
			glVertex2f(x2, y);
		}
		glEnd();

		/* curve outline */
		glColor4f(0.f, 0.f, 0.f, 0.25f);

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBegin(GL_LINE_STRIP);
		for (int i = 0; i < res; i++) {
			float x2 = x + i * (w / (float)res);
			glVertex2f(x2, y + (data[i] * h));
		}
		glEnd();
	}

	glDisable(GL_LINE_SMOOTH);
}

#define HISTOGRAM_TOT_GRID_LINES 4

void ui_draw_but_HISTOGRAM(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), const rcti *recti)
{
	Histogram *hist = (Histogram *)but->poin;
	int res = hist->x_resolution;
	const bool is_line = (hist->flag & HISTO_FLAG_LINE) != 0;

	rctf rect = {
		.xmin = (float)recti->xmin + 1,
		.xmax = (float)recti->xmax - 1,
		.ymin = (float)recti->ymin + 1,
		.ymax = (float)recti->ymax - 1
	};

	float w = BLI_rctf_size_x(&rect);
	float h = BLI_rctf_size_y(&rect) * hist->ymax;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	UI_ThemeColor4(TH_PREVIEW_BACK);
	UI_draw_roundbox_corner_set(UI_CNR_ALL);
	UI_draw_roundbox_gl_mode(GL_POLYGON, rect.xmin - 1, rect.ymin - 1, rect.xmax + 1, rect.ymax + 1, 3.0f);

	/* need scissor test, histogram can draw outside of boundary */
	GLint scissor[4];
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin - 1),
	          ar->winrct.ymin + (rect.ymin - 1),
	          (rect.xmax + 1) - (rect.xmin - 1),
	          (rect.ymax + 1) - (rect.ymin - 1));

	glColor4f(1.f, 1.f, 1.f, 0.08f);
	/* draw grid lines here */
	for (int i = 1; i <= HISTOGRAM_TOT_GRID_LINES; i++) {
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

	/* outline */
	draw_scope_end(&rect, scissor);
}

#undef HISTOGRAM_TOT_GRID_LINES

void ui_draw_but_WAVEFORM(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), const rcti *recti)
{
	Scopes *scopes = (Scopes *)but->poin;
	GLint scissor[4];
	float colors[3][3];
	float colorsycc[3][3] = {{1, 0, 1}, {1, 1, 0}, {0, 1, 1}};
	float colors_alpha[3][3], colorsycc_alpha[3][3]; /* colors  pre multiplied by alpha for speed up */
	float min, max;

	if (scopes == NULL) return;

	rctf rect = {
		.xmin = (float)recti->xmin + 1,
		.xmax = (float)recti->xmax - 1,
		.ymin = (float)recti->ymin + 1,
		.ymax = (float)recti->ymax - 1
	};

	if (scopes->wavefrm_yfac < 0.5f)
		scopes->wavefrm_yfac = 0.98f;
	float w = BLI_rctf_size_x(&rect) - 7;
	float h = BLI_rctf_size_y(&rect) * scopes->wavefrm_yfac;
	float yofs = rect.ymin + (BLI_rctf_size_y(&rect) - h) / 2.0f;
	float w3 = w / 3.0f;

	/* log scale for alpha */
	float alpha = scopes->wavefrm_alpha * scopes->wavefrm_alpha;

	unit_m3(colors);

	for (int c = 0; c < 3; c++) {
		for (int i = 0; i < 3; i++) {
			colors_alpha[c][i] = colors[c][i] * alpha;
			colorsycc_alpha[c][i] = colorsycc[c][i] * alpha;
		}
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	UI_ThemeColor4(TH_PREVIEW_BACK);
	UI_draw_roundbox_corner_set(UI_CNR_ALL);
	UI_draw_roundbox_gl_mode(GL_POLYGON, rect.xmin - 1, rect.ymin - 1, rect.xmax + 1, rect.ymax + 1, 3.0f);

	/* need scissor test, waveform can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin - 1),
	          ar->winrct.ymin + (rect.ymin - 1),
	          (rect.xmax + 1) - (rect.xmin - 1),
	          (rect.ymax + 1) - (rect.ymin - 1));

	glColor4f(1.f, 1.f, 1.f, 0.08f);
	/* draw grid lines here */
	for (int i = 0; i < 6; i++) {
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
		for (int i = 1; i < 3; i++) {
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
		glPointSize(1.0);

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
		/* RGB (3 channel) */
		else if (scopes->wavefrm_mode == SCOPES_WAVEFRM_RGB) {
			glBlendFunc(GL_ONE, GL_ONE);

			glEnableClientState(GL_VERTEX_ARRAY);

			glPushMatrix();

			glTranslatef(rect.xmin, yofs, 0.f);
			glScalef(w, h, 0.f);

			glColor3fv( colors_alpha[0] );
			glVertexPointer(2, GL_FLOAT, 0, scopes->waveform_1);
			glDrawArrays(GL_POINTS, 0, scopes->waveform_tot);

			glColor3fv( colors_alpha[1] );
			glVertexPointer(2, GL_FLOAT, 0, scopes->waveform_2);
			glDrawArrays(GL_POINTS, 0, scopes->waveform_tot);

			glColor3fv( colors_alpha[2] );
			glVertexPointer(2, GL_FLOAT, 0, scopes->waveform_3);
			glDrawArrays(GL_POINTS, 0, scopes->waveform_tot);

			glDisableClientState(GL_VERTEX_ARRAY);
			glPopMatrix();
		}
		/* PARADE / YCC (3 channels) */
		else if (ELEM(scopes->wavefrm_mode,
		              SCOPES_WAVEFRM_RGB_PARADE,
		              SCOPES_WAVEFRM_YCC_601,
		              SCOPES_WAVEFRM_YCC_709,
		              SCOPES_WAVEFRM_YCC_JPEG
		              ))
		{
			int rgb = (scopes->wavefrm_mode == SCOPES_WAVEFRM_RGB_PARADE);

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
		}
		/* min max */
		if (scopes->wavefrm_mode != SCOPES_WAVEFRM_LUMA ) {
			for (int c = 0; c < 3; c++) {
				if (ELEM(scopes->wavefrm_mode, SCOPES_WAVEFRM_RGB_PARADE, SCOPES_WAVEFRM_RGB))
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

	/* outline */
	draw_scope_end(&rect, scissor);
}

static float polar_to_x(float center, float diam, float ampli, float angle)
{
	return center + diam * ampli * cosf(angle);
}

static float polar_to_y(float center, float diam, float ampli, float angle)
{
	return center + diam * ampli * sinf(angle);
}

static void vectorscope_draw_target(float centerx, float centery, float diam, const float colf[3])
{
	float y, u, v;
	float tangle = 0.f, tampli;
	float dangle, dampli, dangle2, dampli2;

	rgb_to_yuv(colf[0], colf[1], colf[2], &y, &u, &v, BLI_YUV_ITU_BT709);
	if (u > 0 && v >= 0) tangle = atanf(v / u);
	else if (u > 0 && v < 0) tangle = atanf(v / u) + 2.0f * (float)M_PI;
	else if (u < 0) tangle = atanf(v / u) + (float)M_PI;
	else if (u == 0 && v > 0.0f) tangle = M_PI_2;
	else if (u == 0 && v < 0.0f) tangle = -M_PI_2;
	tampli = sqrtf(u * u + v * v);

	/* small target vary by 2.5 degree and 2.5 IRE unit */
	glColor4f(1.0f, 1.0f, 1.0, 0.12f);
	dangle = DEG2RADF(2.5f);
	dampli = 2.5f / 200.0f;
	glBegin(GL_LINE_LOOP);
	glVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle + dangle), polar_to_y(centery, diam, tampli + dampli, tangle + dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli - dampli, tangle + dangle), polar_to_y(centery, diam, tampli - dampli, tangle + dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli - dampli, tangle - dangle), polar_to_y(centery, diam, tampli - dampli, tangle - dangle));
	glVertex2f(polar_to_x(centerx, diam, tampli + dampli, tangle - dangle), polar_to_y(centery, diam, tampli + dampli, tangle - dangle));
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

	const float colors[6][3] = {
	    {0.75, 0.0, 0.0},  {0.75, 0.75, 0.0}, {0.0, 0.75, 0.0},
	    {0.0, 0.75, 0.75}, {0.0, 0.0, 0.75},  {0.75, 0.0, 0.75}};

	rctf rect = {
		.xmin = (float)recti->xmin + 1,
		.xmax = (float)recti->xmax - 1,
		.ymin = (float)recti->ymin + 1,
		.ymax = (float)recti->ymax - 1
	};

	float w = BLI_rctf_size_x(&rect);
	float h = BLI_rctf_size_y(&rect);
	float centerx = rect.xmin + w / 2;
	float centery = rect.ymin + h / 2;
	float diam = (w < h) ? w : h;

	float alpha = scopes->vecscope_alpha * scopes->vecscope_alpha * scopes->vecscope_alpha;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	UI_ThemeColor4(TH_PREVIEW_BACK);
	UI_draw_roundbox_corner_set(UI_CNR_ALL);
	UI_draw_roundbox_gl_mode(GL_POLYGON, rect.xmin - 1, rect.ymin - 1, rect.xmax + 1, rect.ymax + 1, 3.0f);

	/* need scissor test, hvectorscope can draw outside of boundary */
	GLint scissor[4];
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
	for (int j = 0; j < 5; j++) {
		glBegin(GL_LINE_LOOP);
		const int increment = 15;
		for (int i = 0; i <= 360 - increment; i += increment) {
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
	for (int i = 0; i < 6; i++)
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
		glPointSize(1.0);
		glDrawArrays(GL_POINTS, 0, scopes->waveform_tot);

		glDisableClientState(GL_VERTEX_ARRAY);
		glPopMatrix();
	}

	/* outline */
	draw_scope_end(&rect, scissor);

	glDisable(GL_BLEND);
}

static void ui_draw_colorband_handle_tri_hlight(float x1, float y1, float halfwidth, float height)
{
	glEnable(GL_LINE_SMOOTH);

	glBegin(GL_LINE_STRIP);
	glVertex2f(x1 + halfwidth, y1);
	glVertex2f(x1, y1 + height);
	glVertex2f(x1 - halfwidth, y1);
	glEnd();

	glDisable(GL_LINE_SMOOTH);
}

static void ui_draw_colorband_handle_tri(float x1, float y1, float halfwidth, float height, bool fill)
{
	glEnable(fill ? GL_POLYGON_SMOOTH : GL_LINE_SMOOTH);

	glBegin(fill ? GL_TRIANGLES : GL_LINE_LOOP);
	glVertex2f(x1 + halfwidth, y1);
	glVertex2f(x1, y1 + height);
	glVertex2f(x1 - halfwidth, y1);
	glEnd();

	glDisable(fill ? GL_POLYGON_SMOOTH : GL_LINE_SMOOTH);
}

static void ui_draw_colorband_handle_box(float x1, float y1, float x2, float y2, bool fill)
{
	glBegin(fill ? GL_QUADS : GL_LINE_LOOP);
	glVertex2f(x1, y1);
	glVertex2f(x1, y2);
	glVertex2f(x2, y2);
	glVertex2f(x2, y1);
	glEnd();
}

static void ui_draw_colorband_handle(
        const rcti *rect, float x,
        const float rgb[3], struct ColorManagedDisplay *display,
        bool active)
{
	const float sizey = BLI_rcti_size_y(rect);
	const float min_width = 3.0f;
	float colf[3] = {UNPACK3(rgb)};

	float half_width = floorf(sizey / 3.5f);
	float height = half_width * 1.4f;

	float y1 = rect->ymin + (sizey * 0.16f);
	float y2 = rect->ymax;

	/* align to pixels */
	x  = floorf(x  + 0.5f);
	y1 = floorf(y1 + 0.5f);

	if (active || half_width < min_width) {
		glBegin(GL_LINES);
		glColor3ub(0, 0, 0);
		glVertex2f(x, y1);
		glVertex2f(x, y2);
		glEnd();
		setlinestyle(active ? 2 : 1);
		glBegin(GL_LINES);
		glColor3ub(200, 200, 200);
		glVertex2f(x, y1);
		glVertex2f(x, y2);
		glEnd();
		setlinestyle(0);

		/* hide handles when zoomed out too far */
		if (half_width < min_width) {
			return;
		}
	}

	/* shift handle down */
	y1 -= half_width;

	glColor3ub(0, 0, 0);
	ui_draw_colorband_handle_box(x - half_width, y1 - 1, x + half_width, y1 + height, false);

	/* draw all triangles blended */
	glEnable(GL_BLEND);

	ui_draw_colorband_handle_tri(x, y1 + height, half_width, half_width, true);

	if (active)
		glColor3ub(196, 196, 196);
	else
		glColor3ub(96, 96, 96);
	ui_draw_colorband_handle_tri(x, y1 + height, half_width, half_width, true);

	if (active)
		glColor3ub(255, 255, 255);
	else
		glColor3ub(128, 128, 128);
	ui_draw_colorband_handle_tri_hlight(x, y1 + height - 1, (half_width - 1), (half_width - 1));

	glColor3ub(0, 0, 0);
	ui_draw_colorband_handle_tri_hlight(x, y1 + height, half_width, half_width);

	glDisable(GL_BLEND);

	glColor3ub(128, 128, 128);
	ui_draw_colorband_handle_box(x - (half_width - 1), y1, x + (half_width - 1), y1 + height, true);

	if (display) {
		IMB_colormanagement_scene_linear_to_display_v3(colf, display);
	}

	glColor3fv(colf);
	ui_draw_colorband_handle_box(x - (half_width - 2), y1 + 1, x + (half_width - 2), y1 + height - 2, true);
}

void ui_draw_but_COLORBAND(uiBut *but, uiWidgetColors *UNUSED(wcol), const rcti *rect)
{
	struct ColorManagedDisplay *display = NULL;

	ColorBand *coba = (ColorBand *)(but->editcoba ? but->editcoba : but->poin);
	if (coba == NULL) return;

	if (but->block->color_profile)
		display = ui_block_cm_display_get(but->block);

	float x1 = rect->xmin;
	float sizex = rect->xmax - x1;
	float sizey = BLI_rcti_size_y(rect);
	float sizey_solid = sizey / 4;
	float y1 = rect->ymin;

	/* Drawing the checkerboard.
	 * This could be optimized with a single checkerboard shader,
	 * instead of drawing twice and using stippling the second time. */
	/* layer: background, to show tranparency */
	glColor4ub(UI_ALPHA_CHECKER_DARK, UI_ALPHA_CHECKER_DARK, UI_ALPHA_CHECKER_DARK, 255);
	glRectf(x1, y1, x1 + sizex, rect->ymax);
	GPU_basic_shader_bind(GPU_SHADER_STIPPLE | GPU_SHADER_USE_COLOR);
	glColor4ub(UI_ALPHA_CHECKER_LIGHT, UI_ALPHA_CHECKER_LIGHT, UI_ALPHA_CHECKER_LIGHT, 255);
	GPU_basic_shader_stipple(GPU_SHADER_STIPPLE_CHECKER_8PX);
	glRectf(x1, y1, x1 + sizex, rect->ymax);
	GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);

	/* layer: color ramp */
	glEnable(GL_BLEND);

	CBData *cbd = coba->data;

	float v1[2], v2[2];
	float colf[4] = {0, 0, 0, 0}; /* initialize in case the colorband isn't valid */

	v1[1] = y1 + sizey_solid;
	v2[1] = rect->ymax;

	glBegin(GL_TRIANGLE_STRIP);
	for (int a = 0; a <= sizex; a++) {
		float pos = ((float)a) / sizex;
		BKE_colorband_evaluate(coba, pos, colf);
		if (display)
			IMB_colormanagement_scene_linear_to_display_v3(colf, display);

		v1[0] = v2[0] = x1 + a;

		glColor4fv(colf);
		glVertex2fv(v1);
		glVertex2fv(v2);
	}
	glEnd();

	/* layer: color ramp without alpha for reference when manipulating ramp properties */
	v1[1] = y1;
	v2[1] = y1 + sizey_solid;

	glBegin(GL_TRIANGLE_STRIP);
	for (int a = 0; a <= sizex; a++) {
		float pos = ((float)a) / sizex;
		BKE_colorband_evaluate(coba, pos, colf);
		if (display)
			IMB_colormanagement_scene_linear_to_display_v3(colf, display);

		v1[0] = v2[0] = x1 + a;

		glColor4f(colf[0], colf[1], colf[2], 1.0f);
		glVertex2fv(v1);
		glVertex2fv(v2);
	}
	glEnd();

	glDisable(GL_BLEND);

	/* layer: box outline */
	glColor4f(0.0, 0.0, 0.0, 1.0);
	fdrawbox(x1, y1, x1 + sizex, rect->ymax);

	/* layer: box outline */
	glEnable(GL_BLEND);
	glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
	fdrawline(x1, y1, x1 + sizex, y1);
	glColor4f(1.0f, 1.0f, 1.0f, 0.25f);
	fdrawline(x1, y1 - 1, x1 + sizex, y1 - 1);
	glDisable(GL_BLEND);

	/* layer: draw handles */
	for (int a = 0; a < coba->tot; a++, cbd++) {
		if (a != coba->cur) {
			float pos = x1 + cbd->pos * (sizex - 1) + 1;
			ui_draw_colorband_handle(rect, pos, &cbd->r, display, false);
		}
	}

	/* layer: active handle */
	if (coba->tot != 0) {
		cbd = &coba->data[coba->cur];
		float pos = x1 + cbd->pos * (sizex - 1) + 1;
		ui_draw_colorband_handle(rect, pos, &cbd->r, display, true);
	}
}

void ui_draw_but_UNITVEC(uiBut *but, uiWidgetColors *wcol, const rcti *rect)
{
	static GLuint displist = 0;
	float diffuse[3] = {1.0f, 1.0f, 1.0f};
	float size;

	/* backdrop */
	glColor3ubv((unsigned char *)wcol->inner);
	UI_draw_roundbox_corner_set(UI_CNR_ALL);
	UI_draw_roundbox_gl_mode(GL_POLYGON, rect->xmin, rect->ymin, rect->xmax, rect->ymax, 5.0f);

	/* sphere color */
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);

	/* setup lights */
	GPULightData light = {0};
	light.type = GPU_LIGHT_SUN;
	copy_v3_v3(light.diffuse, diffuse);
	zero_v3(light.specular);
	ui_but_v3_get(but, light.direction);

	GPU_basic_shader_light_set(0, &light);
	for (int a = 1; a < 8; a++)
		GPU_basic_shader_light_set(a, NULL);

	/* setup shader */
	GPU_basic_shader_colors(diffuse, NULL, 0, 1.0f);
	GPU_basic_shader_bind(GPU_SHADER_LIGHTING);

	/* transform to button */
	glPushMatrix();
	glTranslatef(rect->xmin + 0.5f * BLI_rcti_size_x(rect), rect->ymin + 0.5f * BLI_rcti_size_y(rect), 0.0f);

	if (BLI_rcti_size_x(rect) < BLI_rcti_size_y(rect))
		size = BLI_rcti_size_x(rect) / 200.f;
	else
		size = BLI_rcti_size_y(rect) / 200.f;

	glScalef(size, size, MIN2(size, 1.0f));

	if (displist == 0) {
		GLUquadricObj *qobj;

		displist = glGenLists(1);
		glNewList(displist, GL_COMPILE);

		qobj = gluNewQuadric();
		gluQuadricDrawStyle(qobj, GLU_FILL);
		GPU_basic_shader_bind(GPU_basic_shader_bound_options());
		gluSphere(qobj, 100.0, 32, 24);
		gluDeleteQuadric(qobj);

		glEndList();
	}

	glCallList(displist);

	/* restore */
	GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
	GPU_default_lights();
	glDisable(GL_CULL_FACE);

	/* AA circle */
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	glColor3ubv((unsigned char *)wcol->inner);
	glutil_draw_lined_arc(0.0f, M_PI * 2.0, 100.0f, 32);
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);

	/* matrix after circle */
	glPopMatrix();

	/* We disabled all blender lights above, so restore them here. */
	GPU_default_lights();
}

static void ui_draw_but_curve_grid(const rcti *rect, float zoomx, float zoomy, float offsx, float offsy, float step)
{
	glBegin(GL_LINES);
	float dx = step * zoomx;
	float fx = rect->xmin + zoomx * (-offsx);
	if (fx > rect->xmin) fx -= dx * (floorf(fx - rect->xmin));
	while (fx < rect->xmax) {
		glVertex2f(fx, rect->ymin);
		glVertex2f(fx, rect->ymax);
		fx += dx;
	}

	float dy = step * zoomy;
	float fy = rect->ymin + zoomy * (-offsy);
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

	if (but->editcumap) {
		cumap = but->editcumap;
	}
	else {
		cumap = (CurveMapping *)but->poin;
	}

	CurveMap *cuma = &cumap->cm[cumap->cur];

	/* need scissor test, curve can draw outside of boundary */
	GLint scissor[4];
	glGetIntegerv(GL_VIEWPORT, scissor);
	rcti scissor_new = {
		.xmin = ar->winrct.xmin + rect->xmin,
		.ymin = ar->winrct.ymin + rect->ymin,
		.xmax = ar->winrct.xmin + rect->xmax,
		.ymax = ar->winrct.ymin + rect->ymax
	};
	BLI_rcti_isect(&scissor_new, &ar->winrct, &scissor_new);
	glScissor(scissor_new.xmin,
	          scissor_new.ymin,
	          BLI_rcti_size_x(&scissor_new),
	          BLI_rcti_size_y(&scissor_new));

	/* calculate offset and zoom */
	float zoomx = (BLI_rcti_size_x(rect) - 2.0f) / BLI_rctf_size_x(&cumap->curr);
	float zoomy = (BLI_rcti_size_y(rect) - 2.0f) / BLI_rctf_size_y(&cumap->curr);
	float offsx = cumap->curr.xmin - (1.0f / zoomx);
	float offsy = cumap->curr.ymin - (1.0f / zoomy);

	glLineWidth(1.0f);

	/* backdrop */
	if (but->a1 == UI_GRAD_H) {
		/* magic trigger for curve backgrounds */
		float col[3] = {0.0f, 0.0f, 0.0f}; /* dummy arg */

		rcti grid = {
			.xmin = rect->xmin + zoomx * (-offsx),
			.xmax = grid.xmin + zoomx,
			.ymin = rect->ymin + zoomy * (-offsy),
			.ymax = grid.ymin + zoomy
		};

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
		glBegin(GL_LINES); /* will draw one of the following 3 lines */
		if (but->a1 == UI_GRAD_H) {
			float tsample[3];
			float hsv[3];
			linearrgb_to_srgb_v3_v3(tsample, cumap->sample);
			rgb_to_hsv_v(tsample, hsv);
			glColor3ub(240, 240, 240);

			glVertex2f(rect->xmin + zoomx * (hsv[0] - offsx), rect->ymin);
			glVertex2f(rect->xmin + zoomx * (hsv[0] - offsx), rect->ymax);
		}
		else if (cumap->cur == 3) {
			float lum = IMB_colormanagement_get_luminance(cumap->sample);
			glColor3ub(240, 240, 240);

			glVertex2f(rect->xmin + zoomx * (lum - offsx), rect->ymin);
			glVertex2f(rect->xmin + zoomx * (lum - offsx), rect->ymax);
		}
		else {
			if (cumap->cur == 0)
				glColor3ub(240, 100, 100);
			else if (cumap->cur == 1)
				glColor3ub(100, 240, 100);
			else
				glColor3ub(100, 100, 240);

			glVertex2f(rect->xmin + zoomx * (cumap->sample[cumap->cur] - offsx), rect->ymin);
			glVertex2f(rect->xmin + zoomx * (cumap->sample[cumap->cur] - offsx), rect->ymax);
		}
		glEnd();
	}

	/* the curve */
	glColor3ubv((unsigned char *)wcol->item);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	glBegin(GL_LINE_STRIP);

	if (cuma->table == NULL)
		curvemapping_changed(cumap, false);

	CurveMapPoint *cmp = cuma->table;

	/* first point */
	if ((cuma->flag & CUMA_EXTEND_EXTRAPOLATE) == 0) {
		glVertex2f(rect->xmin, rect->ymin + zoomy * (cmp[0].y - offsy));
	}
	else {
		float fx = rect->xmin + zoomx * (cmp[0].x - offsx + cuma->ext_in[0]);
		float fy = rect->ymin + zoomy * (cmp[0].y - offsy + cuma->ext_in[1]);
		glVertex2f(fx, fy);
	}
	for (int a = 0; a <= CM_TABLE; a++) {
		float fx = rect->xmin + zoomx * (cmp[a].x - offsx);
		float fy = rect->ymin + zoomy * (cmp[a].y - offsy);
		glVertex2f(fx, fy);
	}
	/* last point */
	if ((cuma->flag & CUMA_EXTEND_EXTRAPOLATE) == 0) {
		glVertex2f(rect->xmax, rect->ymin + zoomy * (cmp[CM_TABLE].y - offsy));
	}
	else {
		float fx = rect->xmin + zoomx * (cmp[CM_TABLE].x - offsx - cuma->ext_out[0]);
		float fy = rect->ymin + zoomy * (cmp[CM_TABLE].y - offsy - cuma->ext_out[1]);
		glVertex2f(fx, fy);
	}
	glEnd();
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);

	/* the points, use aspect to make them visible on edges */
	cmp = cuma->curve;
	glPointSize(3.0f);
	glBegin(GL_POINTS);
	for (int a = 0; a < cuma->totpoint; a++) {
		if (cmp[a].flag & CUMA_SELECT)
			UI_ThemeColor(TH_TEXT_HI);
		else
			UI_ThemeColor(TH_TEXT);
		float fx = rect->xmin + zoomx * (cmp[a].x - offsx);
		float fy = rect->ymin + zoomy * (cmp[a].y - offsy);
		glVertex2f(fx, fy);
	}
	glEnd();

	/* restore scissortest */
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);

	/* outline */
	glColor3ubv((unsigned char *)wcol->outline);
	fdrawbox(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
}

void ui_draw_but_TRACKPREVIEW(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), const rcti *recti)
{
	bool ok = false;
	MovieClipScopes *scopes = (MovieClipScopes *)but->poin;

	rctf rect = {
		.xmin = (float)recti->xmin + 1,
		.xmax = (float)recti->xmax - 1,
		.ymin = (float)recti->ymin + 1,
		.ymax = (float)recti->ymax - 1
	};

	int width  = BLI_rctf_size_x(&rect) + 1;
	int height = BLI_rctf_size_y(&rect);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* need scissor test, preview image can draw outside of boundary */
	GLint scissor[4];
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin - 1),
	          ar->winrct.ymin + (rect.ymin - 1),
	          (rect.xmax + 1) - (rect.xmin - 1),
	          (rect.ymax + 1) - (rect.ymin - 1));

	if (scopes->track_disabled) {
		glColor4f(0.7f, 0.3f, 0.3f, 0.3f);
		UI_draw_roundbox_corner_set(UI_CNR_ALL);
		UI_draw_roundbox_gl_mode(GL_POLYGON, rect.xmin - 1, rect.ymin, rect.xmax + 1, rect.ymax + 1, 3.0f);

		ok = true;
	}
	else if ((scopes->track_search) &&
	         ((!scopes->track_preview) ||
	          (scopes->track_preview->x != width || scopes->track_preview->y != height)))
	{
		if (scopes->track_preview)
			IMB_freeImBuf(scopes->track_preview);

		ImBuf *tmpibuf = BKE_tracking_sample_pattern(
		        scopes->frame_width, scopes->frame_height,
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
		glPushMatrix();

		/* draw content of pattern area */
		glScissor(ar->winrct.xmin + rect.xmin, ar->winrct.ymin + rect.ymin, scissor[2], scissor[3]);

		if (width > 0 && height > 0) {
			ImBuf *drawibuf = scopes->track_preview;

			if (scopes->use_track_mask) {
				glColor4f(0.0f, 0.0f, 0.0f, 0.3f);
				UI_draw_roundbox_corner_set(UI_CNR_ALL);
				UI_draw_roundbox_gl_mode(GL_POLYGON, rect.xmin - 1, rect.ymin, rect.xmax + 1, rect.ymax + 1, 3.0f);
			}

			glaDrawPixelsSafe(
			        rect.xmin, rect.ymin + 1, drawibuf->x, drawibuf->y,
			        drawibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, drawibuf->rect);

			/* draw cross for pixel position */
			glTranslatef(rect.xmin + scopes->track_pos[0], rect.ymin + scopes->track_pos[1], 0.f);
			glScissor(ar->winrct.xmin + rect.xmin,
			          ar->winrct.ymin + rect.ymin,
			          BLI_rctf_size_x(&rect),
			          BLI_rctf_size_y(&rect));

			GPU_basic_shader_bind_enable(GPU_SHADER_LINE);

			for (int a = 0; a < 2; a++) {
				if (a == 1) {
					GPU_basic_shader_bind_enable(GPU_SHADER_STIPPLE);
					GPU_basic_shader_line_stipple(3, 0xAAAA);
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

			GPU_basic_shader_bind_disable(GPU_SHADER_LINE | GPU_SHADER_STIPPLE);
		}

		glPopMatrix();

		ok = true;
	}

	if (!ok) {
		glColor4f(0.f, 0.f, 0.f, 0.3f);
		UI_draw_roundbox_corner_set(UI_CNR_ALL);
		UI_draw_roundbox_gl_mode(GL_POLYGON, rect.xmin - 1, rect.ymin, rect.xmax + 1, rect.ymax + 1, 3.0f);
	}

	/* outline */
	draw_scope_end(&rect, scissor);

	glDisable(GL_BLEND);
}

void ui_draw_but_NODESOCKET(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), const rcti *recti)
{
	static const float size = 5.0f;

	/* 16 values of sin function */
	const float si[16] = {
	    0.00000000f, 0.39435585f, 0.72479278f, 0.93775213f,
	    0.99871650f, 0.89780453f, 0.65137248f, 0.29936312f,
	    -0.10116832f, -0.48530196f, -0.79077573f, -0.96807711f,
	    -0.98846832f, -0.84864425f, -0.57126821f, -0.20129852f
	};
	/* 16 values of cos function */
	const float co[16] = {
	    1.00000000f, 0.91895781f, 0.68896691f, 0.34730525f,
	    -0.05064916f, -0.44039415f, -0.75875812f, -0.95413925f,
	    -0.99486932f, -0.87434661f, -0.61210598f, -0.25065253f,
	    0.15142777f, 0.52896401f, 0.82076344f, 0.97952994f,
	};

	GLint scissor[4];

	/* need scissor test, can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);

	rcti scissor_new = {
		.xmin = ar->winrct.xmin + recti->xmin,
		.ymin = ar->winrct.ymin + recti->ymin,
		.xmax = ar->winrct.xmin + recti->xmax,
		.ymax = ar->winrct.ymin + recti->ymax
	};

	BLI_rcti_isect(&scissor_new, &ar->winrct, &scissor_new);
	glScissor(scissor_new.xmin,
	          scissor_new.ymin,
	          BLI_rcti_size_x(&scissor_new),
	          BLI_rcti_size_y(&scissor_new));

	glColor4ubv(but->col);

	float x = 0.5f * (recti->xmin + recti->xmax);
	float y = 0.5f * (recti->ymin + recti->ymax);

	glEnable(GL_BLEND);
	glBegin(GL_POLYGON);
	for (int a = 0; a < 16; a++)
		glVertex2f(x + size * si[a], y + size * co[a]);
	glEnd();

	glColor4ub(0, 0, 0, 150);
	glLineWidth(1);
	glEnable(GL_LINE_SMOOTH);
	glBegin(GL_LINE_LOOP);
	for (int a = 0; a < 16; a++)
		glVertex2f(x + size * si[a], y + size * co[a]);
	glEnd();
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);

	/* restore scissortest */
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
}

/* ****************************************************** */


static void ui_shadowbox(float minx, float miny, float maxx, float maxy, float shadsize, unsigned char alpha)
{
	/* right quad */
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(maxx, miny);
	glVertex2f(maxx, maxy - 0.3f * shadsize);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx + shadsize, maxy - 0.75f * shadsize);
	glVertex2f(maxx + shadsize, miny);

	/* corner shape */
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(maxx, miny);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx + shadsize, miny);
	glVertex2f(maxx + 0.7f * shadsize, miny - 0.7f * shadsize);
	glVertex2f(maxx, miny - shadsize);

	/* bottom quad */
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(minx + 0.3f * shadsize, miny);
	glVertex2f(maxx, miny);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx, miny - shadsize);
	glVertex2f(minx + 0.5f * shadsize, miny - shadsize);
}

void UI_draw_box_shadow(unsigned char alpha, float minx, float miny, float maxx, float maxy)
{
	glEnable(GL_BLEND);

	glBegin(GL_QUADS);

	/* accumulated outline boxes to make shade not linear, is more pleasant */
	ui_shadowbox(minx, miny, maxx, maxy, 11.0, (20 * alpha) >> 8);
	ui_shadowbox(minx, miny, maxx, maxy, 7.0, (40 * alpha) >> 8);
	ui_shadowbox(minx, miny, maxx, maxy, 5.0, (80 * alpha) >> 8);

	glEnd();

	glDisable(GL_BLEND);
}


void ui_draw_dropshadow(const rctf *rct, float radius, float aspect, float alpha, int UNUSED(select))
{
	float rad;

	if (radius > (BLI_rctf_size_y(rct) - 10.0f) / 2.0f)
		rad = (BLI_rctf_size_y(rct) - 10.0f) / 2.0f;
	else
		rad = radius;

	int a, i = 12;
#if 0
	if (select) {
		a = i * aspect; /* same as below */
	}
	else
#endif
	{
		a = i * aspect;
	}

	glEnable(GL_BLEND);

	const float dalpha = alpha * 2.0f / 255.0f;
	float calpha = dalpha;
	for (; i--; a -= aspect) {
		/* alpha ranges from 2 to 20 or so */
		glColor4f(0.0f, 0.0f, 0.0f, calpha);
		calpha += dalpha;

		UI_draw_roundbox_gl_mode(GL_POLYGON, rct->xmin - a, rct->ymin - a, rct->xmax + a, rct->ymax - 10.0f + a, rad + a);
	}

	/* outline emphasis */
	glEnable(GL_LINE_SMOOTH);
	glColor4ub(0, 0, 0, 100);
	UI_draw_roundbox_gl_mode(GL_LINE_LOOP, rct->xmin - 0.5f, rct->ymin - 0.5f, rct->xmax + 0.5f, rct->ymax + 0.5f, radius + 0.5f);
	glDisable(GL_LINE_SMOOTH);

	glDisable(GL_BLEND);
}

/**
 * Reset GL state (keep minimal).
 *
 * \note Blender's internal code doesn't assume these are reset,
 * but external callbacks may depend on their state.
 */
void UI_reinit_gl_state(void)
{
	glLineWidth(1.0f);
	glPointSize(1.0f);
}

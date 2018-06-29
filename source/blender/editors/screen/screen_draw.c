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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/screen/screen_draw.c
 *  \ingroup edscr
 */

#include "BIF_gl.h"

#include "WM_api.h"

#include "ED_screen.h"

#include "screen_intern.h"

/**
 * Draw horizontal shape visualizing future joining (left as well right direction of future joining).
 */
static void draw_horizontal_join_shape(ScrArea *sa, char dir)
{
	vec2f points[10];
	short i;
	float w, h;
	float width = sa->v3->vec.x - sa->v1->vec.x;
	float height = sa->v3->vec.y - sa->v1->vec.y;

	if (height < width) {
		h = height / 8;
		w = height / 4;
	}
	else {
		h = width / 8;
		w = width / 4;
	}

	points[0].x = sa->v1->vec.x;
	points[0].y = sa->v1->vec.y + height / 2;

	points[1].x = sa->v1->vec.x;
	points[1].y = sa->v1->vec.y;

	points[2].x = sa->v4->vec.x - w;
	points[2].y = sa->v4->vec.y;

	points[3].x = sa->v4->vec.x - w;
	points[3].y = sa->v4->vec.y + height / 2 - 2 * h;

	points[4].x = sa->v4->vec.x - 2 * w;
	points[4].y = sa->v4->vec.y + height / 2;

	points[5].x = sa->v4->vec.x - w;
	points[5].y = sa->v4->vec.y + height / 2 + 2 * h;

	points[6].x = sa->v3->vec.x - w;
	points[6].y = sa->v3->vec.y;

	points[7].x = sa->v2->vec.x;
	points[7].y = sa->v2->vec.y;

	points[8].x = sa->v4->vec.x;
	points[8].y = sa->v4->vec.y + height / 2 - h;

	points[9].x = sa->v4->vec.x;
	points[9].y = sa->v4->vec.y + height / 2 + h;

	if (dir == 'l') {
		/* when direction is left, then we flip direction of arrow */
		float cx = sa->v1->vec.x + width;
		for (i = 0; i < 10; i++) {
			points[i].x -= cx;
			points[i].x = -points[i].x;
			points[i].x += sa->v1->vec.x;
		}
	}

	glBegin(GL_POLYGON);
	for (i = 0; i < 5; i++)
		glVertex2f(points[i].x, points[i].y);
	glEnd();
	glBegin(GL_POLYGON);
	for (i = 4; i < 8; i++)
		glVertex2f(points[i].x, points[i].y);
	glVertex2f(points[0].x, points[0].y);
	glEnd();

	glRectf(points[2].x, points[2].y, points[8].x, points[8].y);
	glRectf(points[6].x, points[6].y, points[9].x, points[9].y);
}

/**
 * Draw vertical shape visualizing future joining (up/down direction).
 */
static void draw_vertical_join_shape(ScrArea *sa, char dir)
{
	vec2f points[10];
	short i;
	float w, h;
	float width = sa->v3->vec.x - sa->v1->vec.x;
	float height = sa->v3->vec.y - sa->v1->vec.y;

	if (height < width) {
		h = height / 4;
		w = height / 8;
	}
	else {
		h = width / 4;
		w = width / 8;
	}

	points[0].x = sa->v1->vec.x + width / 2;
	points[0].y = sa->v3->vec.y;

	points[1].x = sa->v2->vec.x;
	points[1].y = sa->v2->vec.y;

	points[2].x = sa->v1->vec.x;
	points[2].y = sa->v1->vec.y + h;

	points[3].x = sa->v1->vec.x + width / 2 - 2 * w;
	points[3].y = sa->v1->vec.y + h;

	points[4].x = sa->v1->vec.x + width / 2;
	points[4].y = sa->v1->vec.y + 2 * h;

	points[5].x = sa->v1->vec.x + width / 2 + 2 * w;
	points[5].y = sa->v1->vec.y + h;

	points[6].x = sa->v4->vec.x;
	points[6].y = sa->v4->vec.y + h;

	points[7].x = sa->v3->vec.x;
	points[7].y = sa->v3->vec.y;

	points[8].x = sa->v1->vec.x + width / 2 - w;
	points[8].y = sa->v1->vec.y;

	points[9].x = sa->v1->vec.x + width / 2 + w;
	points[9].y = sa->v1->vec.y;

	if (dir == 'u') {
		/* when direction is up, then we flip direction of arrow */
		float cy = sa->v1->vec.y + height;
		for (i = 0; i < 10; i++) {
			points[i].y -= cy;
			points[i].y = -points[i].y;
			points[i].y += sa->v1->vec.y;
		}
	}

	glBegin(GL_POLYGON);
	for (i = 0; i < 5; i++)
		glVertex2f(points[i].x, points[i].y);
	glEnd();
	glBegin(GL_POLYGON);
	for (i = 4; i < 8; i++)
		glVertex2f(points[i].x, points[i].y);
	glVertex2f(points[0].x, points[0].y);
	glEnd();

	glRectf(points[2].x, points[2].y, points[8].x, points[8].y);
	glRectf(points[6].x, points[6].y, points[9].x, points[9].y);
}

/**
 * Draw join shape due to direction of joining.
 */
static void draw_join_shape(ScrArea *sa, char dir)
{
	if (dir == 'u' || dir == 'd') {
		draw_vertical_join_shape(sa, dir);
	}
	else {
		draw_horizontal_join_shape(sa, dir);
	}
}

/**
 * Draw screen area darker with arrow (visualization of future joining).
 */
static void scrarea_draw_shape_dark(ScrArea *sa, char dir)
{
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4ub(0, 0, 0, 50);
	draw_join_shape(sa, dir);
}

/**
 * Draw screen area ligher with arrow shape ("eraser" of previous dark shape).
 */
static void scrarea_draw_shape_light(ScrArea *sa, char UNUSED(dir))
{
	glBlendFunc(GL_DST_COLOR, GL_SRC_ALPHA);
	/* value 181 was hardly computed: 181~105 */
	glColor4ub(255, 255, 255, 50);
	/* draw_join_shape(sa, dir); */
	glRecti(sa->v1->vec.x, sa->v1->vec.y, sa->v3->vec.x, sa->v3->vec.y);
}

static void drawscredge_area_draw(int sizex, int sizey, short x1, short y1, short x2, short y2)
{
	/* right border area */
	if (x2 < sizex - 1) {
		glVertex2s(x2, y1);
		glVertex2s(x2, y2);
	}

	/* left border area */
	if (x1 > 0) { /* otherwise it draws the emboss of window over */
		glVertex2s(x1, y1);
		glVertex2s(x1, y2);
	}

	/* top border area */
	if (y2 < sizey - 1) {
		glVertex2s(x1, y2);
		glVertex2s(x2, y2);
	}

	/* bottom border area */
	if (y1 > 0) {
		glVertex2s(x1, y1);
		glVertex2s(x2, y1);
	}
}

/**
 * \brief Screen edges drawing.
 */
static void drawscredge_area(ScrArea *sa, int sizex, int sizey)
{
	short x1 = sa->v1->vec.x;
	short y1 = sa->v1->vec.y;
	short x2 = sa->v3->vec.x;
	short y2 = sa->v3->vec.y;

	drawscredge_area_draw(sizex, sizey, x1, y1, x2, y2);
}

/**
 * Only for edge lines between areas.
 */
void ED_screen_draw_edges(wmWindow *win)
{
	const int winsize_x = WM_window_pixels_x(win);
	const int winsize_y = WM_window_pixels_y(win);

	ScrArea *sa;

	wmSubWindowSet(win, win->screen->mainwin);

	/* Note: first loop only draws if U.pixelsize > 1, skip otherwise */
	if (U.pixelsize > 1.0f) {
		/* FIXME: doesn't our glLineWidth already scale by U.pixelsize? */
		glLineWidth((2.0f * U.pixelsize) - 1);
		glColor3ub(0x50, 0x50, 0x50);
		glBegin(GL_LINES);
		for (sa = win->screen->areabase.first; sa; sa = sa->next)
			drawscredge_area(sa, winsize_x, winsize_y);
		glEnd();
	}

	glLineWidth(1);
	glColor3ub(0, 0, 0);
	glBegin(GL_LINES);
	for (sa = win->screen->areabase.first; sa; sa = sa->next) {
		drawscredge_area(sa, winsize_x, winsize_y);
	}
	glEnd();

	win->screen->do_draw = false;
}

/**
 * The blended join arrows.
 *
 * \param sa1: Area from which the resultant originates.
 * \param sa2: Target area that will be replaced.
 */
void ED_screen_draw_join_shape(ScrArea *sa1, ScrArea *sa2)
{
	glLineWidth(1);

	/* blended join arrow */
	int dir = area_getorientation(sa1, sa2);
	int dira = -1;
	if (dir != -1) {
		switch (dir) {
			case 0: /* W */
				dir = 'r';
				dira = 'l';
				break;
			case 1: /* N */
				dir = 'd';
				dira = 'u';
				break;
			case 2: /* E */
				dir = 'l';
				dira = 'r';
				break;
			case 3: /* S */
				dir = 'u';
				dira = 'd';
				break;
		}
		glEnable(GL_BLEND);
		scrarea_draw_shape_dark(sa2, dir);
		scrarea_draw_shape_light(sa1, dira);
		glDisable(GL_BLEND);
	}
}

void ED_screen_draw_split_preview(ScrArea *sa, const int dir, const float fac)
{
	/* splitpoint */
	glEnable(GL_BLEND);
	glBegin(GL_LINES);
	glColor4ub(255, 255, 255, 100);

	if (dir == 'h') {
		const float y = (1 - fac) * sa->totrct.ymin + fac * sa->totrct.ymax;
		glVertex2s(sa->totrct.xmin, y);
		glVertex2s(sa->totrct.xmax, y);
		glColor4ub(0, 0, 0, 100);
		glVertex2s(sa->totrct.xmin, y + 1);
		glVertex2s(sa->totrct.xmax, y + 1);
	}
	else {
		const float x = (1 - fac) * sa->totrct.xmin + fac * sa->totrct.xmax;
		glVertex2s(x, sa->totrct.ymin);
		glVertex2s(x, sa->totrct.ymax);
		glColor4ub(0, 0, 0, 100);
		glVertex2s(x + 1, sa->totrct.ymin);
		glVertex2s(x + 1, sa->totrct.ymax);
	}
	glEnd();
	glDisable(GL_BLEND);
}

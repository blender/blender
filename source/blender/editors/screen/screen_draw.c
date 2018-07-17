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

#include "ED_screen.h"

#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "BLI_math.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "screen_intern.h"

/**
 * Draw horizontal shape visualizing future joining (left as well right direction of future joining).
 */
static void draw_horizontal_join_shape(ScrArea *sa, char dir, unsigned int pos)
{
	const float width = screen_geom_area_width(sa) - 1;
	const float height = screen_geom_area_height(sa) - 1;
	vec2f points[10];
	short i;
	float w, h;

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

	immBegin(GPU_PRIM_TRI_FAN, 5);

	for (i = 0; i < 5; i++) {
		immVertex2f(pos, points[i].x, points[i].y);
	}

	immEnd();

	immBegin(GPU_PRIM_TRI_FAN, 5);

	for (i = 4; i < 8; i++) {
		immVertex2f(pos, points[i].x, points[i].y);
	}

	immVertex2f(pos, points[0].x, points[0].y);
	immEnd();

	immRectf(pos, points[2].x, points[2].y, points[8].x, points[8].y);
	immRectf(pos, points[6].x, points[6].y, points[9].x, points[9].y);
}

/**
 * Draw vertical shape visualizing future joining (up/down direction).
 */
static void draw_vertical_join_shape(ScrArea *sa, char dir, unsigned int pos)
{
	const float width = screen_geom_area_width(sa) - 1;
	const float height = screen_geom_area_height(sa) - 1;
	vec2f points[10];
	short i;
	float w, h;

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

	immBegin(GPU_PRIM_TRI_FAN, 5);

	for (i = 0; i < 5; i++) {
		immVertex2f(pos, points[i].x, points[i].y);
	}

	immEnd();

	immBegin(GPU_PRIM_TRI_FAN, 5);

	for (i = 4; i < 8; i++) {
		immVertex2f(pos, points[i].x, points[i].y);
	}

	immVertex2f(pos, points[0].x, points[0].y);
	immEnd();

	immRectf(pos, points[2].x, points[2].y, points[8].x, points[8].y);
	immRectf(pos, points[6].x, points[6].y, points[9].x, points[9].y);
}

/**
 * Draw join shape due to direction of joining.
 */
static void draw_join_shape(ScrArea *sa, char dir, unsigned int pos)
{
	if (dir == 'u' || dir == 'd') {
		draw_vertical_join_shape(sa, dir, pos);
	}
	else {
		draw_horizontal_join_shape(sa, dir, pos);
	}
}

#define CORNER_RESOLUTION 10
static void drawscredge_corner_geometry(
        int sizex, int sizey,
        int corner_x, int corner_y,
        int center_x, int center_y,
        double angle_offset,
        const float *color)
{
	const int radius = ABS(corner_x - center_x);
	const int line_thickness = U.pixelsize;

	if (corner_x < center_x) {
		if (corner_x > 0.0f) {
			/* Left (internal) edge. */
			corner_x += line_thickness;
			center_x += line_thickness;
		}
	}
	else {
		/* Right (internal) edge. */
		if (corner_x < sizex - 1) {
			corner_x += 1 - line_thickness;
			center_x += 1 - line_thickness;
		}
		else {
			/* Corner case, extreme right edge. */
			corner_x += 1;
			center_x += 1;
		}
	}

	if (corner_y < center_y) {
		if (corner_y > 0.0f) {
			/* Bottom (internal) edge. */
			corner_y += line_thickness;
			center_y += line_thickness;
		}
	}
	else {
		/* Top (internal) edge. */
		if (corner_y < sizey) {
			corner_y += 1 - line_thickness;
			center_y += 1 - line_thickness;
		}
	}

	float tri_array[CORNER_RESOLUTION + 1][2];

	tri_array[0][0] = corner_x;
	tri_array[0][1] = corner_y;

	for (int i = 0; i < CORNER_RESOLUTION; i++) {
		double angle = angle_offset + (M_PI_2 * ((float)i / (CORNER_RESOLUTION - 1)));
		float x = center_x + (radius * cos(angle));
		float y = center_y + (radius * sin(angle));
		tri_array[i + 1][0] = x;
		tri_array[i + 1][1] = y;
	}

	UI_draw_anti_fan(tri_array, CORNER_RESOLUTION + 1, color);
}

#undef CORNER_RESOLUTION

static void drawscredge_corner(ScrArea *sa, int sizex, int sizey)
{
	int size = 10 * U.pixelsize;
	float color[4] = {0};
	UI_GetThemeColor4fv(TH_EDITOR_OUTLINE, color);

	/* Bottom-Left. */
	drawscredge_corner_geometry(sizex, sizey,
	                            sa->v1->vec.x,
	                            sa->v1->vec.y,
	                            sa->v1->vec.x + size,
	                            sa->v1->vec.y + size,
	                            M_PI_2 * 2.0f,
	                            color);

	/* Top-Left. */
	drawscredge_corner_geometry(sizex, sizey,
	                            sa->v2->vec.x,
	                            sa->v2->vec.y,
	                            sa->v2->vec.x + size,
	                            sa->v2->vec.y - size,
	                            M_PI_2,
	                            color);

	/* Top-Right. */
	drawscredge_corner_geometry(sizex, sizey,
	                            sa->v3->vec.x,
	                            sa->v3->vec.y,
	                            sa->v3->vec.x - size,
	                            sa->v3->vec.y - size,
	                            0.0f,
	                            color);

	/* Bottom-Right. */
	drawscredge_corner_geometry(sizex, sizey,
	                            sa->v4->vec.x,
	                            sa->v4->vec.y,
	                            sa->v4->vec.x - size,
	                            sa->v4->vec.y + size,
	                            M_PI_2 * 3.0f,
	                            color);

	/* Wrap up the corners with a nice embossing. */
	rcti rect = sa->totrct;

	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	immUniformColor4fv(color);
	immBeginAtMost(GPU_PRIM_LINES, 8);

	/* Right. */
	immVertex2f(pos, rect.xmax, rect.ymax);
	immVertex2f(pos, rect.xmax, rect.ymin);

	/* Bottom. */
	immVertex2f(pos, rect.xmax, rect.ymin);
	immVertex2f(pos, rect.xmin, rect.ymin);

	/* Left. */
	immVertex2f(pos, rect.xmin, rect.ymin);
	immVertex2f(pos, rect.xmin, rect.ymax);

	/* Top. */
	immVertex2f(pos, rect.xmin, rect.ymax);
	immVertex2f(pos, rect.xmax, rect.ymax);

	immEnd();
	immUnbindProgram();
}

/**
 * Draw screen area darker with arrow (visualization of future joining).
 */
static void scrarea_draw_shape_dark(ScrArea *sa, char dir, unsigned int pos)
{
	GPU_blend_set_func_separate(GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
	immUniformColor4ub(0, 0, 0, 50);

	draw_join_shape(sa, dir, pos);
}

/**
 * Draw screen area ligher with arrow shape ("eraser" of previous dark shape).
 */
static void scrarea_draw_shape_light(ScrArea *sa, char UNUSED(dir), unsigned int pos)
{
	GPU_blend_set_func(GPU_DST_COLOR, GPU_SRC_ALPHA);
	/* value 181 was hardly computed: 181~105 */
	immUniformColor4ub(255, 255, 255, 50);
	/* draw_join_shape(sa, dir); */

	immRectf(pos, sa->v1->vec.x, sa->v1->vec.y, sa->v3->vec.x, sa->v3->vec.y);
}

static void drawscredge_area_draw(int sizex, int sizey, short x1, short y1, short x2, short y2, unsigned int pos)
{
	int count = 0;

	if (x2 < sizex - 1) count += 2;
	if (x1 > 0) count += 2;
	if (y2 < sizey - 1) count += 2;
	if (y1 > 0) count += 2;

	if (count == 0) {
		return;
	}

	immBegin(GPU_PRIM_LINES, count);

	/* right border area */
	if (x2 < sizex - 1) {
		immVertex2f(pos, x2, y1);
		immVertex2f(pos, x2, y2);
	}

	/* left border area */
	if (x1 > 0) { /* otherwise it draws the emboss of window over */
		immVertex2f(pos, x1, y1);
		immVertex2f(pos, x1, y2);
	}

	/* top border area */
	if (y2 < sizey - 1) {
		immVertex2f(pos, x1, y2);
		immVertex2f(pos, x2, y2);
	}

	/* bottom border area */
	if (y1 > 0) {
		immVertex2f(pos, x1, y1);
		immVertex2f(pos, x2, y1);
	}

	immEnd();
}

/**
 * \brief Screen edges drawing.
 */
static void drawscredge_area(ScrArea *sa, int sizex, int sizey, unsigned int pos)
{
	short x1 = sa->v1->vec.x;
	short y1 = sa->v1->vec.y;
	short x2 = sa->v3->vec.x;
	short y2 = sa->v3->vec.y;

	drawscredge_area_draw(sizex, sizey, x1, y1, x2, y2, pos);
}

/**
 * Only for edge lines between areas.
 */
void ED_screen_draw_edges(wmWindow *win)
{
	bScreen *screen = WM_window_get_active_screen(win);
	const int winsize_x = WM_window_pixels_x(win);
	const int winsize_y = WM_window_pixels_y(win);

	ScrArea *sa;

	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	/* Note: first loop only draws if U.pixelsize > 1, skip otherwise */
	if (U.pixelsize > 1.0f) {
		/* FIXME: doesn't our glLineWidth already scale by U.pixelsize? */
		GPU_line_width((2.0f * U.pixelsize) - 1);
		immUniformThemeColor(TH_EDITOR_OUTLINE);

		for (sa = screen->areabase.first; sa; sa = sa->next) {
			drawscredge_area(sa, winsize_x, winsize_y, pos);
		}
	}

	GPU_line_width(1);
	immUniformThemeColor(TH_EDITOR_OUTLINE);

	for (sa = screen->areabase.first; sa; sa = sa->next) {
		drawscredge_area(sa, winsize_x, winsize_y, pos);
	}

	immUnbindProgram();

	for (sa = screen->areabase.first; sa; sa = sa->next) {
		drawscredge_corner(sa, winsize_x, winsize_y);
	}

	screen->do_draw = false;
}

/**
 * The blended join arrows.
 *
 * \param sa1: Area from which the resultant originates.
 * \param sa2: Target area that will be replaced.
 */
void ED_screen_draw_join_shape(ScrArea *sa1, ScrArea *sa2)
{
	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	GPU_line_width(1);

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

		GPU_blend(true);

		scrarea_draw_shape_dark(sa2, dir, pos);
		scrarea_draw_shape_light(sa1, dira, pos);

		GPU_blend(false);
	}

	immUnbindProgram();
}

void ED_screen_draw_split_preview(ScrArea *sa, const int dir, const float fac)
{
	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

	/* splitpoint */
	GPU_blend(true);
	immUniformColor4ub(255, 255, 255, 100);

	immBegin(GPU_PRIM_LINES, 2);

	if (dir == 'h') {
		const float y = (1 - fac) * sa->totrct.ymin + fac * sa->totrct.ymax;

		immVertex2f(pos, sa->totrct.xmin, y);
		immVertex2f(pos, sa->totrct.xmax, y);

		immEnd();

		immUniformColor4ub(0, 0, 0, 100);

		immBegin(GPU_PRIM_LINES, 2);

		immVertex2f(pos, sa->totrct.xmin, y + 1);
		immVertex2f(pos, sa->totrct.xmax, y + 1);

		immEnd();
	}
	else {
		BLI_assert(dir == 'v');
		const float x = (1 - fac) * sa->totrct.xmin + fac * sa->totrct.xmax;

		immVertex2f(pos, x, sa->totrct.ymin);
		immVertex2f(pos, x, sa->totrct.ymax);

		immEnd();

		immUniformColor4ub(0, 0, 0, 100);

		immBegin(GPU_PRIM_LINES, 2);

		immVertex2f(pos, x + 1, sa->totrct.ymin);
		immVertex2f(pos, x + 1, sa->totrct.ymax);

		immEnd();
	}

	GPU_blend(false);

	immUnbindProgram();
}


/* -------------------------------------------------------------------- */
/* Screen Thumbnail Preview */

/**
 * Calculates a scale factor to squash the preview for \a screen into a rectangle of given size and aspect.
 */
static void screen_preview_scale_get(
        const bScreen *screen, float size_x, float size_y,
        const float asp[2],
        float r_scale[2])
{
	float max_x = 0, max_y = 0;

	for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
		max_x = MAX2(max_x, sa->totrct.xmax);
		max_y = MAX2(max_y, sa->totrct.ymax);
	}
	r_scale[0] = (size_x * asp[0]) / max_x;
	r_scale[1] = (size_y * asp[1]) / max_y;
}

static void screen_preview_draw_areas(const bScreen *screen, const float scale[2], const float col[4],
                                      const float ofs_between_areas)
{
	const float ofs_h = ofs_between_areas * 0.5f;
	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	immUniformColor4fv(col);

	for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
		rctf rect = {
			.xmin = sa->totrct.xmin * scale[0] + ofs_h,
			.xmax = sa->totrct.xmax * scale[0] - ofs_h,
			.ymin = sa->totrct.ymin * scale[1] + ofs_h,
			.ymax = sa->totrct.ymax * scale[1] - ofs_h
		};

		immBegin(GPU_PRIM_TRI_FAN, 4);
		immVertex2f(pos, rect.xmin, rect.ymin);
		immVertex2f(pos, rect.xmax, rect.ymin);
		immVertex2f(pos, rect.xmax, rect.ymax);
		immVertex2f(pos, rect.xmin, rect.ymax);
		immEnd();
	}

	immUnbindProgram();
}

static void screen_preview_draw(const bScreen *screen, int size_x, int size_y)
{
	const float asp[2] = {1.0f, 0.8f}; /* square previews look a bit ugly */
	/* could use theme color (tui.wcol_menu_item.text), but then we'd need to regenerate all previews when changing */
	const float col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	float scale[2];

	wmOrtho2(0.0f, size_x, 0.0f, size_y);
	/* center */
	GPU_matrix_push();
	GPU_matrix_identity_set();
	GPU_matrix_translate_2f(size_x * (1.0f - asp[0]) * 0.5f, size_y * (1.0f - asp[1]) * 0.5f);

	screen_preview_scale_get(screen, size_x, size_y, asp, scale);
	screen_preview_draw_areas(screen, scale, col, 1.5f);

	GPU_matrix_pop();
}

/**
 * Render the preview for a screen layout in \a screen.
 */
void ED_screen_preview_render(const bScreen *screen, int size_x, int size_y, unsigned int *r_rect)
{
	char err_out[256] = "unknown";
	GPUOffScreen *offscreen = GPU_offscreen_create(size_x, size_y, 0, true, false, err_out);

	GPU_offscreen_bind(offscreen, true);
	GPU_clear_color(0.0, 0.0, 0.0, 0.0);
	GPU_clear(GPU_COLOR_BIT | GPU_DEPTH_BIT);

	screen_preview_draw(screen, size_x, size_y);

	GPU_offscreen_read_pixels(offscreen, GL_UNSIGNED_BYTE, r_rect);
	GPU_offscreen_unbind(offscreen, true);

	GPU_offscreen_free(offscreen);
}

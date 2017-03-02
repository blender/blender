/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/intern/draw_view.c
 *  \ingroup draw
 *
 * Contains dynamic drawing using immediate mode
 */

#include "DNA_brush_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "GPU_draw.h"
#include "GPU_shader.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "UI_resources.h"

#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_unit.h"

#include "DRW_render.h"

#include "view3d_intern.h"

#include "draw_view.h"

/* ******************** region info ***************** */

void DRW_draw_region_info(void)
{
	const bContext *C = DRW_get_context();
	ARegion *ar = CTX_wm_region(C);

	DRW_draw_cursor();
	view3d_draw_region_info(C, ar);
}

/* ************************* Grid ************************** */

static void gridline_range(double x0, double dx, double max, int *r_first, int *r_count)
{
	/* determine range of gridlines that appear in this Area -- similar calc but separate ranges for x & y
	* x0 is gridline 0, the axis in screen space
	* Area covers [0 .. max) pixels */

	int first = (int)ceil(-x0 / dx);
	int last = (int)floor((max - x0) / dx);

	if (first <= last) {
		*r_first = first;
		*r_count = last - first + 1;
	}
	else {
		*r_first = 0;
		*r_count = 0;
	}
}

static int gridline_count(ARegion *ar, double x0, double y0, double dx)
{
	/* x0 & y0 establish the "phase" of the grid within this 2D region
	* dx is the frequency, shared by x & y directions
	* pass in dx of smallest (highest precision) grid we want to draw */

	int first, x_ct, y_ct;

	gridline_range(x0, dx, ar->winx, &first, &x_ct);
	gridline_range(y0, dx, ar->winy, &first, &y_ct);

	int total_ct = x_ct + y_ct;

	return total_ct;
}

static bool drawgrid_draw(ARegion *ar, double x0, double y0, double dx, int skip_mod, unsigned pos, unsigned col, GLubyte col_value[3])
{
	/* skip every skip_mod lines relative to each axis; they will be overlaid by another drawgrid_draw
	* always skip exact x0 & y0 axes; they will be drawn later in color
	*
	* set grid color once, just before the first line is drawn
	* it's harmless to set same color for every line, or every vertex
	* but if no lines are drawn, color must not be set! */

	const float x_max = (float)ar->winx;
	const float y_max = (float)ar->winy;

	int first, ct;
	int x_ct = 0, y_ct = 0; /* count of lines actually drawn */
	int lines_skipped_for_next_unit = 0;

	/* draw vertical lines */
	gridline_range(x0, dx, x_max, &first, &ct);

	for (int i = first; i < first + ct; ++i) {
		if (i == 0)
			continue;
		else if (skip_mod && (i % skip_mod) == 0) {
			++lines_skipped_for_next_unit;
			continue;
		}

		if (x_ct == 0)
			immAttrib3ub(col, col_value[0], col_value[1], col_value[2]);

		float x = (float)(x0 + i * dx);
		immVertex2f(pos, x, 0.0f);
		immVertex2f(pos, x, y_max);
		++x_ct;
	}

	/* draw horizontal lines */
	gridline_range(y0, dx, y_max, &first, &ct);

	for (int i = first; i < first + ct; ++i) {
		if (i == 0)
			continue;
		else if (skip_mod && (i % skip_mod) == 0) {
			++lines_skipped_for_next_unit;
			continue;
		}

		if (x_ct + y_ct == 0)
			immAttrib3ub(col, col_value[0], col_value[1], col_value[2]);

		float y = (float)(y0 + i * dx);
		immVertex2f(pos, 0.0f, y);
		immVertex2f(pos, x_max, y);
		++y_ct;
	}

	return lines_skipped_for_next_unit > 0;
}

#define GRID_MIN_PX_D 6.0
#define GRID_MIN_PX_F 6.0f

static void drawgrid(UnitSettings *unit, ARegion *ar, View3D *v3d, const char **grid_unit)
{
	RegionView3D *rv3d = ar->regiondata;

	double fx = rv3d->persmat[3][0];
	double fy = rv3d->persmat[3][1];
	double fw = rv3d->persmat[3][3];

	const double wx = 0.5 * ar->winx;  /* use double precision to avoid rounding errors */
	const double wy = 0.5 * ar->winy;

	double x = wx * fx / fw;
	double y = wy * fy / fw;

	double vec4[4] = { v3d->grid, v3d->grid, 0.0, 1.0 };
	mul_m4_v4d(rv3d->persmat, vec4);
	fx = vec4[0];
	fy = vec4[1];
	fw = vec4[3];

	double dx = fabs(x - wx * fx / fw);
	if (dx == 0) dx = fabs(y - wy * fy / fw);

	x += wx;
	y += wy;

	/* now x, y, and dx have their final values
	* (x,y) is the world origin (0,0,0) mapped to Area-relative screen space
	* dx is the distance in pixels between grid lines -- same for horiz or vert grid lines */

	glLineWidth(1.0f);

#if 0 /* TODO: write to UI/widget depth buffer, not scene depth */
	glDepthMask(GL_FALSE);  /* disable write in zbuffer */
#endif

	VertexFormat *format = immVertexFormat();
	unsigned int pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
	unsigned int color = add_attrib(format, "color", COMP_U8, 3, NORMALIZE_INT_TO_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);

	unsigned char col[3], col2[3];
	UI_GetThemeColor3ubv(TH_GRID, col);

	if (unit->system) {
		const void *usys;
		int len;

		bUnit_GetSystem(unit->system, B_UNIT_LENGTH, &usys, &len);

		bool first = true;

		if (usys) {
			int i = len;
			while (i--) {
				double scalar = bUnit_GetScaler(usys, i);

				double dx_scalar = dx * scalar / (double)unit->scale_length;
				if (dx_scalar < (GRID_MIN_PX_D * 2.0)) {
					/* very very small grid items are less useful when dealing with units */
					continue;
				}

				if (first) {
					first = false;

					/* Store the smallest drawn grid size units name so users know how big each grid cell is */
					*grid_unit = bUnit_GetNameDisplay(usys, i);
					rv3d->gridview = (float)((scalar * (double)v3d->grid) / (double)unit->scale_length);

					int gridline_ct = gridline_count(ar, x, y, dx_scalar);
					if (gridline_ct == 0)
						goto drawgrid_cleanup; /* nothing to draw */

					immBegin(GL_LINES, gridline_ct * 2);
				}

				float blend_fac = 1.0f - ((GRID_MIN_PX_F * 2.0f) / (float)dx_scalar);
				/* tweak to have the fade a bit nicer */
				blend_fac = (blend_fac * blend_fac) * 2.0f;
				CLAMP(blend_fac, 0.3f, 1.0f);

				UI_GetThemeColorBlend3ubv(TH_HIGH_GRAD, TH_GRID, blend_fac, col2);

				const int skip_mod = (i == 0) ? 0 : (int)round(bUnit_GetScaler(usys, i - 1) / scalar);
				if (!drawgrid_draw(ar, x, y, dx_scalar, skip_mod, pos, color, col2))
					break;
			}
		}
	}
	else {
		const double sublines = v3d->gridsubdiv;
		const float  sublines_fl = v3d->gridsubdiv;

		int grids_to_draw = 2; /* first the faint fine grid, then the bold coarse grid */

		if (dx < GRID_MIN_PX_D) {
			rv3d->gridview *= sublines_fl;
			dx *= sublines;
			if (dx < GRID_MIN_PX_D) {
				rv3d->gridview *= sublines_fl;
				dx *= sublines;
				if (dx < GRID_MIN_PX_D) {
					rv3d->gridview *= sublines_fl;
					dx *= sublines;
					grids_to_draw = (dx < GRID_MIN_PX_D) ? 0 : 1;
				}
			}
		}
		else {
			if (dx > (GRID_MIN_PX_D * 10.0)) {  /* start blending in */
				rv3d->gridview /= sublines_fl;
				dx /= sublines;
				if (dx > (GRID_MIN_PX_D * 10.0)) {  /* start blending in */
					rv3d->gridview /= sublines_fl;
					dx /= sublines;
					if (dx > (GRID_MIN_PX_D * 10.0)) {
						grids_to_draw = 1;
					}
				}
			}
		}

		int gridline_ct = gridline_count(ar, x, y, dx);
		if (gridline_ct == 0)
			goto drawgrid_cleanup; /* nothing to draw */

		immBegin(GL_LINES, gridline_ct * 2);

		if (grids_to_draw == 2) {
			UI_GetThemeColorBlend3ubv(TH_HIGH_GRAD, TH_GRID, dx / (GRID_MIN_PX_D * 6.0), col2);
			if (drawgrid_draw(ar, x, y, dx, v3d->gridsubdiv, pos, color, col2))
				drawgrid_draw(ar, x, y, dx * sublines, 0, pos, color, col);
		}
		else if (grids_to_draw == 1) {
			drawgrid_draw(ar, x, y, dx, 0, pos, color, col);
		}
	}

	/* draw visible axes */
	/* horizontal line */
	if (0 <= y && y < ar->winy) {
		UI_make_axis_color(col, col2, ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT) ? 'Y' : 'X');
		immAttrib3ub(color, col2[0], col2[1], col2[2]);
		immVertex2f(pos, 0.0f, y);
		immVertex2f(pos, (float)ar->winx, y);
	}

	/* vertical line */
	if (0 <= x && x < ar->winx) {
		UI_make_axis_color(col, col2, ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM) ? 'Y' : 'Z');
		immAttrib3ub(color, col2[0], col2[1], col2[2]);
		immVertex2f(pos, x, 0.0f);
		immVertex2f(pos, x, (float)ar->winy);
	}

	immEnd();

drawgrid_cleanup:
	immUnbindProgram();

#if 0 /* depth write is left enabled above */
	glDepthMask(GL_TRUE);  /* enable write in zbuffer */
#endif
}

#undef DEBUG_GRID
#undef GRID_MIN_PX_D
#undef GRID_MIN_PX_F

static void drawfloor(Scene *scene, View3D *v3d, const char **grid_unit)
{
	/* draw only if there is something to draw */
	if (v3d->gridflag & (V3D_SHOW_FLOOR | V3D_SHOW_X | V3D_SHOW_Y | V3D_SHOW_Z)) {
		/* draw how many lines?
		* trunc(v3d->gridlines / 2) * 4
		* + 2 for xy axes (possibly with special colors)
		* + 1 for z axis (the only line not in xy plane)
		* even v3d->gridlines are honored, odd rounded down */
		const int gridlines = v3d->gridlines / 2;
		const float grid_scale = ED_view3d_grid_scale(scene, v3d, grid_unit);
		const float grid = gridlines * grid_scale;

		const bool show_floor = (v3d->gridflag & V3D_SHOW_FLOOR) && gridlines >= 1;

		bool show_axis_x = v3d->gridflag & V3D_SHOW_X;
		bool show_axis_y = v3d->gridflag & V3D_SHOW_Y;
		bool show_axis_z = v3d->gridflag & V3D_SHOW_Z;

		unsigned char col_grid[3], col_axis[3];

		glLineWidth(1.0f);

		UI_GetThemeColor3ubv(TH_GRID, col_grid);

		if (show_floor) {
			const unsigned vertex_ct = 2 * (gridlines * 4 + 2);
			const int sublines = v3d->gridsubdiv;

			unsigned char col_bg[3], col_grid_emphasise[3], col_grid_light[3];

			VertexFormat *format = immVertexFormat();
			unsigned int pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
			unsigned int color = add_attrib(format, "color", COMP_U8, 3, NORMALIZE_INT_TO_FLOAT);

			immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);

			immBegin(GL_LINES, vertex_ct);

			/* draw normal grid lines */
			UI_GetColorPtrShade3ubv(col_grid, col_grid_light, 10);

			for (int a = 1; a <= gridlines; a++) {
				/* skip emphasised divider lines */
				if (a % sublines != 0) {
					const float line = a * grid_scale;

					immAttrib3ubv(color, col_grid_light);

					immVertex2f(pos, -grid, -line);
					immVertex2f(pos, +grid, -line);
					immVertex2f(pos, -grid, +line);
					immVertex2f(pos, +grid, +line);

					immVertex2f(pos, -line, -grid);
					immVertex2f(pos, -line, +grid);
					immVertex2f(pos, +line, -grid);
					immVertex2f(pos, +line, +grid);
				}
			}

			/* draw emphasised grid lines */
			UI_GetThemeColor3ubv(TH_BACK, col_bg);
			/* emphasise division lines lighter instead of darker, if background is darker than grid */
			UI_GetColorPtrShade3ubv(col_grid, col_grid_emphasise,
				(col_grid[0] + col_grid[1] + col_grid[2] + 30 >
				col_bg[0] + col_bg[1] + col_bg[2]) ? 20 : -10);

			if (sublines <= gridlines) {
				immAttrib3ubv(color, col_grid_emphasise);

				for (int a = sublines; a <= gridlines; a += sublines) {
					const float line = a * grid_scale;

					immVertex2f(pos, -grid, -line);
					immVertex2f(pos, +grid, -line);
					immVertex2f(pos, -grid, +line);
					immVertex2f(pos, +grid, +line);

					immVertex2f(pos, -line, -grid);
					immVertex2f(pos, -line, +grid);
					immVertex2f(pos, +line, -grid);
					immVertex2f(pos, +line, +grid);
				}
			}

			/* draw X axis */
			if (show_axis_x) {
				show_axis_x = false; /* drawing now, won't need to draw later */
				UI_make_axis_color(col_grid, col_axis, 'X');
				immAttrib3ubv(color, col_axis);
			}
			else
				immAttrib3ubv(color, col_grid_emphasise);

			immVertex2f(pos, -grid, 0.0f);
			immVertex2f(pos, +grid, 0.0f);

			/* draw Y axis */
			if (show_axis_y) {
				show_axis_y = false; /* drawing now, won't need to draw later */
				UI_make_axis_color(col_grid, col_axis, 'Y');
				immAttrib3ubv(color, col_axis);
			}
			else
				immAttrib3ubv(color, col_grid_emphasise);

			immVertex2f(pos, 0.0f, -grid);
			immVertex2f(pos, 0.0f, +grid);

			immEnd();
			immUnbindProgram();

			/* done with XY plane */
		}

		if (show_axis_x || show_axis_y || show_axis_z) {
			/* draw axis lines -- sometimes grid floor is off, other times we still need to draw the Z axis */

			VertexFormat *format = immVertexFormat();
			unsigned int pos = add_attrib(format, "pos", COMP_F32, 3, KEEP_FLOAT);
			unsigned int color = add_attrib(format, "color", COMP_U8, 3, NORMALIZE_INT_TO_FLOAT);

			immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);
			immBegin(GL_LINES, (show_axis_x + show_axis_y + show_axis_z) * 2);

			if (show_axis_x) {
				UI_make_axis_color(col_grid, col_axis, 'X');
				immAttrib3ubv(color, col_axis);
				immVertex3f(pos, -grid, 0.0f, 0.0f);
				immVertex3f(pos, +grid, 0.0f, 0.0f);
			}

			if (show_axis_y) {
				UI_make_axis_color(col_grid, col_axis, 'Y');
				immAttrib3ubv(color, col_axis);
				immVertex3f(pos, 0.0f, -grid, 0.0f);
				immVertex3f(pos, 0.0f, +grid, 0.0f);
			}

			if (show_axis_z) {
				UI_make_axis_color(col_grid, col_axis, 'Z');
				immAttrib3ubv(color, col_axis);
				immVertex3f(pos, 0.0f, 0.0f, -grid);
				immVertex3f(pos, 0.0f, 0.0f, +grid);
			}

			immEnd();
			immUnbindProgram();
		}
	}
}

void DRW_draw_grid(void)
{
	/* TODO viewport
	 * Missing is the flags to check whether to draw it
	 * for now now we are using the flags in v3d itself.
	 *
	 * Also for now always assume depth is there, so we
	 * draw on top of it.
	 */
	/**
	 * Calculate pixel-size factor once, is used for lamps and object centers.
	 * Used by #ED_view3d_pixel_size and typically not accessed directly.
	 *
	 * \note #BKE_camera_params_compute_viewplane' also calculates a pixel-size value,
	 * passed to #RE_SetPixelSize, in ortho mode this is compatible with this value,
	 * but in perspective mode its offset by the near-clip.
	 *
	 * 'RegionView3D.pixsize' is used for viewport drawing, not rendering.
	 */
	const bContext *C = DRW_get_context();
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	ARegion *ar = CTX_wm_region(C);
	RegionView3D *rv3d = ar->regiondata;

	const bool draw_floor = (rv3d->view == RV3D_VIEW_USER) || (rv3d->persp != RV3D_ORTHO);
	const char *grid_unit = NULL;

	/* ortho grid goes first, does not write to depth buffer and doesn't need depth test so it will override
	 * objects if done last
	 * needs to be done always, gridview is adjusted in drawgrid() now, but only for ortho views.
	 */
	rv3d->gridview = ED_view3d_grid_scale(scene, v3d, &grid_unit);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	if (!draw_floor) {
		/* Do not get in front of overlays */
		glDepthMask(GL_FALSE);

		ED_region_pixelspace(ar);
		*(&grid_unit) = NULL;  /* drawgrid need this to detect/affect smallest valid unit... */
		drawgrid(&scene->unit, ar, v3d, &grid_unit);

		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf((float *)rv3d->winmat);
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf((float *)rv3d->viewmat);
	}
	else {
		glDepthMask(GL_TRUE);
		drawfloor(scene, v3d, &grid_unit);
	}
}

/* ************************* Background ************************** */

void DRW_draw_background(void)
{
	/* Just to make sure */
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	if (UI_GetThemeValue(TH_SHOW_BACK_GRAD)) {
		/* Gradient background Color */
		gpuMatrixBegin3D(); /* TODO: finish 2D API */

		glClear(GL_DEPTH_BUFFER_BIT);

		VertexFormat *format = immVertexFormat();
		unsigned pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
		unsigned color = add_attrib(format, "color", COMP_U8, 3, NORMALIZE_INT_TO_FLOAT);
		unsigned char col_hi[3], col_lo[3];

		immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);

		UI_GetThemeColor3ubv(TH_LOW_GRAD, col_lo);
		UI_GetThemeColor3ubv(TH_HIGH_GRAD, col_hi);

		immBegin(GL_QUADS, 4);
		immAttrib3ubv(color, col_lo);
		immVertex2f(pos, -1.0f, -1.0f);
		immVertex2f(pos, 1.0f, -1.0f);

		immAttrib3ubv(color, col_hi);
		immVertex2f(pos, 1.0f, 1.0f);
		immVertex2f(pos, -1.0f, 1.0f);
		immEnd();

		immUnbindProgram();

		gpuMatrixEnd();
	}
	else {
		/* Solid background Color */
		UI_ThemeClearColorAlpha(TH_HIGH_GRAD, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
}

/* **************************** 3D Cursor ******************************** */

static bool is_cursor_visible(Scene *scene, SceneLayer *sl)
{
	Object *ob = OBACT_NEW;

	/* don't draw cursor in paint modes, but with a few exceptions */
	if (ob && ob->mode & OB_MODE_ALL_PAINT) {
		/* exception: object is in weight paint and has deforming armature in pose mode */
		if (ob->mode & OB_MODE_WEIGHT_PAINT) {
			if (BKE_object_pose_armature_get(ob) != NULL) {
				return true;
			}
		}
		/* exception: object in texture paint mode, clone brush, use_clone_layer disabled */
		else if (ob->mode & OB_MODE_TEXTURE_PAINT) {
			const Paint *p = BKE_paint_get_active(scene);

			if (p && p->brush && p->brush->imagepaint_tool == PAINT_TOOL_CLONE) {
				if ((scene->toolsettings->imapaint.flag & IMAGEPAINT_PROJECT_LAYER_CLONE) == 0) {
					return true;
				}
			}
		}

		/* no exception met? then don't draw cursor! */
		return false;
	}

	return true;
}

void DRW_draw_cursor(void)
{
	const bContext *C = DRW_get_context();
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	Scene *scene = CTX_data_scene(C);
	SceneLayer *sl = CTX_data_scene_layer(C);

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glLineWidth(1.0f);

	if (is_cursor_visible(scene, sl)) {
		float *co = ED_view3d_cursor3d_get(scene, v3d);
		unsigned char crosshair_color[3];

		const float f5 = 0.25f;
		const float f10 = 0.5f;
		const float f20 = 1.0f;

		VertexFormat *format = immVertexFormat();
		unsigned int pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
		unsigned int color = add_attrib(format, "color", COMP_U8, 3, NORMALIZE_INT_TO_FLOAT);
		unsigned int wpos = add_attrib(format, "world_pos", COMP_F32, 3, KEEP_FLOAT);

		/* XXX Using instance shader without instance */
		immBindBuiltinProgram(GPU_SHADER_3D_SCREENSPACE_VARIYING_COLOR);
		immUniform1f("size", U.widget_unit);
		immUniform1f("pixel_size", *DRW_viewport_pixelsize_get());
		immUniformArray3fv("screen_vecs", DRW_viewport_screenvecs_get(), 2);
		immUniformMatrix4fv("ViewProjectionMatrix", rv3d->persmat);

		const int segments = 16;

		immBegin(GL_LINE_LOOP, segments);
		immAttrib3fv(wpos, co);

		for (int i = 0; i < segments; ++i) {
			float angle = 2 * M_PI * ((float)i / (float)segments);
			float x = f10 * cosf(angle);
			float y = f10 * sinf(angle);

			if (i % 2 == 0)
				immAttrib3ub(color, 255, 0, 0);
			else
				immAttrib3ub(color, 255, 255, 255);

			immVertex2f(pos, x, y);
		}
		immEnd();

		UI_GetThemeColor3ubv(TH_VIEW_OVERLAY, crosshair_color);

		immBegin(GL_LINES, 8);
		immAttrib3ubv(color, crosshair_color);
		immAttrib3fv(wpos, co);

		immVertex2f(pos, -f20, 0);
		immVertex2f(pos, -f5, 0);
		immVertex2f(pos, +f5, 0);
		immVertex2f(pos, +f20, 0);
		immVertex2f(pos, 0, -f20);
		immVertex2f(pos, 0, -f5);
		immVertex2f(pos, 0, +f5);
		immVertex2f(pos, 0, +f20);
		immEnd();

		immUnbindProgram();
	}
}

/* **************************** 3D Manipulator ******************************** */

void DRW_draw_manipulator(void)
{
	const bContext *C = DRW_get_context();
	View3D *v3d = CTX_wm_view3d(C);
	v3d->zbuf = false;
	BIF_draw_manipulator(C);
}

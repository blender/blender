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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_draw.c
 *  \ingroup spview3d
 */

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_group_types.h"
#include "DNA_mesh_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "DNA_brush_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_jitter.h"
#include "BLI_utildefines.h"
#include "BLI_endian_switch.h"
#include "BLI_threads.h"

#include "BKE_anim.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_unit.h"
#include "BKE_movieclip.h"

#include "RE_engine.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_colormanagement.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "ED_armature.h"
#include "ED_keyframing.h"
#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_screen_types.h"
#include "ED_transform.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "GPU_draw.h"
#include "GPU_framebuffer.h"
#include "GPU_material.h"
#include "GPU_compositing.h"
#include "GPU_extensions.h"
#include "GPU_select.h"

#include "view3d_intern.h"  /* own include */

/* prototypes */
static bool view3d_stereo3d_active(wmWindow *win, Scene *scene, View3D *v3d, RegionView3D *rv3d);
static void view3d_stereo3d_setup(Scene *scene, View3D *v3d, ARegion *ar, const rcti *rect);
static void view3d_stereo3d_setup_offscreen(Scene *scene, View3D *v3d, ARegion *ar,
                                            float winmat[4][4], const char *viewname);

/* handy utility for drawing shapes in the viewport for arbitrary code.
 * could add lines and points too */
// #define DEBUG_DRAW
#ifdef DEBUG_DRAW
static void bl_debug_draw(void);
/* add these locally when using these functions for testing */
extern void bl_debug_draw_quad_clear(void);
extern void bl_debug_draw_quad_add(const float v0[3], const float v1[3], const float v2[3], const float v3[3]);
extern void bl_debug_draw_edge_add(const float v0[3], const float v1[3]);
extern void bl_debug_color_set(const unsigned int col);
#endif

void circf(float x, float y, float rad)
{
	GLUquadricObj *qobj = gluNewQuadric(); 
	
	gluQuadricDrawStyle(qobj, GLU_FILL); 
	
	glPushMatrix(); 
	
	glTranslatef(x, y, 0.0);
	
	gluDisk(qobj, 0.0,  rad, 32, 1);
	
	glPopMatrix(); 
	
	gluDeleteQuadric(qobj);
}

void circ(float x, float y, float rad)
{
	GLUquadricObj *qobj = gluNewQuadric(); 
	
	gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); 
	
	glPushMatrix(); 
	
	glTranslatef(x, y, 0.0);
	
	gluDisk(qobj, 0.0,  rad, 32, 1);
	
	glPopMatrix(); 
	
	gluDeleteQuadric(qobj);
}


/* ********* custom clipping *********** */

static void view3d_draw_clipping(RegionView3D *rv3d)
{
	BoundBox *bb = rv3d->clipbb;

	if (bb) {
		const unsigned int clipping_index[6][4] = {
			{0, 1, 2, 3},
			{0, 4, 5, 1},
			{4, 7, 6, 5},
			{7, 3, 2, 6},
			{1, 5, 6, 2},
			{7, 4, 0, 3}
		};

		/* fill in zero alpha for rendering & re-projection [#31530] */
		unsigned char col[4];
		UI_GetThemeColor4ubv(TH_V3D_CLIPPING_BORDER, col);
		glColor4ubv(col);

		glEnable(GL_BLEND);
		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, bb->vec);
		glDrawElements(GL_QUADS, sizeof(clipping_index) / sizeof(unsigned int), GL_UNSIGNED_INT, clipping_index);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisable(GL_BLEND);
	}
}

void ED_view3d_clipping_set(RegionView3D *rv3d)
{
	double plane[4];
	const unsigned int tot = (rv3d->viewlock & RV3D_BOXCLIP) ? 4 : 6;
	unsigned int a;

	for (a = 0; a < tot; a++) {
		copy_v4db_v4fl(plane, rv3d->clip[a]);
		glClipPlane(GL_CLIP_PLANE0 + a, plane);
		glEnable(GL_CLIP_PLANE0 + a);
	}
}

/* use these to temp disable/enable clipping when 'rv3d->rflag & RV3D_CLIPPING' is set */
void ED_view3d_clipping_disable(void)
{
	unsigned int a;

	for (a = 0; a < 6; a++) {
		glDisable(GL_CLIP_PLANE0 + a);
	}
}
void ED_view3d_clipping_enable(void)
{
	unsigned int a;

	for (a = 0; a < 6; a++) {
		glEnable(GL_CLIP_PLANE0 + a);
	}
}

static bool view3d_clipping_test(const float co[3], const float clip[6][4])
{
	if (plane_point_side_v3(clip[0], co) > 0.0f)
		if (plane_point_side_v3(clip[1], co) > 0.0f)
			if (plane_point_side_v3(clip[2], co) > 0.0f)
				if (plane_point_side_v3(clip[3], co) > 0.0f)
					return false;

	return true;
}

/* for 'local' ED_view3d_clipping_local must run first
 * then all comparisons can be done in localspace */
bool ED_view3d_clipping_test(const RegionView3D *rv3d, const float co[3], const bool is_local)
{
	return view3d_clipping_test(co, is_local ? rv3d->clip_local : rv3d->clip);
}

/* ********* end custom clipping *********** */


static void drawgrid_draw(ARegion *ar, double wx, double wy, double x, double y, double dx)
{	
	double verts[2][2];

	x += (wx);
	y += (wy);

	/* set fixed 'Y' */
	verts[0][1] = 0.0f;
	verts[1][1] = (double)ar->winy;

	/* iter over 'X' */
	verts[0][0] = verts[1][0] = x - dx * floor(x / dx);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_DOUBLE, 0, verts);

	while (verts[0][0] < ar->winx) {
		glDrawArrays(GL_LINES, 0, 2);
		verts[0][0] = verts[1][0] = verts[0][0] + dx;
	}

	/* set fixed 'X' */
	verts[0][0] = 0.0f;
	verts[1][0] = (double)ar->winx;

	/* iter over 'Y' */
	verts[0][1] = verts[1][1] = y - dx * floor(y / dx);
	while (verts[0][1] < ar->winy) {
		glDrawArrays(GL_LINES, 0, 2);
		verts[0][1] = verts[1][1] = verts[0][1] + dx;
	}

	glDisableClientState(GL_VERTEX_ARRAY);
}

#define GRID_MIN_PX_D   6.0
#define GRID_MIN_PX_F 6.0f

static void drawgrid(UnitSettings *unit, ARegion *ar, View3D *v3d, const char **grid_unit)
{
	/* extern short bgpicmode; */
	RegionView3D *rv3d = ar->regiondata;
	double wx, wy, x, y, fw, fx, fy, dx;
	double vec4[4];
	unsigned char col[3], col2[3];

	fx = rv3d->persmat[3][0];
	fy = rv3d->persmat[3][1];
	fw = rv3d->persmat[3][3];

	wx = (ar->winx / 2.0); /* because of rounding errors, grid at wrong location */
	wy = (ar->winy / 2.0);

	x = (wx) * fx / fw;
	y = (wy) * fy / fw;

	vec4[0] = vec4[1] = v3d->grid;

	vec4[2] = 0.0;
	vec4[3] = 1.0;
	mul_m4_v4d(rv3d->persmat, vec4);
	fx = vec4[0];
	fy = vec4[1];
	fw = vec4[3];

	dx = fabs(x - (wx) * fx / fw);
	if (dx == 0) dx = fabs(y - (wy) * fy / fw);
	
	glLineWidth(1.0f);

	glDepthMask(GL_FALSE);     /* disable write in zbuffer */

	/* check zoom out */
	UI_ThemeColor(TH_GRID);
	
	if (unit->system) {
		/* Use GRID_MIN_PX * 2 for units because very very small grid
		 * items are less useful when dealing with units */
		const void *usys;
		int len, i;
		double dx_scalar;
		float blend_fac;

		bUnit_GetSystem(unit->system, B_UNIT_LENGTH, &usys, &len);

		if (usys) {
			i = len;
			while (i--) {
				double scalar = bUnit_GetScaler(usys, i);

				dx_scalar = dx * scalar / (double)unit->scale_length;
				if (dx_scalar < (GRID_MIN_PX_D * 2.0))
					continue;

				/* Store the smallest drawn grid size units name so users know how big each grid cell is */
				if (*grid_unit == NULL) {
					*grid_unit = bUnit_GetNameDisplay(usys, i);
					rv3d->gridview = (float)((scalar * (double)v3d->grid) / (double)unit->scale_length);
				}
				blend_fac = 1.0f - ((GRID_MIN_PX_F * 2.0f) / (float)dx_scalar);

				/* tweak to have the fade a bit nicer */
				blend_fac = (blend_fac * blend_fac) * 2.0f;
				CLAMP(blend_fac, 0.3f, 1.0f);


				UI_ThemeColorBlend(TH_HIGH_GRAD, TH_GRID, blend_fac);

				drawgrid_draw(ar, wx, wy, x, y, dx_scalar);
			}
		}
	}
	else {
		const double sublines    = v3d->gridsubdiv;
		const float  sublines_fl = v3d->gridsubdiv;

		if (dx < GRID_MIN_PX_D) {
			rv3d->gridview *= sublines_fl;
			dx *= sublines;

			if (dx < GRID_MIN_PX_D) {
				rv3d->gridview *= sublines_fl;
				dx *= sublines;

				if (dx < GRID_MIN_PX_D) {
					rv3d->gridview *= sublines_fl;
					dx *= sublines;
					if (dx < GRID_MIN_PX_D) {
						/* pass */
					}
					else {
						UI_ThemeColor(TH_GRID);
						drawgrid_draw(ar, wx, wy, x, y, dx);
					}
				}
				else {  /* start blending out */
					UI_ThemeColorBlend(TH_HIGH_GRAD, TH_GRID, dx / (GRID_MIN_PX_D * 6.0));
					drawgrid_draw(ar, wx, wy, x, y, dx);

					UI_ThemeColor(TH_GRID);
					drawgrid_draw(ar, wx, wy, x, y, sublines * dx);
				}
			}
			else {  /* start blending out (GRID_MIN_PX < dx < (GRID_MIN_PX * 10)) */
				UI_ThemeColorBlend(TH_HIGH_GRAD, TH_GRID, dx / (GRID_MIN_PX_D * 6.0));
				drawgrid_draw(ar, wx, wy, x, y, dx);

				UI_ThemeColor(TH_GRID);
				drawgrid_draw(ar, wx, wy, x, y, sublines * dx);
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
						UI_ThemeColor(TH_GRID);
						drawgrid_draw(ar, wx, wy, x, y, dx);
					}
					else {
						UI_ThemeColorBlend(TH_HIGH_GRAD, TH_GRID, dx / (GRID_MIN_PX_D * 6.0));
						drawgrid_draw(ar, wx, wy, x, y, dx);
						UI_ThemeColor(TH_GRID);
						drawgrid_draw(ar, wx, wy, x, y, dx * sublines);
					}
				}
				else {
					UI_ThemeColorBlend(TH_HIGH_GRAD, TH_GRID, dx / (GRID_MIN_PX_D * 6.0));
					drawgrid_draw(ar, wx, wy, x, y, dx);
					UI_ThemeColor(TH_GRID);
					drawgrid_draw(ar, wx, wy, x, y, dx * sublines);
				}
			}
			else {
				UI_ThemeColorBlend(TH_HIGH_GRAD, TH_GRID, dx / (GRID_MIN_PX_D * 6.0));
				drawgrid_draw(ar, wx, wy, x, y, dx);
				UI_ThemeColor(TH_GRID);
				drawgrid_draw(ar, wx, wy, x, y, dx * sublines);
			}
		}
	}


	x += (wx);
	y += (wy);
	UI_GetThemeColor3ubv(TH_GRID, col);

	setlinestyle(0);
	
	/* center cross */
	/* horizontal line */
	if (ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT))
		UI_make_axis_color(col, col2, 'Y');
	else UI_make_axis_color(col, col2, 'X');
	glColor3ubv(col2);
	
	fdrawline(0.0,  y,  (float)ar->winx,  y); 
	
	/* vertical line */
	if (ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM))
		UI_make_axis_color(col, col2, 'Y');
	else UI_make_axis_color(col, col2, 'Z');
	glColor3ubv(col2);

	fdrawline(x, 0.0, x, (float)ar->winy); 

	glDepthMask(GL_TRUE);  /* enable write in zbuffer */
}
#undef GRID_MIN_PX

/** could move this elsewhere, but tied into #ED_view3d_grid_scale */
float ED_scene_grid_scale(Scene *scene, const char **grid_unit)
{
	/* apply units */
	if (scene->unit.system) {
		const void *usys;
		int len;

		bUnit_GetSystem(scene->unit.system, B_UNIT_LENGTH, &usys, &len);

		if (usys) {
			int i = bUnit_GetBaseUnit(usys);
			if (grid_unit)
				*grid_unit = bUnit_GetNameDisplay(usys, i);
			return (float)bUnit_GetScaler(usys, i) / scene->unit.scale_length;
		}
	}

	return 1.0f;
}

float ED_view3d_grid_scale(Scene *scene, View3D *v3d, const char **grid_unit)
{
	return v3d->grid * ED_scene_grid_scale(scene, grid_unit);
}

static void drawfloor(Scene *scene, View3D *v3d, const char **grid_unit, bool write_depth)
{
	float grid, grid_scale;
	unsigned char col_grid[3];
	const int gridlines = v3d->gridlines / 2;

	if (v3d->gridlines < 3) return;
	
	/* use 'grid_scale' instead of 'v3d->grid' from now on */
	grid_scale = ED_view3d_grid_scale(scene, v3d, grid_unit);
	grid = gridlines * grid_scale;

	if (!write_depth)
		glDepthMask(GL_FALSE);

	UI_GetThemeColor3ubv(TH_GRID, col_grid);

	glLineWidth(1);

	/* draw the Y axis and/or grid lines */
	if (v3d->gridflag & V3D_SHOW_FLOOR) {
		const int sublines = v3d->gridsubdiv;
		float vert[4][3] = {{0.0f}};
		unsigned char col_bg[3];
		unsigned char col_grid_emphasise[3], col_grid_light[3];
		int a;
		int prev_emphasise = -1;

		UI_GetThemeColor3ubv(TH_BACK, col_bg);

		/* emphasise division lines lighter instead of darker, if background is darker than grid */
		UI_GetColorPtrShade3ubv(col_grid, col_grid_light, 10);
		UI_GetColorPtrShade3ubv(col_grid, col_grid_emphasise,
		                        (((col_grid[0] + col_grid[1] + col_grid[2]) + 30) >
		                         (col_bg[0] + col_bg[1] + col_bg[2])) ? 20 : -10);

		/* set fixed axis */
		vert[0][0] = vert[2][1] = grid;
		vert[1][0] = vert[3][1] = -grid;

		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, vert);

		for (a = -gridlines; a <= gridlines; a++) {
			const float line = a * grid_scale;
			const int is_emphasise = (a % sublines) == 0;

			if (is_emphasise != prev_emphasise) {
				glColor3ubv(is_emphasise ? col_grid_emphasise : col_grid_light);
				prev_emphasise = is_emphasise;
			}

			/* set variable axis */
			vert[0][1] = vert[1][1] = vert[2][0] = vert[3][0] = line;

			glDrawArrays(GL_LINES, 0, 4);
		}

		glDisableClientState(GL_VERTEX_ARRAY);
	}
	
	/* draw the Z axis line */
	/* check for the 'show Z axis' preference */
	if (v3d->gridflag & (V3D_SHOW_X | V3D_SHOW_Y | V3D_SHOW_Z)) {
		glBegin(GL_LINES);
		int axis;
		for (axis = 0; axis < 3; axis++) {
			if (v3d->gridflag & (V3D_SHOW_X << axis)) {
				float vert[3];
				unsigned char tcol[3];

				UI_make_axis_color(col_grid, tcol, 'X' + axis);
				glColor3ubv(tcol);

				zero_v3(vert);
				vert[axis] = grid;
				glVertex3fv(vert);
				vert[axis] = -grid;
				glVertex3fv(vert);
			}
		}
		glEnd();
	}
	
	glDepthMask(GL_TRUE);
}


static void drawcursor(Scene *scene, ARegion *ar, View3D *v3d)
{
	int co[2];

	/* we don't want the clipping for cursor */
	if (ED_view3d_project_int_global(ar, ED_view3d_cursor3d_get(scene, v3d), co, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
		const float f5 = 0.25f * U.widget_unit;
		const float f10 = 0.5f * U.widget_unit;
		const float f20 = U.widget_unit;
		
		glLineWidth(1);
		setlinestyle(0); 
		cpack(0xFF);
		circ((float)co[0], (float)co[1], f10);
		setlinestyle(4);
		cpack(0xFFFFFF);
		circ((float)co[0], (float)co[1], f10);
		setlinestyle(0);

		UI_ThemeColor(TH_VIEW_OVERLAY);
		sdrawline(co[0] - f20, co[1], co[0] - f5, co[1]);
		sdrawline(co[0] + f5, co[1], co[0] + f20, co[1]);
		sdrawline(co[0], co[1] - f20, co[0], co[1] - f5);
		sdrawline(co[0], co[1] + f5, co[0], co[1] + f20);
	}
}

/* Draw a live substitute of the view icon, which is always shown
 * colors copied from transform_manipulator.c, we should keep these matching. */
static void draw_view_axis(RegionView3D *rv3d, rcti *rect)
{
	const float k = U.rvisize * U.pixelsize;   /* axis size */
	const float toll = 0.5;      /* used to see when view is quasi-orthogonal */
	float startx = k + 1.0f; /* axis center in screen coordinates, x=y */
	float starty = k + 1.0f;
	float ydisp = 0.0;          /* vertical displacement to allow obj info text */
	int bright = - 20 * (10 - U.rvibright); /* axis alpha offset (rvibright has range 0-10) */
	float vec[3];
	float dx, dy;

	int axis_order[3] = {0, 1, 2};
	int axis_i;

	startx += rect->xmin;
	starty += rect->ymin;

	axis_sort_v3(rv3d->viewinv[2], axis_order);

	/* thickness of lines is proportional to k */
	glLineWidth(2);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	for (axis_i = 0; axis_i < 3; axis_i++) {
		int i = axis_order[axis_i];
		const char axis_text[2] = {'x' + i, '\0'};

		zero_v3(vec);
		vec[i] = 1.0f;
		mul_qt_v3(rv3d->viewquat, vec);
		dx = vec[0] * k;
		dy = vec[1] * k;

		UI_ThemeColorShadeAlpha(TH_AXIS_X + i, 0, bright);
		glBegin(GL_LINES);
		glVertex2f(startx, starty + ydisp);
		glVertex2f(startx + dx, starty + dy + ydisp);
		glEnd();

		if (fabsf(dx) > toll || fabsf(dy) > toll) {
			BLF_draw_default_ascii(startx + dx + 2, starty + dy + ydisp + 2, 0.0f, axis_text, 1);

			/* BLF_draw_default disables blending */
			glEnable(GL_BLEND);
		}
	}

	glDisable(GL_BLEND);
}

#ifdef WITH_INPUT_NDOF
/* draw center and axis of rotation for ongoing 3D mouse navigation */
static void draw_rotation_guide(RegionView3D *rv3d)
{
	float o[3];    /* center of rotation */
	float end[3];  /* endpoints for drawing */

	float color[4] = {0.0f, 0.4235f, 1.0f, 1.0f};  /* bright blue so it matches device LEDs */

	negate_v3_v3(o, rv3d->ofs);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPointSize(5);
	glEnable(GL_POINT_SMOOTH);
	glDepthMask(0);  /* don't overwrite zbuf */

	if (rv3d->rot_angle != 0.0f) {
		/* -- draw rotation axis -- */
		float scaled_axis[3];
		const float scale = rv3d->dist;
		mul_v3_v3fl(scaled_axis, rv3d->rot_axis, scale);


		glBegin(GL_LINE_STRIP);
		color[3] = 0.0f;  /* more transparent toward the ends */
		glColor4fv(color);
		add_v3_v3v3(end, o, scaled_axis);
		glVertex3fv(end);

#if 0
		color[3] = 0.2f + fabsf(rv3d->rot_angle);  /* modulate opacity with angle */
		/* ^^ neat idea, but angle is frame-rate dependent, so it's usually close to 0.2 */
#endif

		color[3] = 0.5f;  /* more opaque toward the center */
		glColor4fv(color);
		glVertex3fv(o);

		color[3] = 0.0f;
		glColor4fv(color);
		sub_v3_v3v3(end, o, scaled_axis);
		glVertex3fv(end);
		glEnd();
		
		/* -- draw ring around rotation center -- */
		{
#define     ROT_AXIS_DETAIL 13

			const float s = 0.05f * scale;
			const float step = 2.0f * (float)(M_PI / ROT_AXIS_DETAIL);
			float angle;
			int i;

			float q[4];  /* rotate ring so it's perpendicular to axis */
			const int upright = fabsf(rv3d->rot_axis[2]) >= 0.95f;
			if (!upright) {
				const float up[3] = {0.0f, 0.0f, 1.0f};
				float vis_angle, vis_axis[3];

				cross_v3_v3v3(vis_axis, up, rv3d->rot_axis);
				vis_angle = acosf(dot_v3v3(up, rv3d->rot_axis));
				axis_angle_to_quat(q, vis_axis, vis_angle);
			}

			color[3] = 0.25f;  /* somewhat faint */
			glColor4fv(color);
			glBegin(GL_LINE_LOOP);
			for (i = 0, angle = 0.0f; i < ROT_AXIS_DETAIL; ++i, angle += step) {
				float p[3] = {s * cosf(angle), s * sinf(angle), 0.0f};

				if (!upright) {
					mul_qt_v3(q, p);
				}

				add_v3_v3(p, o);
				glVertex3fv(p);
			}
			glEnd();

#undef      ROT_AXIS_DETAIL
		}

		color[3] = 1.0f;  /* solid dot */
	}
	else
		color[3] = 0.5f;  /* see-through dot */

	/* -- draw rotation center -- */
	glColor4fv(color);
	glBegin(GL_POINTS);
	glVertex3fv(o);
	glEnd();

#if 0
	/* find screen coordinates for rotation center, then draw pretty icon */
	mul_m4_v3(rv3d->persinv, rot_center);
	UI_icon_draw(rot_center[0], rot_center[1], ICON_NDOF_TURN);
	/* ^^ just playing around, does not work */
#endif

	glDisable(GL_BLEND);
	glDisable(GL_POINT_SMOOTH);
	glDepthMask(1);
}
#endif /* WITH_INPUT_NDOF */

static void draw_view_icon(RegionView3D *rv3d, rcti *rect)
{
	BIFIconID icon;
	
	if (ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM))
		icon = ICON_AXIS_TOP;
	else if (ELEM(rv3d->view, RV3D_VIEW_FRONT, RV3D_VIEW_BACK))
		icon = ICON_AXIS_FRONT;
	else if (ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT))
		icon = ICON_AXIS_SIDE;
	else return;
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA); 
	
	UI_icon_draw(5.0 + rect->xmin, 5.0 + rect->ymin, icon);
	
	glDisable(GL_BLEND);
}

static const char *view3d_get_name(View3D *v3d, RegionView3D *rv3d)
{
	const char *name = NULL;
	
	switch (rv3d->view) {
		case RV3D_VIEW_FRONT:
			if (rv3d->persp == RV3D_ORTHO) name = IFACE_("Front Ortho");
			else name = IFACE_("Front Persp");
			break;
		case RV3D_VIEW_BACK:
			if (rv3d->persp == RV3D_ORTHO) name = IFACE_("Back Ortho");
			else name = IFACE_("Back Persp");
			break;
		case RV3D_VIEW_TOP:
			if (rv3d->persp == RV3D_ORTHO) name = IFACE_("Top Ortho");
			else name = IFACE_("Top Persp");
			break;
		case RV3D_VIEW_BOTTOM:
			if (rv3d->persp == RV3D_ORTHO) name = IFACE_("Bottom Ortho");
			else name = IFACE_("Bottom Persp");
			break;
		case RV3D_VIEW_RIGHT:
			if (rv3d->persp == RV3D_ORTHO) name = IFACE_("Right Ortho");
			else name = IFACE_("Right Persp");
			break;
		case RV3D_VIEW_LEFT:
			if (rv3d->persp == RV3D_ORTHO) name = IFACE_("Left Ortho");
			else name = IFACE_("Left Persp");
			break;
			
		default:
			if (rv3d->persp == RV3D_CAMOB) {
				if ((v3d->camera) && (v3d->camera->type == OB_CAMERA)) {
					Camera *cam;
					cam = v3d->camera->data;
					if (cam->type == CAM_PERSP) {
						name = IFACE_("Camera Persp");
					}
					else if (cam->type == CAM_ORTHO) {
						name = IFACE_("Camera Ortho");
					}
					else {
						BLI_assert(cam->type == CAM_PANO);
						name = IFACE_("Camera Pano");
					}
				}
				else {
					name = IFACE_("Object as Camera");
				}
			}
			else {
				name = (rv3d->persp == RV3D_ORTHO) ? IFACE_("User Ortho") : IFACE_("User Persp");
			}
			break;
	}
	
	return name;
}

static void draw_viewport_name(ARegion *ar, View3D *v3d, rcti *rect)
{
	RegionView3D *rv3d = ar->regiondata;
	const char *name = view3d_get_name(v3d, rv3d);
	/* increase size for unicode languages (Chinese in utf-8...) */
#ifdef WITH_INTERNATIONAL
	char tmpstr[96];
#else
	char tmpstr[32];
#endif

	if (v3d->localvd) {
		BLI_snprintf(tmpstr, sizeof(tmpstr), IFACE_("%s (Local)"), name);
		name = tmpstr;
	}

	UI_ThemeColor(TH_TEXT_HI);
#ifdef WITH_INTERNATIONAL
	BLF_draw_default(U.widget_unit + rect->xmin,  rect->ymax - U.widget_unit, 0.0f, name, sizeof(tmpstr));
#else
	BLF_draw_default_ascii(U.widget_unit + rect->xmin,  rect->ymax - U.widget_unit, 0.0f, name, sizeof(tmpstr));
#endif
}

/* draw info beside axes in bottom left-corner: 
 * framenum, object name, bone name (if available), marker name (if available)
 */

static void draw_selected_name(Scene *scene, Object *ob, rcti *rect)
{
	const int cfra = CFRA;
	const char *msg_pin = " (Pinned)";
	const char *msg_sep = " : ";

	char info[300];
	const char *markern;
	char *s = info;
	short offset = 1.5f * UI_UNIT_X + rect->xmin;

	s += sprintf(s, "(%d)", cfra);

	/* 
	 * info can contain:
	 * - a frame (7 + 2)
	 * - 3 object names (MAX_NAME)
	 * - 2 BREAD_CRUMB_SEPARATORs (6)
	 * - a SHAPE_KEY_PINNED marker and a trailing '\0' (9+1) - translated, so give some room!
	 * - a marker name (MAX_NAME + 3)
	 */

	/* get name of marker on current frame (if available) */
	markern = BKE_scene_find_marker_name(scene, cfra);
	
	/* check if there is an object */
	if (ob) {
		*s++ = ' ';
		s += BLI_strcpy_rlen(s, ob->id.name + 2);

		/* name(s) to display depends on type of object */
		if (ob->type == OB_ARMATURE) {
			bArmature *arm = ob->data;
			
			/* show name of active bone too (if possible) */
			if (arm->edbo) {
				if (arm->act_edbone) {
					s += BLI_strcpy_rlen(s, msg_sep);
					s += BLI_strcpy_rlen(s, arm->act_edbone->name);
				}
			}
			else if (ob->mode & OB_MODE_POSE) {
				if (arm->act_bone) {

					if (arm->act_bone->layer & arm->layer) {
						s += BLI_strcpy_rlen(s, msg_sep);
						s += BLI_strcpy_rlen(s, arm->act_bone->name);
					}
				}
			}
		}
		else if (ELEM(ob->type, OB_MESH, OB_LATTICE, OB_CURVE)) {
			Key *key = NULL;
			KeyBlock *kb = NULL;

			/* try to display active bone and active shapekey too (if they exist) */

			if (ob->type == OB_MESH && ob->mode & OB_MODE_WEIGHT_PAINT) {
				Object *armobj = BKE_object_pose_armature_get(ob);
				if (armobj  && armobj->mode & OB_MODE_POSE) {
					bArmature *arm = armobj->data;
					if (arm->act_bone) {
						if (arm->act_bone->layer & arm->layer) {
							s += BLI_strcpy_rlen(s, msg_sep);
							s += BLI_strcpy_rlen(s, arm->act_bone->name);
						}
					}
				}
			}

			key = BKE_key_from_object(ob);
			if (key) {
				kb = BLI_findlink(&key->block, ob->shapenr - 1);
				if (kb) {
					s += BLI_strcpy_rlen(s, msg_sep);
					s += BLI_strcpy_rlen(s, kb->name);
					if (ob->shapeflag & OB_SHAPE_LOCK) {
						s += BLI_strcpy_rlen(s, IFACE_(msg_pin));
					}
				}
			}
		}
		
		/* color depends on whether there is a keyframe */
		if (id_frame_has_keyframe((ID *)ob, /* BKE_scene_frame_get(scene) */ (float)cfra, ANIMFILTER_KEYS_LOCAL))
			UI_ThemeColor(TH_TIME_KEYFRAME);
		else if (ED_gpencil_has_keyframe_v3d(scene, ob, cfra))
			UI_ThemeColor(TH_TIME_GP_KEYFRAME);
		else
			UI_ThemeColor(TH_TEXT_HI);
	}
	else {
		/* no object */
		if (ED_gpencil_has_keyframe_v3d(scene, NULL, cfra))
			UI_ThemeColor(TH_TIME_GP_KEYFRAME);
		else
			UI_ThemeColor(TH_TEXT_HI);
	}

	if (markern) {
		s += sprintf(s, " <%s>", markern);
	}
	
	if (U.uiflag & USER_SHOW_ROTVIEWICON)
		offset = U.widget_unit + (U.rvisize * 2) + rect->xmin;

	BLF_draw_default(offset, 0.5f * U.widget_unit, 0.0f, info, sizeof(info));
}

static void view3d_camera_border(
        const Scene *scene, const ARegion *ar, const View3D *v3d, const RegionView3D *rv3d,
        rctf *r_viewborder, const bool no_shift, const bool no_zoom)
{
	CameraParams params;
	rctf rect_view, rect_camera;

	/* get viewport viewplane */
	BKE_camera_params_init(&params);
	BKE_camera_params_from_view3d(&params, v3d, rv3d);
	if (no_zoom)
		params.zoom = 1.0f;
	BKE_camera_params_compute_viewplane(&params, ar->winx, ar->winy, 1.0f, 1.0f);
	rect_view = params.viewplane;

	/* get camera viewplane */
	BKE_camera_params_init(&params);
	/* fallback for non camera objects */
	params.clipsta = v3d->near;
	params.clipend = v3d->far;
	BKE_camera_params_from_object(&params, v3d->camera);
	if (no_shift) {
		params.shiftx = 0.0f;
		params.shifty = 0.0f;
	}
	BKE_camera_params_compute_viewplane(&params, scene->r.xsch, scene->r.ysch, scene->r.xasp, scene->r.yasp);
	rect_camera = params.viewplane;

	/* get camera border within viewport */
	r_viewborder->xmin = ((rect_camera.xmin - rect_view.xmin) / BLI_rctf_size_x(&rect_view)) * ar->winx;
	r_viewborder->xmax = ((rect_camera.xmax - rect_view.xmin) / BLI_rctf_size_x(&rect_view)) * ar->winx;
	r_viewborder->ymin = ((rect_camera.ymin - rect_view.ymin) / BLI_rctf_size_y(&rect_view)) * ar->winy;
	r_viewborder->ymax = ((rect_camera.ymax - rect_view.ymin) / BLI_rctf_size_y(&rect_view)) * ar->winy;
}

void ED_view3d_calc_camera_border_size(
        const Scene *scene, const ARegion *ar, const View3D *v3d, const RegionView3D *rv3d,
        float r_size[2])
{
	rctf viewborder;

	view3d_camera_border(scene, ar, v3d, rv3d, &viewborder, true, true);
	r_size[0] = BLI_rctf_size_x(&viewborder);
	r_size[1] = BLI_rctf_size_y(&viewborder);
}

void ED_view3d_calc_camera_border(
        const Scene *scene, const ARegion *ar, const View3D *v3d, const RegionView3D *rv3d,
        rctf *r_viewborder, const bool no_shift)
{
	view3d_camera_border(scene, ar, v3d, rv3d, r_viewborder, no_shift, false);
}

static void drawviewborder_grid3(float x1, float x2, float y1, float y2, float fac)
{
	float x3, y3, x4, y4;

	x3 = x1 + fac * (x2 - x1);
	y3 = y1 + fac * (y2 - y1);
	x4 = x1 + (1.0f - fac) * (x2 - x1);
	y4 = y1 + (1.0f - fac) * (y2 - y1);

	glBegin(GL_LINES);
	glVertex2f(x1, y3);
	glVertex2f(x2, y3);

	glVertex2f(x1, y4);
	glVertex2f(x2, y4);

	glVertex2f(x3, y1);
	glVertex2f(x3, y2);

	glVertex2f(x4, y1);
	glVertex2f(x4, y2);
	glEnd();
}

/* harmonious triangle */
static void drawviewborder_triangle(float x1, float x2, float y1, float y2, const char golden, const char dir)
{
	float ofs;
	float w = x2 - x1;
	float h = y2 - y1;

	glBegin(GL_LINES);
	if (w > h) {
		if (golden) {
			ofs = w * (1.0f - (1.0f / 1.61803399f));
		}
		else {
			ofs = h * (h / w);
		}
		if (dir == 'B') SWAP(float, y1, y2);

		glVertex2f(x1, y1);
		glVertex2f(x2, y2);

		glVertex2f(x2, y1);
		glVertex2f(x1 + (w - ofs), y2);

		glVertex2f(x1, y2);
		glVertex2f(x1 + ofs, y1);
	}
	else {
		if (golden) {
			ofs = h * (1.0f - (1.0f / 1.61803399f));
		}
		else {
			ofs = w * (w / h);
		}
		if (dir == 'B') SWAP(float, x1, x2);

		glVertex2f(x1, y1);
		glVertex2f(x2, y2);

		glVertex2f(x2, y1);
		glVertex2f(x1, y1 + ofs);

		glVertex2f(x1, y2);
		glVertex2f(x2, y1 + (h - ofs));
	}
	glEnd();
}

static void drawviewborder(Scene *scene, ARegion *ar, View3D *v3d)
{
	float x1, x2, y1, y2;
	float x1i, x2i, y1i, y2i;

	rctf viewborder;
	Camera *ca = NULL;
	RegionView3D *rv3d = ar->regiondata;

	if (v3d->camera == NULL)
		return;
	if (v3d->camera->type == OB_CAMERA)
		ca = v3d->camera->data;
	
	ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &viewborder, false);
	/* the offsets */
	x1 = viewborder.xmin;
	y1 = viewborder.ymin;
	x2 = viewborder.xmax;
	y2 = viewborder.ymax;
	
	glLineWidth(1.0f);

	/* apply offsets so the real 3D camera shows through */

	/* note: quite un-scientific but without this bit extra
	 * 0.0001 on the lower left the 2D border sometimes
	 * obscures the 3D camera border */
	/* note: with VIEW3D_CAMERA_BORDER_HACK defined this error isn't noticeable
	 * but keep it here in case we need to remove the workaround */
	x1i = (int)(x1 - 1.0001f);
	y1i = (int)(y1 - 1.0001f);
	x2i = (int)(x2 + (1.0f - 0.0001f));
	y2i = (int)(y2 + (1.0f - 0.0001f));
	
	/* passepartout, specified in camera edit buttons */
	if (ca && (ca->flag & CAM_SHOWPASSEPARTOUT) && ca->passepartalpha > 0.000001f) {
		const float winx = (ar->winx + 1);
		const float winy = (ar->winy + 1);

		if (ca->passepartalpha == 1.0f) {
			glColor3f(0, 0, 0);
		}
		else {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_BLEND);
			glColor4f(0, 0, 0, ca->passepartalpha);
		}

		if (x1i > 0.0f)
			glRectf(0.0, winy, x1i, 0.0);
		if (x2i < winx)
			glRectf(x2i, winy, winx, 0.0);
		if (y2i < winy)
			glRectf(x1i, winy, x2i, y2i);
		if (y2i > 0.0f)
			glRectf(x1i, y1i, x2i, 0.0);
		
		glDisable(GL_BLEND);
	}

	setlinestyle(0);

	UI_ThemeColor(TH_BACK);
		
	fdrawbox(x1i, y1i, x2i, y2i);

#ifdef VIEW3D_CAMERA_BORDER_HACK
	if (view3d_camera_border_hack_test == true) {
		glColor3ubv(view3d_camera_border_hack_col);
		fdrawbox(x1i + 1, y1i + 1, x2i - 1, y2i - 1);
		view3d_camera_border_hack_test = false;
	}
#endif

	setlinestyle(3);

	/* outer line not to confuse with object selecton */
	if (v3d->flag2 & V3D_LOCK_CAMERA) {
		UI_ThemeColor(TH_REDALERT);
		fdrawbox(x1i - 1, y1i - 1, x2i + 1, y2i + 1);
	}

	UI_ThemeColor(TH_VIEW_OVERLAY);
	fdrawbox(x1i, y1i, x2i, y2i);

	/* border */
	if (scene->r.mode & R_BORDER) {
		float x3, y3, x4, y4;

		x3 = floorf(x1 + (scene->r.border.xmin * (x2 - x1))) - 1;
		y3 = floorf(y1 + (scene->r.border.ymin * (y2 - y1))) - 1;
		x4 = floorf(x1 + (scene->r.border.xmax * (x2 - x1))) + (U.pixelsize - 1);
		y4 = floorf(y1 + (scene->r.border.ymax * (y2 - y1))) + (U.pixelsize - 1);

		cpack(0x4040FF);
		sdrawbox(x3,  y3,  x4,  y4);
	}

	/* safety border */
	if (ca) {
		if (ca->dtx & CAM_DTX_CENTER) {
			float x3, y3;

			UI_ThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25, 0);

			x3 = x1 + 0.5f * (x2 - x1);
			y3 = y1 + 0.5f * (y2 - y1);

			glBegin(GL_LINES);
			glVertex2f(x1, y3);
			glVertex2f(x2, y3);

			glVertex2f(x3, y1);
			glVertex2f(x3, y2);
			glEnd();
		}

		if (ca->dtx & CAM_DTX_CENTER_DIAG) {
			UI_ThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25, 0);

			glBegin(GL_LINES);
			glVertex2f(x1, y1);
			glVertex2f(x2, y2);

			glVertex2f(x1, y2);
			glVertex2f(x2, y1);
			glEnd();
		}

		if (ca->dtx & CAM_DTX_THIRDS) {
			UI_ThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25, 0);
			drawviewborder_grid3(x1, x2, y1, y2, 1.0f / 3.0f);
		}

		if (ca->dtx & CAM_DTX_GOLDEN) {
			UI_ThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25, 0);
			drawviewborder_grid3(x1, x2, y1, y2, 1.0f - (1.0f / 1.61803399f));
		}

		if (ca->dtx & CAM_DTX_GOLDEN_TRI_A) {
			UI_ThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25, 0);
			drawviewborder_triangle(x1, x2, y1, y2, 0, 'A');
		}

		if (ca->dtx & CAM_DTX_GOLDEN_TRI_B) {
			UI_ThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25, 0);
			drawviewborder_triangle(x1, x2, y1, y2, 0, 'B');
		}

		if (ca->dtx & CAM_DTX_HARMONY_TRI_A) {
			UI_ThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25, 0);
			drawviewborder_triangle(x1, x2, y1, y2, 1, 'A');
		}

		if (ca->dtx & CAM_DTX_HARMONY_TRI_B) {
			UI_ThemeColorBlendShade(TH_VIEW_OVERLAY, TH_BACK, 0.25, 0);
			drawviewborder_triangle(x1, x2, y1, y2, 1, 'B');
		}

		if (ca->flag & CAM_SHOW_SAFE_MARGINS) {
			UI_draw_safe_areas(
			        x1, x2, y1, y2,
			        scene->safe_areas.title,
			        scene->safe_areas.action);

			if (ca->flag & CAM_SHOW_SAFE_CENTER) {
				UI_draw_safe_areas(
				        x1, x2, y1, y2,
				        scene->safe_areas.title_center,
				        scene->safe_areas.action_center);
			}
		}

		if (ca->flag & CAM_SHOWSENSOR) {
			/* determine sensor fit, and get sensor x/y, for auto fit we
			 * assume and square sensor and only use sensor_x */
			float sizex = scene->r.xsch * scene->r.xasp;
			float sizey = scene->r.ysch * scene->r.yasp;
			int sensor_fit = BKE_camera_sensor_fit(ca->sensor_fit, sizex, sizey);
			float sensor_x = ca->sensor_x;
			float sensor_y = (ca->sensor_fit == CAMERA_SENSOR_FIT_AUTO) ? ca->sensor_x : ca->sensor_y;

			/* determine sensor plane */
			rctf rect;

			if (sensor_fit == CAMERA_SENSOR_FIT_HOR) {
				float sensor_scale = (x2i - x1i) / sensor_x;
				float sensor_height = sensor_scale * sensor_y;

				rect.xmin = x1i;
				rect.xmax = x2i;
				rect.ymin = (y1i + y2i) * 0.5f - sensor_height * 0.5f;
				rect.ymax = rect.ymin + sensor_height;
			}
			else {
				float sensor_scale = (y2i - y1i) / sensor_y;
				float sensor_width = sensor_scale * sensor_x;

				rect.xmin = (x1i + x2i) * 0.5f - sensor_width * 0.5f;
				rect.xmax = rect.xmin + sensor_width;
				rect.ymin = y1i;
				rect.ymax = y2i;
			}

			/* draw */
			UI_ThemeColorShade(TH_VIEW_OVERLAY, 100);
			UI_draw_roundbox_gl_mode(GL_LINE_LOOP, rect.xmin, rect.ymin, rect.xmax, rect.ymax, 2.0f);
		}
	}

	setlinestyle(0);

	/* camera name - draw in highlighted text color */
	if (ca && (ca->flag & CAM_SHOWNAME)) {
		UI_ThemeColor(TH_TEXT_HI);
		BLF_draw_default(
		        x1i, y1i - (0.7f * U.widget_unit), 0.0f,
		        v3d->camera->id.name + 2, sizeof(v3d->camera->id.name) - 2);
	}
}

/* *********************** backdraw for selection *************** */

static void backdrawview3d(Scene *scene, wmWindow *win, ARegion *ar, View3D *v3d)
{
	RegionView3D *rv3d = ar->regiondata;
	struct Base *base = scene->basact;
	int multisample_enabled;

	BLI_assert(ar->regiontype == RGN_TYPE_WINDOW);

	if (base && (base->object->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT) ||
	             BKE_paint_select_face_test(base->object)))
	{
		/* do nothing */
	}
	/* texture paint mode sampling */
	else if (base && (base->object->mode & OB_MODE_TEXTURE_PAINT) &&
	         (v3d->drawtype > OB_WIRE))
	{
		/* do nothing */
	}
	else if ((base && (base->object->mode & OB_MODE_PARTICLE_EDIT)) &&
	         V3D_IS_ZBUF(v3d))
	{
		/* do nothing */
	}
	else if (scene->obedit &&
	         V3D_IS_ZBUF(v3d))
	{
		/* do nothing */
	}
	else {
		v3d->flag &= ~V3D_INVALID_BACKBUF;
		return;
	}

	if (!(v3d->flag & V3D_INVALID_BACKBUF))
		return;

#if 0
	if (test) {
		if (qtest()) {
			addafterqueue(ar->win, BACKBUFDRAW, 1);
			return;
		}
	}
#endif

	if (v3d->drawtype > OB_WIRE) v3d->zbuf = true;
	
	/* dithering and AA break color coding, so disable */
	glDisable(GL_DITHER);

	multisample_enabled = glIsEnabled(GL_MULTISAMPLE);
	if (multisample_enabled)
		glDisable(GL_MULTISAMPLE);

	if (win->multisamples != USER_MULTISAMPLE_NONE) {
		/* for multisample we use an offscreen FBO. multisample drawing can fail
		 * with color coded selection drawing, and reading back depths from such
		 * a buffer can also cause a few seconds freeze on OS X / NVidia. */
		int w = BLI_rcti_size_x(&ar->winrct);
		int h = BLI_rcti_size_y(&ar->winrct);
		char error[256];

		if (rv3d->gpuoffscreen) {
			if (GPU_offscreen_width(rv3d->gpuoffscreen)  != w ||
			    GPU_offscreen_height(rv3d->gpuoffscreen) != h)
			{
				GPU_offscreen_free(rv3d->gpuoffscreen);
				rv3d->gpuoffscreen = NULL;
			}
		}

		if (!rv3d->gpuoffscreen) {
			rv3d->gpuoffscreen = GPU_offscreen_create(w, h, 0, error);

			if (!rv3d->gpuoffscreen)
				fprintf(stderr, "Failed to create offscreen selection buffer for multisample: %s\n", error);
		}
	}

	if (rv3d->gpuoffscreen)
		GPU_offscreen_bind(rv3d->gpuoffscreen, true);
	else
		glScissor(ar->winrct.xmin, ar->winrct.ymin, BLI_rcti_size_x(&ar->winrct), BLI_rcti_size_y(&ar->winrct));

	glClearColor(0.0, 0.0, 0.0, 0.0);
	if (v3d->zbuf) {
		glEnable(GL_DEPTH_TEST);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	else {
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);
	}
	
	if (rv3d->rflag & RV3D_CLIPPING)
		ED_view3d_clipping_set(rv3d);
	
	G.f |= G_BACKBUFSEL;
	
	if (base && (base->lay & v3d->lay))
		draw_object_backbufsel(scene, v3d, rv3d, base->object);
	
	if (rv3d->gpuoffscreen)
		GPU_offscreen_unbind(rv3d->gpuoffscreen, true);
	else
		ar->swap = 0; /* mark invalid backbuf for wm draw */

	v3d->flag &= ~V3D_INVALID_BACKBUF;

	G.f &= ~G_BACKBUFSEL;
	v3d->zbuf = false;
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_DITHER);
	if (multisample_enabled)
		glEnable(GL_MULTISAMPLE);

	if (rv3d->rflag & RV3D_CLIPPING)
		ED_view3d_clipping_disable();
}

void view3d_opengl_read_pixels(ARegion *ar, int x, int y, int w, int h, int format, int type, void *data)
{
	RegionView3D *rv3d = ar->regiondata;

	if (rv3d->gpuoffscreen) {
		GPU_offscreen_bind(rv3d->gpuoffscreen, true);
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glReadPixels(x, y, w, h, format, type, data);
		GPU_offscreen_unbind(rv3d->gpuoffscreen, true);
	}
	else {
		glReadPixels(ar->winrct.xmin + x, ar->winrct.ymin + y, w, h, format, type, data);
	}
}

/* XXX depth reading exception, for code not using gpu offscreen */
static void view3d_opengl_read_Z_pixels(ARegion *ar, int x, int y, int w, int h, int format, int type, void *data)
{

	glReadPixels(ar->winrct.xmin + x, ar->winrct.ymin + y, w, h, format, type, data);
}

void ED_view3d_backbuf_validate(ViewContext *vc)
{
	if (vc->v3d->flag & V3D_INVALID_BACKBUF)
		backdrawview3d(vc->scene, vc->win, vc->ar, vc->v3d);
}

/**
 * allow for small values [0.5 - 2.5],
 * and large values, FLT_MAX by clamping by the area size
 */
int ED_view3d_backbuf_sample_size_clamp(ARegion *ar, const float dist)
{
	return (int)min_ff(ceilf(dist), (float)max_ii(ar->winx, ar->winx));
}

/* samples a single pixel (copied from vpaint) */
unsigned int ED_view3d_backbuf_sample(ViewContext *vc, int x, int y)
{
	unsigned int col;
	
	if (x >= vc->ar->winx || y >= vc->ar->winy) {
		return 0;
	}

	ED_view3d_backbuf_validate(vc);

	view3d_opengl_read_pixels(vc->ar, x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &col);
	glReadBuffer(GL_BACK);
	
	if (ENDIAN_ORDER == B_ENDIAN) {
		BLI_endian_switch_uint32(&col);
	}
	
	return GPU_select_to_index(col);
}

/* reads full rect, converts indices */
ImBuf *ED_view3d_backbuf_read(ViewContext *vc, int xmin, int ymin, int xmax, int ymax)
{
	struct ImBuf *ibuf_clip;
	/* clip */
	const rcti clip = {
	    max_ii(xmin, 0), min_ii(xmax, vc->ar->winx - 1),
	    max_ii(ymin, 0), min_ii(ymax, vc->ar->winy - 1)};
	const int size_clip[2] = {
	    BLI_rcti_size_x(&clip) + 1,
	    BLI_rcti_size_y(&clip) + 1};

	if (UNLIKELY((clip.xmin > clip.xmax) ||
	             (clip.ymin > clip.ymax)))
	{
		return NULL;
	}

	ibuf_clip = IMB_allocImBuf(size_clip[0], size_clip[1], 32, IB_rect);

	ED_view3d_backbuf_validate(vc);

	view3d_opengl_read_pixels(vc->ar, clip.xmin, clip.ymin, size_clip[0], size_clip[1], GL_RGBA, GL_UNSIGNED_BYTE, ibuf_clip->rect);

	glReadBuffer(GL_BACK);

	if (ENDIAN_ORDER == B_ENDIAN) {
		IMB_convert_rgba_to_abgr(ibuf_clip);
	}

	GPU_select_to_index_array(ibuf_clip->rect, size_clip[0] * size_clip[1]);
	
	if ((clip.xmin == xmin) &&
	    (clip.xmax == xmax) &&
	    (clip.ymin == ymin) &&
	    (clip.ymax == ymax))
	{
		return ibuf_clip;
	}
	else {
		/* put clipped result into a non-clipped buffer */
		struct ImBuf *ibuf_full;
		const int size[2] = {
		    (xmax - xmin + 1),
		    (ymax - ymin + 1)};

		ibuf_full = IMB_allocImBuf(size[0], size[1], 32, IB_rect);

		IMB_rectcpy(
		        ibuf_full, ibuf_clip,
		        clip.xmin - xmin, clip.ymin - ymin,
		        0, 0,
		        size_clip[0], size_clip[1]);
		IMB_freeImBuf(ibuf_clip);
		return ibuf_full;
	}
}

/* smart function to sample a rect spiralling outside, nice for backbuf selection */
unsigned int ED_view3d_backbuf_sample_rect(
        ViewContext *vc, const int mval[2], int size,
        unsigned int min, unsigned int max, float *r_dist)
{
	struct ImBuf *buf;
	const unsigned int *bufmin, *bufmax, *tbuf;
	int minx, miny;
	int a, b, rc, nr, amount, dirvec[4][2];
	unsigned int index = 0;

	amount = (size - 1) / 2;

	minx = mval[0] - (amount + 1);
	miny = mval[1] - (amount + 1);
	buf = ED_view3d_backbuf_read(vc, minx, miny, minx + size - 1, miny + size - 1);
	if (!buf) return 0;

	rc = 0;
	
	dirvec[0][0] = 1; dirvec[0][1] = 0;
	dirvec[1][0] = 0; dirvec[1][1] = -size;
	dirvec[2][0] = -1; dirvec[2][1] = 0;
	dirvec[3][0] = 0; dirvec[3][1] = size;
	
	bufmin = buf->rect;
	tbuf = buf->rect;
	bufmax = buf->rect + size * size;
	tbuf += amount * size + amount;
	
	for (nr = 1; nr <= size; nr++) {
		
		for (a = 0; a < 2; a++) {
			for (b = 0; b < nr; b++) {
				if (*tbuf && *tbuf >= min && *tbuf < max) {
					/* we got a hit */

					/* get x,y pixel coords from the offset
					 * (manhatten distance in keeping with other screen-based selection) */
					*r_dist = (float)(
					        abs(((int)(tbuf - buf->rect) % size) - (size / 2)) +
					        abs(((int)(tbuf - buf->rect) / size) - (size / 2)));

					/* indices start at 1 here */
					index = (*tbuf - min) + 1;
					goto exit;
				}
				
				tbuf += (dirvec[rc][0] + dirvec[rc][1]);
				
				if (tbuf < bufmin || tbuf >= bufmax) {
					goto exit;
				}
			}
			rc++;
			rc &= 3;
		}
	}

exit:
	IMB_freeImBuf(buf);
	return index;
}


/* ************************************************************* */

static void view3d_stereo_bgpic_setup(Scene *scene, View3D *v3d, Image *ima, ImageUser *iuser)
{
	if (BKE_image_is_stereo(ima)) {
		iuser->flag |= IMA_SHOW_STEREO;

		if ((scene->r.scemode & R_MULTIVIEW) == 0) {
			iuser->multiview_eye = STEREO_LEFT_ID;
		}
		else if (v3d->stereo3d_camera != STEREO_3D_ID) {
			/* show only left or right camera */
			iuser->multiview_eye = v3d->stereo3d_camera;
		}

		BKE_image_multiview_index(ima, iuser);
	}
	else {
		iuser->flag &= ~IMA_SHOW_STEREO;
	}
}

static void view3d_draw_bgpic(Scene *scene, ARegion *ar, View3D *v3d,
                              const bool do_foreground, const bool do_camera_frame)
{
	RegionView3D *rv3d = ar->regiondata;
	BGpic *bgpic;
	int fg_flag = do_foreground ? V3D_BGPIC_FOREGROUND : 0;

	for (bgpic = v3d->bgpicbase.first; bgpic; bgpic = bgpic->next) {
		bgpic->iuser.scene = scene;  /* Needed for render results. */

		if ((bgpic->flag & V3D_BGPIC_FOREGROUND) != fg_flag)
			continue;

		if ((bgpic->view == 0) || /* zero for any */
		    (bgpic->view & (1 << rv3d->view)) || /* check agaist flags */
		    (rv3d->persp == RV3D_CAMOB && bgpic->view == (1 << RV3D_VIEW_CAMERA)))
		{
			float image_aspect[2];
			float fac, asp, zoomx, zoomy;
			float x1, y1, x2, y2, centx, centy;

			ImBuf *ibuf = NULL, *freeibuf, *releaseibuf;
			void *lock;
			rctf clip_rect;

			Image *ima = NULL;
			MovieClip *clip = NULL;

			/* disable individual images */
			if ((bgpic->flag & V3D_BGPIC_DISABLED))
				continue;

			freeibuf = NULL;
			releaseibuf = NULL;
			if (bgpic->source == V3D_BGPIC_IMAGE) {
				ima = bgpic->ima;
				if (ima == NULL)
					continue;
				BKE_image_user_frame_calc(&bgpic->iuser, CFRA, 0);
				if (ima->source == IMA_SRC_SEQUENCE && !(bgpic->iuser.flag & IMA_USER_FRAME_IN_RANGE)) {
					ibuf = NULL; /* frame is out of range, dont show */
				}
				else {
					view3d_stereo_bgpic_setup(scene, v3d, ima, &bgpic->iuser);
					ibuf = BKE_image_acquire_ibuf(ima, &bgpic->iuser, &lock);
					releaseibuf = ibuf;
				}

				image_aspect[0] = ima->aspx;
				image_aspect[1] = ima->aspy;
			}
			else if (bgpic->source == V3D_BGPIC_MOVIE) {
				/* TODO: skip drawing when out of frame range (as image sequences do above) */

				if (bgpic->flag & V3D_BGPIC_CAMERACLIP) {
					if (scene->camera)
						clip = BKE_object_movieclip_get(scene, scene->camera, true);
				}
				else {
					clip = bgpic->clip;
				}

				if (clip == NULL)
					continue;

				BKE_movieclip_user_set_frame(&bgpic->cuser, CFRA);
				ibuf = BKE_movieclip_get_ibuf(clip, &bgpic->cuser);

				image_aspect[0] = clip->aspx;
				image_aspect[1] = clip->aspy;

				/* working with ibuf from image and clip has got different workflow now.
				 * ibuf acquired from clip is referenced by cache system and should
				 * be dereferenced after usage. */
				freeibuf = ibuf;
			}
			else {
				/* perhaps when loading future files... */
				BLI_assert(0);
				copy_v2_fl(image_aspect, 1.0f);
			}

			if (ibuf == NULL)
				continue;

			if ((ibuf->rect == NULL && ibuf->rect_float == NULL) || ibuf->channels != 4) { /* invalid image format */
				if (freeibuf)
					IMB_freeImBuf(freeibuf);
				if (releaseibuf)
					BKE_image_release_ibuf(ima, releaseibuf, lock);

				continue;
			}

			if (ibuf->rect == NULL)
				IMB_rect_from_float(ibuf);

			if (rv3d->persp == RV3D_CAMOB) {

				if (do_camera_frame) {
					rctf vb;
					ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &vb, false);
					x1 = vb.xmin;
					y1 = vb.ymin;
					x2 = vb.xmax;
					y2 = vb.ymax;
				}
				else {
					x1 = ar->winrct.xmin;
					y1 = ar->winrct.ymin;
					x2 = ar->winrct.xmax;
					y2 = ar->winrct.ymax;
				}

				/* apply offset last - camera offset is different to offset in blender units */
				/* so this has some sane way of working - this matches camera's shift _exactly_ */
				{
					const float max_dim = max_ff(x2 - x1, y2 - y1);
					const float xof_scale = bgpic->xof * max_dim;
					const float yof_scale = bgpic->yof * max_dim;

					x1 += xof_scale;
					y1 += yof_scale;
					x2 += xof_scale;
					y2 += yof_scale;
				}

				centx = (x1 + x2) / 2.0f;
				centy = (y1 + y2) / 2.0f;

				/* aspect correction */
				if (bgpic->flag & V3D_BGPIC_CAMERA_ASPECT) {
					/* apply aspect from clip */
					const float w_src = ibuf->x * image_aspect[0];
					const float h_src = ibuf->y * image_aspect[1];

					/* destination aspect is already applied from the camera frame */
					const float w_dst = x1 - x2;
					const float h_dst = y1 - y2;

					const float asp_src = w_src / h_src;
					const float asp_dst = w_dst / h_dst;

					if (fabsf(asp_src - asp_dst) >= FLT_EPSILON) {
						if ((asp_src > asp_dst) == ((bgpic->flag & V3D_BGPIC_CAMERA_CROP) != 0)) {
							/* fit X */
							const float div = asp_src / asp_dst;
							x1 = ((x1 - centx) * div) + centx;
							x2 = ((x2 - centx) * div) + centx;
						}
						else {
							/* fit Y */
							const float div = asp_dst / asp_src;
							y1 = ((y1 - centy) * div) + centy;
							y2 = ((y2 - centy) * div) + centy;
						}
					}
				}
			}
			else {
				float tvec[3];
				float sco[2];
				const float mval_f[2] = {1.0f, 0.0f};
				const float co_zero[3] = {0};
				float zfac;

				/* calc window coord */
				zfac = ED_view3d_calc_zfac(rv3d, co_zero, NULL);
				ED_view3d_win_to_delta(ar, mval_f, tvec, zfac);
				fac = max_ff(fabsf(tvec[0]), max_ff(fabsf(tvec[1]), fabsf(tvec[2]))); /* largest abs axis */
				fac = 1.0f / fac;

				asp = (float)ibuf->y / (float)ibuf->x;

				zero_v3(tvec);
				ED_view3d_project_float_v2_m4(ar, tvec, sco, rv3d->persmat);

				x1 =  sco[0] + fac * (bgpic->xof - bgpic->size);
				y1 =  sco[1] + asp * fac * (bgpic->yof - bgpic->size);
				x2 =  sco[0] + fac * (bgpic->xof + bgpic->size);
				y2 =  sco[1] + asp * fac * (bgpic->yof + bgpic->size);

				centx = (x1 + x2) / 2.0f;
				centy = (y1 + y2) / 2.0f;
			}

			/* complete clip? */
			BLI_rctf_init(&clip_rect, x1, x2, y1, y2);
			if (bgpic->rotation) {
				BLI_rctf_rotate_expand(&clip_rect, &clip_rect, bgpic->rotation);
			}

			if (clip_rect.xmax < 0 || clip_rect.ymax < 0 || clip_rect.xmin > ar->winx || clip_rect.ymin > ar->winy) {
				if (freeibuf)
					IMB_freeImBuf(freeibuf);
				if (releaseibuf)
					BKE_image_release_ibuf(ima, releaseibuf, lock);

				continue;
			}

			zoomx = (x2 - x1) / ibuf->x;
			zoomy = (y2 - y1) / ibuf->y;

			/* for some reason; zoomlevels down refuses to use GL_ALPHA_SCALE */
			if (zoomx < 1.0f || zoomy < 1.0f) {
				float tzoom = min_ff(zoomx, zoomy);
				int mip = 0;

				if ((ibuf->userflags & IB_MIPMAP_INVALID) != 0) {
					IMB_remakemipmap(ibuf, 0);
					ibuf->userflags &= ~IB_MIPMAP_INVALID;
				}
				else if (ibuf->mipmap[0] == NULL)
					IMB_makemipmap(ibuf, 0);

				while (tzoom < 1.0f && mip < 8 && ibuf->mipmap[mip]) {
					tzoom *= 2.0f;
					zoomx *= 2.0f;
					zoomy *= 2.0f;
					mip++;
				}
				if (mip > 0)
					ibuf = ibuf->mipmap[mip - 1];
			}

			if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
			glDepthMask(0);

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA);

			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			ED_region_pixelspace(ar);

			glTranslatef(centx, centy, 0.0);
			glRotatef(RAD2DEGF(-bgpic->rotation), 0.0f, 0.0f, 1.0f);

			if (bgpic->flag & V3D_BGPIC_FLIP_X) {
				zoomx *= -1.0f;
				x1 = x2;
			}
			if (bgpic->flag & V3D_BGPIC_FLIP_Y) {
				zoomy *= -1.0f;
				y1 = y2;
			}
			glPixelZoom(zoomx, zoomy);
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f - bgpic->blend);

			/* could not use glaDrawPixelsAuto because it could fallback to
			 * glaDrawPixelsSafe in some cases, which will end up in missing
			 * alpha transparency for the background image (sergey)
			 */
			glaDrawPixelsTex(x1 - centx, y1 - centy, ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR, ibuf->rect);

			glPixelZoom(1.0, 1.0);
			glPixelTransferf(GL_ALPHA_SCALE, 1.0f);

			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();

			glDisable(GL_BLEND);

			glDepthMask(1);
			if (v3d->zbuf) glEnable(GL_DEPTH_TEST);

			if (freeibuf)
				IMB_freeImBuf(freeibuf);
			if (releaseibuf)
				BKE_image_release_ibuf(ima, releaseibuf, lock);
		}
	}
}

static void view3d_draw_bgpic_test(Scene *scene, ARegion *ar, View3D *v3d,
                                   const bool do_foreground, const bool do_camera_frame)
{
	RegionView3D *rv3d = ar->regiondata;

	if ((v3d->flag & V3D_DISPBGPICS) == 0)
		return;

	/* disabled - mango request, since footage /w only render is quite useful
	 * and this option is easy to disable all background images at once */
#if 0
	if (v3d->flag2 & V3D_RENDER_OVERRIDE)
		return;
#endif

	if ((rv3d->view == RV3D_VIEW_USER) || (rv3d->persp != RV3D_ORTHO)) {
		if (rv3d->persp == RV3D_CAMOB) {
			view3d_draw_bgpic(scene, ar, v3d, do_foreground, do_camera_frame);
		}
	}
	else {
		view3d_draw_bgpic(scene, ar, v3d, do_foreground, do_camera_frame);
	}
}

/* ****************** View3d afterdraw *************** */

typedef struct View3DAfter {
	struct View3DAfter *next, *prev;
	struct Base *base;
	short dflag;
} View3DAfter;

/* temp storage of Objects that need to be drawn as last */
void ED_view3d_after_add(ListBase *lb, Base *base, const short dflag)
{
	View3DAfter *v3da = MEM_callocN(sizeof(View3DAfter), "View 3d after");
	BLI_assert((base->flag & OB_FROMDUPLI) == 0);
	BLI_addtail(lb, v3da);
	v3da->base = base;
	v3da->dflag = dflag;
}

/* disables write in zbuffer and draws it over */
static void view3d_draw_transp(Scene *scene, ARegion *ar, View3D *v3d)
{
	View3DAfter *v3da;
	
	glDepthMask(GL_FALSE);
	v3d->transp = true;
	
	while ((v3da = BLI_pophead(&v3d->afterdraw_transp))) {
		draw_object(scene, ar, v3d, v3da->base, v3da->dflag);
		MEM_freeN(v3da);
	}
	v3d->transp = false;
	
	glDepthMask(GL_TRUE);
	
}

/* clears zbuffer and draws it over */
static void view3d_draw_xray(Scene *scene, ARegion *ar, View3D *v3d, bool *clear)
{
	View3DAfter *v3da;

	if (*clear && v3d->zbuf) {
		glClear(GL_DEPTH_BUFFER_BIT);
		*clear = false;
	}

	v3d->xray = true;
	while ((v3da = BLI_pophead(&v3d->afterdraw_xray))) {
		draw_object(scene, ar, v3d, v3da->base, v3da->dflag);
		MEM_freeN(v3da);
	}
	v3d->xray = false;
}


/* clears zbuffer and draws it over */
static void view3d_draw_xraytransp(Scene *scene, ARegion *ar, View3D *v3d, const bool clear)
{
	View3DAfter *v3da;

	if (clear && v3d->zbuf)
		glClear(GL_DEPTH_BUFFER_BIT);

	v3d->xray = true;
	v3d->transp = true;
	
	glDepthMask(GL_FALSE);

	while ((v3da = BLI_pophead(&v3d->afterdraw_xraytransp))) {
		draw_object(scene, ar, v3d, v3da->base, v3da->dflag);
		MEM_freeN(v3da);
	}

	v3d->transp = false;
	v3d->xray = false;

	glDepthMask(GL_TRUE);
}

/* clears zbuffer and draws it over,
 * note that in the select version we don't care about transparent flag as with regular drawing */
static void view3d_draw_xray_select(Scene *scene, ARegion *ar, View3D *v3d, bool *clear)
{
	/* Not ideal, but we need to read from the previous depths before clearing
	 * otherwise we could have a function to load the depths after drawing.
	 *
	 * Clearing the depth buffer isn't all that common between drawing objects so accept this for now.
	 */
	if (U.gpu_select_pick_deph) {
		GPU_select_load_id(-1);
	}

	View3DAfter *v3da;
	if (*clear && v3d->zbuf) {
		glClear(GL_DEPTH_BUFFER_BIT);
		*clear = false;
	}

	v3d->xray = true;
	while ((v3da = BLI_pophead(&v3d->afterdraw_xray))) {
		if (GPU_select_load_id(v3da->base->selcol)) {
			draw_object_select(scene, ar, v3d, v3da->base, v3da->dflag);
		}
		MEM_freeN(v3da);
	}
	v3d->xray = false;
}

/* *********************** */

/*
 * In most cases call draw_dupli_objects,
 * draw_dupli_objects_color was added because when drawing set dupli's
 * we need to force the color
 */

#if 0
int dupli_ob_sort(void *arg1, void *arg2)
{
	void *p1 = ((DupliObject *)arg1)->ob;
	void *p2 = ((DupliObject *)arg2)->ob;
	int val = 0;
	if (p1 < p2) val = -1;
	else if (p1 > p2) val = 1;
	return val;
}
#endif


static DupliObject *dupli_step(DupliObject *dob)
{
	while (dob && dob->no_draw)
		dob = dob->next;
	return dob;
}

static void draw_dupli_objects_color(
        Scene *scene, ARegion *ar, View3D *v3d, Base *base,
        const short dflag, const int color)
{
	RegionView3D *rv3d = ar->regiondata;
	ListBase *lb;
	LodLevel *savedlod;
	DupliObject *dob_prev = NULL, *dob, *dob_next = NULL;
	Base tbase = {NULL};
	BoundBox bb, *bb_tmp; /* use a copy because draw_object, calls clear_mesh_caches */
	GLuint displist = 0;
	unsigned char color_rgb[3];
	const short dflag_dupli = dflag | DRAW_CONSTCOLOR;
	short transflag;
	bool use_displist = false;  /* -1 is initialize */
	char dt;
	short dtx;
	DupliApplyData *apply_data;

	if (base->object->restrictflag & OB_RESTRICT_VIEW) return;
	if ((base->object->restrictflag & OB_RESTRICT_RENDER) && (v3d->flag2 & V3D_RENDER_OVERRIDE)) return;

	if (dflag & DRAW_CONSTCOLOR) {
		BLI_assert(color == TH_UNDEFINED);
	}
	else {
		UI_GetThemeColorBlend3ubv(color, TH_BACK, 0.5f, color_rgb);
	}

	tbase.flag = OB_FROMDUPLI | base->flag;
	lb = object_duplilist(G.main->eval_ctx, scene, base->object);
	// BLI_listbase_sort(lb, dupli_ob_sort); /* might be nice to have if we have a dupli list with mixed objects. */

	apply_data = duplilist_apply(base->object, scene, lb);

	dob = dupli_step(lb->first);
	if (dob) dob_next = dupli_step(dob->next);

	for (; dob; dob_prev = dob, dob = dob_next, dob_next = dob_next ? dupli_step(dob_next->next) : NULL) {
		bool testbb = false;

		tbase.object = dob->ob;

		/* Make sure lod is updated from dupli's position */
		savedlod = dob->ob->currentlod;

#ifdef WITH_GAMEENGINE
		if (rv3d->rflag & RV3D_IS_GAME_ENGINE) {
			BKE_object_lod_update(dob->ob, rv3d->viewinv[3]);
		}
#endif

		/* extra service: draw the duplicator in drawtype of parent, minimum taken
		 * to allow e.g. boundbox box objects in groups for LOD */
		dt = tbase.object->dt;
		tbase.object->dt = MIN2(tbase.object->dt, base->object->dt);

		/* inherit draw extra, but not if a boundbox under the assumption that this
		 * is intended to speed up drawing, and drawing extra (especially wire) can
		 * slow it down too much */
		dtx = tbase.object->dtx;
		if (tbase.object->dt != OB_BOUNDBOX)
			tbase.object->dtx = base->object->dtx;

		/* negative scale flag has to propagate */
		transflag = tbase.object->transflag;

		if (is_negative_m4(dob->mat))
			tbase.object->transflag |= OB_NEG_SCALE;
		else
			tbase.object->transflag &= ~OB_NEG_SCALE;
		
		/* should move outside the loop but possible color is set in draw_object still */
		if ((dflag & DRAW_CONSTCOLOR) == 0) {
			glColor3ubv(color_rgb);
		}
		
		/* generate displist, test for new object */
		if (dob_prev && dob_prev->ob != dob->ob) {
			if (use_displist == true)
				glDeleteLists(displist, 1);
			
			use_displist = false;
		}
		
		if ((bb_tmp = BKE_object_boundbox_get(dob->ob))) {
			bb = *bb_tmp; /* must make a copy  */
			testbb = true;
		}

		if (!testbb || ED_view3d_boundbox_clip_ex(rv3d, &bb, dob->mat)) {
			/* generate displist */
			if (use_displist == false) {
				
				/* note, since this was added, its checked (dob->type == OB_DUPLIGROUP)
				 * however this is very slow, it was probably needed for the NLA
				 * offset feature (used in group-duplicate.blend but no longer works in 2.5)
				 * so for now it should be ok to - campbell */
				
				if ( /* if this is the last no need  to make a displist */
				     (dob_next == NULL || dob_next->ob != dob->ob) ||
				     /* lamp drawing messes with matrices, could be handled smarter... but this works */
				     (dob->ob->type == OB_LAMP) ||
				     (dob->type == OB_DUPLIGROUP && dob->animated) ||
				     !bb_tmp ||
				     draw_glsl_material(scene, dob->ob, v3d, dt) ||
				     check_object_draw_texture(scene, v3d, dt) ||
				     (v3d->flag2 & V3D_SOLID_MATCAP) != 0)
				{
					// printf("draw_dupli_objects_color: skipping displist for %s\n", dob->ob->id.name + 2);
					use_displist = false;
				}
				else {
					// printf("draw_dupli_objects_color: using displist for %s\n", dob->ob->id.name + 2);
					
					/* disable boundbox check for list creation */
					BKE_object_boundbox_flag(dob->ob, BOUNDBOX_DISABLED, 1);
					/* need this for next part of code */
					unit_m4(dob->ob->obmat);    /* obmat gets restored */
					
					displist = glGenLists(1);
					glNewList(displist, GL_COMPILE);
					draw_object(scene, ar, v3d, &tbase, dflag_dupli);
					glEndList();
					
					use_displist = true;
					BKE_object_boundbox_flag(dob->ob, BOUNDBOX_DISABLED, 0);
				}		
			}
			
			if (use_displist) {
				glPushMatrix();
				glMultMatrixf(dob->mat);
				glCallList(displist);
				glPopMatrix();
			}	
			else {
				copy_m4_m4(dob->ob->obmat, dob->mat);
				GPU_begin_dupli_object(dob);
				draw_object(scene, ar, v3d, &tbase, dflag_dupli);
				GPU_end_dupli_object();
			}
		}
		
		tbase.object->dt = dt;
		tbase.object->dtx = dtx;
		tbase.object->transflag = transflag;
		tbase.object->currentlod = savedlod;
	}

	if (apply_data) {
		duplilist_restore(lb, apply_data);
		duplilist_free_apply_data(apply_data);
	}

	free_object_duplilist(lb);
	
	if (use_displist)
		glDeleteLists(displist, 1);
}

static void draw_dupli_objects(Scene *scene, ARegion *ar, View3D *v3d, Base *base)
{
	/* define the color here so draw_dupli_objects_color can be called
	 * from the set loop */
	
	int color = (base->flag & SELECT) ? TH_SELECT : TH_WIRE;
	/* debug */
	if (base->object->dup_group && base->object->dup_group->id.us < 1)
		color = TH_REDALERT;
	
	draw_dupli_objects_color(scene, ar, v3d, base, 0, color);
}

/* XXX warning, not using gpu offscreen here */
void view3d_update_depths_rect(ARegion *ar, ViewDepths *d, rcti *rect)
{
	int x, y, w, h;
	rcti r;
	/* clamp rect by region */

	r.xmin = 0;
	r.xmax = ar->winx - 1;
	r.ymin = 0;
	r.ymax = ar->winy - 1;

	/* Constrain rect to depth bounds */
	BLI_rcti_isect(&r, rect, rect);

	/* assign values to compare with the ViewDepths */
	x = rect->xmin;
	y = rect->ymin;

	w = BLI_rcti_size_x(rect);
	h = BLI_rcti_size_y(rect);

	if (w <= 0 || h <= 0) {
		if (d->depths)
			MEM_freeN(d->depths);
		d->depths = NULL;

		d->damaged = false;
	}
	else if (d->w != w ||
	         d->h != h ||
	         d->x != x ||
	         d->y != y ||
	         d->depths == NULL
	         )
	{
		d->x = x;
		d->y = y;
		d->w = w;
		d->h = h;

		if (d->depths)
			MEM_freeN(d->depths);

		d->depths = MEM_mallocN(sizeof(float) * d->w * d->h, "View depths Subset");
		
		d->damaged = true;
	}

	if (d->damaged) {
		/* XXX using special function here, it doesn't use the gpu offscreen system */
		view3d_opengl_read_Z_pixels(ar, d->x, d->y, d->w, d->h, GL_DEPTH_COMPONENT, GL_FLOAT, d->depths);
		glGetDoublev(GL_DEPTH_RANGE, d->depth_range);
		d->damaged = false;
	}
}

/* note, with nouveau drivers the glReadPixels() is very slow. [#24339] */
void ED_view3d_depth_update(ARegion *ar)
{
	RegionView3D *rv3d = ar->regiondata;
	
	/* Create storage for, and, if necessary, copy depth buffer */
	if (!rv3d->depths) rv3d->depths = MEM_callocN(sizeof(ViewDepths), "ViewDepths");
	if (rv3d->depths) {
		ViewDepths *d = rv3d->depths;
		if (d->w != ar->winx ||
		    d->h != ar->winy ||
		    !d->depths)
		{
			d->w = ar->winx;
			d->h = ar->winy;
			if (d->depths)
				MEM_freeN(d->depths);
			d->depths = MEM_mallocN(sizeof(float) * d->w * d->h, "View depths");
			d->damaged = true;
		}
		
		if (d->damaged) {
			view3d_opengl_read_pixels(ar, 0, 0, d->w, d->h, GL_DEPTH_COMPONENT, GL_FLOAT, d->depths);
			glGetDoublev(GL_DEPTH_RANGE, d->depth_range);
			
			d->damaged = false;
		}
	}
}

/* utility function to find the closest Z value, use for autodepth */
float view3d_depth_near(ViewDepths *d)
{
	/* convert to float for comparisons */
	const float near = (float)d->depth_range[0];
	const float far_real = (float)d->depth_range[1];
	float far = far_real;

	const float *depths = d->depths;
	float depth = FLT_MAX;
	int i = (int)d->w * (int)d->h; /* cast to avoid short overflow */

	/* far is both the starting 'far' value
	 * and the closest value found. */
	while (i--) {
		depth = *depths++;
		if ((depth < far) && (depth > near)) {
			far = depth;
		}
	}

	return far == far_real ? FLT_MAX : far;
}

void ED_view3d_draw_depth_gpencil(Scene *scene, ARegion *ar, View3D *v3d)
{
	short zbuf = v3d->zbuf;
	RegionView3D *rv3d = ar->regiondata;

	/* Setup view matrix. */
	ED_view3d_draw_setup_view(NULL, scene, ar, v3d, rv3d->viewmat, rv3d->winmat, NULL);

	glClear(GL_DEPTH_BUFFER_BIT);

	v3d->zbuf = true;
	glEnable(GL_DEPTH_TEST);

	if (v3d->flag2 & V3D_SHOW_GPENCIL) {
		ED_gpencil_draw_view3d(NULL, scene, v3d, ar, true);
	}
	
	v3d->zbuf = zbuf;
}

static void view3d_draw_depth_loop(Scene *scene, ARegion *ar, View3D *v3d)
{
	Base *base;

	/* no need for color when drawing depth buffer */
	const short dflag_depth = DRAW_CONSTCOLOR;

	/* draw set first */
	if (scene->set) {
		Scene *sce_iter;
		for (SETLOOPER(scene->set, sce_iter, base)) {
			if (v3d->lay & base->lay) {
				draw_object(scene, ar, v3d, base, 0);
				if (base->object->transflag & OB_DUPLI) {
					draw_dupli_objects_color(scene, ar, v3d, base, dflag_depth, TH_UNDEFINED);
				}
			}
		}
	}
	
	for (base = scene->base.first; base; base = base->next) {
		if (v3d->lay & base->lay) {
			/* dupli drawing */
			if (base->object->transflag & OB_DUPLI) {
				draw_dupli_objects_color(scene, ar, v3d, base, dflag_depth, TH_UNDEFINED);
			}
			draw_object(scene, ar, v3d, base, dflag_depth);
		}
	}
	
	/* this isn't that nice, draw xray objects as if they are normal */
	if (v3d->afterdraw_transp.first ||
	    v3d->afterdraw_xray.first ||
	    v3d->afterdraw_xraytransp.first)
	{
		View3DAfter *v3da;
		int mask_orig;

		v3d->xray = true;
		
		/* transp materials can change the depth mask, see #21388 */
		glGetIntegerv(GL_DEPTH_WRITEMASK, &mask_orig);


		if (v3d->afterdraw_xray.first || v3d->afterdraw_xraytransp.first) {
			glDepthFunc(GL_ALWAYS); /* always write into the depth bufer, overwriting front z values */
			for (v3da = v3d->afterdraw_xray.first; v3da; v3da = v3da->next) {
				draw_object(scene, ar, v3d, v3da->base, dflag_depth);
			}
			glDepthFunc(GL_LEQUAL); /* Now write the depth buffer normally */
		}

		/* draw 3 passes, transp/xray/xraytransp */
		v3d->xray = false;
		v3d->transp = true;
		while ((v3da = BLI_pophead(&v3d->afterdraw_transp))) {
			draw_object(scene, ar, v3d, v3da->base, dflag_depth);
			MEM_freeN(v3da);
		}

		v3d->xray = true;
		v3d->transp = false;
		while ((v3da = BLI_pophead(&v3d->afterdraw_xray))) {
			draw_object(scene, ar, v3d, v3da->base, dflag_depth);
			MEM_freeN(v3da);
		}

		v3d->xray = true;
		v3d->transp = true;
		while ((v3da = BLI_pophead(&v3d->afterdraw_xraytransp))) {
			draw_object(scene, ar, v3d, v3da->base, dflag_depth);
			MEM_freeN(v3da);
		}

		
		v3d->xray = false;
		v3d->transp = false;

		glDepthMask(mask_orig);
	}
}

void ED_view3d_draw_depth(Scene *scene, ARegion *ar, View3D *v3d, bool alphaoverride)
{
	RegionView3D *rv3d = ar->regiondata;
	short zbuf = v3d->zbuf;
	short flag = v3d->flag;
	float glalphaclip = U.glalphaclip;
	int obcenter_dia = U.obcenter_dia;
	/* temp set drawtype to solid */
	
	/* Setting these temporarily is not nice */
	v3d->flag &= ~V3D_SELECT_OUTLINE;
	U.glalphaclip = alphaoverride ? 0.5f : glalphaclip; /* not that nice but means we wont zoom into billboards */
	U.obcenter_dia = 0;
	
	/* Setup view matrix. */
	ED_view3d_draw_setup_view(NULL, scene, ar, v3d, rv3d->viewmat, rv3d->winmat, NULL);
	
	glClear(GL_DEPTH_BUFFER_BIT);
	
	if (rv3d->rflag & RV3D_CLIPPING) {
		ED_view3d_clipping_set(rv3d);
	}
	/* get surface depth without bias */
	rv3d->rflag |= RV3D_ZOFFSET_DISABLED;

	v3d->zbuf = true;
	glEnable(GL_DEPTH_TEST);

	view3d_draw_depth_loop(scene, ar, v3d);

	if (rv3d->rflag & RV3D_CLIPPING) {
		ED_view3d_clipping_disable();
	}
	rv3d->rflag &= ~RV3D_ZOFFSET_DISABLED;
	
	v3d->zbuf = zbuf;
	if (!v3d->zbuf) glDisable(GL_DEPTH_TEST);

	U.glalphaclip = glalphaclip;
	v3d->flag = flag;
	U.obcenter_dia = obcenter_dia;
}

void ED_view3d_draw_select_loop(
        ViewContext *vc, Scene *scene, View3D *v3d, ARegion *ar,
        bool use_obedit_skip, bool use_nearest)
{
	short code = 1;
	const short dflag = DRAW_PICKING | DRAW_CONSTCOLOR;

	if (vc->obedit && vc->obedit->type == OB_MBALL) {
		draw_object(scene, ar, v3d, BASACT, dflag);
	}
	else if ((vc->obedit && vc->obedit->type == OB_ARMATURE)) {
		/* if not drawing sketch, draw bones */
		if (!BDR_drawSketchNames(vc)) {
			draw_object(scene, ar, v3d, BASACT, dflag);
		}
	}
	else {
		Base *base;

		for (base = scene->base.first; base; base = base->next) {
			if (base->lay & v3d->lay) {

				if ((base->object->restrictflag & OB_RESTRICT_SELECT) ||
				    (use_obedit_skip && (scene->obedit->data == base->object->data)))
				{
					base->selcol = 0;
				}
				else {
					base->selcol = code;

					if (use_nearest && (base->object->dtx & OB_DRAWXRAY)) {
						ED_view3d_after_add(&v3d->afterdraw_xray, base, dflag);
					}
					else {
						if (GPU_select_load_id(code)) {
							draw_object_select(scene, ar, v3d, base, dflag);
						}
					}
					code++;
				}
			}
		}

		if (use_nearest) {
			bool xrayclear = true;
			if (v3d->afterdraw_xray.first) {
				view3d_draw_xray_select(scene, ar, v3d, &xrayclear);
			}
		}
	}
}

typedef struct View3DShadow {
	struct View3DShadow *next, *prev;
	GPULamp *lamp;
} View3DShadow;

static void gpu_render_lamp_update(Scene *scene, View3D *v3d,
                                   Object *ob, Object *par,
                                   float obmat[4][4], unsigned int lay,
                                   ListBase *shadows, SceneRenderLayer *srl)
{
	GPULamp *lamp;
	Lamp *la = (Lamp *)ob->data;
	View3DShadow *shadow;
	unsigned int layers;
	
	lamp = GPU_lamp_from_blender(scene, ob, par);
	
	if (lamp) {
		GPU_lamp_update(lamp, lay, (ob->restrictflag & OB_RESTRICT_RENDER), obmat);
		GPU_lamp_update_colors(lamp, la->r, la->g, la->b, la->energy);
		
		layers = lay & v3d->lay;
		if (srl)
			layers &= srl->lay;

		if (layers &&
		    GPU_lamp_has_shadow_buffer(lamp) &&
		    /* keep last, may do string lookup */
		    GPU_lamp_override_visible(lamp, srl, NULL))
		{
			shadow = MEM_callocN(sizeof(View3DShadow), "View3DShadow");
			shadow->lamp = lamp;
			BLI_addtail(shadows, shadow);
		}
	}
}

static void gpu_update_lamps_shadows_world(Scene *scene, View3D *v3d)
{
	ListBase shadows;
	View3DShadow *shadow;
	Scene *sce_iter;
	Base *base;
	Object *ob;
	World *world = scene->world;
	SceneRenderLayer *srl = v3d->scenelock ? BLI_findlink(&scene->r.layers, scene->r.actlay) : NULL;
	
	BLI_listbase_clear(&shadows);
	
	/* update lamp transform and gather shadow lamps */
	for (SETLOOPER(scene, sce_iter, base)) {
		ob = base->object;
		
		if (ob->type == OB_LAMP)
			gpu_render_lamp_update(scene, v3d, ob, NULL, ob->obmat, ob->lay, &shadows, srl);
		
		if (ob->transflag & OB_DUPLI) {
			DupliObject *dob;
			ListBase *lb = object_duplilist(G.main->eval_ctx, scene, ob);
			
			for (dob = lb->first; dob; dob = dob->next)
				if (dob->ob->type == OB_LAMP)
					gpu_render_lamp_update(scene, v3d, dob->ob, ob, dob->mat, ob->lay, &shadows, srl);
			
			free_object_duplilist(lb);
		}
	}
	
	/* render shadows after updating all lamps, nested object_duplilist
	 * don't work correct since it's replacing object matrices */
	for (shadow = shadows.first; shadow; shadow = shadow->next) {
		/* this needs to be done better .. */
		float viewmat[4][4], winmat[4][4];
		int drawtype, lay, winsize, flag2 = v3d->flag2;
		ARegion ar = {NULL};
		RegionView3D rv3d = {{{0}}};
		
		drawtype = v3d->drawtype;
		lay = v3d->lay;
		
		v3d->drawtype = OB_SOLID;
		v3d->lay &= GPU_lamp_shadow_layer(shadow->lamp);
		v3d->flag2 &= ~(V3D_SOLID_TEX | V3D_SHOW_SOLID_MATCAP);
		v3d->flag2 |= V3D_RENDER_OVERRIDE | V3D_RENDER_SHADOW;
		
		GPU_lamp_shadow_buffer_bind(shadow->lamp, viewmat, &winsize, winmat);

		ar.regiondata = &rv3d;
		ar.regiontype = RGN_TYPE_WINDOW;
		rv3d.persp = RV3D_CAMOB;
		copy_m4_m4(rv3d.winmat, winmat);
		copy_m4_m4(rv3d.viewmat, viewmat);
		invert_m4_m4(rv3d.viewinv, rv3d.viewmat);
		mul_m4_m4m4(rv3d.persmat, rv3d.winmat, rv3d.viewmat);
		invert_m4_m4(rv3d.persinv, rv3d.viewinv);

		/* no need to call ED_view3d_draw_offscreen_init since shadow buffers were already updated */
		ED_view3d_draw_offscreen(
		            scene, v3d, &ar, winsize, winsize, viewmat, winmat,
		            false, false, true,
		            NULL, NULL, NULL, NULL);
		GPU_lamp_shadow_buffer_unbind(shadow->lamp);
		
		v3d->drawtype = drawtype;
		v3d->lay = lay;
		v3d->flag2 = flag2;
	}
	
	BLI_freelistN(&shadows);

	/* update world values */
	if (world) {
		GPU_mist_update_enable(world->mode & WO_MIST);
		GPU_mist_update_values(world->mistype, world->miststa, world->mistdist, world->misi, &world->horr);
		GPU_horizon_update_color(&world->horr);
		GPU_ambient_update_color(&world->ambr);
		GPU_zenith_update_color(&world->zenr);
	}
}

/* *********************** customdata **************** */

CustomDataMask ED_view3d_datamask(const Scene *scene, const View3D *v3d)
{
	CustomDataMask mask = 0;
	const int drawtype = view3d_effective_drawtype(v3d);

	if (ELEM(drawtype, OB_TEXTURE, OB_MATERIAL) ||
	    ((drawtype == OB_SOLID) && (v3d->flag2 & V3D_SOLID_TEX)))
	{
		mask |= CD_MASK_MTEXPOLY | CD_MASK_MLOOPUV | CD_MASK_MLOOPCOL;

		if (BKE_scene_use_new_shading_nodes(scene)) {
			if (drawtype == OB_MATERIAL)
				mask |= CD_MASK_ORCO;
		}
		else {
			if ((scene->gm.matmode == GAME_MAT_GLSL && drawtype == OB_TEXTURE) || 
			    (drawtype == OB_MATERIAL))
			{
				mask |= CD_MASK_ORCO;
			}
		}
	}

	return mask;
}

/* goes over all modes and view3d settings */
CustomDataMask ED_view3d_screen_datamask(const bScreen *screen)
{
	const Scene *scene = screen->scene;
	CustomDataMask mask = CD_MASK_BAREMESH;
	const ScrArea *sa;
	
	/* check if we need tfaces & mcols due to view mode */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		if (sa->spacetype == SPACE_VIEW3D) {
			mask |= ED_view3d_datamask(scene, sa->spacedata.first);
		}
	}

	return mask;
}

/**
 * \note keep this synced with #ED_view3d_mats_rv3d_backup/#ED_view3d_mats_rv3d_restore
 */
void ED_view3d_update_viewmat(
        Scene *scene, View3D *v3d, ARegion *ar, float viewmat[4][4], float winmat[4][4], const rcti *rect)
{
	RegionView3D *rv3d = ar->regiondata;

	/* setup window matrices */
	if (winmat)
		copy_m4_m4(rv3d->winmat, winmat);
	else
		view3d_winmatrix_set(ar, v3d, rect);

	/* setup view matrix */
	if (viewmat) {
		copy_m4_m4(rv3d->viewmat, viewmat);
	}
	else {
		float rect_scale[2];
		if (rect) {
			rect_scale[0] = (float)BLI_rcti_size_x(rect) / (float)ar->winx;
			rect_scale[1] = (float)BLI_rcti_size_y(rect) / (float)ar->winy;
		}
		view3d_viewmatrix_set(scene, v3d, rv3d, rect ? rect_scale : NULL);  /* note: calls BKE_object_where_is_calc for camera... */
	}

	/* update utility matrices */
	mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);
	invert_m4_m4(rv3d->persinv, rv3d->persmat);
	invert_m4_m4(rv3d->viewinv, rv3d->viewmat);
	
	/* calculate GLSL view dependent values */

	/* store window coordinates scaling/offset */
	if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
		rctf cameraborder;
		ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &cameraborder, false);
		rv3d->viewcamtexcofac[0] = (float)ar->winx / BLI_rctf_size_x(&cameraborder);
		rv3d->viewcamtexcofac[1] = (float)ar->winy / BLI_rctf_size_y(&cameraborder);
		
		rv3d->viewcamtexcofac[2] = -rv3d->viewcamtexcofac[0] * cameraborder.xmin / (float)ar->winx;
		rv3d->viewcamtexcofac[3] = -rv3d->viewcamtexcofac[1] * cameraborder.ymin / (float)ar->winy;
	}
	else {
		rv3d->viewcamtexcofac[0] = rv3d->viewcamtexcofac[1] = 1.0f;
		rv3d->viewcamtexcofac[2] = rv3d->viewcamtexcofac[3] = 0.0f;
	}

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
	{
		/* note:  '1.0f / len_v3(v1)'  replaced  'len_v3(rv3d->viewmat[0])'
		 * because of float point precision problems at large values [#23908] */
		float v1[3], v2[3];
		float len_px, len_sc;

		v1[0] = rv3d->persmat[0][0];
		v1[1] = rv3d->persmat[1][0];
		v1[2] = rv3d->persmat[2][0];

		v2[0] = rv3d->persmat[0][1];
		v2[1] = rv3d->persmat[1][1];
		v2[2] = rv3d->persmat[2][1];

		len_px = 2.0f / sqrtf(min_ff(len_squared_v3(v1), len_squared_v3(v2)));
		len_sc = (float)MAX2(ar->winx, ar->winy);

		rv3d->pixsize = len_px / len_sc;
	}
}

/**
 * Shared by #ED_view3d_draw_offscreen and #view3d_main_region_draw_objects
 *
 * \note \a C and \a grid_unit will be NULL when \a draw_offscreen is set.
 * \note Drawing lamps and opengl render uses this, so dont do grease pencil or view widgets here.
 */
static void view3d_draw_objects(
        const bContext *C,
        Scene *scene, View3D *v3d, ARegion *ar,
        const char **grid_unit,
        const bool do_bgpic, const bool draw_offscreen, GPUFX *fx)
{
	RegionView3D *rv3d = ar->regiondata;
	Base *base;
	const bool do_camera_frame = !draw_offscreen;
	const bool draw_grids = !draw_offscreen && (v3d->flag2 & V3D_RENDER_OVERRIDE) == 0;
	const bool draw_floor = (rv3d->view == RV3D_VIEW_USER) || (rv3d->persp != RV3D_ORTHO);
	/* only draw grids after in solid modes, else it hovers over mesh wires */
	const bool draw_grids_after = draw_grids && draw_floor && (v3d->drawtype > OB_WIRE) && fx;
	bool do_composite_xray = false;
	bool xrayclear = true;

	if (!draw_offscreen) {
		ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);
	}

	if (rv3d->rflag & RV3D_CLIPPING)
		view3d_draw_clipping(rv3d);

	/* set zbuffer after we draw clipping region */
	if (v3d->drawtype > OB_WIRE) {
		v3d->zbuf = true;
	}
	else {
		v3d->zbuf = false;
	}

	/* special case (depth for wire color) */
	if (v3d->drawtype <= OB_WIRE) {
		if (scene->obedit && scene->obedit->type == OB_MESH) {
			Mesh *me = scene->obedit->data;
			if (me->drawflag & ME_DRAWEIGHT) {
				v3d->zbuf = true;
			}
		}
	}

	if (v3d->zbuf) {
		glEnable(GL_DEPTH_TEST);
	}

	/* ortho grid goes first, does not write to depth buffer and doesn't need depth test so it will override
	 * objects if done last */
	if (draw_grids) {
		/* needs to be done always, gridview is adjusted in drawgrid() now, but only for ortho views. */
		rv3d->gridview = ED_view3d_grid_scale(scene, v3d, grid_unit);

		if (!draw_floor) {
			ED_region_pixelspace(ar);
			*grid_unit = NULL;  /* drawgrid need this to detect/affect smallest valid unit... */
			drawgrid(&scene->unit, ar, v3d, grid_unit);
			/* XXX make function? replaces persp(1) */
			glMatrixMode(GL_PROJECTION);
			glLoadMatrixf(rv3d->winmat);
			glMatrixMode(GL_MODELVIEW);
			glLoadMatrixf(rv3d->viewmat);
		}
		else if (!draw_grids_after) {
			drawfloor(scene, v3d, grid_unit, true);
		}
	}

	/* important to do before clipping */
	if (do_bgpic) {
		view3d_draw_bgpic_test(scene, ar, v3d, false, do_camera_frame);
	}

	if (rv3d->rflag & RV3D_CLIPPING) {
		ED_view3d_clipping_set(rv3d);
	}

	/* draw set first */
	if (scene->set) {
		const short dflag = DRAW_CONSTCOLOR | DRAW_SCENESET;
		Scene *sce_iter;
		for (SETLOOPER(scene->set, sce_iter, base)) {
			if (v3d->lay & base->lay) {
				UI_ThemeColorBlend(TH_WIRE, TH_BACK, 0.6f);
				draw_object(scene, ar, v3d, base, dflag);

				if (base->object->transflag & OB_DUPLI) {
					draw_dupli_objects_color(scene, ar, v3d, base, dflag, TH_UNDEFINED);
				}
			}
		}

		/* Transp and X-ray afterdraw stuff for sets is done later */
	}


	if (draw_offscreen) {
		for (base = scene->base.first; base; base = base->next) {
			if (v3d->lay & base->lay) {
				/* dupli drawing */
				if (base->object->transflag & OB_DUPLI)
					draw_dupli_objects(scene, ar, v3d, base);

				draw_object(scene, ar, v3d, base, 0);
			}
		}
	}
	else {
		unsigned int lay_used = 0;

		/* then draw not selected and the duplis, but skip editmode object */
		for (base = scene->base.first; base; base = base->next) {
			lay_used |= base->lay;

			if (v3d->lay & base->lay) {

				/* dupli drawing */
				if (base->object->transflag & OB_DUPLI) {
					draw_dupli_objects(scene, ar, v3d, base);
				}
				if ((base->flag & SELECT) == 0) {
					if (base->object != scene->obedit)
						draw_object(scene, ar, v3d, base, 0);
				}
			}
		}

		/* mask out localview */
		v3d->lay_used = lay_used & ((1 << 20) - 1);

		/* draw selected and editmode */
		for (base = scene->base.first; base; base = base->next) {
			if (v3d->lay & base->lay) {
				if (base->object == scene->obedit || (base->flag & SELECT)) {
					draw_object(scene, ar, v3d, base, 0);
				}
			}
		}
	}

	/* perspective floor goes last to use scene depth and avoid writing to depth buffer */
	if (draw_grids_after) {
		drawfloor(scene, v3d, grid_unit, false);
	}

	/* must be before xray draw which clears the depth buffer */
	if (v3d->flag2 & V3D_SHOW_GPENCIL) {
		wmWindowManager *wm = (C != NULL) ? CTX_wm_manager(C) : NULL;
		
		/* must be before xray draw which clears the depth buffer */
		if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
		ED_gpencil_draw_view3d(wm, scene, v3d, ar, true);
		if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
	}

	/* transp and X-ray afterdraw stuff */
	if (v3d->afterdraw_transp.first)     view3d_draw_transp(scene, ar, v3d);

	/* always do that here to cleanup depth buffers if none needed */
	if (fx) {
		do_composite_xray = v3d->zbuf && (v3d->afterdraw_xray.first || v3d->afterdraw_xraytransp.first);
		GPU_fx_compositor_setup_XRay_pass(fx, do_composite_xray);
	}

	if (v3d->afterdraw_xray.first)       view3d_draw_xray(scene, ar, v3d, &xrayclear);
	if (v3d->afterdraw_xraytransp.first) view3d_draw_xraytransp(scene, ar, v3d, xrayclear);

	if (fx && do_composite_xray) {
		GPU_fx_compositor_XRay_resolve(fx);
	}

	if (!draw_offscreen) {
		ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);
	}

	if (rv3d->rflag & RV3D_CLIPPING)
		ED_view3d_clipping_disable();

	/* important to do after clipping */
	if (do_bgpic) {
		view3d_draw_bgpic_test(scene, ar, v3d, true, do_camera_frame);
	}

	if (!draw_offscreen) {
		BIF_draw_manipulator(C);
	}

	/* cleanup */
	if (v3d->zbuf) {
		v3d->zbuf = false;
		glDisable(GL_DEPTH_TEST);
	}

	if ((v3d->flag2 & V3D_RENDER_SHADOW) == 0) {
		GPU_free_images_old();
	}
}

static void view3d_main_region_setup_view(
        Scene *scene, View3D *v3d, ARegion *ar, float viewmat[4][4], float winmat[4][4], const rcti *rect)
{
	RegionView3D *rv3d = ar->regiondata;

	ED_view3d_update_viewmat(scene, v3d, ar, viewmat, winmat, rect);

	/* set for opengl */
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(rv3d->winmat);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(rv3d->viewmat);
}

/**
 * Store values from #RegionView3D, set when drawing.
 * This is needed when we draw with to a viewport using a different matrix (offscreen drawing for example).
 *
 * Values set by #ED_view3d_update_viewmat should be handled here.
 */
struct RV3DMatrixStore {
	float winmat[4][4];
	float viewmat[4][4];
	float viewinv[4][4];
	float persmat[4][4];
	float persinv[4][4];
	float viewcamtexcofac[4];
	float pixsize;
};

struct RV3DMatrixStore *ED_view3d_mats_rv3d_backup(struct RegionView3D *rv3d)
{
	struct RV3DMatrixStore *rv3dmat = MEM_mallocN(sizeof(*rv3dmat), __func__);
	copy_m4_m4(rv3dmat->winmat, rv3d->winmat);
	copy_m4_m4(rv3dmat->viewmat, rv3d->viewmat);
	copy_m4_m4(rv3dmat->persmat, rv3d->persmat);
	copy_m4_m4(rv3dmat->persinv, rv3d->persinv);
	copy_m4_m4(rv3dmat->viewinv, rv3d->viewinv);
	copy_v4_v4(rv3dmat->viewcamtexcofac, rv3d->viewcamtexcofac);
	rv3dmat->pixsize = rv3d->pixsize;
	return (void *)rv3dmat;
}

void ED_view3d_mats_rv3d_restore(struct RegionView3D *rv3d, struct RV3DMatrixStore *rv3dmat)
{
	copy_m4_m4(rv3d->winmat, rv3dmat->winmat);
	copy_m4_m4(rv3d->viewmat, rv3dmat->viewmat);
	copy_m4_m4(rv3d->persmat, rv3dmat->persmat);
	copy_m4_m4(rv3d->persinv, rv3dmat->persinv);
	copy_m4_m4(rv3d->viewinv, rv3dmat->viewinv);
	copy_v4_v4(rv3d->viewcamtexcofac, rv3dmat->viewcamtexcofac);
	rv3d->pixsize = rv3dmat->pixsize;
}

void ED_view3d_draw_offscreen_init(Scene *scene, View3D *v3d)
{
	/* shadow buffers, before we setup matrices */
	if (draw_glsl_material(scene, NULL, v3d, v3d->drawtype))
		gpu_update_lamps_shadows_world(scene, v3d);
}

/*
 * Function to clear the view
 */
static void view3d_main_region_clear(Scene *scene, View3D *v3d, ARegion *ar)
{
	if (scene->world && (v3d->flag3 & V3D_SHOW_WORLD)) {
		RegionView3D *rv3d = ar->regiondata;
		GPUMaterial *gpumat = GPU_material_world(scene, scene->world);

		/* calculate full shader for background */
		GPU_material_bind(gpumat, 1, 1, 1.0, false, rv3d->viewmat, rv3d->viewinv, rv3d->viewcamtexcofac, (v3d->scenelock != 0));
		
		bool material_not_bound = !GPU_material_bound(gpumat);

		if (material_not_bound) {
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glLoadIdentity();
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			glLoadIdentity();
			glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
		}

		/* Draw world */
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_ALWAYS);
		glBegin(GL_TRIANGLE_STRIP);
		glVertex3f(-1.0, -1.0, 1.0);
		glVertex3f(1.0, -1.0, 1.0);
		glVertex3f(-1.0, 1.0, 1.0);
		glVertex3f(1.0, 1.0, 1.0);
		glEnd();

		if (material_not_bound) {
			glMatrixMode(GL_PROJECTION);
			glPopMatrix();
			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
		}

		GPU_material_unbind(gpumat);
		
		glDepthFunc(GL_LEQUAL);
		glDisable(GL_DEPTH_TEST);
	}
	else {
		if (UI_GetThemeValue(TH_SHOW_BACK_GRAD)) {
			glMatrixMode(GL_PROJECTION);
			glPushMatrix();
			glLoadIdentity();
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			glLoadIdentity();

			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_ALWAYS);
			glBegin(GL_QUADS);
			UI_ThemeColor(TH_LOW_GRAD);
			glVertex3f(-1.0, -1.0, 1.0);
			glVertex3f(1.0, -1.0, 1.0);
			UI_ThemeColor(TH_HIGH_GRAD);
			glVertex3f(1.0, 1.0, 1.0);
			glVertex3f(-1.0, 1.0, 1.0);
			glEnd();
			glDepthFunc(GL_LEQUAL);
			glDisable(GL_DEPTH_TEST);

			glMatrixMode(GL_PROJECTION);
			glPopMatrix();

			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
		}
		else {
			UI_ThemeClearColorAlpha(TH_HIGH_GRAD, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}
	}
}

/* ED_view3d_draw_offscreen_init should be called before this to initialize
 * stuff like shadow buffers
 */
void ED_view3d_draw_offscreen(
        Scene *scene, View3D *v3d, ARegion *ar, int winx, int winy,
        float viewmat[4][4], float winmat[4][4],
        bool do_bgpic, bool do_sky, bool is_persp, const char *viewname,
        GPUFX *fx, GPUFXSettings *fx_settings,
        GPUOffScreen *ofs)
{
	struct bThemeState theme_state;
	int bwinx, bwiny;
	rcti brect;
	bool do_compositing = false;
	RegionView3D *rv3d = ar->regiondata;

	glPushMatrix();

	/* set temporary new size */
	bwinx = ar->winx;
	bwiny = ar->winy;
	brect = ar->winrct;

	ar->winx = winx;
	ar->winy = winy;
	ar->winrct.xmin = 0;
	ar->winrct.ymin = 0;
	ar->winrct.xmax = winx;
	ar->winrct.ymax = winy;

	UI_Theme_Store(&theme_state);
	UI_SetTheme(SPACE_VIEW3D, RGN_TYPE_WINDOW);

	/* set flags */
	G.f |= G_RENDER_OGL;

	if ((v3d->flag2 & V3D_RENDER_SHADOW) == 0) {
		/* free images which can have changed on frame-change
		 * warning! can be slow so only free animated images - campbell */
		GPU_free_images_anim();
	}

	/* setup view matrices before fx or unbinding the offscreen buffers will cause issues */
	if ((viewname != NULL && viewname[0] != '\0') && (viewmat == NULL) && rv3d->persp == RV3D_CAMOB && v3d->camera)
		view3d_stereo3d_setup_offscreen(scene, v3d, ar, winmat, viewname);
	else
		view3d_main_region_setup_view(scene, v3d, ar, viewmat, winmat, NULL);

	/* framebuffer fx needed, we need to draw offscreen first */
	if (v3d->fx_settings.fx_flag && fx) {
		GPUSSAOSettings *ssao = NULL;

		if (v3d->drawtype < OB_SOLID) {
			ssao = v3d->fx_settings.ssao;
			v3d->fx_settings.ssao = NULL;
		}

		do_compositing = GPU_fx_compositor_initialize_passes(fx, &ar->winrct, NULL, fx_settings);

		if (ssao)
			v3d->fx_settings.ssao = ssao;
	}

	/* clear opengl buffers */
	if (do_sky) {
		view3d_main_region_clear(scene, v3d, ar);
	}
	else {
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	/* main drawing call */
	view3d_draw_objects(NULL, scene, v3d, ar, NULL, do_bgpic, true, do_compositing ? fx : NULL);

	/* post process */
	if (do_compositing) {
		if (!winmat)
			is_persp = rv3d->is_persp;
		GPU_fx_do_composite_pass(fx, winmat, is_persp, scene, ofs);
	}

	if ((v3d->flag2 & V3D_RENDER_SHADOW) == 0) {
		/* draw grease-pencil stuff */
		ED_region_pixelspace(ar);


		if (v3d->flag2 & V3D_SHOW_GPENCIL) {
			/* draw grease-pencil stuff - needed to get paint-buffer shown too (since it's 2D) */
			ED_gpencil_draw_view3d(NULL, scene, v3d, ar, false);
		}

		/* freeing the images again here could be done after the operator runs, leaving for now */
		GPU_free_images_anim();
	}

	/* restore size */
	ar->winx = bwinx;
	ar->winy = bwiny;
	ar->winrct = brect;

	glPopMatrix();

	UI_Theme_Restore(&theme_state);

	G.f &= ~G_RENDER_OGL;
}

/**
 * Set the correct matrices
 */
void ED_view3d_draw_setup_view(
        wmWindow *win, Scene *scene, ARegion *ar, View3D *v3d, float viewmat[4][4], float winmat[4][4], const rcti *rect)
{
	RegionView3D *rv3d = ar->regiondata;

	/* Setup the view matrix. */
	if (view3d_stereo3d_active(win, scene, v3d, rv3d)) {
		view3d_stereo3d_setup(scene, v3d, ar, rect);
	}
	else {
		view3d_main_region_setup_view(scene, v3d, ar, viewmat, winmat, rect);
	}
}

/**
 * Utility func for ED_view3d_draw_offscreen
 *
 * \param ofs: Optional off-screen buffer, can be NULL.
 * (avoids re-creating when doing multiple GL renders).
 */
ImBuf *ED_view3d_draw_offscreen_imbuf(
        Scene *scene, View3D *v3d, ARegion *ar, int sizex, int sizey,
        unsigned int flag, bool draw_background,
        int alpha_mode, int samples, bool full_samples, const char *viewname,
        /* output vars */
        GPUFX *fx, GPUOffScreen *ofs, char err_out[256])
{
	RegionView3D *rv3d = ar->regiondata;
	ImBuf *ibuf;
	const bool draw_sky = (alpha_mode == R_ADDSKY);

	/* view state */
	GPUFXSettings fx_settings = v3d->fx_settings;
	bool is_ortho = false;
	float winmat[4][4];

	if (ofs && ((GPU_offscreen_width(ofs) != sizex) || (GPU_offscreen_height(ofs) != sizey))) {
		/* sizes differ, can't reuse */
		ofs = NULL;
	}

	const bool own_ofs = (ofs == NULL);

	if (own_ofs) {
		/* bind */
		ofs = GPU_offscreen_create(sizex, sizey, full_samples ? 0 : samples, err_out);
		if (ofs == NULL) {
			return NULL;
		}
	}

	ED_view3d_draw_offscreen_init(scene, v3d);

	GPU_offscreen_bind(ofs, true);

	/* read in pixels & stamp */
	ibuf = IMB_allocImBuf(sizex, sizey, 32, flag);

	/* render 3d view */
	if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
		CameraParams params;
		Object *camera = BKE_camera_multiview_render(scene, v3d->camera, viewname);

		BKE_camera_params_init(&params);
		/* fallback for non camera objects */
		params.clipsta = v3d->near;
		params.clipend = v3d->far;
		BKE_camera_params_from_object(&params, camera);
		BKE_camera_multiview_params(&scene->r, &params, camera, viewname);
		BKE_camera_params_compute_viewplane(&params, sizex, sizey, scene->r.xasp, scene->r.yasp);
		BKE_camera_params_compute_matrix(&params);

		BKE_camera_to_gpu_dof(camera, &fx_settings);

		is_ortho = params.is_ortho;
		copy_m4_m4(winmat, params.winmat);
	}
	else {
		rctf viewplane;
		float clipsta, clipend;

		is_ortho = ED_view3d_viewplane_get(v3d, rv3d, sizex, sizey, &viewplane, &clipsta, &clipend, NULL);
		if (is_ortho) {
			orthographic_m4(winmat, viewplane.xmin, viewplane.xmax, viewplane.ymin, viewplane.ymax, -clipend, clipend);
		}
		else {
			perspective_m4(winmat, viewplane.xmin, viewplane.xmax, viewplane.ymin, viewplane.ymax, clipsta, clipend);
		}
	}

	if ((samples && full_samples) == 0) {
		/* Single-pass render, common case */
		ED_view3d_draw_offscreen(
		        scene, v3d, ar, sizex, sizey, NULL, winmat,
		        draw_background, draw_sky, !is_ortho, viewname,
		        fx, &fx_settings, ofs);

		if (ibuf->rect_float) {
			GPU_offscreen_read_pixels(ofs, GL_FLOAT, ibuf->rect_float);
		}
		else if (ibuf->rect) {
			GPU_offscreen_read_pixels(ofs, GL_UNSIGNED_BYTE, ibuf->rect);
		}
	}
	else {
		/* Multi-pass render, use accumulation buffer & jitter for 'full' oversampling.
		 * Use because OpenGL may use a lower quality MSAA, and only over-sample edges. */
		static float jit_ofs[32][2];
		float winmat_jitter[4][4];
		/* use imbuf as temp storage, before writing into it from accumulation buffer */
		unsigned char *rect_temp = ibuf->rect ? (void *)ibuf->rect : (void *)ibuf->rect_float;
		unsigned int *accum_buffer = MEM_mallocN(sizex * sizey * sizeof(int[4]), "accum1");
		unsigned int i;
		int j;

		BLI_jitter_init(jit_ofs, samples);

		/* first sample buffer, also initializes 'rv3d->persmat' */
		ED_view3d_draw_offscreen(
		        scene, v3d, ar, sizex, sizey, NULL, winmat,
		        draw_background, draw_sky, !is_ortho, viewname,
		        fx, &fx_settings, ofs);
		GPU_offscreen_read_pixels(ofs, GL_UNSIGNED_BYTE, rect_temp);

		i = sizex * sizey * 4;
		while (i--) {
			accum_buffer[i] = rect_temp[i];
		}

		/* skip the first sample */
		for (j = 1; j < samples; j++) {
			copy_m4_m4(winmat_jitter, winmat);
			window_translate_m4(
			        winmat_jitter, rv3d->persmat,
			        (jit_ofs[j][0] * 2.0f) / sizex,
			        (jit_ofs[j][1] * 2.0f) / sizey);

			ED_view3d_draw_offscreen(
			        scene, v3d, ar, sizex, sizey, NULL, winmat_jitter,
			        draw_background, draw_sky, !is_ortho, viewname,
			        fx, &fx_settings, ofs);
			GPU_offscreen_read_pixels(ofs, GL_UNSIGNED_BYTE, rect_temp);

			i = sizex * sizey * 4;
			while (i--) {
				accum_buffer[i] += rect_temp[i];
			}
		}

		if (ibuf->rect_float) {
			float *rect_float = ibuf->rect_float;
			i = sizex * sizey * 4;
			while (i--) {
				rect_float[i] = (float)(accum_buffer[i] / samples) * (1.0f / 255.0f);
			}
		}
		else {
			unsigned char *rect_ub = (unsigned char *)ibuf->rect;
			i = sizex * sizey * 4;
			while (i--) {
				rect_ub[i] = accum_buffer[i] / samples;
			}
		}

		MEM_freeN(accum_buffer);
	}

	/* unbind */
	GPU_offscreen_unbind(ofs, true);

	if (own_ofs) {
		GPU_offscreen_free(ofs);
	}

	if (ibuf->rect_float && ibuf->rect)
		IMB_rect_from_float(ibuf);

	return ibuf;
}

/**
 * Creates own fake 3d views (wrapping #ED_view3d_draw_offscreen_imbuf)
 *
 * \param ofs: Optional off-screen buffer can be NULL.
 * (avoids re-creating when doing multiple GL renders).
 *
 * \note used by the sequencer
 */
ImBuf *ED_view3d_draw_offscreen_imbuf_simple(
        Scene *scene, Object *camera, int width, int height,
        unsigned int flag, int drawtype, bool use_solid_tex, bool use_gpencil, bool draw_background,
        int alpha_mode, int samples, bool full_samples, const char *viewname,
        GPUFX *fx, GPUOffScreen *ofs, char err_out[256])
{
	View3D v3d = {NULL};
	ARegion ar = {NULL};
	RegionView3D rv3d = {{{0}}};

	/* connect data */
	v3d.regionbase.first = v3d.regionbase.last = &ar;
	ar.regiondata = &rv3d;
	ar.regiontype = RGN_TYPE_WINDOW;

	v3d.camera = camera;
	v3d.lay = scene->lay;
	v3d.drawtype = drawtype;
	v3d.flag2 = V3D_RENDER_OVERRIDE;
	
	if (use_gpencil)
		v3d.flag2 |= V3D_SHOW_GPENCIL;

	if (use_solid_tex)
		v3d.flag2 |= V3D_SOLID_TEX;
		
	if (draw_background)
		v3d.flag3 |= V3D_SHOW_WORLD;

	rv3d.persp = RV3D_CAMOB;

	copy_m4_m4(rv3d.viewinv, v3d.camera->obmat);
	normalize_m4(rv3d.viewinv);
	invert_m4_m4(rv3d.viewmat, rv3d.viewinv);

	{
		CameraParams params;
		Object *view_camera = BKE_camera_multiview_render(scene, v3d.camera, viewname);

		BKE_camera_params_init(&params);
		BKE_camera_params_from_object(&params, view_camera);
		BKE_camera_multiview_params(&scene->r, &params, view_camera, viewname);
		BKE_camera_params_compute_viewplane(&params, width, height, scene->r.xasp, scene->r.yasp);
		BKE_camera_params_compute_matrix(&params);

		copy_m4_m4(rv3d.winmat, params.winmat);
		v3d.near = params.clipsta;
		v3d.far = params.clipend;
		v3d.lens = params.lens;
	}

	mul_m4_m4m4(rv3d.persmat, rv3d.winmat, rv3d.viewmat);
	invert_m4_m4(rv3d.persinv, rv3d.viewinv);

	return ED_view3d_draw_offscreen_imbuf(
	        scene, &v3d, &ar, width, height, flag,
	        draw_background, alpha_mode, samples, full_samples, viewname,
	        fx, ofs, err_out);
}


/**
 * \note The info that this uses is updated in #ED_refresh_viewport_fps,
 * which currently gets called during #SCREEN_OT_animation_step.
 */
void ED_scene_draw_fps(Scene *scene, const rcti *rect)
{
	ScreenFrameRateInfo *fpsi = scene->fps_info;
	float fps;
	char printable[16];
	int i, tot;
	
	if (!fpsi || !fpsi->lredrawtime || !fpsi->redrawtime)
		return;
	
	printable[0] = '\0';
	
#if 0
	/* this is too simple, better do an average */
	fps = (float)(1.0 / (fpsi->lredrawtime - fpsi->redrawtime))
#else
	fpsi->redrawtimes_fps[fpsi->redrawtime_index] = (float)(1.0 / (fpsi->lredrawtime - fpsi->redrawtime));
	
	for (i = 0, tot = 0, fps = 0.0f; i < REDRAW_FRAME_AVERAGE; i++) {
		if (fpsi->redrawtimes_fps[i]) {
			fps += fpsi->redrawtimes_fps[i];
			tot++;
		}
	}
	if (tot) {
		fpsi->redrawtime_index = (fpsi->redrawtime_index + 1) % REDRAW_FRAME_AVERAGE;
		
		//fpsi->redrawtime_index++;
		//if (fpsi->redrawtime >= REDRAW_FRAME_AVERAGE)
		//	fpsi->redrawtime = 0;
		
		fps = fps / tot;
	}
#endif

	/* is this more than half a frame behind? */
	if (fps + 0.5f < (float)(FPS)) {
		UI_ThemeColor(TH_REDALERT);
		BLI_snprintf(printable, sizeof(printable), IFACE_("fps: %.2f"), fps);
	}
	else {
		UI_ThemeColor(TH_TEXT_HI);
		BLI_snprintf(printable, sizeof(printable), IFACE_("fps: %i"), (int)(fps + 0.5f));
	}

#ifdef WITH_INTERNATIONAL
	BLF_draw_default(rect->xmin + U.widget_unit,  rect->ymax - U.widget_unit, 0.0f, printable, sizeof(printable));
#else
	BLF_draw_default_ascii(rect->xmin + U.widget_unit,  rect->ymax - U.widget_unit, 0.0f, printable, sizeof(printable));
#endif
}

static bool view3d_main_region_do_render_draw(Scene *scene)
{
	RenderEngineType *type = RE_engines_find(scene->r.engine);

	return (type && type->view_update && type->view_draw);
}

bool ED_view3d_calc_render_border(Scene *scene, View3D *v3d, ARegion *ar, rcti *rect)
{
	RegionView3D *rv3d = ar->regiondata;
	rctf viewborder;
	bool use_border;

	/* test if there is a 3d view rendering */
	if (v3d->drawtype != OB_RENDER || !view3d_main_region_do_render_draw(scene))
		return false;

	/* test if there is a border render */
	if (rv3d->persp == RV3D_CAMOB)
		use_border = (scene->r.mode & R_BORDER) != 0;
	else
		use_border = (v3d->flag2 & V3D_RENDER_BORDER) != 0;
	
	if (!use_border)
		return false;

	/* compute border */
	if (rv3d->persp == RV3D_CAMOB) {
		ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &viewborder, false);

		rect->xmin = viewborder.xmin + scene->r.border.xmin * BLI_rctf_size_x(&viewborder);
		rect->ymin = viewborder.ymin + scene->r.border.ymin * BLI_rctf_size_y(&viewborder);
		rect->xmax = viewborder.xmin + scene->r.border.xmax * BLI_rctf_size_x(&viewborder);
		rect->ymax = viewborder.ymin + scene->r.border.ymax * BLI_rctf_size_y(&viewborder);
	}
	else {
		rect->xmin = v3d->render_border.xmin * ar->winx;
		rect->xmax = v3d->render_border.xmax * ar->winx;
		rect->ymin = v3d->render_border.ymin * ar->winy;
		rect->ymax = v3d->render_border.ymax * ar->winy;
	}

	BLI_rcti_translate(rect, ar->winrct.xmin, ar->winrct.ymin);
	BLI_rcti_isect(&ar->winrct, rect, rect);

	return true;
}

static bool view3d_main_region_draw_engine(const bContext *C, Scene *scene,
                                         ARegion *ar, View3D *v3d,
                                         bool clip_border, const rcti *border_rect)
{
	RegionView3D *rv3d = ar->regiondata;
	RenderEngineType *type;
	GLint scissor[4];

	/* create render engine */
	if (!rv3d->render_engine) {
		RenderEngine *engine;

		type = RE_engines_find(scene->r.engine);

		if (!(type->view_update && type->view_draw))
			return false;

		engine = RE_engine_create_ex(type, true);

		engine->tile_x = scene->r.tilex;
		engine->tile_y = scene->r.tiley;

		type->view_update(engine, C);

		rv3d->render_engine = engine;
	}

	/* setup view matrices */
	view3d_main_region_setup_view(scene, v3d, ar, NULL, NULL, NULL);

	/* background draw */
	ED_region_pixelspace(ar);

	if (clip_border) {
		/* for border draw, we only need to clear a subset of the 3d view */
		if (border_rect->xmax > border_rect->xmin && border_rect->ymax > border_rect->ymin) {
			glGetIntegerv(GL_SCISSOR_BOX, scissor);
			glScissor(border_rect->xmin, border_rect->ymin,
			          BLI_rcti_size_x(border_rect), BLI_rcti_size_y(border_rect));
		}
		else {
			return false;
		}
	}

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	if (v3d->flag & V3D_DISPBGPICS)
		view3d_draw_bgpic_test(scene, ar, v3d, false, true);
	else
		fdrawcheckerboard(0, 0, ar->winx, ar->winy);

	/* render result draw */
	type = rv3d->render_engine->type;
	type->view_draw(rv3d->render_engine, C);

	if (v3d->flag & V3D_DISPBGPICS)
		view3d_draw_bgpic_test(scene, ar, v3d, true, true);

	if (clip_border) {
		/* restore scissor as it was before */
		glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
	}

	return true;
}

static void view3d_main_region_draw_engine_info(View3D *v3d, RegionView3D *rv3d, ARegion *ar, bool render_border)
{
	float fill_color[4] = {0.0f, 0.0f, 0.0f, 0.25f};

	if (!rv3d->render_engine || !rv3d->render_engine->text[0])
		return;
	
	if (render_border) {
		/* draw darkened background color. no alpha because border render does
		 * partial redraw and will not redraw the region behind this info bar */
		float alpha = 1.0f - fill_color[3];
		Camera *camera = ED_view3d_camera_data_get(v3d, rv3d);

		if (camera) {
			if (camera->flag & CAM_SHOWPASSEPARTOUT) {
				alpha *= (1.0f - camera->passepartalpha);
			}
		}

		UI_GetThemeColor3fv(TH_HIGH_GRAD, fill_color);
		mul_v3_fl(fill_color, alpha);
		fill_color[3] = 1.0f;
	}

	ED_region_info_draw(ar, rv3d->render_engine->text, fill_color, true);
}

static bool view3d_stereo3d_active(wmWindow *win, Scene *scene, View3D *v3d, RegionView3D *rv3d)
{
	if ((scene->r.scemode & R_MULTIVIEW) == 0) {
		return false;
	}

	if ((v3d->camera == NULL) || (v3d->camera->type != OB_CAMERA) || rv3d->persp != RV3D_CAMOB) {
		return false;
	}

	switch (v3d->stereo3d_camera) {
		case STEREO_MONO_ID:
			return false;
			break;
		case STEREO_3D_ID:
			/* win will be NULL when calling this from the selection or draw loop. */
			if ((win == NULL) || (WM_stereo3d_enabled(win, true) == false)) {
				return false;
			}
			if (((scene->r.views_format & SCE_VIEWS_FORMAT_MULTIVIEW) != 0) &&
			    !BKE_scene_multiview_is_stereo3d(&scene->r))
			{
				return false;
			}
			break;
		/* We always need the stereo calculation for left and right cameras. */
		case STEREO_LEFT_ID:
		case STEREO_RIGHT_ID:
		default:
			break;
	}
	return true;
}

/* setup the view and win matrices for the multiview cameras
 *
 * unlike view3d_stereo3d_setup_offscreen, when view3d_stereo3d_setup is called
 * we have no winmatrix (i.e., projection matrix) defined at that time.
 * Since the camera and the camera shift are needed for the winmat calculation
 * we do a small hack to replace it temporarily so we don't need to change the
 * view3d)main_region_setup_view() code to account for that.
 */
static void view3d_stereo3d_setup(Scene *scene, View3D *v3d, ARegion *ar, const rcti *rect)
{
	bool is_left;
	const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
	const char *viewname;

	/* show only left or right camera */
	if (v3d->stereo3d_camera != STEREO_3D_ID)
		v3d->multiview_eye = v3d->stereo3d_camera;

	is_left = v3d->multiview_eye == STEREO_LEFT_ID;
	viewname = names[is_left ? STEREO_LEFT_ID : STEREO_RIGHT_ID];

	/* update the viewport matrices with the new camera */
	if (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D) {
		Camera *data;
		float viewmat[4][4];
		float shiftx;

		data = (Camera *)v3d->camera->data;
		shiftx = data->shiftx;

		BLI_lock_thread(LOCK_VIEW3D);
		data->shiftx = BKE_camera_multiview_shift_x(&scene->r, v3d->camera, viewname);

		BKE_camera_multiview_view_matrix(&scene->r, v3d->camera, is_left, viewmat);
		view3d_main_region_setup_view(scene, v3d, ar, viewmat, NULL, rect);

		data->shiftx = shiftx;
		BLI_unlock_thread(LOCK_VIEW3D);
	}
	else { /* SCE_VIEWS_FORMAT_MULTIVIEW */
		float viewmat[4][4];
		Object *view_ob = v3d->camera;
		Object *camera = BKE_camera_multiview_render(scene, v3d->camera, viewname);

		BLI_lock_thread(LOCK_VIEW3D);
		v3d->camera = camera;

		BKE_camera_multiview_view_matrix(&scene->r, camera, false, viewmat);
		view3d_main_region_setup_view(scene, v3d, ar, viewmat, NULL, rect);

		v3d->camera = view_ob;
		BLI_unlock_thread(LOCK_VIEW3D);
	}
}

static void view3d_stereo3d_setup_offscreen(Scene *scene, View3D *v3d, ARegion *ar,
                                            float winmat[4][4], const char *viewname)
{
	/* update the viewport matrices with the new camera */
	if (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D) {
		float viewmat[4][4];
		const bool is_left = STREQ(viewname, STEREO_LEFT_NAME);

		BKE_camera_multiview_view_matrix(&scene->r, v3d->camera, is_left, viewmat);
		view3d_main_region_setup_view(scene, v3d, ar, viewmat, winmat, NULL);
	}
	else { /* SCE_VIEWS_FORMAT_MULTIVIEW */
		float viewmat[4][4];
		Object *camera = BKE_camera_multiview_render(scene, v3d->camera, viewname);

		BKE_camera_multiview_view_matrix(&scene->r, camera, false, viewmat);
		view3d_main_region_setup_view(scene, v3d, ar, viewmat, winmat, NULL);
	}
}

#ifdef WITH_GAMEENGINE
static void update_lods(Scene *scene, float camera_pos[3])
{
	Scene *sce_iter;
	Base *base;
	Object *ob;

	for (SETLOOPER(scene, sce_iter, base)) {
		ob = base->object;
		BKE_object_lod_update(ob, camera_pos);
	}
}
#endif

static void view3d_main_region_draw_objects(const bContext *C, Scene *scene, View3D *v3d,
                                          ARegion *ar, const char **grid_unit)
{
	wmWindow *win = CTX_wm_window(C);
	RegionView3D *rv3d = ar->regiondata;
	unsigned int lay_used = v3d->lay_used;
	
	/* post processing */
	bool do_compositing = false;
	
	/* shadow buffers, before we setup matrices */
	if (draw_glsl_material(scene, NULL, v3d, v3d->drawtype))
		gpu_update_lamps_shadows_world(scene, v3d);

	/* reset default OpenGL lights if needed (i.e. after preferences have been altered) */
	if (rv3d->rflag & RV3D_GPULIGHT_UPDATE) {
		rv3d->rflag &= ~RV3D_GPULIGHT_UPDATE;
		GPU_default_lights();
	}

	/* Setup the view matrix. */
	ED_view3d_draw_setup_view(CTX_wm_window(C), scene, ar, v3d, NULL, NULL, NULL);

	rv3d->rflag &= ~RV3D_IS_GAME_ENGINE;
#ifdef WITH_GAMEENGINE
	if (STREQ(scene->r.engine, RE_engine_id_BLENDER_GAME)) {
		rv3d->rflag |= RV3D_IS_GAME_ENGINE;

		/* Make sure LoDs are up to date */
		update_lods(scene, rv3d->viewinv[3]);
	}
#endif

	/* framebuffer fx needed, we need to draw offscreen first */
	if (v3d->fx_settings.fx_flag && v3d->drawtype >= OB_SOLID) {
		GPUFXSettings fx_settings;
		BKE_screen_gpu_fx_validate(&v3d->fx_settings);
		fx_settings = v3d->fx_settings;
		if (!rv3d->compositor)
			rv3d->compositor = GPU_fx_compositor_create();
		
		if (rv3d->persp == RV3D_CAMOB && v3d->camera)
			BKE_camera_to_gpu_dof(v3d->camera, &fx_settings);
		else {
			fx_settings.dof = NULL;
		}

		do_compositing = GPU_fx_compositor_initialize_passes(rv3d->compositor, &ar->winrct, &ar->drawrct, &fx_settings);
	}
	
	/* clear the background */
	view3d_main_region_clear(scene, v3d, ar);

	/* enables anti-aliasing for 3D view drawing */
	if (win->multisamples != USER_MULTISAMPLE_NONE) {
		glEnable(GL_MULTISAMPLE);
	}

	/* main drawing call */
	view3d_draw_objects(C, scene, v3d, ar, grid_unit, true, false, do_compositing ? rv3d->compositor : NULL);

	/* post process */
	if (do_compositing) {
		GPU_fx_do_composite_pass(rv3d->compositor, rv3d->winmat, rv3d->is_persp, scene, NULL);
	}

	/* Disable back anti-aliasing */
	if (win->multisamples != USER_MULTISAMPLE_NONE) {
		glDisable(GL_MULTISAMPLE);
	}

	if (v3d->lay_used != lay_used) { /* happens when loading old files or loading with UI load */
		/* find header and force tag redraw */
		ScrArea *sa = CTX_wm_area(C);
		ARegion *ar_header = BKE_area_find_region_type(sa, RGN_TYPE_HEADER);
		ED_region_tag_redraw(ar_header); /* can be NULL */
	}

	if ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {
		BDR_drawSketch(C);
	}

#ifdef WITH_INPUT_NDOF
	if ((U.ndof_flag & NDOF_SHOW_GUIDE) && ((rv3d->viewlock & RV3D_LOCKED) == 0) && (rv3d->persp != RV3D_CAMOB))
		/* TODO: draw something else (but not this) during fly mode */
		draw_rotation_guide(rv3d);
#endif
}

static bool is_cursor_visible(Scene *scene)
{
	Object *ob = OBACT;

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

static void view3d_main_region_draw_info(const bContext *C, Scene *scene,
                                       ARegion *ar, View3D *v3d,
                                       const char *grid_unit, bool render_border)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	RegionView3D *rv3d = ar->regiondata;
	rcti rect;
	
	/* local coordinate visible rect inside region, to accomodate overlapping ui */
	ED_region_visible_rect(ar, &rect);

	if (rv3d->persp == RV3D_CAMOB) {
		drawviewborder(scene, ar, v3d);
	}
	else if (v3d->flag2 & V3D_RENDER_BORDER) {
		glLineWidth(1.0f);
		setlinestyle(3);
		cpack(0x4040FF);

		sdrawbox(v3d->render_border.xmin * ar->winx, v3d->render_border.ymin * ar->winy,
		         v3d->render_border.xmax * ar->winx, v3d->render_border.ymax * ar->winy);

		setlinestyle(0);
	}

	if (v3d->flag2 & V3D_SHOW_GPENCIL) {
		/* draw grease-pencil stuff - needed to get paint-buffer shown too (since it's 2D) */
		ED_gpencil_draw_view3d(wm, scene, v3d, ar, false);
	}

	if ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {
		Object *ob;

		/* 3d cursor */
		if (is_cursor_visible(scene)) {
			drawcursor(scene, ar, v3d);
		}

		if (U.uiflag & USER_SHOW_ROTVIEWICON)
			draw_view_axis(rv3d, &rect);
		else
			draw_view_icon(rv3d, &rect);

		ob = OBACT;
		if (U.uiflag & USER_DRAWVIEWINFO)
			draw_selected_name(scene, ob, &rect);
	}

	if (rv3d->render_engine) {
		view3d_main_region_draw_engine_info(v3d, rv3d, ar, render_border);
		return;
	}

	if ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0) {
		if ((U.uiflag & USER_SHOW_FPS) && ED_screen_animation_no_scrub(wm)) {
			ED_scene_draw_fps(scene, &rect);
		}
		else if (U.uiflag & USER_SHOW_VIEWPORTNAME) {
			draw_viewport_name(ar, v3d, &rect);
		}

		if (grid_unit) { /* draw below the viewport name */
			char numstr[32] = "";

			UI_ThemeColor(TH_TEXT_HI);
			if (v3d->grid != 1.0f) {
				BLI_snprintf(numstr, sizeof(numstr), "%s x %.4g", grid_unit, v3d->grid);
			}

			BLF_draw_default_ascii(rect.xmin + U.widget_unit,
			                       rect.ymax - (USER_SHOW_VIEWPORTNAME ? 2 * U.widget_unit : U.widget_unit), 0.0f,
			                       numstr[0] ? numstr : grid_unit, sizeof(numstr));
		}
	}
}

void view3d_main_region_draw(const bContext *C, ARegion *ar)
{
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	const char *grid_unit = NULL;
	rcti border_rect;
	bool render_border, clip_border;

	/* if we only redraw render border area, skip opengl draw and also
	 * don't do scissor because it's already set */
	render_border = ED_view3d_calc_render_border(scene, v3d, ar, &border_rect);
	clip_border = (render_border && !BLI_rcti_compare(&ar->drawrct, &border_rect));

	/* draw viewport using opengl */
	if (v3d->drawtype != OB_RENDER || !view3d_main_region_do_render_draw(scene) || clip_border) {
		view3d_main_region_draw_objects(C, scene, v3d, ar, &grid_unit);
		
#ifdef DEBUG_DRAW
		bl_debug_draw();
#endif
		if (G.debug & G_DEBUG_SIMDATA)
			draw_sim_debug_data(scene, v3d, ar);
		
		ED_region_pixelspace(ar);
	}

	/* draw viewport using external renderer */
	if (v3d->drawtype == OB_RENDER)
		view3d_main_region_draw_engine(C, scene, ar, v3d, clip_border, &border_rect);
	
	view3d_main_region_draw_info(C, scene, ar, v3d, grid_unit, render_border);

	v3d->flag |= V3D_INVALID_BACKBUF;

	BLI_assert(BLI_listbase_is_empty(&v3d->afterdraw_transp));
	BLI_assert(BLI_listbase_is_empty(&v3d->afterdraw_xray));
	BLI_assert(BLI_listbase_is_empty(&v3d->afterdraw_xraytransp));
}

#ifdef DEBUG_DRAW
/* debug drawing */
#define _DEBUG_DRAW_QUAD_TOT 1024
#define _DEBUG_DRAW_EDGE_TOT 1024
static float _bl_debug_draw_quads[_DEBUG_DRAW_QUAD_TOT][4][3];
static int   _bl_debug_draw_quads_tot = 0;
static float _bl_debug_draw_edges[_DEBUG_DRAW_QUAD_TOT][2][3];
static int   _bl_debug_draw_edges_tot = 0;
static unsigned int _bl_debug_draw_quads_color[_DEBUG_DRAW_QUAD_TOT];
static unsigned int _bl_debug_draw_edges_color[_DEBUG_DRAW_EDGE_TOT];
static unsigned int _bl_debug_draw_color;

void bl_debug_draw_quad_clear(void)
{
	_bl_debug_draw_quads_tot = 0;
	_bl_debug_draw_edges_tot = 0;
	_bl_debug_draw_color = 0x00FF0000;
}
void bl_debug_color_set(const unsigned int color)
{
	_bl_debug_draw_color = color;
}
void bl_debug_draw_quad_add(const float v0[3], const float v1[3], const float v2[3], const float v3[3])
{
	if (_bl_debug_draw_quads_tot >= _DEBUG_DRAW_QUAD_TOT) {
		printf("%s: max quad count hit %d!", __func__, _bl_debug_draw_quads_tot);
	}
	else {
		float *pt = &_bl_debug_draw_quads[_bl_debug_draw_quads_tot][0][0];
		copy_v3_v3(pt, v0); pt += 3;
		copy_v3_v3(pt, v1); pt += 3;
		copy_v3_v3(pt, v2); pt += 3;
		copy_v3_v3(pt, v3); pt += 3;
		_bl_debug_draw_quads_color[_bl_debug_draw_quads_tot] = _bl_debug_draw_color;
		_bl_debug_draw_quads_tot++;
	}
}
void bl_debug_draw_edge_add(const float v0[3], const float v1[3])
{
	if (_bl_debug_draw_quads_tot >= _DEBUG_DRAW_EDGE_TOT) {
		printf("%s: max edge count hit %d!", __func__, _bl_debug_draw_edges_tot);
	}
	else {
		float *pt = &_bl_debug_draw_edges[_bl_debug_draw_edges_tot][0][0];
		copy_v3_v3(pt, v0); pt += 3;
		copy_v3_v3(pt, v1); pt += 3;
		_bl_debug_draw_edges_color[_bl_debug_draw_edges_tot] = _bl_debug_draw_color;
		_bl_debug_draw_edges_tot++;
	}
}
static void bl_debug_draw(void)
{
	unsigned int color;
	if (_bl_debug_draw_quads_tot) {
		int i;
		color = _bl_debug_draw_quads_color[0];
		cpack(color);
		for (i = 0; i < _bl_debug_draw_quads_tot; i ++) {
			if (_bl_debug_draw_quads_color[i] != color) {
				color = _bl_debug_draw_quads_color[i];
				cpack(color);
			}
			glBegin(GL_LINE_LOOP);
			glVertex3fv(_bl_debug_draw_quads[i][0]);
			glVertex3fv(_bl_debug_draw_quads[i][1]);
			glVertex3fv(_bl_debug_draw_quads[i][2]);
			glVertex3fv(_bl_debug_draw_quads[i][3]);
			glEnd();
		}
	}
	if (_bl_debug_draw_edges_tot) {
		int i;
		color = _bl_debug_draw_edges_color[0];
		cpack(color);
		glBegin(GL_LINES);
		for (i = 0; i < _bl_debug_draw_edges_tot; i ++) {
			if (_bl_debug_draw_edges_color[i] != color) {
				color = _bl_debug_draw_edges_color[i];
				cpack(color);
			}
			glVertex3fv(_bl_debug_draw_edges[i][0]);
			glVertex3fv(_bl_debug_draw_edges[i][1]);
		}
		glEnd();
		color = _bl_debug_draw_edges_color[0];
		cpack(color);
		glPointSize(4.0);
		glBegin(GL_POINTS);
		for (i = 0; i < _bl_debug_draw_edges_tot; i ++) {
			if (_bl_debug_draw_edges_color[i] != color) {
				color = _bl_debug_draw_edges_color[i];
				cpack(color);
			}
			glVertex3fv(_bl_debug_draw_edges[i][0]);
			glVertex3fv(_bl_debug_draw_edges[i][1]);
		}
		glEnd();
	}
}
#endif

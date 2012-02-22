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
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BKE_anim.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_unit.h"
#include "BKE_movieclip.h"

#include "RE_engine.h"
#include "RE_pipeline.h"	// make_stars

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "BLF_api.h"

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
#include "GPU_material.h"
#include "GPU_extensions.h"

#include "view3d_intern.h"	// own include



static void star_stuff_init_func(void)
{
	cpack(-1);
	glPointSize(1.0);
	glBegin(GL_POINTS);
}
static void star_stuff_vertex_func(float* i)
{
	glVertex3fv(i);
}
static void star_stuff_term_func(void)
{
	glEnd();
}

void circf(float x, float y, float rad)
{
	GLUquadricObj *qobj = gluNewQuadric(); 
	
	gluQuadricDrawStyle(qobj, GLU_FILL); 
	
	glPushMatrix(); 
	
	glTranslatef(x,  y, 0.); 
	
	gluDisk( qobj, 0.0,  rad, 32, 1); 
	
	glPopMatrix(); 
	
	gluDeleteQuadric(qobj);
}

void circ(float x, float y, float rad)
{
	GLUquadricObj *qobj = gluNewQuadric(); 
	
	gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); 
	
	glPushMatrix(); 
	
	glTranslatef(x,  y, 0.); 
	
	gluDisk( qobj, 0.0,  rad, 32, 1); 
	
	glPopMatrix(); 
	
	gluDeleteQuadric(qobj);
}


/* ********* custom clipping *********** */

static void view3d_draw_clipping(RegionView3D *rv3d)
{
	BoundBox *bb= rv3d->clipbb;

	if (bb) {
		static unsigned int clipping_index[6][4]= {{0, 1, 2, 3},
		                                           {0, 4, 5, 1},
		                                           {4, 7, 6, 5},
		                                           {7, 3, 2, 6},
		                                           {1, 5, 6, 2},
		                                           {7, 4, 0, 3}};

		UI_ThemeColorShade(TH_BACK, -8);

		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, bb->vec);
		glDrawElements(GL_QUADS, sizeof(clipping_index)/sizeof(unsigned int), GL_UNSIGNED_INT, clipping_index);
		glDisableClientState(GL_VERTEX_ARRAY);

	}
}

void view3d_set_clipping(RegionView3D *rv3d)
{
	double plane[4];
	int a, tot=4;
	
	if (rv3d->viewlock) tot= 6;
	
	for (a=0; a<tot; a++) {
		QUATCOPY(plane, rv3d->clip[a]);
		glClipPlane(GL_CLIP_PLANE0+a, plane);
		glEnable(GL_CLIP_PLANE0+a);
	}
}

void view3d_clr_clipping(void)
{
	int a;
	
	for (a=0; a<6; a++) {
		glDisable(GL_CLIP_PLANE0+a);
	}
}

static int test_clipping(const float vec[3], float clip[][4])
{
	float view[3];
	copy_v3_v3(view, vec);

	if (0.0f < clip[0][3] + dot_v3v3(view, clip[0]))
		if (0.0f < clip[1][3] + dot_v3v3(view, clip[1]))
			if (0.0f < clip[2][3] + dot_v3v3(view, clip[2]))
				if (0.0f < clip[3][3] + dot_v3v3(view, clip[3]))
					return 0;

	return 1;
}

/* for 'local' ED_view3d_local_clipping must run first
 * then all comparisons can be done in localspace */
int ED_view3d_test_clipping(RegionView3D *rv3d, const float vec[3], const int local)
{
	return test_clipping(vec, local ? rv3d->clip_local : rv3d->clip);
}

/* ********* end custom clipping *********** */


static void drawgrid_draw(ARegion *ar, float wx, float wy, float x, float y, float dx)
{	
	float verts[2][2];

	x+= (wx); 
	y+= (wy);

	/* set fixed 'Y' */
	verts[0][1]= 0.0f;
	verts[1][1]= (float)ar->winy;

	/* iter over 'X' */
	verts[0][0] = verts[1][0] = x-dx*floorf(x/dx);
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, verts);

	while (verts[0][0] < ar->winx) {
		glDrawArrays(GL_LINES, 0, 2);
		verts[0][0] = verts[1][0] = verts[0][0] + dx;
	}

	/* set fixed 'X' */
	verts[0][0]= 0.0f;
	verts[1][0]= (float)ar->winx;

	/* iter over 'Y' */
	verts[0][1]= verts[1][1]= y-dx*floorf(y/dx);
	while (verts[0][1] < ar->winy) {
		glDrawArrays(GL_LINES, 0, 2);
		verts[0][1] = verts[1][1] = verts[0][1] + dx;
	}

	glDisableClientState(GL_VERTEX_ARRAY);
}

#define GRID_MIN_PX 6.0f

static void drawgrid(UnitSettings *unit, ARegion *ar, View3D *v3d, const char **grid_unit)
{
	/* extern short bgpicmode; */
	RegionView3D *rv3d= ar->regiondata;
	float wx, wy, x, y, fw, fx, fy, dx;
	float vec4[4];
	unsigned char col[3], col2[3];

	vec4[0]=vec4[1]=vec4[2]=0.0; 
	vec4[3]= 1.0;
	mul_m4_v4(rv3d->persmat, vec4);
	fx= vec4[0]; 
	fy= vec4[1]; 
	fw= vec4[3];

	wx= (ar->winx/2.0);	/* because of rounding errors, grid at wrong location */
	wy= (ar->winy/2.0);

	x= (wx)*fx/fw;
	y= (wy)*fy/fw;

	vec4[0]=vec4[1]= v3d->grid;

	vec4[2]= 0.0;
	vec4[3]= 1.0;
	mul_m4_v4(rv3d->persmat, vec4);
	fx= vec4[0]; 
	fy= vec4[1]; 
	fw= vec4[3];

	dx= fabs(x-(wx)*fx/fw);
	if (dx==0) dx= fabs(y-(wy)*fy/fw);
	
	glDepthMask(0);		// disable write in zbuffer

	/* check zoom out */
	UI_ThemeColor(TH_GRID);
	
	if (unit->system) {
		/* Use GRID_MIN_PX*2 for units because very very small grid
		 * items are less useful when dealing with units */
		void *usys;
		int len, i;
		float dx_scalar;
		float blend_fac;

		bUnit_GetSystem(&usys, &len, unit->system, B_UNIT_LENGTH);

		if (usys) {
			i= len;
			while (i--) {
				float scalar= bUnit_GetScaler(usys, i);

				dx_scalar = dx * scalar / unit->scale_length;
				if (dx_scalar < (GRID_MIN_PX*2))
					continue;

				/* Store the smallest drawn grid size units name so users know how big each grid cell is */
				if (*grid_unit==NULL) {
					*grid_unit= bUnit_GetNameDisplay(usys, i);
					rv3d->gridview= (scalar * v3d->grid) / unit->scale_length;
				}
				blend_fac= 1-((GRID_MIN_PX*2)/dx_scalar);

				/* tweak to have the fade a bit nicer */
				blend_fac= (blend_fac * blend_fac) * 2.0f;
				CLAMP(blend_fac, 0.3f, 1.0f);


				UI_ThemeColorBlend(TH_BACK, TH_GRID, blend_fac);

				drawgrid_draw(ar, wx, wy, x, y, dx_scalar);
			}
		}
	}
	else {
		short sublines = v3d->gridsubdiv;

		if (dx<GRID_MIN_PX) {
			rv3d->gridview*= sublines;
			dx*= sublines;
			
			if (dx<GRID_MIN_PX) {
				rv3d->gridview*= sublines;
				dx*= sublines;

				if (dx<GRID_MIN_PX) {
					rv3d->gridview*= sublines;
					dx*=sublines;
					if (dx<GRID_MIN_PX);
					else {
						UI_ThemeColor(TH_GRID);
						drawgrid_draw(ar, wx, wy, x, y, dx);
					}
				}
				else {	// start blending out
					UI_ThemeColorBlend(TH_BACK, TH_GRID, dx/(GRID_MIN_PX*6));
					drawgrid_draw(ar, wx, wy, x, y, dx);

					UI_ThemeColor(TH_GRID);
					drawgrid_draw(ar, wx, wy, x, y, sublines*dx);
				}
			}
			else {	// start blending out (GRID_MIN_PX < dx < (GRID_MIN_PX*10))
				UI_ThemeColorBlend(TH_BACK, TH_GRID, dx/(GRID_MIN_PX*6));
				drawgrid_draw(ar, wx, wy, x, y, dx);

				UI_ThemeColor(TH_GRID);
				drawgrid_draw(ar, wx, wy, x, y, sublines*dx);
			}
		}
		else {
			if (dx>(GRID_MIN_PX*10)) {		// start blending in
				rv3d->gridview/= sublines;
				dx/= sublines;
				if (dx>(GRID_MIN_PX*10)) {		// start blending in
					rv3d->gridview/= sublines;
					dx/= sublines;
					if (dx>(GRID_MIN_PX*10)) {
						UI_ThemeColor(TH_GRID);
						drawgrid_draw(ar, wx, wy, x, y, dx);
					}
					else {
						UI_ThemeColorBlend(TH_BACK, TH_GRID, dx/(GRID_MIN_PX*6));
						drawgrid_draw(ar, wx, wy, x, y, dx);
						UI_ThemeColor(TH_GRID);
						drawgrid_draw(ar, wx, wy, x, y, dx*sublines);
					}
				}
				else {
					UI_ThemeColorBlend(TH_BACK, TH_GRID, dx/(GRID_MIN_PX*6));
					drawgrid_draw(ar, wx, wy, x, y, dx);
					UI_ThemeColor(TH_GRID);
					drawgrid_draw(ar, wx, wy, x, y, dx*sublines);
				}
			}
			else {
				UI_ThemeColorBlend(TH_BACK, TH_GRID, dx/(GRID_MIN_PX*6));
				drawgrid_draw(ar, wx, wy, x, y, dx);
				UI_ThemeColor(TH_GRID);
				drawgrid_draw(ar, wx, wy, x, y, dx*sublines);
			}
		}
	}


	x+= (wx); 
	y+= (wy);
	UI_GetThemeColor3ubv(TH_GRID, col);

	setlinestyle(0);
	
	/* center cross */
	/* horizontal line */
	if ( ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT))
		UI_make_axis_color(col, col2, 'Y');
	else UI_make_axis_color(col, col2, 'X');
	glColor3ubv(col2);
	
	fdrawline(0.0,  y,  (float)ar->winx,  y); 
	
	/* vertical line */
	if ( ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM))
		UI_make_axis_color(col, col2, 'Y');
	else UI_make_axis_color(col, col2, 'Z');
	glColor3ubv(col2);

	fdrawline(x, 0.0, x, (float)ar->winy); 

	glDepthMask(1);		// enable write in zbuffer
}
#undef GRID_MIN_PX

static void drawfloor(Scene *scene, View3D *v3d, const char **grid_unit)
{
	float grid, grid_scale;
	unsigned char col_grid[3];
	const int gridlines= v3d->gridlines/2;

	if (v3d->gridlines<3) return;
	
	grid_scale= v3d->grid;
	/* use 'grid_scale' instead of 'v3d->grid' from now on */

	/* apply units */
	if (scene->unit.system) {
		void *usys;
		int len;

		bUnit_GetSystem(&usys, &len, scene->unit.system, B_UNIT_LENGTH);

		if (usys) {
			int i= bUnit_GetBaseUnit(usys);
			*grid_unit= bUnit_GetNameDisplay(usys, i);
			grid_scale = (grid_scale * (float)bUnit_GetScaler(usys, i)) / scene->unit.scale_length;
		}
	}

	grid= gridlines * grid_scale;

	if (v3d->zbuf && scene->obedit) glDepthMask(0);	// for zbuffer-select

	UI_GetThemeColor3ubv(TH_GRID, col_grid);

	/* draw the Y axis and/or grid lines */
	if (v3d->gridflag & V3D_SHOW_FLOOR) {
		float vert[4][3]= {{0.0f}};
		unsigned char col_bg[3];
		unsigned char col_grid_emphasise[3], col_grid_light[3];
		int a;
		int prev_emphasise= -1;

		UI_GetThemeColor3ubv(TH_BACK, col_bg);

		/* emphasise division lines lighter instead of darker, if background is darker than grid */
		UI_GetColorPtrShade3ubv(col_grid, col_grid_light, 10);
		UI_GetColorPtrShade3ubv(col_grid, col_grid_emphasise,
		                        (((col_grid[0]+col_grid[1]+col_grid[2])+30) >
		                          (col_bg[0]+col_bg[1]+col_bg[2])) ? 20 : -10);

		/* set fixed axis */
		vert[0][0]= vert[2][1]= grid;
		vert[1][0]= vert[3][1]= -grid;

		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, 0, vert);

		for (a= -gridlines;a<=gridlines;a++) {
			const float line= a * grid_scale;
			const int is_emphasise= (a % 10) == 0;

			if (is_emphasise != prev_emphasise) {
				glColor3ubv(is_emphasise ? col_grid_emphasise : col_grid_light);
				prev_emphasise= is_emphasise;
			}

			/* set variable axis */
			vert[0][1]= vert[1][1]=
			vert[2][0]= vert[3][0]= line;

			glDrawArrays(GL_LINES, 0, 4);
		}

		glDisableClientState(GL_VERTEX_ARRAY);

		GPU_print_error("sdsd");
	}
	
	/* draw the Z axis line */	
	/* check for the 'show Z axis' preference */
	if (v3d->gridflag & (V3D_SHOW_X | V3D_SHOW_Y | V3D_SHOW_Z)) {
		int axis;
		for (axis= 0; axis < 3; axis++)
		if (v3d->gridflag & (V3D_SHOW_X << axis)) {
			float vert[3];
			unsigned char tcol[3];

			UI_make_axis_color(col_grid, tcol, 'X' + axis);
			glColor3ubv(tcol);

			glBegin(GL_LINE_STRIP);
			zero_v3(vert);
			vert[axis]= grid;
			glVertex3fv(vert );
			vert[axis]= -grid;
			glVertex3fv(vert);
			glEnd();
		}
	}




	if (v3d->zbuf && scene->obedit) glDepthMask(1);
	
}

static void drawcursor(Scene *scene, ARegion *ar, View3D *v3d)
{
	int mx, my, co[2];
	int flag;
	
	/* we dont want the clipping for cursor */
	flag= v3d->flag;
	v3d->flag= 0;
	project_int(ar, give_cursor(scene, v3d), co);
	v3d->flag= flag;
	
	mx = co[0];
	my = co[1];
	
	if (mx!=IS_CLIPPED) {
		setlinestyle(0); 
		cpack(0xFF);
		circ((float)mx, (float)my, 10.0);
		setlinestyle(4); 
		cpack(0xFFFFFF);
		circ((float)mx, (float)my, 10.0);
		setlinestyle(0);
		cpack(0x0);
		
		sdrawline(mx-20, my, mx-5, my);
		sdrawline(mx+5, my, mx+20, my);
		sdrawline(mx, my-20, mx, my-5);
		sdrawline(mx, my+5, mx, my+20);
	}
}

/* Draw a live substitute of the view icon, which is always shown
 * colors copied from transform_manipulator.c, we should keep these matching. */
static void draw_view_axis(RegionView3D *rv3d)
{
	const float k = U.rvisize;   /* axis size */
	const float toll = 0.5;      /* used to see when view is quasi-orthogonal */
	const float start = k + 1.0f;/* axis center in screen coordinates, x=y */
	float ydisp = 0.0;          /* vertical displacement to allow obj info text */
	int bright = 25*(float)U.rvibright + 5; /* axis alpha (rvibright has range 0-10) */

	float vec[3];
	float dx, dy;
	
	/* thickness of lines is proportional to k */
	glLineWidth(2);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* X */
	vec[0] = 1;
	vec[1] = vec[2] = 0;
	mul_qt_v3(rv3d->viewquat, vec);
	dx = vec[0] * k;
	dy = vec[1] * k;

	glColor4ub(220, 0, 0, bright);
	glBegin(GL_LINES);
	glVertex2f(start, start + ydisp);
	glVertex2f(start + dx, start + dy + ydisp);
	glEnd();

	if (fabsf(dx) > toll || fabsf(dy) > toll) {
		BLF_draw_default_ascii(start + dx + 2, start + dy + ydisp + 2, 0.0f, "x", 1);
	}
	
	/* BLF_draw_default disables blending */
	glEnable(GL_BLEND);

	/* Y */
	vec[1] = 1;
	vec[0] = vec[2] = 0;
	mul_qt_v3(rv3d->viewquat, vec);
	dx = vec[0] * k;
	dy = vec[1] * k;

	glColor4ub(0, 220, 0, bright);
	glBegin(GL_LINES);
	glVertex2f(start, start + ydisp);
	glVertex2f(start + dx, start + dy + ydisp);
	glEnd();

	if (fabsf(dx) > toll || fabsf(dy) > toll) {
		BLF_draw_default_ascii(start + dx + 2, start + dy + ydisp + 2, 0.0f, "y", 1);
	}

	glEnable(GL_BLEND);
	
	/* Z */
	vec[2] = 1;
	vec[1] = vec[0] = 0;
	mul_qt_v3(rv3d->viewquat, vec);
	dx = vec[0] * k;
	dy = vec[1] * k;

	glColor4ub(30, 30, 220, bright);
	glBegin(GL_LINES);
	glVertex2f(start, start + ydisp);
	glVertex2f(start + dx, start + dy + ydisp);
	glEnd();

	if (fabsf(dx) > toll || fabsf(dy) > toll) {
		BLF_draw_default_ascii(start + dx + 2, start + dy + ydisp + 2, 0.0f, "z", 1);
	}

	/* restore line-width */
	
	glLineWidth(1.0);
	glDisable(GL_BLEND);
}

/* draw center and axis of rotation for ongoing 3D mouse navigation */
static void draw_rotation_guide(RegionView3D *rv3d)
{
	float o[3]; // center of rotation
	float end[3]; // endpoints for drawing

	float color[4] = {0.f ,0.4235f, 1.f, 1.f}; // bright blue so it matches device LEDs

	negate_v3_v3(o, rv3d->ofs);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glShadeModel(GL_SMOOTH);
	glPointSize(5);
	glEnable(GL_POINT_SMOOTH);
	glDepthMask(0); // don't overwrite zbuf

	if (rv3d->rot_angle != 0.f) {
		// -- draw rotation axis --
		float scaled_axis[3];
		const float scale = rv3d->dist;
		mul_v3_v3fl(scaled_axis, rv3d->rot_axis, scale);


		glBegin(GL_LINE_STRIP);
		color[3] = 0.f; // more transparent toward the ends
		glColor4fv(color);
		add_v3_v3v3(end, o, scaled_axis);
		glVertex3fv(end);

		// color[3] = 0.2f + fabsf(rv3d->rot_angle); // modulate opacity with angle
		// ^^ neat idea, but angle is frame-rate dependent, so it's usually close to 0.2

		color[3] = 0.5f; // more opaque toward the center
		glColor4fv(color);
		glVertex3fv(o);

		color[3] = 0.f;
		glColor4fv(color);
		sub_v3_v3v3(end, o, scaled_axis);
		glVertex3fv(end);
		glEnd();
		
		// -- draw ring around rotation center --
		{
#define		ROT_AXIS_DETAIL 13

			const float s = 0.05f * scale;
			const float step = 2.f * (float)(M_PI / ROT_AXIS_DETAIL);
			float angle;
			int i;

			float q[4]; // rotate ring so it's perpendicular to axis
			const int upright = fabsf(rv3d->rot_axis[2]) >= 0.95f;
			if (!upright) {
				const float up[3] = {0.f, 0.f, 1.f};
				float vis_angle, vis_axis[3];

				cross_v3_v3v3(vis_axis, up, rv3d->rot_axis);
				vis_angle = acosf(dot_v3v3(up, rv3d->rot_axis));
				axis_angle_to_quat(q, vis_axis, vis_angle);
			}

			color[3] = 0.25f; // somewhat faint
			glColor4fv(color);
			glBegin(GL_LINE_LOOP);
			for (i = 0, angle = 0.f; i < ROT_AXIS_DETAIL; ++i, angle += step) {
				float p[3] = {s * cosf(angle), s * sinf(angle), 0.0f};

				if (!upright) {
					mul_qt_v3(q, p);
				}

				add_v3_v3(p, o);
				glVertex3fv(p);
			}
			glEnd();

#undef		ROT_AXIS_DETAIL
		}

		color[3] = 1.f; // solid dot
	}
	else
		color[3] = 0.5f; // see-through dot

	// -- draw rotation center --
	glColor4fv(color);
	glBegin(GL_POINTS);
		glVertex3fv(o);
	glEnd();

	// find screen coordinates for rotation center, then draw pretty icon
	// mul_m4_v3(rv3d->persinv, rot_center);
	// UI_icon_draw(rot_center[0], rot_center[1], ICON_NDOF_TURN);
	// ^^ just playing around, does not work

	glDisable(GL_BLEND);
	glDisable(GL_POINT_SMOOTH);
	glDepthMask(1);
}

static void draw_view_icon(RegionView3D *rv3d)
{
	BIFIconID icon;
	
	if ( ELEM(rv3d->view, RV3D_VIEW_TOP, RV3D_VIEW_BOTTOM))
		icon= ICON_AXIS_TOP;
	else if ( ELEM(rv3d->view, RV3D_VIEW_FRONT, RV3D_VIEW_BACK))
		icon= ICON_AXIS_FRONT;
	else if ( ELEM(rv3d->view, RV3D_VIEW_RIGHT, RV3D_VIEW_LEFT))
		icon= ICON_AXIS_SIDE;
	else return ;
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,  GL_ONE_MINUS_SRC_ALPHA); 
	
	UI_icon_draw(5.0, 5.0, icon);
	
	glDisable(GL_BLEND);
}

static const char *view3d_get_name(View3D *v3d, RegionView3D *rv3d)
{
	const char *name = NULL;
	
	switch (rv3d->view) {
		case RV3D_VIEW_FRONT:
			if (rv3d->persp == RV3D_ORTHO) name = "Front Ortho";
			else name = "Front Persp";
			break;
		case RV3D_VIEW_BACK:
			if (rv3d->persp == RV3D_ORTHO) name = "Back Ortho";
			else name = "Back Persp";
			break;
		case RV3D_VIEW_TOP:
			if (rv3d->persp == RV3D_ORTHO) name = "Top Ortho";
			else name = "Top Persp";
			break;
		case RV3D_VIEW_BOTTOM:
			if (rv3d->persp == RV3D_ORTHO) name = "Bottom Ortho";
			else name = "Bottom Persp";
			break;
		case RV3D_VIEW_RIGHT:
			if (rv3d->persp == RV3D_ORTHO) name = "Right Ortho";
			else name = "Right Persp";
			break;
		case RV3D_VIEW_LEFT:
			if (rv3d->persp == RV3D_ORTHO) name = "Left Ortho";
			else name = "Left Persp";
			break;
			
		default:
			if (rv3d->persp==RV3D_CAMOB) {
				if ((v3d->camera) && (v3d->camera->type == OB_CAMERA)) {
					Camera *cam;
					cam = v3d->camera->data;
					name = (cam->type != CAM_ORTHO) ? "Camera Persp" : "Camera Ortho";
				}
				else {
					name = "Object as Camera";
				}
			}
			else {
				name = (rv3d->persp == RV3D_ORTHO) ? "User Ortho" : "User Persp";
			}
			break;
	}
	
	return name;
}

static void draw_viewport_name(ARegion *ar, View3D *v3d)
{
	RegionView3D *rv3d= ar->regiondata;
	const char *name= view3d_get_name(v3d, rv3d);
	char tmpstr[24];
	
	if (v3d->localvd) {
		BLI_snprintf(tmpstr, sizeof(tmpstr), "%s (Local)", name);
		name= tmpstr;
	}

	if (name) {
		UI_ThemeColor(TH_TEXT_HI);
		BLF_draw_default_ascii(22,  ar->winy-17, 0.0f, name, sizeof(tmpstr));
	}
}

/* draw info beside axes in bottom left-corner: 
* 	framenum, object name, bone name (if available), marker name (if available)
*/
static void draw_selected_name(Scene *scene, Object *ob)
{
	char info[256], *markern;
	short offset=30;
	
	/* get name of marker on current frame (if available) */
	markern= scene_find_marker_name(scene, CFRA);
	
	/* check if there is an object */
	if (ob) {
		/* name(s) to display depends on type of object */
		if (ob->type==OB_ARMATURE) {
			bArmature *arm= ob->data;
			char *name= NULL;
			
			/* show name of active bone too (if possible) */
			if (arm->edbo) {

				if (arm->act_edbone)
					name= ((EditBone *)arm->act_edbone)->name;

			}
			else if (ob->mode & OB_MODE_POSE) {
				if (arm->act_bone) {

					if (arm->act_bone->layer & arm->layer)
						name= arm->act_bone->name;

				}
			}
			if (name && markern)
				BLI_snprintf(info, sizeof(info), "(%d) %s %s <%s>", CFRA, ob->id.name+2, name, markern);
			else if (name)
				BLI_snprintf(info, sizeof(info), "(%d) %s %s", CFRA, ob->id.name+2, name);
			else
				BLI_snprintf(info, sizeof(info), "(%d) %s", CFRA, ob->id.name+2);
		}
		else if (ELEM3(ob->type, OB_MESH, OB_LATTICE, OB_CURVE)) {
			Key *key= NULL;
			KeyBlock *kb = NULL;
			char shapes[MAX_NAME + 10];
			
			/* try to display active shapekey too */
			shapes[0] = '\0';
			key = ob_get_key(ob);
			if (key) {
				kb = BLI_findlink(&key->block, ob->shapenr-1);
				if (kb) {
					BLI_snprintf(shapes, sizeof(shapes), ": %s ", kb->name);
					if (ob->shapeflag == OB_SHAPE_LOCK) {
						strcat(shapes, " (Pinned)");
					}
				}
			}
			
			if (markern)
				BLI_snprintf(info, sizeof(info), "(%d) %s %s <%s>", CFRA, ob->id.name+2, shapes, markern);
			else
				BLI_snprintf(info, sizeof(info), "(%d) %s %s", CFRA, ob->id.name+2, shapes);
		}
		else {
			/* standard object */
			if (markern)
				BLI_snprintf(info, sizeof(info), "(%d) %s <%s>", CFRA, ob->id.name+2, markern);
			else
				BLI_snprintf(info, sizeof(info), "(%d) %s", CFRA, ob->id.name+2);
		}
		
		/* color depends on whether there is a keyframe */
		if (id_frame_has_keyframe((ID *)ob, /*BKE_curframe(scene)*/(float)(CFRA), ANIMFILTER_KEYS_LOCAL))
			UI_ThemeColor(TH_VERTEX_SELECT);
		else
			UI_ThemeColor(TH_TEXT_HI);
	}
	else {
		/* no object */
		if (markern)
			BLI_snprintf(info, sizeof(info), "(%d) <%s>", CFRA, markern);
		else
			BLI_snprintf(info, sizeof(info), "(%d)", CFRA);
		
		/* color is always white */
		UI_ThemeColor(TH_TEXT_HI);
	}
	
	if (U.uiflag & USER_SHOW_ROTVIEWICON)
		offset = 14 + (U.rvisize * 2);

	BLF_draw_default(offset,  10, 0.0f, info, sizeof(info));
}

static void view3d_camera_border(Scene *scene, ARegion *ar, View3D *v3d, RegionView3D *rv3d,
                                 rctf *viewborder_r, short no_shift, short no_zoom)
{
	CameraParams params;
	rctf rect_view, rect_camera;

	/* get viewport viewplane */
	camera_params_init(&params);
	camera_params_from_view3d(&params, v3d, rv3d);
	if (no_zoom)
		params.zoom= 1.0f;
	camera_params_compute_viewplane(&params, ar->winx, ar->winy, 1.0f, 1.0f);
	rect_view= params.viewplane;

	/* get camera viewplane */
	camera_params_init(&params);
	camera_params_from_object(&params, v3d->camera);
	if (no_shift) {
		params.shiftx= 0.0f;
		params.shifty= 0.0f;
	}
	camera_params_compute_viewplane(&params, scene->r.xsch, scene->r.ysch, scene->r.xasp, scene->r.yasp);
	rect_camera= params.viewplane;

	/* get camera border within viewport */
	viewborder_r->xmin= ((rect_camera.xmin - rect_view.xmin)/(rect_view.xmax - rect_view.xmin))*ar->winx;
	viewborder_r->xmax= ((rect_camera.xmax - rect_view.xmin)/(rect_view.xmax - rect_view.xmin))*ar->winx;
	viewborder_r->ymin= ((rect_camera.ymin - rect_view.ymin)/(rect_view.ymax - rect_view.ymin))*ar->winy;
	viewborder_r->ymax= ((rect_camera.ymax - rect_view.ymin)/(rect_view.ymax - rect_view.ymin))*ar->winy;
}

void ED_view3d_calc_camera_border_size(Scene *scene, ARegion *ar, View3D *v3d, RegionView3D *rv3d, float size_r[2])
{
	rctf viewborder;

	view3d_camera_border(scene, ar, v3d, rv3d, &viewborder, TRUE, TRUE);
	size_r[0]= viewborder.xmax - viewborder.xmin;
	size_r[1]= viewborder.ymax - viewborder.ymin;
}

void ED_view3d_calc_camera_border(Scene *scene, ARegion *ar, View3D *v3d, RegionView3D *rv3d,
                                  rctf *viewborder_r, short no_shift)
{
	view3d_camera_border(scene, ar, v3d, rv3d, viewborder_r, no_shift, FALSE);
}

static void drawviewborder_grid3(float x1, float x2, float y1, float y2, float fac)
{
	float x3, y3, x4, y4;

	x3= x1 + fac * (x2-x1);
	y3= y1 + fac * (y2-y1);
	x4= x1 + (1.0f - fac) * (x2-x1);
	y4= y1 + (1.0f - fac) * (y2-y1);

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
	float w= x2 - x1;
	float h= y2 - y1;

	glBegin(GL_LINES);
	if (w > h) {
		if (golden) {
			ofs = w * (1.0f-(1.0f/1.61803399f));
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
			ofs = h * (1.0f-(1.0f/1.61803399f));
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
	float fac, a;
	float x1, x2, y1, y2;
	float x1i, x2i, y1i, y2i;
	float x3, y3, x4, y4;
	rctf viewborder;
	Camera *ca= NULL;
	RegionView3D *rv3d= (RegionView3D *)ar->regiondata;
	
	if (v3d->camera==NULL)
		return;
	if (v3d->camera->type==OB_CAMERA)
		ca = v3d->camera->data;
	
	ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &viewborder, FALSE);
	/* the offsets */
	x1= viewborder.xmin;
	y1= viewborder.ymin;
	x2= viewborder.xmax;
	y2= viewborder.ymax;
	
	/* apply offsets so the real 3D camera shows through */

	/* note: quite un-scientific but without this bit extra
	 * 0.0001 on the lower left the 2D border sometimes
	 * obscures the 3D camera border */
	/* note: with VIEW3D_CAMERA_BORDER_HACK defined this error isn't noticable
	 * but keep it here incase we need to remove the workaround */
	x1i= (int)(x1 - 1.0001f);
	y1i= (int)(y1 - 1.0001f);
	x2i= (int)(x2 + (1.0f-0.0001f));
	y2i= (int)(y2 + (1.0f-0.0001f));
	
	/* passepartout, specified in camera edit buttons */
	if (ca && (ca->flag & CAM_SHOWPASSEPARTOUT) && ca->passepartalpha > 0.000001f) {
		if (ca->passepartalpha == 1.0f) {
			glColor3f(0, 0, 0);
		}
		else {
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			glEnable(GL_BLEND);
			glColor4f(0, 0, 0, ca->passepartalpha);
		}
		if (x1i > 0.0f)
			glRectf(0.0, (float)ar->winy, x1i, 0.0);
		if (x2i < (float)ar->winx)
			glRectf(x2i, (float)ar->winy, (float)ar->winx, 0.0);
		if (y2i < (float)ar->winy)
			glRectf(x1i, (float)ar->winy, x2i, y2i);
		if (y2i > 0.0f)
			glRectf(x1i, y1i, x2i, 0.0);
		
		glDisable(GL_BLEND);
	}

	/* edge */
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);	

	setlinestyle(0);

	UI_ThemeColor(TH_BACK);
		
	glRectf(x1i, y1i, x2i, y2i);

#ifdef VIEW3D_CAMERA_BORDER_HACK
	if (view3d_camera_border_hack_test == TRUE) {
		glColor4fv(view3d_camera_border_hack_col);
		glRectf(x1i+1, y1i+1, x2i-1, y2i-1);
		view3d_camera_border_hack_test= FALSE;
	}
#endif

	setlinestyle(3);

	/* outer line not to confuse with object selecton */
	if (v3d->flag2 & V3D_LOCK_CAMERA) {
		UI_ThemeColor(TH_REDALERT);
		glRectf(x1i - 1, y1i - 1, x2i + 1, y2i + 1);
	}

	UI_ThemeColor(TH_WIRE);
	glRectf(x1i, y1i, x2i, y2i);

	/* border */
	if (scene->r.mode & R_BORDER) {
		
		cpack(0);
		x3= x1+ scene->r.border.xmin*(x2-x1);
		y3= y1+ scene->r.border.ymin*(y2-y1);
		x4= x1+ scene->r.border.xmax*(x2-x1);
		y4= y1+ scene->r.border.ymax*(y2-y1);
		
		cpack(0x4040FF);
		glRectf(x3,  y3,  x4,  y4); 
	}

	/* safety border */
	if (ca) {
		if (ca->dtx & CAM_DTX_CENTER) {
			UI_ThemeColorBlendShade(TH_WIRE, TH_BACK, 0.25, 0);

			x3= x1+ 0.5f*(x2-x1);
			y3= y1+ 0.5f*(y2-y1);

			glBegin(GL_LINES);
			glVertex2f(x1, y3);
			glVertex2f(x2, y3);

			glVertex2f(x3, y1);
			glVertex2f(x3, y2);
			glEnd();
		}

		if (ca->dtx & CAM_DTX_CENTER_DIAG) {
			UI_ThemeColorBlendShade(TH_WIRE, TH_BACK, 0.25, 0);

			glBegin(GL_LINES);
			glVertex2f(x1, y1);
			glVertex2f(x2, y2);

			glVertex2f(x1, y2);
			glVertex2f(x2, y1);
			glEnd();
		}

		if (ca->dtx & CAM_DTX_THIRDS) {
			UI_ThemeColorBlendShade(TH_WIRE, TH_BACK, 0.25, 0);
			drawviewborder_grid3(x1, x2, y1, y2, 1.0f/3.0f);
		}

		if (ca->dtx & CAM_DTX_GOLDEN) {
			UI_ThemeColorBlendShade(TH_WIRE, TH_BACK, 0.25, 0);
			drawviewborder_grid3(x1, x2, y1, y2, 1.0f-(1.0f/1.61803399f));
		}

		if (ca->dtx & CAM_DTX_GOLDEN_TRI_A) {
			UI_ThemeColorBlendShade(TH_WIRE, TH_BACK, 0.25, 0);
			drawviewborder_triangle(x1, x2, y1, y2, 0, 'A');
		}

		if (ca->dtx & CAM_DTX_GOLDEN_TRI_B) {
			UI_ThemeColorBlendShade(TH_WIRE, TH_BACK, 0.25, 0);
			drawviewborder_triangle(x1, x2, y1, y2, 0, 'B');
		}

		if (ca->dtx & CAM_DTX_HARMONY_TRI_A) {
			UI_ThemeColorBlendShade(TH_WIRE, TH_BACK, 0.25, 0);
			drawviewborder_triangle(x1, x2, y1, y2, 1, 'A');
		}

		if (ca->dtx & CAM_DTX_HARMONY_TRI_B) {
			UI_ThemeColorBlendShade(TH_WIRE, TH_BACK, 0.25, 0);
			drawviewborder_triangle(x1, x2, y1, y2, 1, 'B');
		}

		if (ca->flag & CAM_SHOWTITLESAFE) {
			fac= 0.1;

			a= fac*(x2-x1);
			x1+= a;
			x2-= a;

			a= fac*(y2-y1);
			y1+= a;
			y2-= a;

			UI_ThemeColorBlendShade(TH_WIRE, TH_BACK, 0.25, 0);

			uiSetRoundBox(UI_CNR_ALL);
			uiDrawBox(GL_LINE_LOOP, x1, y1, x2, y2, 12.0);
		}
		if (ca && (ca->flag & CAM_SHOWSENSOR)) {
			/* determine sensor fit, and get sensor x/y, for auto fit we
			   assume and square sensor and only use sensor_x */
			float sizex= scene->r.xsch*scene->r.xasp;
			float sizey= scene->r.ysch*scene->r.yasp;
			int sensor_fit = camera_sensor_fit(ca->sensor_fit, sizex, sizey);
			float sensor_x= ca->sensor_x;
			float sensor_y= (ca->sensor_fit == CAMERA_SENSOR_FIT_AUTO)? ca->sensor_x: ca->sensor_y;

			/* determine sensor plane */
			rctf rect;

			if (sensor_fit == CAMERA_SENSOR_FIT_HOR) {
				float sensor_scale = (x2i-x1i) / sensor_x;
				float sensor_height = sensor_scale * sensor_y;

				rect.xmin= x1i;
				rect.xmax= x2i;
				rect.ymin= (y1i + y2i)*0.5f - sensor_height*0.5f;
				rect.ymax= rect.ymin + sensor_height;
			}
			else {
				float sensor_scale = (y2i-y1i) / sensor_y;
				float sensor_width = sensor_scale * sensor_x;

				rect.xmin= (x1i + x2i)*0.5f - sensor_width*0.5f;
				rect.xmax= rect.xmin + sensor_width;
				rect.ymin= y1i;
				rect.ymax= y2i;
			}

			/* draw */
			UI_ThemeColorShade(TH_WIRE, 100);
			uiDrawBox(GL_LINE_LOOP, rect.xmin, rect.ymin, rect.xmax, rect.ymax, 2.0f);
		}
	}

	setlinestyle(0);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	/* camera name - draw in highlighted text color */
	if (ca && (ca->flag & CAM_SHOWNAME)) {
		UI_ThemeColor(TH_TEXT_HI);
		BLF_draw_default(x1i, y1i-15, 0.0f, v3d->camera->id.name+2, sizeof(v3d->camera->id.name)-2);
		UI_ThemeColor(TH_WIRE);
	}
}

/* *********************** backdraw for selection *************** */

static void backdrawview3d(Scene *scene, ARegion *ar, View3D *v3d)
{
	RegionView3D *rv3d= ar->regiondata;
	struct Base *base = scene->basact;
	int multisample_enabled;
	rcti winrct;

	BLI_assert(ar->regiontype == RGN_TYPE_WINDOW);

	if (base && (base->object->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT) ||
	            paint_facesel_test(base->object)))
	{
		/* do nothing */
	}
	else if ((base && (base->object->mode & OB_MODE_TEXTURE_PAINT)) &&
	        scene->toolsettings && (scene->toolsettings->imapaint.flag & IMAGEPAINT_PROJECT_DISABLE))
	{
		/* do nothing */
	}
	else if ((base && (base->object->mode & OB_MODE_PARTICLE_EDIT)) &&
	        v3d->drawtype > OB_WIRE && (v3d->flag & V3D_ZBUF_SELECT))
	{
		/* do nothing */
	}
	else if (scene->obedit && v3d->drawtype>OB_WIRE &&
	        (v3d->flag & V3D_ZBUF_SELECT)) {
		/* do nothing */
	}
	else {
		v3d->flag &= ~V3D_INVALID_BACKBUF;
		return;
	}

	if ( !(v3d->flag & V3D_INVALID_BACKBUF) ) return;

//	if (test) {
//		if (qtest()) {
//			addafterqueue(ar->win, BACKBUFDRAW, 1);
//			return;
//		}
//	}

	if (v3d->drawtype > OB_WIRE) v3d->zbuf= TRUE;
	
	/* dithering and AA break color coding, so disable */
	glDisable(GL_DITHER);

	multisample_enabled= glIsEnabled(GL_MULTISAMPLE_ARB);
	if (multisample_enabled)
		glDisable(GL_MULTISAMPLE_ARB);

	region_scissor_winrct(ar, &winrct);
	glScissor(winrct.xmin, winrct.ymin, winrct.xmax - winrct.xmin, winrct.ymax - winrct.ymin);

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
		view3d_set_clipping(rv3d);
	
	G.f |= G_BACKBUFSEL;
	
	if (base && (base->lay & v3d->lay))
		draw_object_backbufsel(scene, v3d, rv3d, base->object);

	v3d->flag &= ~V3D_INVALID_BACKBUF;
	ar->swap= 0; /* mark invalid backbuf for wm draw */

	G.f &= ~G_BACKBUFSEL;
	v3d->zbuf= FALSE; 
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_DITHER);
	if (multisample_enabled)
		glEnable(GL_MULTISAMPLE_ARB);

	if (rv3d->rflag & RV3D_CLIPPING)
		view3d_clr_clipping();

	/* it is important to end a view in a transform compatible with buttons */
//	persp(PERSP_WIN);  // set ortho

}

void view3d_validate_backbuf(ViewContext *vc)
{
	if (vc->v3d->flag & V3D_INVALID_BACKBUF)
		backdrawview3d(vc->scene, vc->ar, vc->v3d);
}

/* samples a single pixel (copied from vpaint) */
unsigned int view3d_sample_backbuf(ViewContext *vc, int x, int y)
{
	unsigned int col;
	
	if (x >= vc->ar->winx || y >= vc->ar->winy) return 0;
	x+= vc->ar->winrct.xmin;
	y+= vc->ar->winrct.ymin;
	
	view3d_validate_backbuf(vc);

	glReadPixels(x,  y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,  &col);
	glReadBuffer(GL_BACK);	
	
	if (ENDIAN_ORDER==B_ENDIAN) SWITCH_INT(col);
	
	return WM_framebuffer_to_index(col);
}

/* reads full rect, converts indices */
ImBuf *view3d_read_backbuf(ViewContext *vc, short xmin, short ymin, short xmax, short ymax)
{
	unsigned int *dr, *rd;
	struct ImBuf *ibuf, *ibuf1;
	int a;
	short xminc, yminc, xmaxc, ymaxc, xs, ys;
	
	/* clip */
	if (xmin<0) xminc= 0; else xminc= xmin;
	if (xmax >= vc->ar->winx) xmaxc= vc->ar->winx-1; else xmaxc= xmax;
	if (xminc > xmaxc) return NULL;

	if (ymin<0) yminc= 0; else yminc= ymin;
	if (ymax >= vc->ar->winy) ymaxc= vc->ar->winy-1; else ymaxc= ymax;
	if (yminc > ymaxc) return NULL;
	
	ibuf= IMB_allocImBuf((xmaxc-xminc+1), (ymaxc-yminc+1), 32, IB_rect);

	view3d_validate_backbuf(vc); 
	
	glReadPixels(vc->ar->winrct.xmin + xminc,
	             vc->ar->winrct.ymin + yminc,
	             (xmaxc-xminc + 1),
	             (ymaxc-yminc + 1),
	             GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);

	glReadBuffer(GL_BACK);	

	if (ENDIAN_ORDER==B_ENDIAN) IMB_convert_rgba_to_abgr(ibuf);

	a= (xmaxc-xminc+1)*(ymaxc-yminc+1);
	dr= ibuf->rect;
	while (a--) {
		if (*dr) *dr= WM_framebuffer_to_index(*dr);
		dr++;
	}
	
	/* put clipped result back, if needed */
	if (xminc==xmin && xmaxc==xmax && yminc==ymin && ymaxc==ymax)
		return ibuf;
	
	ibuf1= IMB_allocImBuf( (xmax-xmin+1),(ymax-ymin+1),32,IB_rect);
	rd= ibuf->rect;
	dr= ibuf1->rect;
		
	for (ys= ymin; ys<=ymax; ys++) {
		for (xs= xmin; xs<=xmax; xs++, dr++) {
			if ( xs>=xminc && xs<=xmaxc && ys>=yminc && ys<=ymaxc) {
				*dr= *rd;
				rd++;
			}
		}
	}
	IMB_freeImBuf(ibuf);
	return ibuf1;
}

/* smart function to sample a rect spiralling outside, nice for backbuf selection */
unsigned int view3d_sample_backbuf_rect(ViewContext *vc, const int mval[2], int size,
										unsigned int min, unsigned int max, int *dist, short strict, 
										void *handle, unsigned int (*indextest)(void *handle, unsigned int index))
{
	struct ImBuf *buf;
	unsigned int *bufmin, *bufmax, *tbuf;
	int minx, miny;
	int a, b, rc, nr, amount, dirvec[4][2];
	int distance=0;
	unsigned int index = 0;
	short indexok = 0;	

	amount= (size-1)/2;

	minx = mval[0]-(amount+1);
	miny = mval[1]-(amount+1);
	buf = view3d_read_backbuf(vc, minx, miny, minx+size-1, miny+size-1);
	if (!buf) return 0;

	rc= 0;
	
	dirvec[0][0]= 1; dirvec[0][1]= 0;
	dirvec[1][0]= 0; dirvec[1][1]= -size;
	dirvec[2][0]= -1; dirvec[2][1]= 0;
	dirvec[3][0]= 0; dirvec[3][1]= size;
	
	bufmin = buf->rect;
	tbuf = buf->rect;
	bufmax = buf->rect + size*size;
	tbuf+= amount*size+ amount;
	
	for (nr=1; nr<=size; nr++) {
		
		for (a=0; a<2; a++) {
			for (b=0; b<nr; b++, distance++) {
				if (*tbuf && *tbuf>=min && *tbuf<max) { //we got a hit
					if (strict) {
						indexok =  indextest(handle, *tbuf - min+1);
						if (indexok) {
							*dist= (short) sqrt( (float)distance   );
							index = *tbuf - min+1;
							goto exit; 
						}						
					}
					else{
						*dist= (short) sqrt( (float)distance ); // XXX, this distance is wrong - 
						index = *tbuf - min+1; // messy yah, but indices start at 1
						goto exit;
					}			
				}
				
				tbuf+= (dirvec[rc][0]+dirvec[rc][1]);
				
				if (tbuf<bufmin || tbuf>=bufmax) {
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

static void draw_bgpic(Scene *scene, ARegion *ar, View3D *v3d)
{
	RegionView3D *rv3d= ar->regiondata;
	BGpic *bgpic;
	Image *ima;
	MovieClip *clip;
	ImBuf *ibuf= NULL, *freeibuf;
	float vec[4], fac, asp, zoomx, zoomy;
	float x1, y1, x2, y2, cx, cy;


	for ( bgpic= v3d->bgpicbase.first; bgpic; bgpic= bgpic->next ) {

		if (	(bgpic->view == 0) || /* zero for any */
			(bgpic->view & (1<<rv3d->view)) || /* check agaist flags */
			(rv3d->persp==RV3D_CAMOB && bgpic->view == (1<<RV3D_VIEW_CAMERA))
		) {
			/* disable individual images */
			if ((bgpic->flag&V3D_BGPIC_DISABLED))
				continue;

			freeibuf= NULL;
			if (bgpic->source==V3D_BGPIC_IMAGE) {
				ima= bgpic->ima;
				if (ima==NULL)
					continue;
				BKE_image_user_calc_frame(&bgpic->iuser, CFRA, 0);
				ibuf= BKE_image_get_ibuf(ima, &bgpic->iuser);
			}
			else {
				clip= NULL;

				if (bgpic->flag&V3D_BGPIC_CAMERACLIP) {
					if (scene->camera)
						clip= object_get_movieclip(scene, scene->camera, 1);
				}
				else clip= bgpic->clip;

				if (clip==NULL)
					continue;

				BKE_movieclip_user_set_frame(&bgpic->cuser, CFRA);
				ibuf= BKE_movieclip_get_ibuf(clip, &bgpic->cuser);

				/* working with ibuf from image and clip has got different workflow now.
				   ibuf acquired from clip is referenced by cache system and should
				   be dereferenced after usage. */
				freeibuf= ibuf;
			}

			if (ibuf==NULL)
				continue;

			if ((ibuf->rect==NULL && ibuf->rect_float==NULL) || ibuf->channels!=4) { /* invalid image format */
				if (freeibuf)
					IMB_freeImBuf(freeibuf);

				continue;
			}

			if (ibuf->rect==NULL)
				IMB_rect_from_float(ibuf);

			if (rv3d->persp==RV3D_CAMOB) {
				rctf vb;

				ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &vb, FALSE);

				x1= vb.xmin;
				y1= vb.ymin;
				x2= vb.xmax;
				y2= vb.ymax;
			}
			else {
				float sco[2];
				const float mval_f[2]= {1.0f, 0.0f};

				/* calc window coord */
				initgrabz(rv3d, 0.0, 0.0, 0.0);
				ED_view3d_win_to_delta(ar, mval_f, vec);
				fac= maxf(fabsf(vec[0]), maxf(fabsf(vec[1]), fabsf(vec[2]))); /* largest abs axis */
				fac= 1.0f/fac;

				asp= ( (float)ibuf->y)/(float)ibuf->x;

				vec[0] = vec[1] = vec[2] = 0.0;
				ED_view3d_project_float(ar, vec, sco, rv3d->persmat);
				cx = sco[0];
				cy = sco[1];

				x1=  cx+ fac*(bgpic->xof-bgpic->size);
				y1=  cy+ asp*fac*(bgpic->yof-bgpic->size);
				x2=  cx+ fac*(bgpic->xof+bgpic->size);
				y2=  cy+ asp*fac*(bgpic->yof+bgpic->size);
			}

			/* complete clip? */

			if (x2 < 0 || y2 < 0 || x1 > ar->winx || y1 > ar->winy) {
				if (freeibuf)
					IMB_freeImBuf(freeibuf);

				continue;
			}

			zoomx= (x2-x1)/ibuf->x;
			zoomy= (y2-y1)/ibuf->y;

			/* for some reason; zoomlevels down refuses to use GL_ALPHA_SCALE */
			if (zoomx < 1.0f || zoomy < 1.0f) {
				float tzoom= MIN2(zoomx, zoomy);
				int mip= 0;

				if ((ibuf->userflags&IB_MIPMAP_INVALID) != 0) {
					IMB_remakemipmap(ibuf, 0);
					ibuf->userflags&= ~IB_MIPMAP_INVALID;
				}
				else if (ibuf->mipmap[0]==NULL)
					IMB_makemipmap(ibuf, 0);

				while (tzoom < 1.0f && mip<8 && ibuf->mipmap[mip]) {
					tzoom*= 2.0f;
					zoomx*= 2.0f;
					zoomy*= 2.0f;
					mip++;
				}
				if (mip>0)
					ibuf= ibuf->mipmap[mip-1];
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

			glPixelZoom(zoomx, zoomy);
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f-bgpic->blend);
			glaDrawPixelsTex(x1, y1, ibuf->x, ibuf->y, GL_UNSIGNED_BYTE, ibuf->rect);

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
		}
	}
}

/* ****************** View3d afterdraw *************** */

typedef struct View3DAfter {
	struct View3DAfter *next, *prev;
	struct Base *base;
	int flag;
} View3DAfter;

/* temp storage of Objects that need to be drawn as last */
void add_view3d_after(ListBase *lb, Base *base, int flag)
{
	View3DAfter *v3da= MEM_callocN(sizeof(View3DAfter), "View 3d after");
	BLI_addtail(lb, v3da);
	v3da->base= base;
	v3da->flag= flag;
}

/* disables write in zbuffer and draws it over */
static void view3d_draw_transp(Scene *scene, ARegion *ar, View3D *v3d)
{
	View3DAfter *v3da, *next;
	
	glDepthMask(0);
	v3d->transp= TRUE;
	
	for (v3da= v3d->afterdraw_transp.first; v3da; v3da= next) {
		next= v3da->next;
		draw_object(scene, ar, v3d, v3da->base, v3da->flag);
		BLI_remlink(&v3d->afterdraw_transp, v3da);
		MEM_freeN(v3da);
	}
	v3d->transp= FALSE;
	
	glDepthMask(1);
	
}

/* clears zbuffer and draws it over */
static void view3d_draw_xray(Scene *scene, ARegion *ar, View3D *v3d, int clear)
{
	View3DAfter *v3da, *next;

	if (clear && v3d->zbuf)
		glClear(GL_DEPTH_BUFFER_BIT);

	v3d->xray= TRUE;
	for (v3da= v3d->afterdraw_xray.first; v3da; v3da= next) {
		next= v3da->next;
		draw_object(scene, ar, v3d, v3da->base, v3da->flag);
		BLI_remlink(&v3d->afterdraw_xray, v3da);
		MEM_freeN(v3da);
	}
	v3d->xray= FALSE;
}


/* clears zbuffer and draws it over */
static void view3d_draw_xraytransp(Scene *scene, ARegion *ar, View3D *v3d, int clear)
{
	View3DAfter *v3da, *next;

	if (clear && v3d->zbuf)
		glClear(GL_DEPTH_BUFFER_BIT);

	v3d->xray= TRUE;
	v3d->transp= TRUE;
	
	for (v3da= v3d->afterdraw_xraytransp.first; v3da; v3da= next) {
		next= v3da->next;
		draw_object(scene, ar, v3d, v3da->base, v3da->flag);
		BLI_remlink(&v3d->afterdraw_xraytransp, v3da);
		MEM_freeN(v3da);
	}

	v3d->transp= FALSE;
	v3d->xray= FALSE;

}

/* *********************** */

/*
	In most cases call draw_dupli_objects,
	draw_dupli_objects_color was added because when drawing set dupli's
	we need to force the color
 */

#if 0
int dupli_ob_sort(void *arg1, void *arg2)
{
	void *p1= ((DupliObject *)arg1)->ob;
	void *p2= ((DupliObject *)arg2)->ob;
	int val = 0;
	if (p1 < p2)		val = -1;
	else if (p1 > p2)	val = 1;
	return val;
}
#endif


static DupliObject *dupli_step(DupliObject *dob)
{
	while (dob && dob->no_draw)
		dob= dob->next;
	return dob;
}

static void draw_dupli_objects_color(Scene *scene, ARegion *ar, View3D *v3d, Base *base, int color)
{
	RegionView3D *rv3d= ar->regiondata;
	ListBase *lb;
	DupliObject *dob_prev= NULL, *dob, *dob_next= NULL;
	Base tbase;
	BoundBox bb, *bb_tmp; /* use a copy because draw_object, calls clear_mesh_caches */
	GLuint displist=0;
	short transflag, use_displist= -1;	/* -1 is initialize */
	char dt, dtx;
	
	if (base->object->restrictflag & OB_RESTRICT_VIEW) return;
	
	tbase.flag= OB_FROMDUPLI|base->flag;
	lb= object_duplilist(scene, base->object);
	// BLI_sortlist(lb, dupli_ob_sort); // might be nice to have if we have a dupli list with mixed objects.

	dob=dupli_step(lb->first);
	if (dob) dob_next= dupli_step(dob->next);

	for ( ; dob ; dob_prev= dob, dob= dob_next, dob_next= dob_next ? dupli_step(dob_next->next) : NULL) {
		tbase.object= dob->ob;

		/* extra service: draw the duplicator in drawtype of parent */
		/* MIN2 for the drawtype to allow bounding box objects in groups for lods */
		dt= tbase.object->dt;	tbase.object->dt= MIN2(tbase.object->dt, base->object->dt);
		dtx= tbase.object->dtx; tbase.object->dtx= base->object->dtx;

		/* negative scale flag has to propagate */
		transflag= tbase.object->transflag;
		if (base->object->transflag & OB_NEG_SCALE)
			tbase.object->transflag ^= OB_NEG_SCALE;

		UI_ThemeColorBlend(color, TH_BACK, 0.5);

		/* generate displist, test for new object */
		if (dob_prev && dob_prev->ob != dob->ob) {
			if (use_displist==1)
				glDeleteLists(displist, 1);

			use_displist= -1;
		}

		/* generate displist */
		if (use_displist == -1) {

			/* note, since this was added, its checked dob->type==OB_DUPLIGROUP
			 * however this is very slow, it was probably needed for the NLA
			 * offset feature (used in group-duplicate.blend but no longer works in 2.5)
			 * so for now it should be ok to - campbell */

			if ( /* if this is the last no need  to make a displist */
			     (dob_next==NULL || dob_next->ob != dob->ob) ||
			     /* lamp drawing messes with matrices, could be handled smarter... but this works */
			     (dob->ob->type == OB_LAMP) ||
			     (dob->type == OB_DUPLIGROUP && dob->animated) ||
			     !(bb_tmp= object_get_boundbox(dob->ob)))
			{
				// printf("draw_dupli_objects_color: skipping displist for %s\n", dob->ob->id.name+2);
				use_displist= 0;
			}
			else {
				// printf("draw_dupli_objects_color: using displist for %s\n", dob->ob->id.name+2);
				bb= *bb_tmp; /* must make a copy  */

				/* disable boundbox check for list creation */
				object_boundbox_flag(dob->ob, OB_BB_DISABLED, 1);
				/* need this for next part of code */
				unit_m4(dob->ob->obmat);	/* obmat gets restored */

				displist= glGenLists(1);
				glNewList(displist, GL_COMPILE);
				draw_object(scene, ar, v3d, &tbase, DRAW_CONSTCOLOR);
				glEndList();

				use_displist= 1;
				object_boundbox_flag(dob->ob, OB_BB_DISABLED, 0);
			}
		}
		if (use_displist) {
			glMultMatrixf(dob->mat);
			if (ED_view3d_boundbox_clip(rv3d, dob->mat, &bb))
				glCallList(displist);
			glLoadMatrixf(rv3d->viewmat);
		}
		else {
			copy_m4_m4(dob->ob->obmat, dob->mat);
			draw_object(scene, ar, v3d, &tbase, DRAW_CONSTCOLOR);
		}

		tbase.object->dt= dt;
		tbase.object->dtx= dtx;
		tbase.object->transflag= transflag;
	}
	
	/* Transp afterdraw disabled, afterdraw only stores base pointers, and duplis can be same obj */
	
	free_object_duplilist(lb);	/* does restore */
	
	if (use_displist)
		glDeleteLists(displist, 1);
}

static void draw_dupli_objects(Scene *scene, ARegion *ar, View3D *v3d, Base *base)
{
	/* define the color here so draw_dupli_objects_color can be called
	* from the set loop */
	
	int color= (base->flag & SELECT)?TH_SELECT:TH_WIRE;
	/* debug */
	if (base->object->dup_group && base->object->dup_group->id.us<1)
		color= TH_REDALERT;
	
	draw_dupli_objects_color(scene, ar, v3d, base, color);
}

void view3d_update_depths_rect(ARegion *ar, ViewDepths *d, rcti *rect)
{
	int x, y, w, h;	
	rcti r;
	/* clamp rect by area */

	r.xmin= 0;
	r.xmax= ar->winx-1;
	r.ymin= 0;
	r.ymax= ar->winy-1;

	/* Constrain rect to depth bounds */
	BLI_isect_rcti(&r, rect, rect);

	/* assign values to compare with the ViewDepths */
	x= rect->xmin;
	y= rect->ymin;

	w= rect->xmax - rect->xmin;
	h= rect->ymax - rect->ymin;

	if (w <= 0 || h <= 0) {
		if (d->depths)
			MEM_freeN(d->depths);
		d->depths= NULL;

		d->damaged= FALSE;
	}
	else if (	d->w != w ||
		d->h != h ||
		d->x != x ||
		d->y != y ||
		d->depths==NULL
	) {
		d->x= x;
		d->y= y;
		d->w= w;
		d->h= h;

		if (d->depths)
			MEM_freeN(d->depths);

		d->depths= MEM_mallocN(sizeof(float)*d->w*d->h,"View depths Subset");
		
		d->damaged= TRUE;
	}

	if (d->damaged) {
		glReadPixels(ar->winrct.xmin+d->x,ar->winrct.ymin+d->y, d->w,d->h, GL_DEPTH_COMPONENT,GL_FLOAT, d->depths);
		glGetDoublev(GL_DEPTH_RANGE,d->depth_range);
		d->damaged= FALSE;
	}
}

/* note, with nouveau drivers the glReadPixels() is very slow. [#24339] */
void ED_view3d_depth_update(ARegion *ar)
{
	RegionView3D *rv3d= ar->regiondata;
	
	/* Create storage for, and, if necessary, copy depth buffer */
	if (!rv3d->depths) rv3d->depths= MEM_callocN(sizeof(ViewDepths),"ViewDepths");
	if (rv3d->depths) {
		ViewDepths *d= rv3d->depths;
		if (d->w != ar->winx ||
		   d->h != ar->winy ||
		   !d->depths) {
			d->w= ar->winx;
			d->h= ar->winy;
			if (d->depths)
				MEM_freeN(d->depths);
			d->depths= MEM_mallocN(sizeof(float)*d->w*d->h,"View depths");
			d->damaged= 1;
		}
		
		if (d->damaged) {
			glReadPixels(ar->winrct.xmin,ar->winrct.ymin,d->w,d->h,
						 GL_DEPTH_COMPONENT,GL_FLOAT, d->depths);
			
			glGetDoublev(GL_DEPTH_RANGE,d->depth_range);
			
			d->damaged= 0;
		}
	}
}

/* utility function to find the closest Z value, use for autodepth */
float view3d_depth_near(ViewDepths *d)
{
	/* convert to float for comparisons */
	const float near= (float)d->depth_range[0];
	const float far_real= (float)d->depth_range[1];
	float far= far_real;

	const float *depths= d->depths;
	float depth= FLT_MAX;
	int i= (int)d->w * (int)d->h; /* cast to avoid short overflow */

	/* far is both the starting 'far' value
	 * and the closest value found. */	
	while (i--) {
		depth= *depths++;
		if ((depth < far) && (depth > near)) {
			far= depth;
		}
	}

	return far == far_real ? FLT_MAX : far;
}

void draw_depth_gpencil(Scene *scene, ARegion *ar, View3D *v3d)
{
	short zbuf= v3d->zbuf;
	RegionView3D *rv3d= ar->regiondata;

	setwinmatrixview3d(ar, v3d, NULL);	/* 0= no pick rect */
	setviewmatrixview3d(scene, v3d, rv3d);	/* note: calls where_is_object for camera... */

	mult_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);
	invert_m4_m4(rv3d->persinv, rv3d->persmat);
	invert_m4_m4(rv3d->viewinv, rv3d->viewmat);

	glClear(GL_DEPTH_BUFFER_BIT);

	glLoadMatrixf(rv3d->viewmat);

	v3d->zbuf= TRUE;
	glEnable(GL_DEPTH_TEST);

	draw_gpencil_view3d(scene, v3d, ar, 1);
	
	v3d->zbuf= zbuf;

}

void draw_depth(Scene *scene, ARegion *ar, View3D *v3d, int (* func)(void *))
{
	RegionView3D *rv3d= ar->regiondata;
	Base *base;
	short zbuf= v3d->zbuf;
	short flag= v3d->flag;
	float glalphaclip= U.glalphaclip;
	int obcenter_dia= U.obcenter_dia;
	/* temp set drawtype to solid */
	
	/* Setting these temporarily is not nice */
	v3d->flag &= ~V3D_SELECT_OUTLINE;
	U.glalphaclip = 0.5; /* not that nice but means we wont zoom into billboards */
	U.obcenter_dia= 0;
	
	setwinmatrixview3d(ar, v3d, NULL);	/* 0= no pick rect */
	setviewmatrixview3d(scene, v3d, rv3d);	/* note: calls where_is_object for camera... */
	
	mult_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);
	invert_m4_m4(rv3d->persinv, rv3d->persmat);
	invert_m4_m4(rv3d->viewinv, rv3d->viewmat);
	
	glClear(GL_DEPTH_BUFFER_BIT);
	
	glLoadMatrixf(rv3d->viewmat);
//	persp(PERSP_STORE);  // store correct view for persp(PERSP_VIEW) calls
	
	if (rv3d->rflag & RV3D_CLIPPING) {
		view3d_set_clipping(rv3d);
	}
	
	v3d->zbuf= TRUE;
	glEnable(GL_DEPTH_TEST);
	
	/* draw set first */
	if (scene->set) {
		Scene *sce_iter;
		for (SETLOOPER(scene->set, sce_iter, base)) {
			if (v3d->lay & base->lay) {
				if (func == NULL || func(base)) {
					draw_object(scene, ar, v3d, base, 0);
					if (base->object->transflag & OB_DUPLI) {
						draw_dupli_objects_color(scene, ar, v3d, base, TH_WIRE);
					}
				}
			}
		}
	}
	
	for (base= scene->base.first; base; base= base->next) {
		if (v3d->lay & base->lay) {
			if (func == NULL || func(base)) {
				/* dupli drawing */
				if (base->object->transflag & OB_DUPLI) {
					draw_dupli_objects(scene, ar, v3d, base);
				}
				draw_object(scene, ar, v3d, base, 0);
			}
		}
	}
	
	/* this isnt that nice, draw xray objects as if they are normal */
	if (	v3d->afterdraw_transp.first ||
			v3d->afterdraw_xray.first || 
			v3d->afterdraw_xraytransp.first
	) {
		View3DAfter *v3da, *next;
		int mask_orig;

		v3d->xray= TRUE;
		
		/* transp materials can change the depth mask, see #21388 */
		glGetIntegerv(GL_DEPTH_WRITEMASK, &mask_orig);


		if (v3d->afterdraw_xray.first || v3d->afterdraw_xraytransp.first) {
			glDepthFunc(GL_ALWAYS); /* always write into the depth bufer, overwriting front z values */
			for (v3da= v3d->afterdraw_xray.first; v3da; v3da= next) {
				next= v3da->next;
				draw_object(scene, ar, v3d, v3da->base, 0);
			}
			glDepthFunc(GL_LEQUAL); /* Now write the depth buffer normally */
		}

		/* draw 3 passes, transp/xray/xraytransp */
		v3d->xray= FALSE;
		v3d->transp= TRUE;
		for (v3da= v3d->afterdraw_transp.first; v3da; v3da= next) {
			next= v3da->next;
			draw_object(scene, ar, v3d, v3da->base, 0);
			BLI_remlink(&v3d->afterdraw_transp, v3da);
			MEM_freeN(v3da);
		}

		v3d->xray= TRUE;
		v3d->transp= FALSE;  
		for (v3da= v3d->afterdraw_xray.first; v3da; v3da= next) {
			next= v3da->next;
			draw_object(scene, ar, v3d, v3da->base, 0);
			BLI_remlink(&v3d->afterdraw_xray, v3da);
			MEM_freeN(v3da);
		}

		v3d->xray= TRUE;
		v3d->transp= TRUE;
		for (v3da= v3d->afterdraw_xraytransp.first; v3da; v3da= next) {
			next= v3da->next;
			draw_object(scene, ar, v3d, v3da->base, 0);
			BLI_remlink(&v3d->afterdraw_xraytransp, v3da);
			MEM_freeN(v3da);
		}

		
		v3d->xray= FALSE;
		v3d->transp= FALSE;

		glDepthMask(mask_orig);
	}
	
	if (rv3d->rflag & RV3D_CLIPPING)
		view3d_clr_clipping();
	
	v3d->zbuf = zbuf;
	if (!v3d->zbuf) glDisable(GL_DEPTH_TEST);

	U.glalphaclip = glalphaclip;
	v3d->flag = flag;
	U.obcenter_dia= obcenter_dia;
}

typedef struct View3DShadow {
	struct View3DShadow *next, *prev;
	GPULamp *lamp;
} View3DShadow;

static void gpu_render_lamp_update(Scene *scene, View3D *v3d, Object *ob, Object *par,
                                   float obmat[][4], ListBase *shadows)
{
	GPULamp *lamp;
	Lamp *la = (Lamp*)ob->data;
	View3DShadow *shadow;
	
	lamp = GPU_lamp_from_blender(scene, ob, par);
	
	if (lamp) {
		GPU_lamp_update(lamp, ob->lay, (ob->restrictflag & OB_RESTRICT_RENDER), obmat);
		GPU_lamp_update_colors(lamp, la->r, la->g, la->b, la->energy);
		
		if ((ob->lay & v3d->lay) && GPU_lamp_has_shadow_buffer(lamp)) {
			shadow= MEM_callocN(sizeof(View3DShadow), "View3DShadow");
			shadow->lamp = lamp;
			BLI_addtail(shadows, shadow);
		}
	}
}

static void gpu_update_lamps_shadows(Scene *scene, View3D *v3d)
{
	ListBase shadows;
	View3DShadow *shadow;
	Scene *sce_iter;
	Base *base;
	Object *ob;
	
	shadows.first= shadows.last= NULL;
	
	/* update lamp transform and gather shadow lamps */
	for (SETLOOPER(scene, sce_iter, base)) {
		ob= base->object;
		
		if (ob->type == OB_LAMP)
			gpu_render_lamp_update(scene, v3d, ob, NULL, ob->obmat, &shadows);
		
		if (ob->transflag & OB_DUPLI) {
			DupliObject *dob;
			ListBase *lb = object_duplilist(scene, ob);
			
			for (dob=lb->first; dob; dob=dob->next)
				if (dob->ob->type==OB_LAMP)
					gpu_render_lamp_update(scene, v3d, dob->ob, ob, dob->mat, &shadows);
			
			free_object_duplilist(lb);
		}
	}
	
	/* render shadows after updating all lamps, nested object_duplilist
		* don't work correct since it's replacing object matrices */
	for (shadow=shadows.first; shadow; shadow=shadow->next) {
		/* this needs to be done better .. */
		float viewmat[4][4], winmat[4][4];
		int drawtype, lay, winsize, flag2=v3d->flag2;
		ARegion ar= {NULL};
		RegionView3D rv3d= {{{0}}};
		
		drawtype= v3d->drawtype;
		lay= v3d->lay;
		
		v3d->drawtype = OB_SOLID;
		v3d->lay &= GPU_lamp_shadow_layer(shadow->lamp);
		v3d->flag2 &= ~V3D_SOLID_TEX;
		v3d->flag2 |= V3D_RENDER_OVERRIDE | V3D_RENDER_SHADOW;
		
		GPU_lamp_shadow_buffer_bind(shadow->lamp, viewmat, &winsize, winmat);

		ar.regiondata= &rv3d;
		ar.regiontype= RGN_TYPE_WINDOW;
		rv3d.persp= RV3D_CAMOB;
		copy_m4_m4(rv3d.winmat, winmat);
		copy_m4_m4(rv3d.viewmat, viewmat);
		invert_m4_m4(rv3d.viewinv, rv3d.viewmat);
		mult_m4_m4m4(rv3d.persmat, rv3d.winmat, rv3d.viewmat);
		invert_m4_m4(rv3d.persinv, rv3d.viewinv);

		ED_view3d_draw_offscreen(scene, v3d, &ar, winsize, winsize, viewmat, winmat);
		GPU_lamp_shadow_buffer_unbind(shadow->lamp);
		
		v3d->drawtype= drawtype;
		v3d->lay= lay;
		v3d->flag2 = flag2;
	}
	
	BLI_freelistN(&shadows);
}

/* *********************** customdata **************** */

CustomDataMask ED_view3d_datamask(Scene *scene, View3D *v3d)
{
	CustomDataMask mask= 0;

	if ( ELEM(v3d->drawtype, OB_TEXTURE, OB_MATERIAL) ||
	     ((v3d->drawtype == OB_SOLID) && (v3d->flag2 & V3D_SOLID_TEX)))
	{
		mask |= CD_MASK_MTFACE | CD_MASK_MCOL;

		if (scene_use_new_shading_nodes(scene)) {
			if (v3d->drawtype == OB_MATERIAL)
				mask |= CD_MASK_ORCO;
		}
		else {
			if (scene->gm.matmode == GAME_MAT_GLSL)
				mask |= CD_MASK_ORCO;
		}
	}

	return mask;
}

CustomDataMask ED_view3d_object_datamask(Scene *scene)
{
	Object *ob= scene->basact ? scene->basact->object : NULL;
	CustomDataMask mask= 0;

	if (ob) {
		/* check if we need tfaces & mcols due to face select or texture paint */
		if (paint_facesel_test(ob) || (ob->mode & OB_MODE_TEXTURE_PAINT)) {
			mask |= CD_MASK_MTFACE | CD_MASK_MCOL;
		}

		/* check if we need mcols due to vertex paint or weightpaint */
		if (ob->mode & OB_MODE_VERTEX_PAINT) {
			mask |= CD_MASK_MCOL;
		}

		if (ob->mode & OB_MODE_WEIGHT_PAINT) {
			mask |= CD_MASK_WEIGHT_MCOL;
		}
	}

	return mask;
}

/* goes over all modes and view3d settings */
CustomDataMask ED_view3d_screen_datamask(bScreen *screen)
{
	Scene *scene= screen->scene;
	CustomDataMask mask = CD_MASK_BAREMESH;
	ScrArea *sa;
	
	/* check if we need tfaces & mcols due to view mode */
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		if (sa->spacetype == SPACE_VIEW3D) {
			mask |= ED_view3d_datamask(scene, (View3D *)sa->spacedata.first);
		}
	}

	mask |= ED_view3d_object_datamask(scene);

	return mask;
}

static void view3d_main_area_setup_view(Scene *scene, View3D *v3d, ARegion *ar, float viewmat[][4], float winmat[][4])
{
	RegionView3D *rv3d= ar->regiondata;

	/* setup window matrices */
	if (winmat)
		copy_m4_m4(rv3d->winmat, winmat);
	else
		setwinmatrixview3d(ar, v3d, NULL); /* NULL= no pickrect */
	
	/* setup view matrix */
	if (viewmat)
		copy_m4_m4(rv3d->viewmat, viewmat);
	else
		setviewmatrixview3d(scene, v3d, rv3d);	/* note: calls where_is_object for camera... */
	
	/* update utilitity matrices */
	mult_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);
	invert_m4_m4(rv3d->persinv, rv3d->persmat);
	invert_m4_m4(rv3d->viewinv, rv3d->viewmat);

	/* calculate pixelsize factor once, is used for lamps and obcenters */
	{
		/* note:  '1.0f / len_v3(v1)'  replaced  'len_v3(rv3d->viewmat[0])'
		 * because of float point precision problems at large values [#23908] */
		float v1[3], v2[3];
		float len1, len2;

		v1[0]= rv3d->persmat[0][0];
		v1[1]= rv3d->persmat[1][0];
		v1[2]= rv3d->persmat[2][0];

		v2[0]= rv3d->persmat[0][1];
		v2[1]= rv3d->persmat[1][1];
		v2[2]= rv3d->persmat[2][1];
		
		len1= 1.0f / len_v3(v1);
		len2= 1.0f / len_v3(v2);

		rv3d->pixsize = (2.0f * MAX2(len1, len2)) / (float)MAX2(ar->winx, ar->winy);
	}

	/* set for opengl */
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(rv3d->winmat);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(rv3d->viewmat);
}

void ED_view3d_draw_offscreen(Scene *scene, View3D *v3d, ARegion *ar,
                              int winx, int winy, float viewmat[][4], float winmat[][4])
{
	RegionView3D *rv3d= ar->regiondata;
	Base *base;
	float backcol[3];
	int bwinx, bwiny;
	rcti brect;

	glPushMatrix();

	/* set temporary new size */
	bwinx= ar->winx;
	bwiny= ar->winy;
	brect= ar->winrct;
	
	ar->winx= winx;
	ar->winy= winy;	
	ar->winrct.xmin= 0;
	ar->winrct.ymin= 0;
	ar->winrct.xmax= winx;
	ar->winrct.ymax= winy;
	
	
	/* set flags */
	G.f |= G_RENDER_OGL;

	/* free images which can have changed on frame-change
	 * warning! can be slow so only free animated images - campbell */
	GPU_free_images_anim();
	
	/* shadow buffers, before we setup matrices */
	if (draw_glsl_material(scene, NULL, v3d, v3d->drawtype))
		gpu_update_lamps_shadows(scene, v3d);

	/* set background color, fallback on the view background color */
	if (scene->world) {
		if (scene->r.color_mgt_flag & R_COLOR_MANAGEMENT)
			linearrgb_to_srgb_v3_v3(backcol, &scene->world->horr);
		else
			copy_v3_v3(backcol, &scene->world->horr);
		glClearColor(backcol[0], backcol[1], backcol[2], 0.0);
	}
	else {
		UI_ThemeClearColor(TH_BACK);	
	}

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	/* setup view matrices */
	view3d_main_area_setup_view(scene, v3d, ar, viewmat, winmat);

	if (rv3d->rflag & RV3D_CLIPPING)
		view3d_draw_clipping(rv3d);

	/* set zbuffer */
	if (v3d->drawtype > OB_WIRE) {
		v3d->zbuf= TRUE;
		glEnable(GL_DEPTH_TEST);
	}
	else
		v3d->zbuf= FALSE;

	if (rv3d->rflag & RV3D_CLIPPING)
		view3d_set_clipping(rv3d);

	/* draw set first */
	if (scene->set) {
		Scene *sce_iter;
		for (SETLOOPER(scene->set, sce_iter, base)) {
			if (v3d->lay & base->lay) {
				UI_ThemeColorBlend(TH_WIRE, TH_BACK, 0.6f);
				draw_object(scene, ar, v3d, base, DRAW_CONSTCOLOR|DRAW_SCENESET);
				
				if (base->object->transflag & OB_DUPLI)
					draw_dupli_objects_color(scene, ar, v3d, base, TH_WIRE);
			}
		}
	}
	
	/* then draw not selected and the duplis, but skip editmode object */
	for (base= scene->base.first; base; base= base->next) {
		if (v3d->lay & base->lay) {
			/* dupli drawing */
			if (base->object->transflag & OB_DUPLI)
				draw_dupli_objects(scene, ar, v3d, base);

			draw_object(scene, ar, v3d, base, 0);
		}
	}

	/* must be before xray draw which clears the depth buffer */
	if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
	draw_gpencil_view3d(scene, v3d, ar, 1);
	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);

	/* transp and X-ray afterdraw stuff */
	if (v3d->afterdraw_transp.first)		view3d_draw_transp(scene, ar, v3d);
	if (v3d->afterdraw_xray.first)		view3d_draw_xray(scene, ar, v3d, 1);	// clears zbuffer if it is used!
	if (v3d->afterdraw_xraytransp.first)	view3d_draw_xraytransp(scene, ar, v3d, 1);

	if (rv3d->rflag & RV3D_CLIPPING)
		view3d_clr_clipping();

	/* cleanup */
	if (v3d->zbuf) {
		v3d->zbuf= FALSE;
		glDisable(GL_DEPTH_TEST);
	}

	/* draw grease-pencil stuff */
	ED_region_pixelspace(ar);

	/* draw grease-pencil stuff - needed to get paint-buffer shown too (since it's 2D) */
	draw_gpencil_view3d(scene, v3d, ar, 0);

	/* freeing the images again here could be done after the operator runs, leaving for now */
	GPU_free_images_anim();

	/* restore size */
	ar->winx= bwinx;
	ar->winy= bwiny;
	ar->winrct = brect;

	glPopMatrix();

	// XXX, without this the sequencer flickers with opengl draw enabled, need to find out why - campbell
	glColor4ub(255, 255, 255, 255);

	G.f &= ~G_RENDER_OGL;
}

/* utility func for ED_view3d_draw_offscreen */
ImBuf *ED_view3d_draw_offscreen_imbuf(Scene *scene, View3D *v3d, ARegion *ar,
                                      int sizex, int sizey, unsigned int flag, char err_out[256])
{
	RegionView3D *rv3d= ar->regiondata;
	ImBuf *ibuf;
	GPUOffScreen *ofs;
	
	/* state changes make normal drawing go weird otherwise */
	glPushAttrib(GL_LIGHTING_BIT);

	/* bind */
	ofs= GPU_offscreen_create(sizex, sizey, err_out);
	if (ofs == NULL)
		return NULL;

	GPU_offscreen_bind(ofs);

	/* render 3d view */
	if (rv3d->persp==RV3D_CAMOB && v3d->camera) {
		CameraParams params;

		camera_params_init(&params);
		camera_params_from_object(&params, v3d->camera);
		camera_params_compute_viewplane(&params, sizex, sizey, scene->r.xasp, scene->r.yasp);
		camera_params_compute_matrix(&params);

		ED_view3d_draw_offscreen(scene, v3d, ar, sizex, sizey, NULL, params.winmat);
	}
	else {
		ED_view3d_draw_offscreen(scene, v3d, ar, sizex, sizey, NULL, NULL);
	}

	/* read in pixels & stamp */
	ibuf= IMB_allocImBuf(sizex, sizey, 32, flag);

	if (ibuf->rect_float)
		GPU_offscreen_read_pixels(ofs, GL_FLOAT, ibuf->rect_float);
	else if (ibuf->rect)
		GPU_offscreen_read_pixels(ofs, GL_UNSIGNED_BYTE, ibuf->rect);
	
	//if ((scene->r.stamp & R_STAMP_ALL) && (scene->r.stamp & R_STAMP_DRAW))
	//	BKE_stamp_buf(scene, NULL, rr->rectf, rr->rectx, rr->recty, 4);

	/* unbind */
	GPU_offscreen_unbind(ofs);
	GPU_offscreen_free(ofs);

	glPopAttrib();
	
	if (ibuf->rect_float && ibuf->rect)
		IMB_rect_from_float(ibuf);
	
	return ibuf;
}

/* creates own 3d views, used by the sequencer */
ImBuf *ED_view3d_draw_offscreen_imbuf_simple(Scene *scene, Object *camera, int width, int height,
                                             unsigned int flag, int drawtype, char err_out[256])
{
	View3D v3d= {NULL};
	ARegion ar= {NULL};
	RegionView3D rv3d= {{{0}}};

	/* connect data */
	v3d.regionbase.first= v3d.regionbase.last= &ar;
	ar.regiondata= &rv3d;
	ar.regiontype= RGN_TYPE_WINDOW;

	v3d.camera= camera;
	v3d.lay= scene->lay;
	v3d.drawtype = drawtype;
	v3d.flag2 = V3D_RENDER_OVERRIDE;

	rv3d.persp= RV3D_CAMOB;

	copy_m4_m4(rv3d.viewinv, v3d.camera->obmat);
	normalize_m4(rv3d.viewinv);
	invert_m4_m4(rv3d.viewmat, rv3d.viewinv);

	{
		CameraParams params;

		camera_params_init(&params);
		camera_params_from_object(&params, v3d.camera);
		camera_params_compute_viewplane(&params, width, height, scene->r.xasp, scene->r.yasp);
		camera_params_compute_matrix(&params);

		copy_m4_m4(rv3d.winmat, params.winmat);
		v3d.near= params.clipsta;
		v3d.far= params.clipend;
		v3d.lens= params.lens;
	}

	mult_m4_m4m4(rv3d.persmat, rv3d.winmat, rv3d.viewmat);
	invert_m4_m4(rv3d.persinv, rv3d.viewinv);

	return ED_view3d_draw_offscreen_imbuf(scene, &v3d, &ar, width, height, flag, err_out);

	// seq_view3d_cb(scene, cfra, render_size, seqrectx, seqrecty);
}


/* NOTE: the info that this uses is updated in ED_refresh_viewport_fps(), 
 * which currently gets called during SCREEN_OT_animation_step.
 */
static void draw_viewport_fps(Scene *scene, ARegion *ar)
{
	ScreenFrameRateInfo *fpsi= scene->fps_info;
	float fps;
	char printable[16];
	int i, tot;
	
	if (!fpsi || !fpsi->lredrawtime || !fpsi->redrawtime)
		return;
	
	printable[0] = '\0';
	
#if 0
	/* this is too simple, better do an average */
	fps = (float)(1.0/(fpsi->lredrawtime-fpsi->redrawtime))
#else
	fpsi->redrawtimes_fps[fpsi->redrawtime_index] = (float)(1.0/(fpsi->lredrawtime-fpsi->redrawtime));
	
	for (i=0, tot=0, fps=0.0f ; i < REDRAW_FRAME_AVERAGE ; i++) {
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

	/* is this more then half a frame behind? */
	if (fps+0.5f < (float)(FPS)) {
		UI_ThemeColor(TH_REDALERT);
		BLI_snprintf(printable, sizeof(printable), "fps: %.2f", fps);
	} 
	else {
		UI_ThemeColor(TH_TEXT_HI);
		BLI_snprintf(printable, sizeof(printable), "fps: %i", (int)(fps+0.5f));
	}
	
	BLF_draw_default_ascii(22,  ar->winy-17, 0.0f, printable, sizeof(printable));
}

static int view3d_main_area_draw_engine(const bContext *C, ARegion *ar)
{
	Scene *scene= CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	RenderEngineType *type;

	if (!rv3d->render_engine) {
		type= RE_engines_find(scene->r.engine);

		if (!(type->view_update && type->view_draw))
			return 0;

		rv3d->render_engine= RE_engine_create(type);
		type->view_update(rv3d->render_engine, C);
	}

	view3d_main_area_setup_view(scene, v3d, ar, NULL, NULL);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	ED_region_pixelspace(ar);

	type= rv3d->render_engine->type;
	type->view_draw(rv3d->render_engine, C);

	return 1;
}

static void view3d_main_area_draw_engine_info(RegionView3D *rv3d, ARegion *ar)
{
	if (!rv3d->render_engine || !rv3d->render_engine->text)
		return;

	ED_region_info_draw(ar, rv3d->render_engine->text, 1, 0.25);
}

/* warning: this function has duplicate drawing in ED_view3d_draw_offscreen() */
static void view3d_main_area_draw_objects(const bContext *C, ARegion *ar, const char **grid_unit)
{
	Scene *scene= CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	Base *base;
	float backcol[3];
	unsigned int lay_used;

	/* shadow buffers, before we setup matrices */
	if (draw_glsl_material(scene, NULL, v3d, v3d->drawtype))
		gpu_update_lamps_shadows(scene, v3d);
	
	/* reset default OpenGL lights if needed (i.e. after preferences have been altered) */
	if (rv3d->rflag & RV3D_GPULIGHT_UPDATE) {
		rv3d->rflag &= ~RV3D_GPULIGHT_UPDATE;
		GPU_default_lights();
	}

	/* clear background */
	if ((v3d->flag2 & V3D_RENDER_OVERRIDE) && scene->world) {
		if (scene->r.color_mgt_flag & R_COLOR_MANAGEMENT)
			linearrgb_to_srgb_v3_v3(backcol, &scene->world->horr);
		else
			copy_v3_v3(backcol, &scene->world->horr);
		glClearColor(backcol[0], backcol[1], backcol[2], 0.0);
	}
	else
		UI_ThemeClearColor(TH_BACK);

	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	
	/* setup view matrices */
	view3d_main_area_setup_view(scene, v3d, ar, NULL, NULL);

	ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);

	if (rv3d->rflag & RV3D_CLIPPING)
		view3d_draw_clipping(rv3d);
	
	/* set zbuffer after we draw clipping region */
	if (v3d->drawtype > OB_WIRE) {
		v3d->zbuf= TRUE;
		glEnable(GL_DEPTH_TEST);
	}
	else
		v3d->zbuf= FALSE;

	/* enables anti-aliasing for 3D view drawing */
	/*if (!(U.gameflags & USER_DISABLE_AA))
		glEnable(GL_MULTISAMPLE_ARB);*/
	
	// needs to be done always, gridview is adjusted in drawgrid() now
	rv3d->gridview= v3d->grid;

	if ((rv3d->view == RV3D_VIEW_USER) || (rv3d->persp != RV3D_ORTHO)) {
		if ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
			drawfloor(scene, v3d, grid_unit);
		}
		if (rv3d->persp==RV3D_CAMOB) {
			if (scene->world) {
				if (scene->world->mode & WO_STARS) {
					RE_make_stars(NULL, scene, star_stuff_init_func, star_stuff_vertex_func,
								  star_stuff_term_func);
				}
			}
			if ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
				if (v3d->flag & V3D_DISPBGPICS) draw_bgpic(scene, ar, v3d);
			}
		}
	}
	else {
		if ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
			ED_region_pixelspace(ar);
			drawgrid(&scene->unit, ar, v3d, grid_unit);
			/* XXX make function? replaces persp(1) */
			glMatrixMode(GL_PROJECTION);
			glLoadMatrixf(rv3d->winmat);
			glMatrixMode(GL_MODELVIEW);
			glLoadMatrixf(rv3d->viewmat);

			if (v3d->flag & V3D_DISPBGPICS) {
				draw_bgpic(scene, ar, v3d);
			}
		}
	}
	
	if (rv3d->rflag & RV3D_CLIPPING)
		view3d_set_clipping(rv3d);

	/* draw set first */
	if (scene->set) {
		Scene *sce_iter;
		for (SETLOOPER(scene->set, sce_iter, base)) {
			
			if (v3d->lay & base->lay) {
				
				UI_ThemeColorBlend(TH_WIRE, TH_BACK, 0.6f);
				draw_object(scene, ar, v3d, base, DRAW_CONSTCOLOR|DRAW_SCENESET);
				
				if (base->object->transflag & OB_DUPLI) {
					draw_dupli_objects_color(scene, ar, v3d, base, TH_WIRE);
				}
			}
		}
		
		/* Transp and X-ray afterdraw stuff for sets is done later */
	}

	lay_used= 0;

	/* then draw not selected and the duplis, but skip editmode object */
	for (base= scene->base.first; base; base= base->next) {
		lay_used |= base->lay & ((1<<20)-1);

		if (v3d->lay & base->lay) {
			
			/* dupli drawing */
			if (base->object->transflag & OB_DUPLI) {
				draw_dupli_objects(scene, ar, v3d, base);
			}
			if ((base->flag & SELECT)==0) {
				if (base->object!=scene->obedit)
					draw_object(scene, ar, v3d, base, 0);
			}
		}
	}

	if (v3d->lay_used != lay_used) { /* happens when loading old files or loading with UI load */
		/* find header and force tag redraw */
		ScrArea *sa= CTX_wm_area(C);
		ARegion *ar_header= BKE_area_find_region_type(sa, RGN_TYPE_HEADER);
		ED_region_tag_redraw(ar_header); /* can be NULL */
		v3d->lay_used= lay_used;
	}

	/* draw selected and editmode */
	for (base= scene->base.first; base; base= base->next) {
		if (v3d->lay & base->lay) {
			if (base->object==scene->obedit || ( base->flag & SELECT) ) 
				draw_object(scene, ar, v3d, base, 0);
		}
	}

//	REEB_draw();

	if ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
		/* must be before xray draw which clears the depth buffer */
		if (v3d->zbuf) glDisable(GL_DEPTH_TEST);
		draw_gpencil_view3d(scene, v3d, ar, 1);
		if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
	}

	/* Transp and X-ray afterdraw stuff */
	if (v3d->afterdraw_transp.first)		view3d_draw_transp(scene, ar, v3d);
	if (v3d->afterdraw_xray.first)		view3d_draw_xray(scene, ar, v3d, 1);	// clears zbuffer if it is used!
	if (v3d->afterdraw_xraytransp.first)	view3d_draw_xraytransp(scene, ar, v3d, 1);
	
	ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);

	if (rv3d->rflag & RV3D_CLIPPING)
		view3d_clr_clipping();
	
	BIF_draw_manipulator(C);
	
	/* Disable back anti-aliasing */
	/*if (!(U.gameflags & USER_DISABLE_AA))
		glDisable(GL_MULTISAMPLE_ARB);*/

	if (v3d->zbuf) {
		v3d->zbuf= FALSE;
		glDisable(GL_DEPTH_TEST);
	}

	if ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
		BDR_drawSketch(C);
	}

	if ((U.ndof_flag & NDOF_SHOW_GUIDE) && (rv3d->viewlock != RV3D_LOCKED) && (rv3d->persp != RV3D_CAMOB))
		// TODO: draw something else (but not this) during fly mode
		draw_rotation_guide(rv3d);

}

static void view3d_main_area_draw_info(const bContext *C, ARegion *ar, const char *grid_unit)
{
	Scene *scene= CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	bScreen *screen= CTX_wm_screen(C);

	Object *ob;

	if (rv3d->persp==RV3D_CAMOB)
		drawviewborder(scene, ar, v3d);

	if ((v3d->flag2 & V3D_RENDER_OVERRIDE)==0) {
		/* draw grease-pencil stuff - needed to get paint-buffer shown too (since it's 2D) */
	//	if (v3d->flag2 & V3D_DISPGP)
			draw_gpencil_view3d(scene, v3d, ar, 0);

		drawcursor(scene, ar, v3d);
	}
	
	if (U.uiflag & USER_SHOW_ROTVIEWICON)
		draw_view_axis(rv3d);
	else	
		draw_view_icon(rv3d);
	
	ob= OBACT;
	if (U.uiflag & USER_DRAWVIEWINFO)
		draw_selected_name(scene, ob);

	if (rv3d->render_engine) {
		view3d_main_area_draw_engine_info(rv3d, ar);
		return;
	}

	if ((U.uiflag & USER_SHOW_FPS) && screen->animtimer) {
		draw_viewport_fps(scene, ar);
	}
	else if (U.uiflag & USER_SHOW_VIEWPORTNAME) {
		draw_viewport_name(ar, v3d);
	}

	if (grid_unit) { /* draw below the viewport name */
		char numstr[32]= "";

		UI_ThemeColor(TH_TEXT_HI);
		if (v3d->grid != 1.0f) {
			BLI_snprintf(numstr, sizeof(numstr), "%s x %.4g", grid_unit, v3d->grid);
		}

		BLF_draw_default_ascii(22,  ar->winy-(USER_SHOW_VIEWPORTNAME?40:20), 0.0f,
		                       numstr[0] ? numstr : grid_unit, sizeof(numstr));
	}
}

void view3d_main_area_draw(const bContext *C, ARegion *ar)
{
	View3D *v3d = CTX_wm_view3d(C);
	const char *grid_unit= NULL;

	/* draw viewport using external renderer? */
	if (!(v3d->drawtype == OB_RENDER && view3d_main_area_draw_engine(C, ar))) {
		/* draw viewport using opengl */
		view3d_main_area_draw_objects(C, ar, &grid_unit);
		ED_region_pixelspace(ar);
	}
	
	view3d_main_area_draw_info(C, ar, grid_unit);

	v3d->flag |= V3D_INVALID_BACKBUF;
}


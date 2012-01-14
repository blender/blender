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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins, Tom Musgrove.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/editors/sculpt_paint/paint_stroke.c
 *  \ingroup edsculpt
 */


#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "RNA_access.h"

#include "BKE_context.h"
#include "BKE_paint.h"
#include "BKE_brush.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "paint_intern.h"
/* still needed for sculpt_stroke_get_location, should be
   removed eventually (TODO) */
#include "sculpt_intern.h"

#include <float.h>
#include <math.h>

typedef struct PaintStroke {
	void *mode_data;
	void *smooth_stroke_cursor;
	wmTimer *timer;

	/* Cached values */
	ViewContext vc;
	bglMats mats;
	Brush *brush;

	float last_mouse_position[2];

	/* Set whether any stroke step has yet occurred
	   e.g. in sculpt mode, stroke doesn't start until cursor
	   passes over the mesh */
	int stroke_started;
	/* event that started stroke, for modal() return */
	int event_type;
	
	StrokeGetLocation get_location;
	StrokeTestStart test_start;
	StrokeUpdateStep update_step;
	StrokeDone done;
} PaintStroke;

/*** Cursor ***/
static void paint_draw_smooth_stroke(bContext *C, int x, int y, void *customdata) 
{
	Brush *brush = paint_brush(paint_get_active(CTX_data_scene(C)));
	PaintStroke *stroke = customdata;

	glColor4ubv(paint_get_active(CTX_data_scene(C))->paint_cursor_col);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);

	if(stroke && brush && (brush->flag & BRUSH_SMOOTH_STROKE)) {
		ARegion *ar = CTX_wm_region(C);
		sdrawline(x, y, (int)stroke->last_mouse_position[0] - ar->winrct.xmin,
			  (int)stroke->last_mouse_position[1] - ar->winrct.ymin);
	}

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

typedef struct Snapshot {
	float size[3];
	float ofs[3];
	float rot;
	int brush_size;
	int winx;
	int winy;
	int brush_map_mode;
	int curve_changed_timestamp;
} Snapshot;

static int same_snap(Snapshot* snap, Brush* brush, ViewContext* vc)
{
	MTex* mtex = &brush->mtex;

	return (((mtex->tex) &&
			 equals_v3v3(mtex->ofs, snap->ofs) &&
			 equals_v3v3(mtex->size, snap->size) &&
			 mtex->rot == snap->rot) &&

			/* make brush smaller shouldn't cause a resample */
			((mtex->brush_map_mode == MTEX_MAP_MODE_FIXED &&
			  (brush_size(vc->scene, brush) <= snap->brush_size)) ||
			 (brush_size(vc->scene, brush) == snap->brush_size)) &&

			(mtex->brush_map_mode == snap->brush_map_mode) &&
			(vc->ar->winx == snap->winx) &&
			(vc->ar->winy == snap->winy));
}

static void make_snap(Snapshot* snap, Brush* brush, ViewContext* vc)
{
	if (brush->mtex.tex) {
		snap->brush_map_mode = brush->mtex.brush_map_mode;
		copy_v3_v3(snap->ofs, brush->mtex.ofs);
		copy_v3_v3(snap->size, brush->mtex.size);
		snap->rot = brush->mtex.rot;
	}
	else {
		snap->brush_map_mode = -1;
		snap->ofs[0]= snap->ofs[1]= snap->ofs[2]= -1;
		snap->size[0]= snap->size[1]= snap->size[2]= -1;
		snap->rot = -1;
	}

	snap->brush_size = brush_size(vc->scene, brush);
	snap->winx = vc->ar->winx;
	snap->winy = vc->ar->winy;
}

static int load_tex(Sculpt *sd, Brush* br, ViewContext* vc)
{
	static GLuint overlay_texture = 0;
	static int init = 0;
	static int tex_changed_timestamp = -1;
	static int curve_changed_timestamp = -1;
	static Snapshot snap;
	static int old_size = -1;

	GLubyte* buffer = NULL;

	int size;
	int j;
	int refresh;

#ifndef _OPENMP
	(void)sd; /* quied unused warning */
#endif
	
	if (br->mtex.brush_map_mode == MTEX_MAP_MODE_TILED && !br->mtex.tex) return 0;
	
	refresh = 
		!overlay_texture ||
		(br->mtex.tex && 
		    (!br->mtex.tex->preview ||
		      br->mtex.tex->preview->changed_timestamp[0] != tex_changed_timestamp)) ||
		!br->curve ||
		br->curve->changed_timestamp != curve_changed_timestamp ||
		!same_snap(&snap, br, vc);

	if (refresh) {
		if (br->mtex.tex && br->mtex.tex->preview)
			tex_changed_timestamp = br->mtex.tex->preview->changed_timestamp[0];

		if (br->curve)
			curve_changed_timestamp = br->curve->changed_timestamp;

		make_snap(&snap, br, vc);

		if (br->mtex.brush_map_mode == MTEX_MAP_MODE_FIXED) {
			int s = brush_size(vc->scene, br);
			int r = 1;

			for (s >>= 1; s > 0; s >>= 1)
				r++;

			size = (1<<r);

			if (size < 256)
				size = 256;

			if (size < old_size)
				size = old_size;
		}
		else
			size = 512;

		if (old_size != size) {
			if (overlay_texture) {
				glDeleteTextures(1, &overlay_texture);
				overlay_texture = 0;
			}

			init = 0;

			old_size = size;
		}

		buffer = MEM_mallocN(sizeof(GLubyte)*size*size, "load_tex");

		#pragma omp parallel for schedule(static) if (sd->flags & SCULPT_USE_OPENMP)
		for (j= 0; j < size; j++) {
			int i;
			float y;
			float len;

			for (i= 0; i < size; i++) {

				// largely duplicated from tex_strength

				const float rotation = -br->mtex.rot;
				float radius = brush_size(vc->scene, br);
				int index = j*size + i;
				float x;
				float avg;

				x = (float)i/size;
				y = (float)j/size;

				x -= 0.5f;
				y -= 0.5f;

				if (br->mtex.brush_map_mode == MTEX_MAP_MODE_TILED) {
					x *= vc->ar->winx / radius;
					y *= vc->ar->winy / radius;
				}
				else {
					x *= 2;
					y *= 2;
				}

				len = sqrtf(x*x + y*y);

				if ((br->mtex.brush_map_mode == MTEX_MAP_MODE_TILED) || len <= 1) {
					/* it is probably worth optimizing for those cases where 
					   the texture is not rotated by skipping the calls to
					   atan2, sqrtf, sin, and cos. */
					if (br->mtex.tex && (rotation > 0.001f || rotation < -0.001f)) {
						const float angle    = atan2f(y, x) + rotation;

						x = len * cosf(angle);
						y = len * sinf(angle);
					}

					x *= br->mtex.size[0];
					y *= br->mtex.size[1];

					x += br->mtex.ofs[0];
					y += br->mtex.ofs[1];

					avg = br->mtex.tex ? paint_get_tex_pixel(br, x, y) : 1;

					avg += br->texture_sample_bias;

					if (br->mtex.brush_map_mode == MTEX_MAP_MODE_FIXED)
						avg *= brush_curve_strength(br, len, 1); /* Falloff curve */

					buffer[index] = 255 - (GLubyte)(255*avg);
				}
				else {
					buffer[index] = 0;
				}
			}
		}

		if (!overlay_texture)
			glGenTextures(1, &overlay_texture);
	}
	else {
		size= old_size;
	}

	glBindTexture(GL_TEXTURE_2D, overlay_texture);

	if (refresh) {
		if (!init) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, size, size, 0, GL_ALPHA, GL_UNSIGNED_BYTE, buffer);
			init = 1;
		}
		else {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_ALPHA, GL_UNSIGNED_BYTE, buffer);
		}

		if (buffer)
			MEM_freeN(buffer);
	}

	glEnable(GL_TEXTURE_2D);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (br->mtex.brush_map_mode == MTEX_MAP_MODE_FIXED) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	}

	return 1;
}

static int project_brush_radius(RegionView3D* rv3d, float radius, float location[3], bglMats* mats)
{
	float view[3], nonortho[3], ortho[3], offset[3], p1[2], p2[2];

	ED_view3d_global_to_vector(rv3d, location, view);

	// create a vector that is not orthogonal to view

	if (fabsf(view[0]) < 0.1f) {
		nonortho[0] = view[0] + 1.0f;
		nonortho[1] = view[1];
		nonortho[2] = view[2];
	}
	else if (fabsf(view[1]) < 0.1f) {
		nonortho[0] = view[0];
		nonortho[1] = view[1] + 1.0f;
		nonortho[2] = view[2];
	}
	else {
		nonortho[0] = view[0];
		nonortho[1] = view[1];
		nonortho[2] = view[2] + 1.0f;
	}

	// get a vector in the plane of the view
	cross_v3_v3v3(ortho, nonortho, view);
	normalize_v3(ortho);

	// make a point on the surface of the brush tagent to the view
	mul_v3_fl(ortho, radius);
	add_v3_v3v3(offset, location, ortho);

	// project the center of the brush, and the tagent point to the view onto the screen
	projectf(mats, location, p1);
	projectf(mats, offset, p2);

	// the distance between these points is the size of the projected brush in pixels
	return len_v2v2(p1, p2);
}

static int sculpt_get_brush_geometry(bContext* C, int x, int y, int* pixel_radius,
			      float location[3])
{
	struct PaintStroke *stroke;
	const Scene *scene = CTX_data_scene(C);
	float window[2];
	int hit;

	stroke = paint_stroke_new(C, NULL, NULL, NULL, NULL, 0);

	window[0] = x + stroke->vc.ar->winrct.xmin;
	window[1] = y + stroke->vc.ar->winrct.ymin;

	if(stroke->vc.obact->sculpt && stroke->vc.obact->sculpt->pbvh &&
	   sculpt_stroke_get_location(C, stroke, location, window)) {
		*pixel_radius = project_brush_radius(stroke->vc.rv3d,
						     brush_unprojected_radius(scene, stroke->brush),
						     location, &stroke->mats);

		if (*pixel_radius == 0)
			*pixel_radius = brush_size(scene, stroke->brush);

		mul_m4_v3(stroke->vc.obact->obmat, location);

		hit = 1;
	}
	else {
		Sculpt* sd    = CTX_data_tool_settings(C)->sculpt;
		Brush*  brush = paint_brush(&sd->paint);

		*pixel_radius = brush_size(scene, brush);
		hit = 0;
	}

	paint_stroke_free(stroke);

	return hit;
}

/* Draw an overlay that shows what effect the brush's texture will
   have on brush strength */
/* TODO: sculpt only for now */
static void paint_draw_alpha_overlay(Sculpt *sd, Brush *brush,
				     ViewContext *vc, int x, int y)
{
	rctf quad;

	/* check for overlay mode */
	if(!(brush->flag & BRUSH_TEXTURE_OVERLAY) ||
	   !(ELEM(brush->mtex.brush_map_mode, MTEX_MAP_MODE_FIXED, MTEX_MAP_MODE_TILED)))
		return;

	/* save lots of GL state
	   TODO: check on whether all of these are needed? */
	glPushAttrib(GL_COLOR_BUFFER_BIT|
	             GL_CURRENT_BIT|
	             GL_DEPTH_BUFFER_BIT|
	             GL_ENABLE_BIT|
	             GL_LINE_BIT|
	             GL_POLYGON_BIT|
	             GL_STENCIL_BUFFER_BIT|
	             GL_TRANSFORM_BIT|
	             GL_VIEWPORT_BIT|
	             GL_TEXTURE_BIT);

	if(load_tex(sd, brush, vc)) {
		glEnable(GL_BLEND);

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_FALSE);
		glDepthFunc(GL_ALWAYS);

		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		glLoadIdentity();

		if(brush->mtex.brush_map_mode == MTEX_MAP_MODE_FIXED) {
			/* brush rotation */
			glTranslatef(0.5, 0.5, 0);
			glRotatef((double)RAD2DEGF((brush->flag & BRUSH_RAKE) ?
			                           sd->last_angle : sd->special_rotation),
			                           0.0, 0.0, 1.0);
			glTranslatef(-0.5f, -0.5f, 0);

			/* scale based on tablet pressure */
			if(sd->draw_pressure && brush_use_size_pressure(vc->scene, brush)) {
				glTranslatef(0.5f, 0.5f, 0);
				glScalef(1.0f/sd->pressure_value, 1.0f/sd->pressure_value, 1);
				glTranslatef(-0.5f, -0.5f, 0);
			}

			if(sd->draw_anchored) {
				const float *aim = sd->anchored_initial_mouse;
				const rcti *win = &vc->ar->winrct;
				quad.xmin = aim[0]-sd->anchored_size - win->xmin;
				quad.ymin = aim[1]-sd->anchored_size - win->ymin;
				quad.xmax = aim[0]+sd->anchored_size - win->xmin;
				quad.ymax = aim[1]+sd->anchored_size - win->ymin;
			}
			else {
				const int radius= brush_size(vc->scene, brush);
				quad.xmin = x - radius;
				quad.ymin = y - radius;
				quad.xmax = x + radius;
				quad.ymax = y + radius;
			}
		}
		else {
			quad.xmin = 0;
			quad.ymin = 0;
			quad.xmax = vc->ar->winrct.xmax - vc->ar->winrct.xmin;
			quad.ymax = vc->ar->winrct.ymax - vc->ar->winrct.ymin;
		}

		/* set quad color */
		glColor4f(U.sculpt_paint_overlay_col[0],
		          U.sculpt_paint_overlay_col[1],
		          U.sculpt_paint_overlay_col[2],
		          brush->texture_overlay_alpha / 100.0f);

		/* draw textured quad */
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2f(quad.xmin, quad.ymin);
		glTexCoord2f(1, 0);
		glVertex2f(quad.xmax, quad.ymin);
		glTexCoord2f(1, 1);
		glVertex2f(quad.xmax, quad.ymax);
		glTexCoord2f(0, 1);
		glVertex2f(quad.xmin, quad.ymax);
		glEnd();

		glPopMatrix();
	}

	glPopAttrib();
}

/* Special actions taken when paint cursor goes over mesh */
/* TODO: sculpt only for now */
static void paint_cursor_on_hit(Sculpt *sd, Brush *brush, ViewContext *vc,
				float location[3], float *visual_strength)
{
	float unprojected_radius, projected_radius;

	/* TODO: check whether this should really only be done when
	   brush is over mesh? */
	if(sd->draw_pressure && brush_use_alpha_pressure(vc->scene, brush))
		(*visual_strength) *= sd->pressure_value;

	if(sd->draw_anchored)
		projected_radius = sd->anchored_size;
	else {
		if(brush->flag & BRUSH_ANCHORED)
			projected_radius = 8;
		else
			projected_radius = brush_size(vc->scene, brush);
	}
	unprojected_radius = paint_calc_object_space_radius(vc, location,
							    projected_radius);

	if(sd->draw_pressure && brush_use_size_pressure(vc->scene, brush))
		unprojected_radius *= sd->pressure_value;

	if(!brush_use_locked_size(vc->scene, brush))
		brush_set_unprojected_radius(vc->scene, brush, unprojected_radius);
}

static void paint_draw_cursor(bContext *C, int x, int y, void *UNUSED(unused))
{
	Scene *scene = CTX_data_scene(C);
	Paint *paint = paint_get_active(scene);
	Brush *brush = paint_brush(paint);
	ViewContext vc;
	float final_radius;
	float translation[2];
	float outline_alpha, *outline_col;
	
	/* set various defaults */
	translation[0] = x;
	translation[1] = y;
	outline_alpha = 0.5;
	outline_col = brush->add_col;
	final_radius = brush_size(scene, brush);

	/* check that brush drawing is enabled */
	if(!(paint->flags & PAINT_SHOW_BRUSH))
		return;

	/* can't use stroke vc here because this will be called during
	   mouse over too, not just during a stroke */	   
	view3d_set_viewcontext(C, &vc);

	/* TODO: as sculpt and other paint modes are unified, this
	   special mode of drawing will go away */
	if(vc.obact->sculpt) {
		Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
		float location[3];
		int pixel_radius, hit;
		const float root_alpha = brush_alpha(scene, brush);
		float visual_strength = root_alpha*root_alpha;
		const float min_alpha = 0.20f;
		const float max_alpha = 0.80f;

		/* this is probably here so that rake takes into
		   account the brush movements before the stroke
		   starts, but this doesn't really belong in draw code
		   (TODO) */
		{
			const float u = 0.5f;
			const float v = 1 - u;
			const float r = 20;

			const float dx = sd->last_x - x;
			const float dy = sd->last_y - y;

			if(dx*dx + dy*dy >= r*r) {
				sd->last_angle = atan2(dx, dy);

				sd->last_x = u*sd->last_x + v*x;
				sd->last_y = u*sd->last_y + v*y;
			}
		}

		/* test if brush is over the mesh */
		hit = sculpt_get_brush_geometry(C, x, y, &pixel_radius, location);

		/* draw overlay */
		paint_draw_alpha_overlay(sd, brush, &vc, x, y);

		if(brush_use_locked_size(scene, brush))
			brush_set_size(scene, brush, pixel_radius);

		/* check if brush is subtracting, use different color then */
		/* TODO: no way currently to know state of pen flip or
		   invert key modifier without starting a stroke */
		if((!(brush->flag & BRUSH_INVERTED) ^
		    !(brush->flag & BRUSH_DIR_IN)) &&
		   ELEM5(brush->sculpt_tool, SCULPT_TOOL_DRAW,
			 SCULPT_TOOL_INFLATE, SCULPT_TOOL_CLAY,
			 SCULPT_TOOL_PINCH, SCULPT_TOOL_CREASE))
			outline_col = brush->sub_col;

		/* only do if brush is over the mesh */
		if(hit)
			paint_cursor_on_hit(sd, brush, &vc, location, &visual_strength);

		/* don't show effect of strength past the soft limit */
		if(visual_strength > 1)
			visual_strength = 1;

		outline_alpha = ((paint->flags & PAINT_SHOW_BRUSH_ON_SURFACE) ?
		                     min_alpha + (visual_strength*(max_alpha-min_alpha)) : 0.50f);

		if(sd->draw_anchored) {
			final_radius = sd->anchored_size;
			translation[0] = sd->anchored_initial_mouse[0] - vc.ar->winrct.xmin;
			translation[1] = sd->anchored_initial_mouse[1] - vc.ar->winrct.ymin;
		}
	}

	/* make lines pretty */
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);

	/* set brush color */
	glColor4f(outline_col[0], outline_col[1], outline_col[2], outline_alpha);

	/* draw brush outline */
	glTranslatef(translation[0], translation[1], 0);
	glutil_draw_lined_arc(0.0, M_PI*2.0, final_radius, 40);
	glTranslatef(-translation[0], -translation[1], 0);

	/* restore GL state */
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

/* if this is a tablet event, return tablet pressure and set *pen_flip
   to 1 if the eraser tool is being used, 0 otherwise */
static float event_tablet_data(wmEvent *event, int *pen_flip)
{
	int erasor = 0;
	float pressure = 1;

	if(event->custom == EVT_DATA_TABLET) {
		wmTabletData *wmtab= event->customdata;

		erasor = (wmtab->Active == EVT_TABLET_ERASER);
		pressure = (wmtab->Active != EVT_TABLET_NONE) ? wmtab->Pressure : 1;
	}

	if(pen_flip)
		(*pen_flip) = erasor;

	return pressure;
}

/* Put the location of the next stroke dot into the stroke RNA and apply it to the mesh */
static void paint_brush_stroke_add_step(bContext *C, wmOperator *op, wmEvent *event, float mouse_in[2])
{
	Scene *scene = CTX_data_scene(C);
	Paint *paint = paint_get_active(scene);
	Brush *brush = paint_brush(paint);
	PaintStroke *stroke = op->customdata;
	float mouse[3];
	PointerRNA itemptr;
	float location[3];
	float pressure;
	int pen_flip;

	/* see if tablet affects event */
	pressure = event_tablet_data(event, &pen_flip);

	/* TODO: as sculpt and other paint modes are unified, this
	   separation will go away */
	if(stroke->vc.obact->sculpt) {
		float delta[2];

		brush_jitter_pos(scene, brush, mouse_in, mouse);

		/* XXX: meh, this is round about because
		   brush_jitter_pos isn't written in the best way to
		   be reused here */
		if(brush->flag & BRUSH_JITTER_PRESSURE) {
			sub_v2_v2v2(delta, mouse, mouse_in);
			mul_v2_fl(delta, pressure);
			add_v2_v2v2(mouse, mouse_in, delta);
		}
	}
	else {
		copy_v2_v2(mouse, mouse_in);
	}

	/* TODO: can remove the if statement once all modes have this */
	if(stroke->get_location)
		stroke->get_location(C, stroke, location, mouse);
	else
		zero_v3(location);

	/* Add to stroke */
	RNA_collection_add(op->ptr, "stroke", &itemptr);

	RNA_float_set_array(&itemptr, "location", location);
	RNA_float_set_array(&itemptr, "mouse", mouse);
	RNA_boolean_set(&itemptr, "pen_flip", pen_flip);
	RNA_float_set(&itemptr, "pressure", pressure);

	stroke->last_mouse_position[0] = mouse[0];
	stroke->last_mouse_position[1] = mouse[1];

	stroke->update_step(C, stroke, &itemptr);
}

/* Returns zero if no sculpt changes should be made, non-zero otherwise */
static int paint_smooth_stroke(PaintStroke *stroke, float output[2], wmEvent *event)
{
	output[0] = event->x; 
	output[1] = event->y;

	if ((stroke->brush->flag & BRUSH_SMOOTH_STROKE) &&  
	    !ELEM4(stroke->brush->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB, SCULPT_TOOL_ROTATE, SCULPT_TOOL_SNAKE_HOOK) &&
	    !(stroke->brush->flag & BRUSH_ANCHORED) &&
	    !(stroke->brush->flag & BRUSH_RESTORE_MESH))
	{
		float u = stroke->brush->smooth_stroke_factor, v = 1.0f - u;
		float dx = stroke->last_mouse_position[0] - event->x, dy = stroke->last_mouse_position[1] - event->y;

		/* If the mouse is moving within the radius of the last move,
		   don't update the mouse position. This allows sharp turns. */
		if(dx*dx + dy*dy < stroke->brush->smooth_stroke_radius * stroke->brush->smooth_stroke_radius)
			return 0;

		output[0] = event->x * v + stroke->last_mouse_position[0] * u;
		output[1] = event->y * v + stroke->last_mouse_position[1] * u;
	}

	return 1;
}

/* For brushes with stroke spacing enabled, moves mouse in steps
   towards the final mouse location. */
static int paint_space_stroke(bContext *C, wmOperator *op, wmEvent *event, const float final_mouse[2])
{
	PaintStroke *stroke = op->customdata;
	int cnt = 0;

	if(paint_space_stroke_enabled(stroke->brush)) {
		float mouse[2];
		float vec[2];
		float length, scale;

		copy_v2_v2(mouse, stroke->last_mouse_position);
		sub_v2_v2v2(vec, final_mouse, mouse);

		length = len_v2(vec);

		if(length > FLT_EPSILON) {
			const Scene *scene = CTX_data_scene(C);
			int steps;
			int i;
			float pressure= 1.0f;

			/* XXX mysterious :) what has 'use size' do with this here... if you don't check for it, pressure fails */
			if(brush_use_size_pressure(scene, stroke->brush))
				pressure = event_tablet_data(event, NULL);
			
			if(pressure > FLT_EPSILON) {
				scale = (brush_size(scene, stroke->brush)*pressure*stroke->brush->spacing/50.0f) / length;
				if(scale > FLT_EPSILON) {
					mul_v2_fl(vec, scale);

					steps = (int)(1.0f / scale);

					for(i = 0; i < steps; ++i, ++cnt) {
						add_v2_v2(mouse, vec);
						paint_brush_stroke_add_step(C, op, event, mouse);
					}
				}
			}
		}
	}

	return cnt;
}

/**** Public API ****/

PaintStroke *paint_stroke_new(bContext *C,
				  StrokeGetLocation get_location,
				  StrokeTestStart test_start,
				  StrokeUpdateStep update_step,
				  StrokeDone done, int event_type)
{
	PaintStroke *stroke = MEM_callocN(sizeof(PaintStroke), "PaintStroke");

	stroke->brush = paint_brush(paint_get_active(CTX_data_scene(C)));
	view3d_set_viewcontext(C, &stroke->vc);
	view3d_get_transformation(stroke->vc.ar, stroke->vc.rv3d, stroke->vc.obact, &stroke->mats);

	stroke->get_location = get_location;
	stroke->test_start = test_start;
	stroke->update_step = update_step;
	stroke->done = done;
	stroke->event_type= event_type;	/* for modal, return event */
	
	return stroke;
}

void paint_stroke_free(PaintStroke *stroke)
{
	MEM_freeN(stroke);
}

/* Returns zero if the stroke dots should not be spaced, non-zero otherwise */
int paint_space_stroke_enabled(Brush *br)
{
	return (br->flag & BRUSH_SPACE) &&
	       !(br->flag & BRUSH_ANCHORED) &&
	       !ELEM4(br->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB, SCULPT_TOOL_ROTATE, SCULPT_TOOL_SNAKE_HOOK);
}

int paint_stroke_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	PaintStroke *stroke = op->customdata;
	float mouse[2];
	int first= 0;

	// let NDOF motion pass through to the 3D view so we can paint and rotate simultaneously!
	// this isn't perfect... even when an extra MOUSEMOVE is spoofed, the stroke discards it
	// since the 2D deltas are zero -- code in this file needs to be updated to use the
	// post-NDOF_MOTION MOUSEMOVE
	if (event->type == NDOF_MOTION)
		return OPERATOR_PASS_THROUGH;

	if(!stroke->stroke_started) {
		stroke->last_mouse_position[0] = event->x;
		stroke->last_mouse_position[1] = event->y;
		stroke->stroke_started = stroke->test_start(C, op, event);

		if(stroke->stroke_started) {
			stroke->smooth_stroke_cursor =
				WM_paint_cursor_activate(CTX_wm_manager(C), paint_poll, paint_draw_smooth_stroke, stroke);

			if(stroke->brush->flag & BRUSH_AIRBRUSH)
				stroke->timer = WM_event_add_timer(CTX_wm_manager(C), CTX_wm_window(C), TIMER, stroke->brush->rate);
		}

		first= 1;
		//ED_region_tag_redraw(ar);
	}

	if(event->type == stroke->event_type && event->val == KM_RELEASE) {
		/* exit stroke, free data */
		if(stroke->smooth_stroke_cursor)
			WM_paint_cursor_end(CTX_wm_manager(C), stroke->smooth_stroke_cursor);

		if(stroke->timer)
			WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), stroke->timer);

		stroke->done(C, stroke);
		MEM_freeN(stroke);
		return OPERATOR_FINISHED;
	}
	else if( (first) ||
	         (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) ||
	         (event->type == TIMER && (event->customdata == stroke->timer)) )
	{
		if(stroke->stroke_started) {
			if(paint_smooth_stroke(stroke, mouse, event)) {
				if(paint_space_stroke_enabled(stroke->brush)) {
					if(!paint_space_stroke(C, op, event, mouse)) {
						//ED_region_tag_redraw(ar);
					}
				}
				else {
					paint_brush_stroke_add_step(C, op, event, mouse);
				}
			}
			else {
				;//ED_region_tag_redraw(ar);
			}
		}
	}

	/* we want the stroke to have the first daub at the start location
	 * instead of waiting till we have moved the space distance */
	if(first &&
	   stroke->stroke_started &&
	   paint_space_stroke_enabled(stroke->brush) &&
	   !(stroke->brush->flag & BRUSH_ANCHORED) &&
	   !(stroke->brush->flag & BRUSH_SMOOTH_STROKE))
	{
		paint_brush_stroke_add_step(C, op, event, mouse);
	}
	
	return OPERATOR_RUNNING_MODAL;
}

int paint_stroke_exec(bContext *C, wmOperator *op)
{
	PaintStroke *stroke = op->customdata;

	/* only when executed for the first time */
	if(stroke->stroke_started == 0) {
		/* XXX stroke->last_mouse_position is unset, this may cause problems */
		stroke->test_start(C, op, NULL);
		stroke->stroke_started= 1;
	}

	RNA_BEGIN(op->ptr, itemptr, "stroke") {
		stroke->update_step(C, stroke, &itemptr);
	}
	RNA_END;

	stroke->done(C, stroke);

	MEM_freeN(stroke);
	op->customdata = NULL;

	return OPERATOR_FINISHED;
}

int paint_stroke_cancel(bContext *C, wmOperator *op)
{
	PaintStroke *stroke = op->customdata;

	if(stroke->done)
		stroke->done(C, stroke);

	MEM_freeN(stroke);
	op->customdata = NULL;

	return OPERATOR_CANCELLED;
}

ViewContext *paint_stroke_view_context(PaintStroke *stroke)
{
	return &stroke->vc;
}

void *paint_stroke_mode_data(struct PaintStroke *stroke)
{
	return stroke->mode_data;
}

void paint_stroke_set_mode_data(PaintStroke *stroke, void *mode_data)
{
	stroke->mode_data = mode_data;
}

int paint_poll(bContext *C)
{
	Paint *p = paint_get_active(CTX_data_scene(C));
	Object *ob = CTX_data_active_object(C);

	return p && ob && paint_brush(p) &&
		CTX_wm_area(C)->spacetype == SPACE_VIEW3D &&
		CTX_wm_region(C)->regiontype == RGN_TYPE_WINDOW;
}

void paint_cursor_start(bContext *C, int (*poll)(bContext *C))
{
	Paint *p = paint_get_active(CTX_data_scene(C));

	if(p && !p->paint_cursor)
		p->paint_cursor = WM_paint_cursor_activate(CTX_wm_manager(C), poll, paint_draw_cursor, NULL);
}


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
 * Contributor(s): Jason Wilkins, Tom Musgrove.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/editors/sculpt_paint/paint_cursor.c
 *  \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_color_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_paint.h"

#include "WM_api.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_view3d.h"

#include "paint_intern.h"
/* still needed for sculpt_stroke_get_location, should be
   removed eventually (TODO) */
#include "sculpt_intern.h"

/* TODOs:

   Some of the cursor drawing code is doing non-draw stuff
   (e.g. updating the brush rake angle). This should be cleaned up
   still.

   There is also some ugliness with sculpt-specific code.
 */

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

static int project_brush_radius(ViewContext *vc,
								float radius,
								const float location[3])
{
	float view[3], nonortho[3], ortho[3], offset[3], p1[2], p2[2];

	ED_view3d_global_to_vector(vc->rv3d, location, view);

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

	// project the center of the brush, and the tangent point to the view onto the screen
	project_float(vc->ar, location, p1);
	project_float(vc->ar, offset, p2);

	// the distance between these points is the size of the projected brush in pixels
	return len_v2v2(p1, p2);
}

static int sculpt_get_brush_geometry(bContext* C, ViewContext *vc,
									 int x, int y, int* pixel_radius,
									 float location[3])
{
	Scene *scene = CTX_data_scene(C);
	Paint *paint = paint_get_active(scene);
	Brush *brush = paint_brush(paint);
	float window[2];
	int hit;

	window[0] = x + vc->ar->winrct.xmin;
	window[1] = y + vc->ar->winrct.ymin;

	if(vc->obact->sculpt && vc->obact->sculpt->pbvh &&
	   sculpt_stroke_get_location(C, NULL, location, window)) {
		*pixel_radius =
			project_brush_radius(vc,
								 brush_unprojected_radius(scene, brush),
								 location);

		if (*pixel_radius == 0)
			*pixel_radius = brush_size(scene, brush);

		mul_m4_v3(vc->obact->obmat, location);

		hit = 1;
	}
	else {
		Sculpt* sd    = CTX_data_tool_settings(C)->sculpt;
		Brush*  brush = paint_brush(&sd->paint);

		*pixel_radius = brush_size(scene, brush);
		hit = 0;
	}

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
								const float location[3])
{
	float unprojected_radius, projected_radius;

	/* update the brush's cached 3D radius */
	if(!brush_use_locked_size(vc->scene, brush)) {
		/* get 2D brush radius */
		if(sd->draw_anchored)
			projected_radius = sd->anchored_size;
		else {
			if(brush->flag & BRUSH_ANCHORED)
				projected_radius = 8;
			else
				projected_radius = brush_size(vc->scene, brush);
		}
	
		/* convert brush radius from 2D to 3D */
		unprojected_radius = paint_calc_object_space_radius(vc, location,
															projected_radius);

		/* scale 3D brush radius by pressure */
		if(sd->draw_pressure && brush_use_size_pressure(vc->scene, brush))
			unprojected_radius *= sd->pressure_value;

		/* set cached value in either Brush or UnifiedPaintSettings */
		brush_set_unprojected_radius(vc->scene, brush, unprojected_radius);
	}
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
		hit = sculpt_get_brush_geometry(C, &vc, x, y, &pixel_radius, location);

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
		if(hit) {
			/* scale the alpha by pen pressure */
			if(sd->draw_pressure && brush_use_alpha_pressure(vc.scene, brush))
				visual_strength *= sd->pressure_value;

			paint_cursor_on_hit(sd, brush, &vc, location);
		}

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

/* Public API */

void paint_cursor_start(bContext *C, int (*poll)(bContext *C))
{
	Paint *p = paint_get_active(CTX_data_scene(C));

	if(p && !p->paint_cursor)
		p->paint_cursor = WM_paint_cursor_activate(CTX_wm_manager(C), poll, paint_draw_cursor, NULL);
}

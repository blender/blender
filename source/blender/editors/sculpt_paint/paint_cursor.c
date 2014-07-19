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
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_color_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_node.h"
#include "BKE_paint.h"
#include "BKE_colortools.h"

#include "WM_api.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "IMB_imbuf_types.h"

#include "ED_view3d.h"

#include "paint_intern.h"
/* still needed for sculpt_stroke_get_location, should be
 * removed eventually (TODO) */
#include "sculpt_intern.h"

#ifdef _OPENMP
#include <omp.h>
#endif

/* TODOs:
 *
 * Some of the cursor drawing code is doing non-draw stuff
 * (e.g. updating the brush rake angle). This should be cleaned up
 * still.
 *
 * There is also some ugliness with sculpt-specific code.
 */

typedef struct TexSnapshot {
	GLuint overlay_texture;
	int winx;
	int winy;
	int old_size;
	float old_zoom;
	bool old_col;
} TexSnapshot;

typedef struct CursorSnapshot {
	GLuint overlay_texture;
	int size;
	int zoom;
} CursorSnapshot;

static TexSnapshot primary_snap = {0};
static TexSnapshot secondary_snap  = {0};
static CursorSnapshot cursor_snap  = {0};

/* delete overlay cursor textures to preserve memory and invalidate all overlay flags */
void paint_cursor_delete_textures(void)
{
	if (primary_snap.overlay_texture)
		glDeleteTextures(1, &primary_snap.overlay_texture);
	if (secondary_snap.overlay_texture)
		glDeleteTextures(1, &secondary_snap.overlay_texture);
	if (cursor_snap.overlay_texture)
		glDeleteTextures(1, &cursor_snap.overlay_texture);

	memset(&primary_snap, 0, sizeof(TexSnapshot));
	memset(&secondary_snap, 0, sizeof(TexSnapshot));
	memset(&cursor_snap, 0, sizeof(CursorSnapshot));

	BKE_paint_invalidate_overlay_all();
}

static int same_tex_snap(TexSnapshot *snap, MTex *mtex, ViewContext *vc, bool col, float zoom)
{
	return (/* make brush smaller shouldn't cause a resample */
	        //(mtex->brush_map_mode != MTEX_MAP_MODE_VIEW ||
	        //(BKE_brush_size_get(vc->scene, brush) <= snap->BKE_brush_size_get)) &&

	        (mtex->brush_map_mode != MTEX_MAP_MODE_TILED ||
	         (vc->ar->winx == snap->winx &&
	          vc->ar->winy == snap->winy)) &&
	        (mtex->brush_map_mode == MTEX_MAP_MODE_STENCIL ||
	        snap->old_zoom == zoom) &&
	        snap->old_col == col
	        );
}

static void make_tex_snap(TexSnapshot *snap, ViewContext *vc, float zoom)
{
	snap->old_zoom = zoom;
	snap->winx = vc->ar->winx;
	snap->winy = vc->ar->winy;
}

static int load_tex(Brush *br, ViewContext *vc, float zoom, bool col, bool primary)
{
	bool init;
	TexSnapshot *target;

	MTex *mtex = (primary) ? &br->mtex : &br->mask_mtex;
	OverlayControlFlags overlay_flags = BKE_paint_get_overlay_flags();
	GLubyte *buffer = NULL;

	int size;
	int j;
	int refresh;
	GLenum format = col ? GL_RGBA : GL_ALPHA;
	OverlayControlFlags invalid = (primary) ? (overlay_flags & PAINT_INVALID_OVERLAY_TEXTURE_PRIMARY) :
	                           (overlay_flags & PAINT_INVALID_OVERLAY_TEXTURE_SECONDARY);

	target = (primary) ? &primary_snap : &secondary_snap;

	refresh = 
	    !target->overlay_texture ||
	    (invalid != 0) ||
	    !same_tex_snap(target, mtex, vc, col, zoom);

	init = (target->overlay_texture != 0);

	if (refresh) {
		struct ImagePool *pool = NULL;
		bool convert_to_linear = false;
		struct ColorSpace *colorspace;
		/* stencil is rotated later */
		const float rotation = (mtex->brush_map_mode != MTEX_MAP_MODE_STENCIL) ?
		                       -mtex->rot : 0;

		float radius = BKE_brush_size_get(vc->scene, br) * zoom;

		make_tex_snap(target, vc, zoom);

		if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
			int s = BKE_brush_size_get(vc->scene, br);
			int r = 1;

			for (s >>= 1; s > 0; s >>= 1)
				r++;

			size = (1 << r);

			if (size < 256)
				size = 256;

			if (size < target->old_size)
				size = target->old_size;
		}
		else
			size = 512;

		if (target->old_size != size) {
			if (target->overlay_texture) {
				glDeleteTextures(1, &target->overlay_texture);
				target->overlay_texture = 0;
			}

			init = false;

			target->old_size = size;
		}
		if (col)
			buffer = MEM_mallocN(sizeof(GLubyte) * size * size * 4, "load_tex");
		else
			buffer = MEM_mallocN(sizeof(GLubyte) * size * size, "load_tex");

		pool = BKE_image_pool_new();

		if (mtex->tex && mtex->tex->nodetree)
			ntreeTexBeginExecTree(mtex->tex->nodetree);  /* has internal flag to detect it only does it once */

#pragma omp parallel for schedule(static)
		for (j = 0; j < size; j++) {
			int i;
			float y;
			float len;
			int thread_num;

#ifdef _OPENMP
			thread_num = omp_get_thread_num();
#else
			thread_num = 0;
#endif

			if (mtex->tex->type == TEX_IMAGE && mtex->tex->ima) {
				ImBuf *tex_ibuf = BKE_image_pool_acquire_ibuf(mtex->tex->ima, &mtex->tex->iuser, pool);
				/* For consistency, sampling always returns color in linear space */
				if (tex_ibuf && tex_ibuf->rect_float == NULL) {
					convert_to_linear = true;
					colorspace = tex_ibuf->rect_colorspace;
				}
				BKE_image_pool_release_ibuf(mtex->tex->ima, tex_ibuf, pool);
			}


			for (i = 0; i < size; i++) {

				// largely duplicated from tex_strength

				int index = j * size + i;
				float x;

				x = (float)i / size;
				y = (float)j / size;

				if (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) {
					x *= vc->ar->winx / radius;
					y *= vc->ar->winy / radius;
				}
				else {
					x -= 0.5f;
					y -= 0.5f;

					x *= 2;
					y *= 2;
				}

				len = sqrtf(x * x + y * y);

				if (ELEM(mtex->brush_map_mode, MTEX_MAP_MODE_TILED, MTEX_MAP_MODE_STENCIL) || len <= 1) {
					/* it is probably worth optimizing for those cases where 
					 * the texture is not rotated by skipping the calls to
					 * atan2, sqrtf, sin, and cos. */
					if (mtex->tex && (rotation > 0.001f || rotation < -0.001f)) {
						const float angle = atan2f(y, x) + rotation;

						x = len * cosf(angle);
						y = len * sinf(angle);
					}

					if (col) {
						float rgba[4];

						paint_get_tex_pixel_col(mtex, x, y, rgba, pool, thread_num, convert_to_linear, colorspace);

						buffer[index * 4]     = rgba[0] * 255;
						buffer[index * 4 + 1] = rgba[1] * 255;
						buffer[index * 4 + 2] = rgba[2] * 255;
						buffer[index * 4 + 3] = rgba[3] * 255;
					}
					else {
						float avg = paint_get_tex_pixel(mtex, x, y, pool, thread_num);

						avg += br->texture_sample_bias;

						/* clamp to avoid precision overflow */
						CLAMP(avg, 0.0f, 1.0f);
						buffer[index] = 255 - (GLubyte)(255 * avg);
					}
				}
				else {
					if (col) {
						buffer[index * 4]     = 0;
						buffer[index * 4 + 1] = 0;
						buffer[index * 4 + 2] = 0;
						buffer[index * 4 + 3] = 0;
					}
					else {
						buffer[index] = 0;
					}
				}
			}
		}

		if (mtex->tex && mtex->tex->nodetree)
			ntreeTexEndExecTree(mtex->tex->nodetree->execdata);

		if (pool)
			BKE_image_pool_free(pool);

		if (!target->overlay_texture)
			glGenTextures(1, &target->overlay_texture);
	}
	else {
		size = target->old_size;
	}

	glBindTexture(GL_TEXTURE_2D, target->overlay_texture);

	if (refresh) {
		if (!init || (target->old_col != col)) {
			glTexImage2D(GL_TEXTURE_2D, 0, format, size, size, 0, format, GL_UNSIGNED_BYTE, buffer);
		}
		else {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, format, GL_UNSIGNED_BYTE, buffer);
		}

		if (buffer)
			MEM_freeN(buffer);

		target->old_col = col;
	}

	glEnable(GL_TEXTURE_2D);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	}

	BKE_paint_reset_overlay_invalid(invalid);

	return 1;
}

static int load_tex_cursor(Brush *br, ViewContext *vc, float zoom)
{
	bool init;

	OverlayControlFlags overlay_flags = BKE_paint_get_overlay_flags();
	GLubyte *buffer = NULL;

	int size;
	int j;
	int refresh;

	refresh =
	    !cursor_snap.overlay_texture ||
	    (overlay_flags & PAINT_INVALID_OVERLAY_CURVE) ||
	    cursor_snap.zoom != zoom;

	init = (cursor_snap.overlay_texture != 0);

	if (refresh) {
		int s, r;

		cursor_snap.zoom = zoom;

		s = BKE_brush_size_get(vc->scene, br);
		r = 1;

		for (s >>= 1; s > 0; s >>= 1)
			r++;

		size = (1 << r);

		if (size < 256)
			size = 256;

		if (size < cursor_snap.size)
			size = cursor_snap.size;

		if (cursor_snap.size != size) {
			if (cursor_snap.overlay_texture) {
				glDeleteTextures(1, &cursor_snap.overlay_texture);
				cursor_snap.overlay_texture = 0;
			}

			init = false;

			cursor_snap.size = size;
		}
		buffer = MEM_mallocN(sizeof(GLubyte) * size * size, "load_tex");

		curvemapping_initialize(br->curve);

#pragma omp parallel for schedule(static)
		for (j = 0; j < size; j++) {
			int i;
			float y;
			float len;

			for (i = 0; i < size; i++) {

				// largely duplicated from tex_strength

				int index = j * size + i;
				float x;

				x = (float)i / size;
				y = (float)j / size;

				x -= 0.5f;
				y -= 0.5f;

				x *= 2;
				y *= 2;

				len = sqrtf(x * x + y * y);

				if (len <= 1) {
					float avg = BKE_brush_curve_strength_clamp(br, len, 1.0f);  /* Falloff curve */

					buffer[index] = 255 - (GLubyte)(255 * avg);

				}
				else {
					buffer[index] = 0;
				}
			}
		}

		if (!cursor_snap.overlay_texture)
			glGenTextures(1, &cursor_snap.overlay_texture);
	}
	else {
		size = cursor_snap.size;
	}

	glBindTexture(GL_TEXTURE_2D, cursor_snap.overlay_texture);

	if (refresh) {
		if (!init) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, size, size, 0, GL_ALPHA, GL_UNSIGNED_BYTE, buffer);
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

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	BKE_paint_reset_overlay_invalid(PAINT_INVALID_OVERLAY_CURVE);

	return 1;
}



static int project_brush_radius(ViewContext *vc,
                                float radius,
                                const float location[3])
{
	float view[3], nonortho[3], ortho[3], offset[3], p1[2], p2[2];

	ED_view3d_global_to_vector(vc->rv3d, location, view);

	/* create a vector that is not orthogonal to view */

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

	/* get a vector in the plane of the view */
	cross_v3_v3v3(ortho, nonortho, view);
	normalize_v3(ortho);

	/* make a point on the surface of the brush tagent to the view */
	mul_v3_fl(ortho, radius);
	add_v3_v3v3(offset, location, ortho);

	/* project the center of the brush, and the tangent point to the view onto the screen */
	if ((ED_view3d_project_float_global(vc->ar, location, p1, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) &&
	    (ED_view3d_project_float_global(vc->ar, offset,   p2, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK))
	{
		/* the distance between these points is the size of the projected brush in pixels */
		return len_v2v2(p1, p2);
	}
	else {
		BLI_assert(0);  /* assert because the code that sets up the vectors should disallow this */
		return 0;
	}
}

static int sculpt_get_brush_geometry(bContext *C, ViewContext *vc,
                                     int x, int y, int *pixel_radius,
                                     float location[3])
{
	Scene *scene = CTX_data_scene(C);
	Paint *paint = BKE_paint_get_active_from_context(C);
	float mouse[2];
	int hit;

	mouse[0] = x;
	mouse[1] = y;

	if (vc->obact->sculpt && vc->obact->sculpt->pbvh &&
	    sculpt_stroke_get_location(C, location, mouse))
	{
		Brush *brush = BKE_paint_brush(paint);
		*pixel_radius =
		    project_brush_radius(vc,
		                         BKE_brush_unprojected_radius_get(scene, brush),
		                         location);

		if (*pixel_radius == 0)
			*pixel_radius = BKE_brush_size_get(scene, brush);

		mul_m4_v3(vc->obact->obmat, location);

		hit = 1;
	}
	else {
		Sculpt *sd    = CTX_data_tool_settings(C)->sculpt;
		Brush *brush = BKE_paint_brush(&sd->paint);

		*pixel_radius = BKE_brush_size_get(scene, brush);
		hit = 0;
	}

	return hit;
}

/* Draw an overlay that shows what effect the brush's texture will
 * have on brush strength */
static void paint_draw_tex_overlay(UnifiedPaintSettings *ups, Brush *brush,
                                     ViewContext *vc, int x, int y, float zoom, bool col, bool primary)
{
	rctf quad;
	/* check for overlay mode */

	MTex *mtex = (primary) ? &brush->mtex : &brush->mask_mtex;
	bool valid = (primary) ? (brush->overlay_flags & BRUSH_OVERLAY_PRIMARY) != 0 :
	                         (brush->overlay_flags & BRUSH_OVERLAY_SECONDARY) != 0;
	int overlay_alpha = (primary) ? brush->texture_overlay_alpha : brush->mask_overlay_alpha;

	if (!(mtex->tex) || !((mtex->brush_map_mode == MTEX_MAP_MODE_STENCIL) ||
	    (valid &&
	    ELEM(mtex->brush_map_mode, MTEX_MAP_MODE_VIEW, MTEX_MAP_MODE_TILED))))
	{
		return;
	}

	if (load_tex(brush, vc, zoom, col, primary)) {
		glEnable(GL_BLEND);

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_FALSE);
		glDepthFunc(GL_ALWAYS);

		glMatrixMode(GL_TEXTURE);
		glPushMatrix();
		glLoadIdentity();

		if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
			/* brush rotation */
			glTranslatef(0.5, 0.5, 0);
			glRotatef((double)RAD2DEGF(ups->brush_rotation),
			          0.0, 0.0, 1.0);
			glTranslatef(-0.5f, -0.5f, 0);

			/* scale based on tablet pressure */
			if (primary && ups->stroke_active && BKE_brush_use_size_pressure(vc->scene, brush)) {
				glTranslatef(0.5f, 0.5f, 0);
				glScalef(1.0f / ups->size_pressure_value, 1.0f / ups->size_pressure_value, 1);
				glTranslatef(-0.5f, -0.5f, 0);
			}

			if (ups->draw_anchored) {
				const float *aim = ups->anchored_initial_mouse;
				quad.xmin = aim[0] - ups->anchored_size;
				quad.ymin = aim[1] - ups->anchored_size;
				quad.xmax = aim[0] + ups->anchored_size;
				quad.ymax = aim[1] + ups->anchored_size;
			}
			else {
				const int radius = BKE_brush_size_get(vc->scene, brush) * zoom;
				quad.xmin = x - radius;
				quad.ymin = y - radius;
				quad.xmax = x + radius;
				quad.ymax = y + radius;
			}
		}
		else if (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) {
			quad.xmin = 0;
			quad.ymin = 0;
			quad.xmax = BLI_rcti_size_x(&vc->ar->winrct);
			quad.ymax = BLI_rcti_size_y(&vc->ar->winrct);
		}
		/* Stencil code goes here */
		else {
			if (primary) {
				quad.xmin = -brush->stencil_dimension[0];
				quad.ymin = -brush->stencil_dimension[1];
				quad.xmax = brush->stencil_dimension[0];
				quad.ymax = brush->stencil_dimension[1];
			}
			else {
				quad.xmin = -brush->mask_stencil_dimension[0];
				quad.ymin = -brush->mask_stencil_dimension[1];
				quad.xmax = brush->mask_stencil_dimension[0];
				quad.ymax = brush->mask_stencil_dimension[1];
			}
			glMatrixMode(GL_MODELVIEW);
			glPushMatrix();
			if (primary)
				glTranslatef(brush->stencil_pos[0], brush->stencil_pos[1], 0);
			else
				glTranslatef(brush->mask_stencil_pos[0], brush->mask_stencil_pos[1], 0);
			glRotatef(RAD2DEGF(mtex->rot), 0, 0, 1);
			glMatrixMode(GL_TEXTURE);
		}

		/* set quad color. Colored overlay does not get blending */
		if (col)
			glColor4f(1.0,
				      1.0,
				      1.0,
				      overlay_alpha / 100.0f);
		else
			glColor4f(U.sculpt_paint_overlay_col[0],
				      U.sculpt_paint_overlay_col[1],
				      U.sculpt_paint_overlay_col[2],
				      overlay_alpha / 100.0f);

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

		if (mtex->brush_map_mode == MTEX_MAP_MODE_STENCIL) {
			glMatrixMode(GL_MODELVIEW);
			glPopMatrix();
		}
	}
}

/* Draw an overlay that shows what effect the brush's texture will
 * have on brush strength */
static void paint_draw_cursor_overlay(UnifiedPaintSettings *ups, Brush *brush,
                                      ViewContext *vc, int x, int y, float zoom)
{
	rctf quad;
	/* check for overlay mode */

	if (!(brush->overlay_flags & BRUSH_OVERLAY_CURSOR)) {
		return;
	}

	if (load_tex_cursor(brush, vc, zoom)) {
		bool do_pop = false;
		float center[2];
		glEnable(GL_BLEND);

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glDepthMask(GL_FALSE);
		glDepthFunc(GL_ALWAYS);

		if (ups->draw_anchored) {
			const float *aim = ups->anchored_initial_mouse;
			copy_v2_v2(center, aim);
			quad.xmin = aim[0] - ups->anchored_size;
			quad.ymin = aim[1] - ups->anchored_size;
			quad.xmax = aim[0] + ups->anchored_size;
			quad.ymax = aim[1] + ups->anchored_size;
		}
		else {
			const int radius = BKE_brush_size_get(vc->scene, brush) * zoom;
			center[0] = x;
			center[1] = y;

			quad.xmin = x - radius;
			quad.ymin = y - radius;
			quad.xmax = x + radius;
			quad.ymax = y + radius;
		}

		/* scale based on tablet pressure */
		if (ups->stroke_active && BKE_brush_use_size_pressure(vc->scene, brush)) {
			do_pop = true;
			glPushMatrix();
			glLoadIdentity();
			glTranslatef(center[0], center[1], 0);
			glScalef(ups->size_pressure_value, ups->size_pressure_value, 1);
			glTranslatef(-center[0], -center[1], 0);
		}

		glColor4f(U.sculpt_paint_overlay_col[0],
		        U.sculpt_paint_overlay_col[1],
		        U.sculpt_paint_overlay_col[2],
		        brush->cursor_overlay_alpha / 100.0f);

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

		if (do_pop)
			glPopMatrix();
	}
}

static void paint_draw_alpha_overlay(UnifiedPaintSettings *ups, Brush *brush,
                                     ViewContext *vc, int x, int y, float zoom, PaintMode mode)
{
	/* color means that primary brush texture is colured and secondary is used for alpha/mask control */
	bool col = ELEM(mode, PAINT_TEXTURE_PROJECTIVE, PAINT_TEXTURE_2D, PAINT_VERTEX) ? true : false;
	OverlayControlFlags flags = BKE_paint_get_overlay_flags();
	/* save lots of GL state
	 * TODO: check on whether all of these are needed? */
	glPushAttrib(GL_COLOR_BUFFER_BIT |
	             GL_CURRENT_BIT |
	             GL_DEPTH_BUFFER_BIT |
	             GL_ENABLE_BIT |
	             GL_LINE_BIT |
	             GL_POLYGON_BIT |
	             GL_STENCIL_BUFFER_BIT |
	             GL_TRANSFORM_BIT |
	             GL_VIEWPORT_BIT |
	             GL_TEXTURE_BIT);


	/* coloured overlay should be drawn separately */
	if (col) {
		if (!(flags & PAINT_OVERLAY_OVERRIDE_PRIMARY))
			paint_draw_tex_overlay(ups, brush, vc, x, y, zoom, true, true);
		if (!(flags & PAINT_OVERLAY_OVERRIDE_SECONDARY))
			paint_draw_tex_overlay(ups, brush, vc, x, y, zoom, false, false);
		if (!(flags & PAINT_OVERLAY_OVERRIDE_CURSOR))
			paint_draw_cursor_overlay(ups, brush, vc, x, y, zoom);
	}
	else {
		if (!(flags & PAINT_OVERLAY_OVERRIDE_PRIMARY))
			paint_draw_tex_overlay(ups, brush, vc, x, y, zoom, false, true);
		if (!(flags & PAINT_OVERLAY_OVERRIDE_CURSOR))
			paint_draw_cursor_overlay(ups, brush, vc, x, y, zoom);
	}

	glPopAttrib();
}

/* Special actions taken when paint cursor goes over mesh */
/* TODO: sculpt only for now */
static void paint_cursor_on_hit(UnifiedPaintSettings *ups, Brush *brush, ViewContext *vc,
                                const float location[3])
{
	float unprojected_radius, projected_radius;

	/* update the brush's cached 3D radius */
	if (!BKE_brush_use_locked_size(vc->scene, brush)) {
		/* get 2D brush radius */
		if (ups->draw_anchored)
			projected_radius = ups->anchored_size;
		else {
			if (brush->flag & BRUSH_ANCHORED)
				projected_radius = 8;
			else
				projected_radius = BKE_brush_size_get(vc->scene, brush);
		}
	
		/* convert brush radius from 2D to 3D */
		unprojected_radius = paint_calc_object_space_radius(vc, location,
		                                                    projected_radius);

		/* scale 3D brush radius by pressure */
		if (ups->stroke_active && BKE_brush_use_size_pressure(vc->scene, brush))
			unprojected_radius *= ups->size_pressure_value;

		/* set cached value in either Brush or UnifiedPaintSettings */
		BKE_brush_unprojected_radius_set(vc->scene, brush, unprojected_radius);
	}
}

static void paint_draw_cursor(bContext *C, int x, int y, void *UNUSED(unused))
{
	Scene *scene = CTX_data_scene(C);
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
	Paint *paint = BKE_paint_get_active_from_context(C);
	Brush *brush = BKE_paint_brush(paint);
	ViewContext vc;
	PaintMode mode;
	float final_radius;
	float translation[2];
	float outline_alpha, *outline_col;
	float zoomx, zoomy;
	
	/* check that brush drawing is enabled */
	if (!(paint->flags & PAINT_SHOW_BRUSH))
		return;

	/* can't use stroke vc here because this will be called during
	 * mouse over too, not just during a stroke */
	view3d_set_viewcontext(C, &vc);

	get_imapaint_zoom(C, &zoomx, &zoomy);
	zoomx = max_ff(zoomx, zoomy);
	mode = BKE_paintmode_get_active_from_context(C);

	/* set various defaults */
	translation[0] = x;
	translation[1] = y;
	outline_alpha = 0.5;
	outline_col = brush->add_col;
	final_radius = BKE_brush_size_get(scene, brush) * zoomx;

	/* don't calculate rake angles while a stroke is active because the rake variables are global and
	 * we may get interference with the stroke itself. For line strokes, such interference is visible */
	if (!ups->stroke_active && (brush->flag & BRUSH_RAKE))
		paint_calculate_rake_rotation(ups, translation);

	/* draw overlay */
	paint_draw_alpha_overlay(ups, brush, &vc, x, y, zoomx, mode);

	/* TODO: as sculpt and other paint modes are unified, this
	 * special mode of drawing will go away */
	if ((mode == PAINT_SCULPT) && vc.obact->sculpt) {
		float location[3];
		int pixel_radius, hit;

		/* test if brush is over the mesh */
		hit = sculpt_get_brush_geometry(C, &vc, x, y, &pixel_radius, location);

		if (BKE_brush_use_locked_size(scene, brush))
			BKE_brush_size_set(scene, brush, pixel_radius);

		/* check if brush is subtracting, use different color then */
		/* TODO: no way currently to know state of pen flip or
		 * invert key modifier without starting a stroke */
		if ((!(brush->flag & BRUSH_INVERTED) ^
		     !(brush->flag & BRUSH_DIR_IN)) &&
		    ELEM(brush->sculpt_tool, SCULPT_TOOL_DRAW,
		          SCULPT_TOOL_INFLATE, SCULPT_TOOL_CLAY,
		          SCULPT_TOOL_PINCH, SCULPT_TOOL_CREASE))
		{
			outline_col = brush->sub_col;
		}

		/* only do if brush is over the mesh */
		if (hit)
			paint_cursor_on_hit(ups, brush, &vc, location);

		if (ups->draw_anchored) {
			final_radius = ups->anchored_size;
			translation[0] = ups->anchored_initial_mouse[0];
			translation[1] = ups->anchored_initial_mouse[1];
		}
	}

	/* make lines pretty */
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);

	/* set brush color */
	glColor4f(outline_col[0], outline_col[1], outline_col[2], outline_alpha);

	/* draw brush outline */
	glTranslatef(translation[0], translation[1], 0);

	/* draw an inner brush */
	if (ups->stroke_active && BKE_brush_use_size_pressure(scene, brush)) {
		/* inner at full alpha */
		glutil_draw_lined_arc(0.0, M_PI * 2.0, final_radius * ups->size_pressure_value, 40);
		/* outer at half alpha */
		glColor4f(outline_col[0], outline_col[1], outline_col[2], outline_alpha * 0.5f);
	}
	glutil_draw_lined_arc(0.0, M_PI * 2.0, final_radius, 40);
	glTranslatef(-translation[0], -translation[1], 0);

	/* restore GL state */
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

/* Public API */

void paint_cursor_start(bContext *C, int (*poll)(bContext *C))
{
	Paint *p = BKE_paint_get_active_from_context(C);

	if (p && !p->paint_cursor)
		p->paint_cursor = WM_paint_cursor_activate(CTX_wm_manager(C), poll, paint_draw_cursor, NULL);

	/* invalidate the paint cursors */
	BKE_paint_invalidate_overlay_all();
}

void paint_cursor_start_explicit(Paint *p, wmWindowManager *wm, int (*poll)(bContext *C))
{
	if (p && !p->paint_cursor)
		p->paint_cursor = WM_paint_cursor_activate(wm, poll, paint_draw_cursor, NULL);
}

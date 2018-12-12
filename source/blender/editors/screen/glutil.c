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

/** \file blender/editors/screen/glutil.c
 *  \ingroup edscr
 */


#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"

#include "BLI_rect.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_context.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"

#include "GPU_basic_shader.h"

#include "UI_interface.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE                        0x812F
#endif

/* UNUSED */
#if 0
void fdrawbezier(float vec[4][3])
{
	float dist;
	float curve_res = 24, spline_step = 0.0f;

	dist = 0.5f * fabsf(vec[0][0] - vec[3][0]);

	/* check direction later, for top sockets */
	vec[1][0] = vec[0][0] + dist;
	vec[1][1] = vec[0][1];

	vec[2][0] = vec[3][0] - dist;
	vec[2][1] = vec[3][1];
	/* we can reuse the dist variable here to increment the GL curve eval amount */
	dist = 1.0f / curve_res;

	cpack(0x0);
	glMap1f(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 4, vec[0]);
	glBegin(GL_LINE_STRIP);
	while (spline_step < 1.000001f) {
#if 0
		if (do_shaded)
			UI_ThemeColorBlend(th_col1, th_col2, spline_step);
#endif
		glEvalCoord1f(spline_step);
		spline_step += dist;
	}
	glEnd();
}
#endif

void fdrawline(float x1, float y1, float x2, float y2)
{
	glBegin(GL_LINES);
	glVertex2f(x1, y1);
	glVertex2f(x2, y2);
	glEnd();
}

void fdrawbox(float x1, float y1, float x2, float y2)
{
	glBegin(GL_LINE_LOOP);

	glVertex2f(x1, y1);
	glVertex2f(x1, y2);
	glVertex2f(x2, y2);
	glVertex2f(x2, y1);

	glEnd();
}

void fdrawcheckerboard(float x1, float y1, float x2, float y2)
{
	unsigned char col1[4] = {40, 40, 40}, col2[4] = {50, 50, 50};

	glColor3ubv(col1);
	glRectf(x1, y1, x2, y2);
	glColor3ubv(col2);

	GPU_basic_shader_bind(GPU_SHADER_STIPPLE | GPU_SHADER_USE_COLOR);
	GPU_basic_shader_stipple(GPU_SHADER_STIPPLE_CHECKER_8PX);
	glRectf(x1, y1, x2, y2);
	GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
}

void sdrawline(int x1, int y1, int x2, int y2)
{
	glBegin(GL_LINES);
	glVertex2i(x1, y1);
	glVertex2i(x2, y2);
	glEnd();
}

/* UNUSED */
#if 0
/*
 *     x1,y2
 *     |  \
 *     |   \
 *     |    \
 *     x1,y1-- x2,y1
 */

static void sdrawtripoints(int x1, int y1, int x2, int y2)
{
	glVertex2i(x1, y1);
	glVertex2i(x1, y2);
	glVertex2i(x2, y1);
}

void sdrawtri(int x1, int y1, int x2, int y2)
{
	glBegin(GL_LINE_STRIP);
	sdrawtripoints(x1, y1, x2, y2);
	glEnd();
}

void sdrawtrifill(int x1, int y1, int x2, int y2)
{
	glBegin(GL_TRIANGLES);
	sdrawtripoints(x1, y1, x2, y2);
	glEnd();
}
#endif

void sdrawbox(int x1, int y1, int x2, int y2)
{
	glBegin(GL_LINE_LOOP);

	glVertex2i(x1, y1);
	glVertex2i(x1, y2);
	glVertex2i(x2, y2);
	glVertex2i(x2, y1);

	glEnd();
}


/* ******************************************** */

void setlinestyle(int nr)
{
	if (nr == 0) {
		glDisable(GL_LINE_STIPPLE);
	}
	else {

		glEnable(GL_LINE_STIPPLE);
		if (U.pixelsize > 1.0f)
			glLineStipple(nr, 0xCCCC);
		else
			glLineStipple(nr, 0xAAAA);
	}
}

/* Invert line handling */

#define GL_TOGGLE(mode, onoff)  (((onoff) ? glEnable : glDisable)(mode))

void set_inverted_drawing(int enable)
{
	glLogicOp(enable ? GL_INVERT : GL_COPY);
	GL_TOGGLE(GL_COLOR_LOGIC_OP, enable);
	GL_TOGGLE(GL_DITHER, !enable);
}

/* UNUSED */
#if 0
void sdrawXORline(int x0, int y0, int x1, int y1)
{
	if (x0 == x1 && y0 == y1) return;

	set_inverted_drawing(1);

	glBegin(GL_LINES);
	glVertex2i(x0, y0);
	glVertex2i(x1, y1);
	glEnd();

	set_inverted_drawing(0);
}

void sdrawXORline4(int nr, int x0, int y0, int x1, int y1)
{
	static int old[4][2][2];
	static char flags[4] = {0, 0, 0, 0};

	/* with builtin memory, max 4 lines */

	set_inverted_drawing(1);

	glBegin(GL_LINES);
	if (nr == -1) { /* flush */
		for (nr = 0; nr < 4; nr++) {
			if (flags[nr]) {
				glVertex2iv(old[nr][0]);
				glVertex2iv(old[nr][1]);
				flags[nr] = 0;
			}
		}
	}
	else {
		if (nr >= 0 && nr < 4) {
			if (flags[nr]) {
				glVertex2iv(old[nr][0]);
				glVertex2iv(old[nr][1]);
			}

			old[nr][0][0] = x0;
			old[nr][0][1] = y0;
			old[nr][1][0] = x1;
			old[nr][1][1] = y1;

			flags[nr] = 1;
		}

		glVertex2i(x0, y0);
		glVertex2i(x1, y1);
	}
	glEnd();

	set_inverted_drawing(0);
}

void fdrawXORellipse(float xofs, float yofs, float hw, float hh)
{
	if (hw == 0) return;

	set_inverted_drawing(1);

	glPushMatrix();
	glTranslatef(xofs, yofs, 0.0f);
	glScalef(1.0f, hh / hw, 1.0f);
	glutil_draw_lined_arc(0.0, M_PI * 2.0, hw, 20);
	glPopMatrix();

	set_inverted_drawing(0);
}

#endif

void fdrawXORcirc(float xofs, float yofs, float rad)
{
	set_inverted_drawing(1);

	glPushMatrix();
	glTranslatef(xofs, yofs, 0.0);
	glutil_draw_lined_arc(0.0, M_PI * 2.0, rad, 20);
	glPopMatrix();

	set_inverted_drawing(0);
}

void glutil_draw_filled_arc(float start, float angle, float radius, int nsegments)
{
	int i;

	glBegin(GL_TRIANGLE_FAN);
	glVertex2f(0.0, 0.0);
	for (i = 0; i < nsegments; i++) {
		float t = (float) i / (nsegments - 1);
		float cur = start + t * angle;

		glVertex2f(cosf(cur) * radius, sinf(cur) * radius);
	}
	glEnd();
}

void glutil_draw_lined_arc(float start, float angle, float radius, int nsegments)
{
	int i;

	glBegin(GL_LINE_STRIP);
	for (i = 0; i < nsegments; i++) {
		float t = (float) i / (nsegments - 1);
		float cur = start + t * angle;

		glVertex2f(cosf(cur) * radius, sinf(cur) * radius);
	}
	glEnd();
}

float glaGetOneFloat(int param)
{
	GLfloat v;
	glGetFloatv(param, &v);
	return v;
}

int glaGetOneInt(int param)
{
	GLint v;
	glGetIntegerv(param, &v);
	return v;
}

void glaRasterPosSafe2f(float x, float y, float known_good_x, float known_good_y)
{
	GLubyte dummy = 0;

	/* As long as known good coordinates are correct
	 * this is guaranteed to generate an ok raster
	 * position (ignoring potential (real) overflow
	 * issues).
	 */
	glRasterPos2f(known_good_x, known_good_y);

	/* Now shift the raster position to where we wanted
	 * it in the first place using the glBitmap trick.
	 */
	glBitmap(0, 0, 0, 0, x - known_good_x, y - known_good_y, &dummy);
}

static int get_cached_work_texture(int *r_w, int *r_h)
{
	static GLint texid = -1;
	static int tex_w = 256;
	static int tex_h = 256;

	if (texid == -1) {
		glGenTextures(1, (GLuint *)&texid);

		glBindTexture(GL_TEXTURE_2D, texid);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	*r_w = tex_w;
	*r_h = tex_h;
	return texid;
}

void glaDrawPixelsTexScaled_clipping(float x, float y, int img_w, int img_h,
                                     int format, int type, int zoomfilter, void *rect,
                                     float scaleX, float scaleY,
                                     float clip_min_x, float clip_min_y,
                                     float clip_max_x, float clip_max_y)
{
	unsigned char *uc_rect = (unsigned char *) rect;
	const float *f_rect = (float *)rect;
	float xzoom = glaGetOneFloat(GL_ZOOM_X), yzoom = glaGetOneFloat(GL_ZOOM_Y);
	int subpart_x, subpart_y, tex_w, tex_h;
	int seamless, offset_x, offset_y, nsubparts_x, nsubparts_y;
	int texid = get_cached_work_texture(&tex_w, &tex_h);
	int components;
	const bool use_clipping = ((clip_min_x < clip_max_x) && (clip_min_y < clip_max_y));

	/* Specify the color outside this function, and tex will modulate it.
	 * This is useful for changing alpha without using glPixelTransferf()
	 */
	glPixelStorei(GL_UNPACK_ROW_LENGTH, img_w);
	glBindTexture(GL_TEXTURE_2D, texid);

	/* don't want nasty border artifacts */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, zoomfilter);

#if defined(__APPLE__) && 0
	/* [merwin] disable this workaround and see if anyone is affected. If not scrap it! Also at end of this function */
	/* workaround for os x 10.5/10.6 driver bug: http://lists.apple.com/archives/Mac-opengl/2008/Jul/msg00117.html */
	glPixelZoom(1.0f, 1.0f);
#endif

	/* setup seamless 2=on, 0=off */
	seamless = ((tex_w < img_w || tex_h < img_h) && tex_w > 2 && tex_h > 2) ? 2 : 0;

	offset_x = tex_w - seamless;
	offset_y = tex_h - seamless;

	nsubparts_x = (img_w + (offset_x - 1)) / (offset_x);
	nsubparts_y = (img_h + (offset_y - 1)) / (offset_y);

	if (format == GL_RGBA)
		components = 4;
	else if (format == GL_RGB)
		components = 3;
	else if (ELEM(format, GL_LUMINANCE, GL_ALPHA))
		components = 1;
	else {
		BLI_assert(!"Incompatible format passed to glaDrawPixelsTexScaled");
		return;
	}

	if (type == GL_FLOAT) {
		/* need to set internal format to higher range float */

		/* NOTE: this could fail on some drivers, like mesa,
		 *       but currently this code is only used by color
		 *       management stuff which already checks on whether
		 *       it's possible to use GL_RGBA16F_ARB
		 */

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F_ARB, tex_w, tex_h, 0, format, GL_FLOAT, NULL);
	}
	else {
		/* switch to 8bit RGBA for byte buffer */
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_w, tex_h, 0, format, GL_UNSIGNED_BYTE, NULL);
	}

	for (subpart_y = 0; subpart_y < nsubparts_y; subpart_y++) {
		for (subpart_x = 0; subpart_x < nsubparts_x; subpart_x++) {
			int remainder_x = img_w - subpart_x * offset_x;
			int remainder_y = img_h - subpart_y * offset_y;
			int subpart_w = (remainder_x < tex_w) ? remainder_x : tex_w;
			int subpart_h = (remainder_y < tex_h) ? remainder_y : tex_h;
			int offset_left = (seamless && subpart_x != 0) ? 1 : 0;
			int offset_bot = (seamless && subpart_y != 0) ? 1 : 0;
			int offset_right = (seamless && remainder_x > tex_w) ? 1 : 0;
			int offset_top = (seamless && remainder_y > tex_h) ? 1 : 0;
			float rast_x = x + subpart_x * offset_x * xzoom;
			float rast_y = y + subpart_y * offset_y * yzoom;
			/* check if we already got these because we always get 2 more when doing seamless */
			if (subpart_w <= seamless || subpart_h <= seamless)
				continue;

			if (use_clipping) {
				if (rast_x + (float)(subpart_w - offset_right) * xzoom * scaleX < clip_min_x ||
				    rast_y + (float)(subpart_h - offset_top) * yzoom * scaleY < clip_min_y)
				{
					continue;
				}
				if (rast_x + (float)offset_left * xzoom > clip_max_x ||
				    rast_y + (float)offset_bot * yzoom > clip_max_y)
				{
					continue;
				}
			}

			if (type == GL_FLOAT) {
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, subpart_w, subpart_h, format, GL_FLOAT, &f_rect[((size_t)subpart_y) * offset_y * img_w * components + subpart_x * offset_x * components]);

				/* add an extra border of pixels so linear looks ok at edges of full image */
				if (subpart_w < tex_w)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, 0, 1, subpart_h, format, GL_FLOAT, &f_rect[((size_t)subpart_y) * offset_y * img_w * components + (subpart_x * offset_x + subpart_w - 1) * components]);
				if (subpart_h < tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, subpart_h, subpart_w, 1, format, GL_FLOAT, &f_rect[(((size_t)subpart_y) * offset_y + subpart_h - 1) * img_w * components + subpart_x * offset_x * components]);
				if (subpart_w < tex_w && subpart_h < tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, subpart_h, 1, 1, format, GL_FLOAT, &f_rect[(((size_t)subpart_y) * offset_y + subpart_h - 1) * img_w * components + (subpart_x * offset_x + subpart_w - 1) * components]);
			}
			else {
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, subpart_w, subpart_h, format, GL_UNSIGNED_BYTE, &uc_rect[((size_t)subpart_y) * offset_y * img_w * components + subpart_x * offset_x * components]);

				if (subpart_w < tex_w)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, 0, 1, subpart_h, format, GL_UNSIGNED_BYTE, &uc_rect[((size_t)subpart_y) * offset_y * img_w * components + (subpart_x * offset_x + subpart_w - 1) * components]);
				if (subpart_h < tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, subpart_h, subpart_w, 1, format, GL_UNSIGNED_BYTE, &uc_rect[(((size_t)subpart_y) * offset_y + subpart_h - 1) * img_w * components + subpart_x * offset_x * components]);
				if (subpart_w < tex_w && subpart_h < tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, subpart_h, 1, 1, format, GL_UNSIGNED_BYTE, &uc_rect[(((size_t)subpart_y) * offset_y + subpart_h - 1) * img_w * components + (subpart_x * offset_x + subpart_w - 1) * components]);
			}

			GPU_basic_shader_bind(GPU_SHADER_TEXTURE_2D | GPU_SHADER_USE_COLOR);
			glBegin(GL_QUADS);
			glTexCoord2f((float)(0 + offset_left) / tex_w, (float)(0 + offset_bot) / tex_h);
			glVertex2f(rast_x + (float)offset_left * xzoom, rast_y + (float)offset_bot * yzoom);

			glTexCoord2f((float)(subpart_w - offset_right) / tex_w, (float)(0 + offset_bot) / tex_h);
			glVertex2f(rast_x + (float)(subpart_w - offset_right) * xzoom * scaleX, rast_y + (float)offset_bot * yzoom);

			glTexCoord2f((float)(subpart_w - offset_right) / tex_w, (float)(subpart_h - offset_top) / tex_h);
			glVertex2f(rast_x + (float)(subpart_w - offset_right) * xzoom * scaleX, rast_y + (float)(subpart_h - offset_top) * yzoom * scaleY);

			glTexCoord2f((float)(0 + offset_left) / tex_w, (float)(subpart_h - offset_top) / tex_h);
			glVertex2f(rast_x + (float)offset_left * xzoom, rast_y + (float)(subpart_h - offset_top) * yzoom * scaleY);
			glEnd();
			GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
		}
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

#if defined(__APPLE__) && 0
	/* workaround for os x 10.5/10.6 driver bug (above) */
	glPixelZoom(xzoom, yzoom);
#endif
}

void glaDrawPixelsTexScaled(float x, float y, int img_w, int img_h,
                            int format, int type, int zoomfilter, void *rect,
                            float scaleX, float scaleY)
{
	glaDrawPixelsTexScaled_clipping(x, y, img_w, img_h, format, type, zoomfilter, rect,
	                                scaleX, scaleY, 0.0f, 0.0f, 0.0f, 0.0f);
}

void glaDrawPixelsTex(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect)
{
	glaDrawPixelsTexScaled_clipping(x, y, img_w, img_h, format, type, zoomfilter, rect, 1.0f, 1.0f,
	                                0.0f, 0.0f, 0.0f, 0.0f);
}

void glaDrawPixelsTex_clipping(float x, float y, int img_w, int img_h,
                               int format, int type, int zoomfilter, void *rect,
                               float clip_min_x, float clip_min_y, float clip_max_x, float clip_max_y)
{
	glaDrawPixelsTexScaled_clipping(x, y, img_w, img_h, format, type, zoomfilter, rect, 1.0f, 1.0f,
	                                clip_min_x, clip_min_y, clip_max_x, clip_max_y);
}

void glaDrawPixelsSafe(float x, float y, int img_w, int img_h, int row_w, int format, int type, void *rect)
{
	float xzoom = glaGetOneFloat(GL_ZOOM_X);
	float yzoom = glaGetOneFloat(GL_ZOOM_Y);

	/* The pixel space coordinate of the intersection of
	 * the [zoomed] image with the origin.
	 */
	float ix = -x / xzoom;
	float iy = -y / yzoom;

	/* The maximum pixel amounts the image can be cropped
	 * at the lower left without exceeding the origin.
	 */
	int off_x = floor(max_ff(ix, 0.0f));
	int off_y = floor(max_ff(iy, 0.0f));

	/* The zoomed space coordinate of the raster position
	 * (starting at the lower left most unclipped pixel).
	 */
	float rast_x = x + off_x * xzoom;
	float rast_y = y + off_y * yzoom;

	GLfloat viewport[4];
	int draw_w, draw_h;

	/* Determine the smallest number of pixels we need to draw
	 * before the image would go off the upper right corner.
	 *
	 * It may seem this is just an optimization but some graphics
	 * cards (ATI) freak out if there is a large zoom factor and
	 * a large number of pixels off the screen (probably at some
	 * level the number of image pixels to draw is getting multiplied
	 * by the zoom and then clamped). Making sure we draw the
	 * fewest pixels possible keeps everyone mostly happy (still
	 * fails if we zoom in on one really huge pixel so that it
	 * covers the entire screen).
	 */
	glGetFloatv(GL_VIEWPORT, viewport);
	draw_w = min_ii(img_w - off_x, ceil((viewport[2] - rast_x) / xzoom));
	draw_h = min_ii(img_h - off_y, ceil((viewport[3] - rast_y) / yzoom));

	if (draw_w > 0 && draw_h > 0) {

		int bound_options;
		GPU_BASIC_SHADER_DISABLE_AND_STORE(bound_options);

		/* Don't use safe RasterPos (slower) if we can avoid it. */
		if (rast_x >= 0 && rast_y >= 0) {
			glRasterPos2f(rast_x, rast_y);
		}
		else {
			glaRasterPosSafe2f(rast_x, rast_y, 0, 0);
		}

		glPixelStorei(GL_UNPACK_ROW_LENGTH, row_w);
		if (format == GL_LUMINANCE || format == GL_RED) {
			if (type == GL_FLOAT) {
				const float *f_rect = (float *)rect;
				glDrawPixels(draw_w, draw_h, format, type, f_rect + (off_y * row_w + off_x));
			}
			else if (type == GL_INT || type == GL_UNSIGNED_INT) {
				const int *i_rect = (int *)rect;
				glDrawPixels(draw_w, draw_h, format, type, i_rect + (off_y * row_w + off_x));
			}
		}
		else { /* RGBA */
			if (type == GL_FLOAT) {
				const float *f_rect = (float *)rect;
				glDrawPixels(draw_w, draw_h, format, type, f_rect + (off_y * row_w + off_x) * 4);
			}
			else if (type == GL_UNSIGNED_BYTE) {
				unsigned char *uc_rect = (unsigned char *) rect;
				glDrawPixels(draw_w, draw_h, format, type, uc_rect + (off_y * row_w + off_x) * 4);
			}
		}

		glPixelStorei(GL_UNPACK_ROW_LENGTH,  0);

		GPU_BASIC_SHADER_ENABLE_AND_RESTORE(bound_options);
	}
}

/* uses either DrawPixelsSafe or DrawPixelsTex, based on user defined maximum */
void glaDrawPixelsAuto_clipping(float x, float y, int img_w, int img_h,
                                int format, int type, int zoomfilter, void *rect,
                                float clip_min_x, float clip_min_y,
                                float clip_max_x, float clip_max_y)
{
	if (U.image_draw_method != IMAGE_DRAW_METHOD_DRAWPIXELS) {
		glColor4f(1.0, 1.0, 1.0, 1.0);
		glaDrawPixelsTex_clipping(x, y, img_w, img_h, format, type, zoomfilter, rect,
		                          clip_min_x, clip_min_y, clip_max_x, clip_max_y);
	}
	else {
		glaDrawPixelsSafe(x, y, img_w, img_h, img_w, format, type, rect);
	}
}

void glaDrawPixelsAuto(float x, float y, int img_w, int img_h, int format, int type, int zoomfilter, void *rect)
{
	glaDrawPixelsAuto_clipping(x, y, img_w, img_h, format, type, zoomfilter, rect,
	                           0.0f, 0.0f, 0.0f, 0.0f);
}

/* 2D Drawing Assistance */

void glaDefine2DArea(rcti *screen_rect)
{
	const int sc_w = BLI_rcti_size_x(screen_rect) + 1;
	const int sc_h = BLI_rcti_size_y(screen_rect) + 1;

	glViewport(screen_rect->xmin, screen_rect->ymin, sc_w, sc_h);
	glScissor(screen_rect->xmin, screen_rect->ymin, sc_w, sc_h);

	/* The GLA_PIXEL_OFS magic number is to shift the matrix so that
	 * both raster and vertex integer coordinates fall at pixel
	 * centers properly. For a longer discussion see the OpenGL
	 * Programming Guide, Appendix H, Correctness Tips.
	 */

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, sc_w, 0.0, sc_h, -1, 1);
	glTranslatef(GLA_PIXEL_OFS, GLA_PIXEL_OFS, 0.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

#if 0 /* UNUSED */

struct gla2DDrawInfo {
	int orig_vp[4], orig_sc[4];
	float orig_projmat[16], orig_viewmat[16];

	rcti screen_rect;
	rctf world_rect;

	float wo_to_sc[2];
};

void gla2DGetMap(gla2DDrawInfo *di, rctf *rect)
{
	*rect = di->world_rect;
}

void gla2DSetMap(gla2DDrawInfo *di, rctf *rect)
{
	int sc_w, sc_h;
	float wo_w, wo_h;

	di->world_rect = *rect;

	sc_w = BLI_rcti_size_x(&di->screen_rect);
	sc_h = BLI_rcti_size_y(&di->screen_rect);
	wo_w = BLI_rcti_size_x(&di->world_rect);
	wo_h = BLI_rcti_size_y(&di->world_rect);

	di->wo_to_sc[0] = sc_w / wo_w;
	di->wo_to_sc[1] = sc_h / wo_h;
}

/** Save the current OpenGL state and initialize OpenGL for 2D
 * rendering. glaEnd2DDraw should be called on the returned structure
 * to free it and to return OpenGL to its previous state. The
 * scissor rectangle is set to match the viewport.
 *
 * See glaDefine2DArea for an explanation of why this function uses integers.
 *
 * \param screen_rect: The screen rectangle to be used for 2D drawing.
 * \param world_rect: The world rectangle that the 2D area represented
 * by \a screen_rect is supposed to represent. If NULL it is assumed the
 * world has a 1 to 1 mapping to the screen.
 */
gla2DDrawInfo *glaBegin2DDraw(rcti *screen_rect, rctf *world_rect)
{
	gla2DDrawInfo *di = MEM_mallocN(sizeof(*di), "gla2DDrawInfo");
	int sc_w, sc_h;
	float wo_w, wo_h;

	glGetIntegerv(GL_VIEWPORT, (GLint *)di->orig_vp);
	glGetIntegerv(GL_SCISSOR_BOX, (GLint *)di->orig_sc);
	glGetFloatv(GL_PROJECTION_MATRIX, (GLfloat *)di->orig_projmat);
	glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat *)di->orig_viewmat);

	di->screen_rect = *screen_rect;
	if (world_rect) {
		di->world_rect = *world_rect;
	}
	else {
		di->world_rect.xmin = di->screen_rect.xmin;
		di->world_rect.ymin = di->screen_rect.ymin;
		di->world_rect.xmax = di->screen_rect.xmax;
		di->world_rect.ymax = di->screen_rect.ymax;
	}

	sc_w = BLI_rcti_size_x(&di->screen_rect);
	sc_h = BLI_rcti_size_y(&di->screen_rect);
	wo_w = BLI_rcti_size_x(&di->world_rect);
	wo_h = BLI_rcti_size_y(&di->world_rect);

	di->wo_to_sc[0] = sc_w / wo_w;
	di->wo_to_sc[1] = sc_h / wo_h;

	glaDefine2DArea(&di->screen_rect);

	return di;
}

/**
 * Translate the (\a wo_x, \a wo_y) point from world coordinates into screen space.
 */
void gla2DDrawTranslatePt(gla2DDrawInfo *di, float wo_x, float wo_y, int *r_sc_x, int *r_sc_y)
{
	*r_sc_x = (wo_x - di->world_rect.xmin) * di->wo_to_sc[0];
	*r_sc_y = (wo_y - di->world_rect.ymin) * di->wo_to_sc[1];
}

/**
 * Translate the \a world point from world coordinates into screen space.
 */
void gla2DDrawTranslatePtv(gla2DDrawInfo *di, float world[2], int r_screen[2])
{
	screen_r[0] = (world[0] - di->world_rect.xmin) * di->wo_to_sc[0];
	screen_r[1] = (world[1] - di->world_rect.ymin) * di->wo_to_sc[1];
}

/**
 * Restores the previous OpenGL state and frees the auxiliary gla data.
 */
void glaEnd2DDraw(gla2DDrawInfo *di)
{
	glViewport(di->orig_vp[0], di->orig_vp[1], di->orig_vp[2], di->orig_vp[3]);
	glScissor(di->orig_vp[0], di->orig_vp[1], di->orig_vp[2], di->orig_vp[3]);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(di->orig_projmat);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(di->orig_viewmat);

	MEM_freeN(di);
}
#endif


/* Uses current OpenGL state to get view matrices for gluProject/gluUnProject */
void bgl_get_mats(bglMats *mats)
{
	const double badvalue = 1.0e-6;

	glGetDoublev(GL_MODELVIEW_MATRIX, mats->modelview);
	glGetDoublev(GL_PROJECTION_MATRIX, mats->projection);
	glGetIntegerv(GL_VIEWPORT, (GLint *)mats->viewport);

	/* Very strange code here - it seems that certain bad values in the
	 * modelview matrix can cause gluUnProject to give bad results. */
	if (mats->modelview[0] < badvalue &&
	    mats->modelview[0] > -badvalue)
	{
		mats->modelview[0] = 0;
	}
	if (mats->modelview[5] < badvalue &&
	    mats->modelview[5] > -badvalue)
	{
		mats->modelview[5] = 0;
	}

	/* Set up viewport so that gluUnProject will give correct values */
	mats->viewport[0] = 0;
	mats->viewport[1] = 0;
}

/* *************** glPolygonOffset hack ************* */

/**
 * \note \a viewdist is only for ortho at the moment.
 */
void bglPolygonOffset(float viewdist, float dist)
{
	static float winmat[16], offset = 0.0f;

	if (dist != 0.0f) {
		float offs;

		// glEnable(GL_POLYGON_OFFSET_FILL);
		// glPolygonOffset(-1.0, -1.0);

		/* hack below is to mimic polygon offset */
		glMatrixMode(GL_PROJECTION);
		glGetFloatv(GL_PROJECTION_MATRIX, (float *)winmat);

		/* dist is from camera to center point */

		if (winmat[15] > 0.5f) {
#if 1
			offs = 0.00001f * dist * viewdist;  // ortho tweaking
#else
			static float depth_fac = 0.0f;
			if (depth_fac == 0.0f) {
				int depthbits;
				glGetIntegerv(GL_DEPTH_BITS, &depthbits);
				depth_fac = 1.0f / (float)((1 << depthbits) - 1);
			}
			offs = (-1.0 / winmat[10]) * dist * depth_fac;

			UNUSED_VARS(viewdist);
#endif
		}
		else {
			/* This adjustment effectively results in reducing the Z value by 0.25%.
			 *
			 * winmat[14] actually evaluates to `-2 * far * near / (far - near)`,
			 * is very close to -0.2 with default clip range, and is used as the coefficient multiplied by `w / z`,
			 * thus controlling the z dependent part of the depth value.
			 */
			offs = winmat[14] * -0.0025f * dist;
		}

		winmat[14] -= offs;
		offset += offs;

		glLoadMatrixf(winmat);
		glMatrixMode(GL_MODELVIEW);
	}
	else {
		glMatrixMode(GL_PROJECTION);
		winmat[14] += offset;
		offset = 0.0;
		glLoadMatrixf(winmat);
		glMatrixMode(GL_MODELVIEW);
	}
}

/* **** Color management helper functions for GLSL display/transform ***** */

/* Draw given image buffer on a screen using GLSL for display transform */
void glaDrawImBuf_glsl_clipping(ImBuf *ibuf, float x, float y, int zoomfilter,
                                ColorManagedViewSettings *view_settings,
                                ColorManagedDisplaySettings *display_settings,
                                float clip_min_x, float clip_min_y,
                                float clip_max_x, float clip_max_y)
{
	bool force_fallback = false;
	bool need_fallback = true;

	/* Early out */
	if (ibuf->rect == NULL && ibuf->rect_float == NULL)
		return;

	/* Single channel images could not be transformed using GLSL yet */
	force_fallback |= ibuf->channels == 1;

	/* If user decided not to use GLSL, fallback to glaDrawPixelsAuto */
	force_fallback |= (U.image_draw_method != IMAGE_DRAW_METHOD_GLSL);

	/* Try to draw buffer using GLSL display transform */
	if (force_fallback == false) {
		int ok;

		if (ibuf->rect_float) {
			if (ibuf->float_colorspace) {
				ok = IMB_colormanagement_setup_glsl_draw_from_space(view_settings, display_settings,
				                                                    ibuf->float_colorspace,
				                                                    ibuf->dither, true);
			}
			else {
				ok = IMB_colormanagement_setup_glsl_draw(view_settings, display_settings,
				                                         ibuf->dither, true);
			}
		}
		else {
			ok = IMB_colormanagement_setup_glsl_draw_from_space(view_settings, display_settings,
			                                                    ibuf->rect_colorspace,
			                                                    ibuf->dither, false);
		}

		if (ok) {
			glColor4f(1.0, 1.0, 1.0, 1.0);

			if (ibuf->rect_float) {
				int format = 0;

				if (ibuf->channels == 3)
					format = GL_RGB;
				else if (ibuf->channels == 4)
					format = GL_RGBA;
				else
					BLI_assert(!"Incompatible number of channels for GLSL display");

				if (format != 0) {
					glaDrawPixelsTex_clipping(x, y, ibuf->x, ibuf->y, format, GL_FLOAT,
					                          zoomfilter, ibuf->rect_float,
					                          clip_min_x, clip_min_y, clip_max_x, clip_max_y);
				}
			}
			else if (ibuf->rect) {
				/* ibuf->rect is always RGBA */
				glaDrawPixelsTex_clipping(x, y, ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE,
				                          zoomfilter, ibuf->rect,
				                          clip_min_x, clip_min_y, clip_max_x, clip_max_y);
			}

			IMB_colormanagement_finish_glsl_draw();

			need_fallback = false;
		}
	}

	/* In case GLSL failed or not usable, fallback to glaDrawPixelsAuto */
	if (need_fallback) {
		unsigned char *display_buffer;
		void *cache_handle;

		display_buffer = IMB_display_buffer_acquire(ibuf, view_settings, display_settings, &cache_handle);

		if (display_buffer) {
			glaDrawPixelsAuto_clipping(x, y, ibuf->x, ibuf->y, GL_RGBA, GL_UNSIGNED_BYTE,
			                           zoomfilter, display_buffer,
			                           clip_min_x, clip_min_y, clip_max_x, clip_max_y);
		}

		IMB_display_buffer_release(cache_handle);
	}
}

void glaDrawImBuf_glsl(ImBuf *ibuf, float x, float y, int zoomfilter,
                       ColorManagedViewSettings *view_settings,
                       ColorManagedDisplaySettings *display_settings)
{
	glaDrawImBuf_glsl_clipping(ibuf, x, y, zoomfilter, view_settings, display_settings,
	                           0.0f, 0.0f, 0.0f, 0.0f);
}

void glaDrawImBuf_glsl_ctx_clipping(const bContext *C,
                                    ImBuf *ibuf,
                                    float x, float y,
                                    int zoomfilter,
                                    float clip_min_x, float clip_min_y,
                                    float clip_max_x, float clip_max_y)
{
	ColorManagedViewSettings *view_settings;
	ColorManagedDisplaySettings *display_settings;

	IMB_colormanagement_display_settings_from_ctx(C, &view_settings, &display_settings);

	glaDrawImBuf_glsl_clipping(ibuf, x, y, zoomfilter, view_settings, display_settings,
	                           clip_min_x, clip_min_y, clip_max_x, clip_max_y);
}

void glaDrawImBuf_glsl_ctx(const bContext *C, ImBuf *ibuf, float x, float y, int zoomfilter)
{
	glaDrawImBuf_glsl_ctx_clipping(C, ibuf, x, y, zoomfilter, 0.0f, 0.0f, 0.0f, 0.0f);
}

void cpack(unsigned int x)
{
	glColor3ub(( (x)        & 0xFF),
	           (((x) >>  8) & 0xFF),
	           (((x) >> 16) & 0xFF));
}

void glaDrawBorderCorners(const rcti *border, float zoomx, float zoomy)
{
	float delta_x = 4.0f * UI_DPI_FAC / zoomx;
	float delta_y = 4.0f * UI_DPI_FAC / zoomy;

	delta_x = min_ff(delta_x, border->xmax - border->xmin);
	delta_y = min_ff(delta_y, border->ymax - border->ymin);

	/* left bottom corner */
	glBegin(GL_LINE_STRIP);
	glVertex2f(border->xmin, border->ymin + delta_y);
	glVertex2f(border->xmin, border->ymin);
	glVertex2f(border->xmin + delta_x, border->ymin);
	glEnd();

	/* left top corner */
	glBegin(GL_LINE_STRIP);
	glVertex2f(border->xmin, border->ymax - delta_y);
	glVertex2f(border->xmin, border->ymax);
	glVertex2f(border->xmin + delta_x, border->ymax);
	glEnd();

	/* right bottom corner */
	glBegin(GL_LINE_STRIP);
	glVertex2f(border->xmax - delta_x, border->ymin);
	glVertex2f(border->xmax, border->ymin);
	glVertex2f(border->xmax, border->ymin + delta_y);
	glEnd();

	/* right top corner */
	glBegin(GL_LINE_STRIP);
	glVertex2f(border->xmax - delta_x, border->ymax);
	glVertex2f(border->xmax, border->ymax);
	glVertex2f(border->xmax, border->ymax - delta_y);
	glEnd();
}

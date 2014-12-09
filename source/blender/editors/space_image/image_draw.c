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
 * Contributor(s): Blender Foundation, 2002-2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_image/image_draw.c
 *  \ingroup spimage
 */


#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_brush_types.h"
#include "DNA_mask_types.h"

#include "PIL_time.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_threads.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"
#include "IMB_moviecache.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_paint.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLF_api.h"

#include "ED_gpencil.h"
#include "ED_image.h"
#include "ED_mask.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RE_pipeline.h"
#include "RE_engine.h"

#include "image_intern.h"

static void draw_render_info(Scene *scene, Image *ima, ARegion *ar, float zoomx, float zoomy)
{
	RenderResult *rr;
	Render *re = RE_GetRender(scene->id.name);
	RenderData *rd = RE_engine_get_render_data(re);

	rr = BKE_image_acquire_renderresult(scene, ima);

	if (rr && rr->text) {
		float fill_color[4] = {0.0f, 0.0f, 0.0f, 0.25f};
		ED_region_info_draw(ar, rr->text, 1, fill_color);
	}

	BKE_image_release_renderresult(scene, ima);

	if (re) {
		int total_tiles;
		rcti *tiles;

		RE_engine_get_current_tiles(re, &total_tiles, &tiles);

		if (total_tiles) {
			int i, x, y;
			rcti *tile;

			/* find window pixel coordinates of origin */
			UI_view2d_view_to_region(&ar->v2d, 0.0f, 0.0f, &x, &y);

			glPushMatrix();
			glTranslatef(x, y, 0.0f);
			glScalef(zoomx, zoomy, 1.0f);

			if (rd->mode & R_BORDER) {
				glTranslatef((int)(-rd->border.xmin * rd->xsch * rd->size / 100.0f),
				             (int)(-rd->border.ymin * rd->ysch * rd->size / 100.0f),
				             0.0f);
			}

			UI_ThemeColor(TH_FACE_SELECT);

			for (i = 0, tile = tiles; i < total_tiles; i++, tile++) {
				glaDrawBorderCorners(tile, zoomx, zoomy);
			}

			MEM_freeN(tiles);

			glPopMatrix();
		}
	}
}

/* used by node view too */
void ED_image_draw_info(Scene *scene, ARegion *ar, bool color_manage, bool use_default_view, int channels, int x, int y,
                        const unsigned char cp[4], const float fp[4], const float linearcol[4], int *zp, float *zpf)
{
	rcti color_rect;
	char str[256];
	int dx = 6;
	const int dy = 0.3f * UI_UNIT_Y;
	/* text colors */
	/* XXX colored text not allowed in Blender UI */
#if 0
	unsigned char red[3] = {255, 50, 50};
	unsigned char green[3] = {0, 255, 0};
	unsigned char blue[3] = {100, 100, 255};
#else
	unsigned char red[3] = {255, 255, 255};
	unsigned char green[3] = {255, 255, 255};
	unsigned char blue[3] = {255, 255, 255};
#endif
	float hue = 0, sat = 0, val = 0, lum = 0, u = 0, v = 0;
	float col[4], finalcol[4];

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	/* noisy, high contrast make impossible to read if lower alpha is used. */
	glColor4ub(0, 0, 0, 190);
	glRecti(0.0, 0.0, BLI_rcti_size_x(&ar->winrct) + 1, UI_UNIT_Y);
	glDisable(GL_BLEND);

	BLF_size(blf_mono_font, 11 * U.pixelsize, U.dpi);

	glColor3ub(255, 255, 255);
	BLI_snprintf(str, sizeof(str), "X:%-4d  Y:%-4d |", x, y);
	BLF_position(blf_mono_font, dx, dy, 0);
	BLF_draw_ascii(blf_mono_font, str, sizeof(str));
	dx += BLF_width(blf_mono_font, str, sizeof(str));

	if (zp) {
		glColor3ub(255, 255, 255);
		BLI_snprintf(str, sizeof(str), " Z:%-.4f |", 0.5f + 0.5f * (((float)*zp) / (float)0x7fffffff));
		BLF_position(blf_mono_font, dx, dy, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str, sizeof(str));
	}
	if (zpf) {
		glColor3ub(255, 255, 255);
		BLI_snprintf(str, sizeof(str), " Z:%-.3f |", *zpf);
		BLF_position(blf_mono_font, dx, dy, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str, sizeof(str));
	}

	if (channels >= 3) {
		glColor3ubv(red);
		if (fp)
			BLI_snprintf(str, sizeof(str), "  R:%-.5f", fp[0]);
		else if (cp)
			BLI_snprintf(str, sizeof(str), "  R:%-3d", cp[0]);
		else
			BLI_snprintf(str, sizeof(str), "  R:-");
		BLF_position(blf_mono_font, dx, dy, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str, sizeof(str));
		
		glColor3ubv(green);
		if (fp)
			BLI_snprintf(str, sizeof(str), "  G:%-.5f", fp[1]);
		else if (cp)
			BLI_snprintf(str, sizeof(str), "  G:%-3d", cp[1]);
		else
			BLI_snprintf(str, sizeof(str), "  G:-");
		BLF_position(blf_mono_font, dx, dy, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str, sizeof(str));
		
		glColor3ubv(blue);
		if (fp)
			BLI_snprintf(str, sizeof(str), "  B:%-.5f", fp[2]);
		else if (cp)
			BLI_snprintf(str, sizeof(str), "  B:%-3d", cp[2]);
		else
			BLI_snprintf(str, sizeof(str), "  B:-");
		BLF_position(blf_mono_font, dx, dy, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str, sizeof(str));
		
		if (channels == 4) {
			glColor3ub(255, 255, 255);
			if (fp)
				BLI_snprintf(str, sizeof(str), "  A:%-.4f", fp[3]);
			else if (cp)
				BLI_snprintf(str, sizeof(str), "  A:%-3d", cp[3]);
			else
				BLI_snprintf(str, sizeof(str), "- ");
			BLF_position(blf_mono_font, dx, dy, 0);
			BLF_draw_ascii(blf_mono_font, str, sizeof(str));
			dx += BLF_width(blf_mono_font, str, sizeof(str));
		}

		if (color_manage) {
			float rgba[4];

			copy_v3_v3(rgba, linearcol);
			if (channels == 3)
				rgba[3] = 1.0f;
			else
				rgba[3] = linearcol[3];

			if (use_default_view)
				IMB_colormanagement_pixel_to_display_space_v4(rgba, rgba,  NULL, &scene->display_settings);
			else
				IMB_colormanagement_pixel_to_display_space_v4(rgba, rgba,  &scene->view_settings, &scene->display_settings);

			BLI_snprintf(str, sizeof(str), "  |  CM  R:%-.4f  G:%-.4f  B:%-.4f", rgba[0], rgba[1], rgba[2]);
			BLF_position(blf_mono_font, dx, dy, 0);
			BLF_draw_ascii(blf_mono_font, str, sizeof(str));
			dx += BLF_width(blf_mono_font, str, sizeof(str));
		}
	}
	
	/* color rectangle */
	if (channels == 1) {
		if (fp) {
			col[0] = col[1] = col[2] = fp[0];
		}
		else if (cp) {
			col[0] = col[1] = col[2] = (float)cp[0] / 255.0f;
		}
		else {
			col[0] = col[1] = col[2] = 0.0f;
		}
		col[3] = 1.0f;
	}
	else if (channels == 3) {
		copy_v3_v3(col, linearcol);
		col[3] = 1.0f;
	}
	else if (channels == 4) {
		copy_v4_v4(col, linearcol);
	}
	else {
		BLI_assert(0);
		zero_v4(col);
	}

	if (color_manage) {
		if (use_default_view)
			IMB_colormanagement_pixel_to_display_space_v4(finalcol, col,  NULL, &scene->display_settings);
		else
			IMB_colormanagement_pixel_to_display_space_v4(finalcol, col,  &scene->view_settings, &scene->display_settings);
	}
	else {
		copy_v4_v4(finalcol, col);
	}

	glDisable(GL_BLEND);
	dx += 0.25f * UI_UNIT_X;

	BLI_rcti_init(&color_rect, dx, dx + (1.5f * UI_UNIT_X), 0.15f * UI_UNIT_Y, 0.85f * UI_UNIT_Y);

	if (channels == 4) {
		rcti color_rect_half;
		int color_quater_x, color_quater_y;

		color_rect_half = color_rect;
		color_rect_half.xmax = BLI_rcti_cent_x(&color_rect);
		glRecti(color_rect.xmin, color_rect.ymin, color_rect.xmax, color_rect.ymax);

		color_rect_half = color_rect;
		color_rect_half.xmin = BLI_rcti_cent_x(&color_rect);

		color_quater_x = BLI_rcti_cent_x(&color_rect_half);
		color_quater_y = BLI_rcti_cent_y(&color_rect_half);

		glColor4ub(UI_ALPHA_CHECKER_DARK, UI_ALPHA_CHECKER_DARK, UI_ALPHA_CHECKER_DARK, 255);
		glRecti(color_rect_half.xmin, color_rect_half.ymin, color_rect_half.xmax, color_rect_half.ymax);

		glColor4ub(UI_ALPHA_CHECKER_LIGHT, UI_ALPHA_CHECKER_LIGHT, UI_ALPHA_CHECKER_LIGHT, 255);
		glRecti(color_quater_x, color_quater_y, color_rect_half.xmax, color_rect_half.ymax);
		glRecti(color_rect_half.xmin, color_rect_half.ymin, color_quater_x, color_quater_y);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4f(UNPACK3(finalcol), fp ? fp[3] : (cp[3] / 255.0f));
		glRecti(color_rect.xmin, color_rect.ymin, color_rect.xmax, color_rect.ymax);
		glDisable(GL_BLEND);
	}
	else {
		glColor3fv(finalcol);
		glRecti(color_rect.xmin, color_rect.ymin, color_rect.xmax, color_rect.ymax);
	}

	/* draw outline */
	glColor3ub(128, 128, 128);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glRecti(color_rect.xmin, color_rect.ymin, color_rect.xmax, color_rect.ymax);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	dx += 1.75f * UI_UNIT_X;

	glColor3ub(255, 255, 255);
	if (channels == 1) {
		if (fp) {
			rgb_to_hsv(fp[0], fp[0], fp[0], &hue, &sat, &val);
			rgb_to_yuv(fp[0], fp[0], fp[0], &lum, &u, &v);
		}
		else if (cp) {
			rgb_to_hsv((float)cp[0] / 255.0f, (float)cp[0] / 255.0f, (float)cp[0] / 255.0f, &hue, &sat, &val);
			rgb_to_yuv((float)cp[0] / 255.0f, (float)cp[0] / 255.0f, (float)cp[0] / 255.0f, &lum, &u, &v);
		}
		
		BLI_snprintf(str, sizeof(str), "V:%-.4f", val);
		BLF_position(blf_mono_font, dx, dy, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str, sizeof(str));

		BLI_snprintf(str, sizeof(str), "   L:%-.4f", lum);
		BLF_position(blf_mono_font, dx, dy, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str, sizeof(str));
	}
	else if (channels >= 3) {
		rgb_to_hsv(finalcol[0], finalcol[1], finalcol[2], &hue, &sat, &val);
		rgb_to_yuv(finalcol[0], finalcol[1], finalcol[2], &lum, &u, &v);

		BLI_snprintf(str, sizeof(str), "H:%-.4f", hue);
		BLF_position(blf_mono_font, dx, dy, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str, sizeof(str));

		BLI_snprintf(str, sizeof(str), "  S:%-.4f", sat);
		BLF_position(blf_mono_font, dx, dy, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str, sizeof(str));

		BLI_snprintf(str, sizeof(str), "  V:%-.4f", val);
		BLF_position(blf_mono_font, dx, dy, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str, sizeof(str));

		BLI_snprintf(str, sizeof(str), "   L:%-.4f", lum);
		BLF_position(blf_mono_font, dx, dy, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str, sizeof(str));
	}

	(void)dx;
}

/* image drawing */

static void sima_draw_alpha_pixels(float x1, float y1, int rectx, int recty, unsigned int *recti)
{
	
	/* swap bytes, so alpha is most significant one, then just draw it as luminance int */
	if (ENDIAN_ORDER == B_ENDIAN)
		glPixelStorei(GL_UNPACK_SWAP_BYTES, 1);

	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_LUMINANCE, GL_UNSIGNED_INT, recti);
	glPixelStorei(GL_UNPACK_SWAP_BYTES, 0);
}

static void sima_draw_alpha_pixelsf(float x1, float y1, int rectx, int recty, float *rectf)
{
	float *trectf = MEM_mallocN(rectx * recty * 4, "temp");
	int a, b;
	
	for (a = rectx * recty - 1, b = 4 * a + 3; a >= 0; a--, b -= 4)
		trectf[a] = rectf[b];
	
	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_LUMINANCE, GL_FLOAT, trectf);
	MEM_freeN(trectf);
	/* ogl trick below is slower... (on ATI 9600) */
//	glColorMask(1, 0, 0, 0);
//	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_RGBA, GL_FLOAT, rectf + 3);
//	glColorMask(0, 1, 0, 0);
//	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_RGBA, GL_FLOAT, rectf + 2);
//	glColorMask(0, 0, 1, 0);
//	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_RGBA, GL_FLOAT, rectf + 1);
//	glColorMask(1, 1, 1, 1);
}

static void sima_draw_zbuf_pixels(float x1, float y1, int rectx, int recty, int *recti)
{
	/* zbuffer values are signed, so we need to shift color range */
	glPixelTransferf(GL_RED_SCALE, 0.5f);
	glPixelTransferf(GL_GREEN_SCALE, 0.5f);
	glPixelTransferf(GL_BLUE_SCALE, 0.5f);
	glPixelTransferf(GL_RED_BIAS, 0.5f);
	glPixelTransferf(GL_GREEN_BIAS, 0.5f);
	glPixelTransferf(GL_BLUE_BIAS, 0.5f);
	
	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_LUMINANCE, GL_INT, recti);
	
	glPixelTransferf(GL_RED_SCALE, 1.0f);
	glPixelTransferf(GL_GREEN_SCALE, 1.0f);
	glPixelTransferf(GL_BLUE_SCALE, 1.0f);
	glPixelTransferf(GL_RED_BIAS, 0.0f);
	glPixelTransferf(GL_GREEN_BIAS, 0.0f);
	glPixelTransferf(GL_BLUE_BIAS, 0.0f);
}

static void sima_draw_zbuffloat_pixels(Scene *scene, float x1, float y1, int rectx, int recty, float *rect_float)
{
	float bias, scale, *rectf, clipend;
	int a;
	
	if (scene->camera && scene->camera->type == OB_CAMERA) {
		bias = ((Camera *)scene->camera->data)->clipsta;
		clipend = ((Camera *)scene->camera->data)->clipend;
		scale = 1.0f / (clipend - bias);
	}
	else {
		bias = 0.1f;
		scale = 0.01f;
		clipend = 100.0f;
	}
	
	rectf = MEM_mallocN(rectx * recty * 4, "temp");
	for (a = rectx * recty - 1; a >= 0; a--) {
		if (rect_float[a] > clipend)
			rectf[a] = 0.0f;
		else if (rect_float[a] < bias)
			rectf[a] = 1.0f;
		else {
			rectf[a] = 1.0f - (rect_float[a] - bias) * scale;
			rectf[a] *= rectf[a];
		}
	}
	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_LUMINANCE, GL_FLOAT, rectf);
	
	MEM_freeN(rectf);
}

static void draw_image_buffer(const bContext *C, SpaceImage *sima, ARegion *ar, Scene *scene, ImBuf *ibuf, float fx, float fy, float zoomx, float zoomy)
{
	int x, y;

	/* set zoom */
	glPixelZoom(zoomx, zoomy);

	glaDefine2DArea(&ar->winrct);
	
	/* find window pixel coordinates of origin */
	UI_view2d_view_to_region(&ar->v2d, fx, fy, &x, &y);

	/* this part is generic image display */
	if (sima->flag & SI_SHOW_ALPHA) {
		if (ibuf->rect)
			sima_draw_alpha_pixels(x, y, ibuf->x, ibuf->y, ibuf->rect);
		else if (ibuf->rect_float && ibuf->channels == 4)
			sima_draw_alpha_pixelsf(x, y, ibuf->x, ibuf->y, ibuf->rect_float);
	}
	else if (sima->flag & SI_SHOW_ZBUF && (ibuf->zbuf || ibuf->zbuf_float || (ibuf->channels == 1))) {
		if (ibuf->zbuf)
			sima_draw_zbuf_pixels(x, y, ibuf->x, ibuf->y, ibuf->zbuf);
		else if (ibuf->zbuf_float)
			sima_draw_zbuffloat_pixels(scene, x, y, ibuf->x, ibuf->y, ibuf->zbuf_float);
		else if (ibuf->channels == 1)
			sima_draw_zbuffloat_pixels(scene, x, y, ibuf->x, ibuf->y, ibuf->rect_float);
	}
	else {
		if (sima->flag & SI_USE_ALPHA) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			fdrawcheckerboard(x, y, x + ibuf->x * zoomx, y + ibuf->y * zoomy);
		}

		glaDrawImBuf_glsl_ctx(C, ibuf, x, y, GL_NEAREST);

		if (sima->flag & SI_USE_ALPHA)
			glDisable(GL_BLEND);
	}

	/* reset zoom */
	glPixelZoom(1.0f, 1.0f);
}

static unsigned int *get_part_from_buffer(unsigned int *buffer, int width, short startx, short starty, short endx, short endy)
{
	unsigned int *rt, *rp, *rectmain;
	short y, heigth, len;

	/* the right offset in rectot */

	rt = buffer + (starty * width + startx);

	len = (endx - startx);
	heigth = (endy - starty);

	rp = rectmain = MEM_mallocN(heigth * len * sizeof(int), "rect");
	
	for (y = 0; y < heigth; y++) {
		memcpy(rp, rt, len * 4);
		rt += width;
		rp += len;
	}
	return rectmain;
}

static void draw_image_buffer_tiled(SpaceImage *sima, ARegion *ar, Scene *scene, Image *ima, ImBuf *ibuf, float fx, float fy, float zoomx, float zoomy)
{
	unsigned char *display_buffer;
	unsigned int *rect;
	int dx, dy, sx, sy, x, y;
	void *cache_handle;

	/* verify valid values, just leave this a while */
	if (ima->xrep < 1) return;
	if (ima->yrep < 1) return;

	if (ima->flag & IMA_VIEW_AS_RENDER)
		display_buffer = IMB_display_buffer_acquire(ibuf, &scene->view_settings, &scene->display_settings, &cache_handle);
	else
		display_buffer = IMB_display_buffer_acquire(ibuf, NULL, &scene->display_settings, &cache_handle);

	if (!display_buffer)
		return;

	glPixelZoom(zoomx, zoomy);

	if (sima->curtile >= ima->xrep * ima->yrep)
		sima->curtile = ima->xrep * ima->yrep - 1;
	
	/* retrieve part of image buffer */
	dx = max_ii(ibuf->x / ima->xrep, 1);
	dy = max_ii(ibuf->y / ima->yrep, 1);
	sx = (sima->curtile % ima->xrep) * dx;
	sy = (sima->curtile / ima->xrep) * dy;
	rect = get_part_from_buffer((unsigned int *)display_buffer, ibuf->x, sx, sy, sx + dx, sy + dy);
	
	/* draw repeated */
	for (sy = 0; sy + dy <= ibuf->y; sy += dy) {
		for (sx = 0; sx + dx <= ibuf->x; sx += dx) {
			UI_view2d_view_to_region(&ar->v2d, fx + (float)sx / (float)ibuf->x, fy + (float)sy / (float)ibuf->y, &x, &y);

			glaDrawPixelsSafe(x, y, dx, dy, dx, GL_RGBA, GL_UNSIGNED_BYTE, rect);
		}
	}

	glPixelZoom(1.0f, 1.0f);

	IMB_display_buffer_release(cache_handle);

	MEM_freeN(rect);
}

static void draw_image_buffer_repeated(const bContext *C, SpaceImage *sima, ARegion *ar, Scene *scene, Image *ima, ImBuf *ibuf, float zoomx, float zoomy)
{
	const double time_current = PIL_check_seconds_timer();

	const int xmax = ceil(ar->v2d.cur.xmax);
	const int ymax = ceil(ar->v2d.cur.ymax);
	const int xmin = floor(ar->v2d.cur.xmin);
	const int ymin = floor(ar->v2d.cur.ymin);

	int x;

	for (x = xmin; x < xmax; x++) {
		int y;
		for (y = ymin; y < ymax; y++) {
			if (ima && (ima->tpageflag & IMA_TILES))
				draw_image_buffer_tiled(sima, ar, scene, ima, ibuf, x, y, zoomx, zoomy);
			else
				draw_image_buffer(C, sima, ar, scene, ibuf, x, y, zoomx, zoomy);

			/* only draw until running out of time */
			if ((PIL_check_seconds_timer() - time_current) > 0.25)
				return;
		}
	}
}

/* draw uv edit */

/* draw grease pencil */
void draw_image_grease_pencil(bContext *C, bool onlyv2d)
{
	/* draw in View2D space? */
	if (onlyv2d) {
		/* draw grease-pencil ('image' strokes) */
		ED_gpencil_draw_2dimage(C);
	}
	else {
		/* assume that UI_view2d_restore(C) has been called... */
		//SpaceImage *sima = (SpaceImage *)CTX_wm_space_data(C);
		
		/* draw grease-pencil ('screen' strokes) */
		ED_gpencil_draw_view2d(C, 0);
	}
}

void draw_image_sample_line(SpaceImage *sima)
{
	if (sima->sample_line_hist.flag & HISTO_FLAG_SAMPLELINE) {
		Histogram *hist = &sima->sample_line_hist;

		glBegin(GL_LINES);
		glColor3ub(0, 0, 0);
		glVertex2fv(hist->co[0]);
		glVertex2fv(hist->co[1]);
		glEnd();

		setlinestyle(1);
		glBegin(GL_LINES);
		glColor3ub(255, 255, 255);
		glVertex2fv(hist->co[0]);
		glVertex2fv(hist->co[1]);
		glEnd();
		setlinestyle(0);

	}
}

/* XXX becomes WM paint cursor */
#if 0
static void draw_image_view_tool(Scene *scene)
{
	ToolSettings *settings = scene->toolsettings;
	Brush *brush = settings->imapaint.brush;
	int mval[2];
	float radius;
	int draw = 0;

	if (brush) {
		if (settings->imapaint.flag & IMAGEPAINT_DRAWING) {
			if (settings->imapaint.flag & IMAGEPAINT_DRAW_TOOL_DRAWING)
				draw = 1;
		}
		else if (settings->imapaint.flag & IMAGEPAINT_DRAW_TOOL)
			draw = 1;
		
		if (draw) {
			getmouseco_areawin(mval);

			radius = BKE_brush_size_get(brush) * G.sima->zoom;
			fdrawXORcirc(mval[0], mval[1], radius);

			if (brush->innerradius != 1.0) {
				radius *= brush->innerradius;
				fdrawXORcirc(mval[0], mval[1], radius);
			}
		}
	}
}
#endif

static unsigned char *get_alpha_clone_image(const bContext *C, Scene *scene, int *width, int *height)
{
	Brush *brush = BKE_paint_brush(&scene->toolsettings->imapaint.paint);
	ImBuf *ibuf;
	unsigned int size, alpha;
	unsigned char *display_buffer;
	unsigned char *rect, *cp;
	void *cache_handle;

	if (!brush || !brush->clone.image)
		return NULL;
	
	ibuf = BKE_image_acquire_ibuf(brush->clone.image, NULL, NULL);

	if (!ibuf)
		return NULL;

	display_buffer = IMB_display_buffer_acquire_ctx(C, ibuf, &cache_handle);

	if (!display_buffer) {
		BKE_image_release_ibuf(brush->clone.image, ibuf, NULL);
		IMB_display_buffer_release(cache_handle);

		return NULL;
	}

	rect = MEM_dupallocN(display_buffer);

	IMB_display_buffer_release(cache_handle);

	if (!rect) {
		BKE_image_release_ibuf(brush->clone.image, ibuf, NULL);
		return NULL;
	}

	*width = ibuf->x;
	*height = ibuf->y;

	size = (*width) * (*height);
	alpha = (unsigned char)255 * brush->clone.alpha;
	cp = rect;

	while (size-- > 0) {
		cp[3] = alpha;
		cp += 4;
	}

	BKE_image_release_ibuf(brush->clone.image, ibuf, NULL);

	return rect;
}

static void draw_image_paint_helpers(const bContext *C, ARegion *ar, Scene *scene, float zoomx, float zoomy)
{
	Brush *brush;
	int x, y, w, h;
	unsigned char *clonerect;

	brush = BKE_paint_brush(&scene->toolsettings->imapaint.paint);

	if (brush && (brush->imagepaint_tool == PAINT_TOOL_CLONE)) {
		/* this is not very efficient, but glDrawPixels doesn't allow
		 * drawing with alpha */
		clonerect = get_alpha_clone_image(C, scene, &w, &h);

		if (clonerect) {
			UI_view2d_view_to_region(&ar->v2d, brush->clone.offset[0], brush->clone.offset[1], &x, &y);

			glPixelZoom(zoomx, zoomy);

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glaDrawPixelsSafe(x, y, w, h, w, GL_RGBA, GL_UNSIGNED_BYTE, clonerect);
			glDisable(GL_BLEND);

			glPixelZoom(1.0, 1.0);

			MEM_freeN(clonerect);
		}
	}
}

/* draw main image area */

void draw_image_main(const bContext *C, ARegion *ar)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	Image *ima;
	ImBuf *ibuf;
	float zoomx, zoomy;
	bool show_viewer, show_render, show_paint;
	void *lock;

	/* XXX can we do this in refresh? */
#if 0
	what_image(sima);
	
	if (sima->image) {
		ED_image_get_aspect(sima->image, &xuser_asp, &yuser_asp);
		
		/* UGLY hack? until now iusers worked fine... but for flipbook viewer we need this */
		if (sima->image->type == IMA_TYPE_COMPOSITE) {
			ImageUser *iuser = ntree_get_active_iuser(scene->nodetree);
			if (iuser) {
				BKE_image_user_calc_imanr(iuser, scene->r.cfra, 0);
				sima->iuser = *iuser;
			}
		}
		/* and we check for spare */
		ibuf = ED_space_image_buffer(sima);
	}
#endif

	/* retrieve the image and information about it */
	ima = ED_space_image(sima);
	ED_space_image_get_zoom(sima, ar, &zoomx, &zoomy);

	show_viewer = (ima && ima->source == IMA_SRC_VIEWER) != 0;
	show_render = (show_viewer && ima->type == IMA_TYPE_R_RESULT) != 0;
	show_paint = (ima && (sima->mode == SI_MODE_PAINT) && (show_viewer == false) && (show_render == false));

	if (show_viewer) {
		/* use locked draw for drawing viewer image buffer since the compositor
		 * is running in separated thread and compositor could free this buffers.
		 * other images are not modifying in such a way so they does not require
		 * lock (sergey)
		 */
		BLI_lock_thread(LOCK_DRAW_IMAGE);
	}

	ibuf = ED_space_image_acquire_buffer(sima, &lock);

	/* draw the image or grid */
	if (ibuf == NULL)
		ED_region_grid_draw(ar, zoomx, zoomy);
	else if (sima->flag & SI_DRAW_TILE)
		draw_image_buffer_repeated(C, sima, ar, scene, ima, ibuf, zoomx, zoomy);
	else if (ima && (ima->tpageflag & IMA_TILES))
		draw_image_buffer_tiled(sima, ar, scene, ima, ibuf, 0.0f, 0.0, zoomx, zoomy);
	else
		draw_image_buffer(C, sima, ar, scene, ibuf, 0.0f, 0.0f, zoomx, zoomy);

	/* paint helpers */
	if (show_paint)
		draw_image_paint_helpers(C, ar, scene, zoomx, zoomy);

	/* XXX integrate this code */
#if 0
	if (ibuf) {
		float xoffs = 0.0f, yoffs = 0.0f;
		
		if (image_preview_active(sa, &xim, &yim)) {
			xoffs = scene->r.disprect.xmin;
			yoffs = scene->r.disprect.ymin;
			glColor3ub(0, 0, 0);
			calc_image_view(sima, 'f');
			myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
			glRectf(0.0f, 0.0f, 1.0f, 1.0f);
			glLoadIdentity();
		}
	}
#endif

	ED_space_image_release_buffer(sima, ibuf, lock);

	if (show_viewer) {
		BLI_unlock_thread(LOCK_DRAW_IMAGE);
	}

	/* render info */
	if (ima && show_render)
		draw_render_info(sima->iuser.scene, ima, ar, zoomx, zoomy);
}

bool ED_space_image_show_cache(SpaceImage *sima)
{
	Image *image = ED_space_image(sima);
	Mask *mask = NULL;
	if (sima->mode == SI_MODE_MASK) {
		mask = ED_space_image_get_mask(sima);
	}
	if (image == NULL && mask == NULL) {
		return false;
	}
	if (mask == NULL) {
		return ELEM(image->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE);
	}
	return true;
}

void draw_image_cache(const bContext *C, ARegion *ar)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Scene *scene = CTX_data_scene(C);
	Image *image = ED_space_image(sima);
	float x, cfra = CFRA, sfra = SFRA, efra = EFRA, framelen = ar->winx / (efra - sfra + 1);
	Mask *mask = NULL;

	if (!ED_space_image_show_cache(sima)) {
		return;
	}

	if (sima->mode == SI_MODE_MASK) {
		mask = ED_space_image_get_mask(sima);
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* Draw cache background. */
	ED_region_cache_draw_background(ar);

	/* Draw cached segments. */
	if (image != NULL && image->cache != NULL && ELEM(image->source, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
		int num_segments = 0;
		int *points = NULL;

		IMB_moviecache_get_cache_segments(image->cache, IMB_PROXY_NONE, 0, &num_segments, &points);
		ED_region_cache_draw_cached_segments(ar, num_segments, points, sfra + sima->iuser.offset, efra + sima->iuser.offset);
	}

	glDisable(GL_BLEND);

	/* Draw current frame. */
	x = (cfra - sfra) / (efra - sfra + 1) * ar->winx;

	UI_ThemeColor(TH_CFRAME);
	glRecti(x, 0, x + ceilf(framelen), 8 * UI_DPI_FAC);
	ED_region_cache_draw_curfra_label(cfra, x, 8.0f * UI_DPI_FAC);

	if (mask != NULL) {
		ED_mask_draw_frames(mask, ar, cfra, sfra, efra);
	}
}

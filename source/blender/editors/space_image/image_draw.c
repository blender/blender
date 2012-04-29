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

#include "PIL_time.h"

#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_paint.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLF_api.h"

#include "ED_gpencil.h"
#include "ED_image.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"


#include "RE_pipeline.h"

#include "image_intern.h"

#define HEADER_HEIGHT 18

static void image_verify_buffer_float(Image *ima, ImBuf *ibuf, int color_manage)
{
	/* detect if we need to redo the curve map.
	 * ibuf->rect is zero for compositor and render results after change 
	 * convert to 32 bits always... drawing float rects isn't supported well (atis)
	 *
	 * NOTE: if float buffer changes, we have to manually remove the rect
	 */

	if (ibuf->rect_float && (ibuf->rect == NULL || (ibuf->userflags & IB_RECT_INVALID)) ) {
		if (color_manage) {
			if (ima && ima->source == IMA_SRC_VIEWER)
				ibuf->profile = IB_PROFILE_LINEAR_RGB;
		}
		else
			ibuf->profile = IB_PROFILE_NONE;

		IMB_rect_from_float(ibuf);
	}
}

static void draw_render_info(Scene *scene, Image *ima, ARegion *ar)
{
	RenderResult *rr;
	
	rr = BKE_image_acquire_renderresult(scene, ima);

	if (rr && rr->text) {
		ED_region_info_draw(ar, rr->text, 1, 0.25);
	}

	BKE_image_release_renderresult(scene, ima);
}

/* used by node view too */
void ED_image_draw_info(ARegion *ar, int color_manage, int channels, int x, int y,
                        const unsigned char cp[4], const float fp[4], int *zp, float *zpf)
{
	char str[256];
	float dx = 6;
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
	glRecti(0.0, 0.0, ar->winrct.xmax - ar->winrct.xmin + 1, 20);
	glDisable(GL_BLEND);

	BLF_size(blf_mono_font, 11, 72);

	glColor3ub(255, 255, 255);
	BLI_snprintf(str, sizeof(str), "X:%-4d  Y:%-4d |", x, y);
	// UI_DrawString(6, 6, str); // works ok but fixed width is nicer.
	BLF_position(blf_mono_font, dx, 6, 0);
	BLF_draw_ascii(blf_mono_font, str, sizeof(str));
	dx += BLF_width(blf_mono_font, str);

	if (zp) {
		glColor3ub(255, 255, 255);
		BLI_snprintf(str, sizeof(str), " Z:%-.4f |", 0.5f + 0.5f * (((float)*zp) / (float)0x7fffffff));
		BLF_position(blf_mono_font, dx, 6, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str);
	}
	if (zpf) {
		glColor3ub(255, 255, 255);
		BLI_snprintf(str, sizeof(str), " Z:%-.3f |", *zpf);
		BLF_position(blf_mono_font, dx, 6, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str);
	}

	if (channels >= 3) {
		glColor3ubv(red);
		if (fp)
			BLI_snprintf(str, sizeof(str), "  R:%-.4f", fp[0]);
		else if (cp)
			BLI_snprintf(str, sizeof(str), "  R:%-3d", cp[0]);
		else
			BLI_snprintf(str, sizeof(str), "  R:-");
		BLF_position(blf_mono_font, dx, 6, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str);
		
		glColor3ubv(green);
		if (fp)
			BLI_snprintf(str, sizeof(str), "  G:%-.4f", fp[1]);
		else if (cp)
			BLI_snprintf(str, sizeof(str), "  G:%-3d", cp[1]);
		else
			BLI_snprintf(str, sizeof(str), "  G:-");
		BLF_position(blf_mono_font, dx, 6, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str);
		
		glColor3ubv(blue);
		if (fp)
			BLI_snprintf(str, sizeof(str), "  B:%-.4f", fp[2]);
		else if (cp)
			BLI_snprintf(str, sizeof(str), "  B:%-3d", cp[2]);
		else
			BLI_snprintf(str, sizeof(str), "  B:-");
		BLF_position(blf_mono_font, dx, 6, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str);
		
		if (channels == 4) {
			glColor3ub(255, 255, 255);
			if (fp)
				BLI_snprintf(str, sizeof(str), "  A:%-.4f", fp[3]);
			else if (cp)
				BLI_snprintf(str, sizeof(str), "  A:%-3d", cp[3]);
			else
				BLI_snprintf(str, sizeof(str), "- ");
			BLF_position(blf_mono_font, dx, 6, 0);
			BLF_draw_ascii(blf_mono_font, str, sizeof(str));
			dx += BLF_width(blf_mono_font, str);
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
		if (fp) {
			copy_v3_v3(col, fp);
		}
		else if (cp) {
			rgb_uchar_to_float(col, cp);
		}
		else {
			zero_v3(col);
		}
		col[3] = 1.0f;
	}
	else if (channels == 4) {
		if (fp)
			copy_v4_v4(col, fp);
		else if (cp) {
			rgba_uchar_to_float(col, cp);
		}
		else {
			zero_v4(col);
		}
	}
	else {
		BLI_assert(0);
		zero_v4(col);
	}

	if (color_manage) {
		linearrgb_to_srgb_v4(finalcol, col);
	}
	else {
		copy_v4_v4(finalcol, col);
	}
	glDisable(GL_BLEND);
	glColor3fv(finalcol);
	dx += 5;
	glBegin(GL_QUADS);
	glVertex2f(dx, 3);
	glVertex2f(dx, 17);
	glVertex2f(dx + 30, 17);
	glVertex2f(dx + 30, 3);
	glEnd();

	/* draw outline */
	glColor3ub(128, 128, 128);
	glBegin(GL_LINE_LOOP);
	glVertex2f(dx, 3);
	glVertex2f(dx, 17);
	glVertex2f(dx + 30, 17);
	glVertex2f(dx + 30, 3);
	glEnd();

	dx += 35;

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
		BLF_position(blf_mono_font, dx, 6, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str);

		BLI_snprintf(str, sizeof(str), "   L:%-.4f", lum);
		BLF_position(blf_mono_font, dx, 6, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str);
	}
	else if (channels >= 3) {
		if (fp) {
			rgb_to_hsv(fp[0], fp[1], fp[2], &hue, &sat, &val);
			rgb_to_yuv(fp[0], fp[1], fp[2], &lum, &u, &v);
		}
		else if (cp) {
			rgb_to_hsv((float)cp[0] / 255.0f, (float)cp[1] / 255.0f, (float)cp[2] / 255.0f, &hue, &sat, &val);
			rgb_to_yuv((float)cp[0] / 255.0f, (float)cp[1] / 255.0f, (float)cp[2] / 255.0f, &lum, &u, &v);
		}

		BLI_snprintf(str, sizeof(str), "H:%-.4f", hue);
		BLF_position(blf_mono_font, dx, 6, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str);

		BLI_snprintf(str, sizeof(str), "  S:%-.4f", sat);
		BLF_position(blf_mono_font, dx, 6, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str);

		BLI_snprintf(str, sizeof(str), "  V:%-.4f", val);
		BLF_position(blf_mono_font, dx, 6, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str);

		BLI_snprintf(str, sizeof(str), "   L:%-.4f", lum);
		BLF_position(blf_mono_font, dx, 6, 0);
		BLF_draw_ascii(blf_mono_font, str, sizeof(str));
		dx += BLF_width(blf_mono_font, str);
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
//	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_RGBA, GL_FLOAT, rectf+3);
//	glColorMask(0, 1, 0, 0);
//	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_RGBA, GL_FLOAT, rectf+2);
//	glColorMask(0, 0, 1, 0);
//	glaDrawPixelsSafe(x1, y1, rectx, recty, rectx, GL_RGBA, GL_FLOAT, rectf+1);
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

static void draw_image_buffer(SpaceImage *sima, ARegion *ar, Scene *scene, Image *ima, ImBuf *ibuf, float fx, float fy, float zoomx, float zoomy)
{
	int x, y;
	int color_manage = scene->r.color_mgt_flag & R_COLOR_MANAGEMENT;

	/* set zoom */
	glPixelZoom(zoomx, zoomy);

	/* find window pixel coordinates of origin */
	UI_view2d_to_region_no_clip(&ar->v2d, fx, fy, &x, &y);

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
			fdrawcheckerboard(x, y, x + ibuf->x * zoomx, y + ibuf->y * zoomy);

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}

		/* we don't draw floats buffers directly but
		 * convert them, and optionally apply curves */
		image_verify_buffer_float(ima, ibuf, color_manage);

		if (ibuf->rect)
			glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
#if 0
		else
			glaDrawPixelsSafe(x, y, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_FLOAT, ibuf->rect_float);
#endif
		
		if (sima->flag & SI_USE_ALPHA)
			glDisable(GL_BLEND);
	}

	/* reset zoom */
	glPixelZoom(1.0f, 1.0f);
}

static unsigned int *get_part_from_ibuf(ImBuf *ibuf, short startx, short starty, short endx, short endy)
{
	unsigned int *rt, *rp, *rectmain;
	short y, heigth, len;

	/* the right offset in rectot */

	rt = ibuf->rect + (starty * ibuf->x + startx);

	len = (endx - startx);
	heigth = (endy - starty);

	rp = rectmain = MEM_mallocN(heigth * len * sizeof(int), "rect");
	
	for (y = 0; y < heigth; y++) {
		memcpy(rp, rt, len * 4);
		rt += ibuf->x;
		rp += len;
	}
	return rectmain;
}

static void draw_image_buffer_tiled(SpaceImage *sima, ARegion *ar, Scene *scene, Image *ima, ImBuf *ibuf, float fx, float fy, float zoomx, float zoomy)
{
	unsigned int *rect;
	int dx, dy, sx, sy, x, y;
	int color_manage = scene->r.color_mgt_flag & R_COLOR_MANAGEMENT;

	/* verify valid values, just leave this a while */
	if (ima->xrep < 1) return;
	if (ima->yrep < 1) return;
	
	glPixelZoom(zoomx, zoomy);

	if (sima->curtile >= ima->xrep * ima->yrep)
		sima->curtile = ima->xrep * ima->yrep - 1;
	
	/* create char buffer from float if needed */
	image_verify_buffer_float(ima, ibuf, color_manage);

	/* retrieve part of image buffer */
	dx = ibuf->x / ima->xrep;
	dy = ibuf->y / ima->yrep;
	sx = (sima->curtile % ima->xrep) * dx;
	sy = (sima->curtile / ima->xrep) * dy;
	rect = get_part_from_ibuf(ibuf, sx, sy, sx + dx, sy + dy);
	
	/* draw repeated */
	for (sy = 0; sy + dy <= ibuf->y; sy += dy) {
		for (sx = 0; sx + dx <= ibuf->x; sx += dx) {
			UI_view2d_to_region_no_clip(&ar->v2d, fx + (float)sx / (float)ibuf->x, fy + (float)sy / (float)ibuf->y, &x, &y);

			glaDrawPixelsSafe(x, y, dx, dy, dx, GL_RGBA, GL_UNSIGNED_BYTE, rect);
		}
	}

	glPixelZoom(1.0f, 1.0f);

	MEM_freeN(rect);
}

static void draw_image_buffer_repeated(SpaceImage *sima, ARegion *ar, Scene *scene, Image *ima, ImBuf *ibuf, float zoomx, float zoomy)
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
				draw_image_buffer(sima, ar, scene, ima, ibuf, x, y, zoomx, zoomy);

			/* only draw until running out of time */
			if ((PIL_check_seconds_timer() - time_current) > 0.25)
				return;
		}
	}
}

/* draw uv edit */

/* draw grease pencil */
void draw_image_grease_pencil(bContext *C, short onlyv2d)
{
	/* draw in View2D space? */
	if (onlyv2d) {
		/* assume that UI_view2d_ortho(C) has been called... */
		SpaceImage *sima = (SpaceImage *)CTX_wm_space_data(C);
		void *lock;
		ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock);
		
		/* draw grease-pencil ('image' strokes) */
		//if (sima->flag & SI_DISPGP)
		draw_gpencil_2dimage(C, ibuf);

		ED_space_image_release_buffer(sima, lock);
	}
	else {
		/* assume that UI_view2d_restore(C) has been called... */
		//SpaceImage *sima= (SpaceImage *)CTX_wm_space_data(C);
		
		/* draw grease-pencil ('screen' strokes) */
		//if (sima->flag & SI_DISPGP)
		draw_gpencil_view2d(C, 0);
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

			radius = brush_size(brush) * G.sima->zoom;
			fdrawXORcirc(mval[0], mval[1], radius);

			if (brush->innerradius != 1.0) {
				radius *= brush->innerradius;
				fdrawXORcirc(mval[0], mval[1], radius);
			}
		}
	}
}
#endif

static unsigned char *get_alpha_clone_image(Scene *scene, int *width, int *height)
{
	Brush *brush = paint_brush(&scene->toolsettings->imapaint.paint);
	ImBuf *ibuf;
	unsigned int size, alpha;
	unsigned char *rect, *cp;

	if (!brush || !brush->clone.image)
		return NULL;
	
	ibuf = BKE_image_get_ibuf(brush->clone.image, NULL);

	if (!ibuf || !ibuf->rect)
		return NULL;

	rect = MEM_dupallocN(ibuf->rect);
	if (!rect)
		return NULL;

	*width = ibuf->x;
	*height = ibuf->y;

	size = (*width) * (*height);
	alpha = (unsigned char)255 * brush->clone.alpha;
	cp = rect;

	while (size-- > 0) {
		cp[3] = alpha;
		cp += 4;
	}

	return rect;
}

static void draw_image_paint_helpers(ARegion *ar, Scene *scene, float zoomx, float zoomy)
{
	Brush *brush;
	int x, y, w, h;
	unsigned char *clonerect;

	brush = paint_brush(&scene->toolsettings->imapaint.paint);

	if (brush && (brush->imagepaint_tool == PAINT_TOOL_CLONE)) {
		/* this is not very efficient, but glDrawPixels doesn't allow
		 * drawing with alpha */
		clonerect = get_alpha_clone_image(scene, &w, &h);

		if (clonerect) {
			UI_view2d_to_region_no_clip(&ar->v2d, brush->clone.offset[0], brush->clone.offset[1], &x, &y);

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

void draw_image_main(SpaceImage *sima, ARegion *ar, Scene *scene)
{
	Image *ima;
	ImBuf *ibuf;
	float zoomx, zoomy;
	int show_viewer, show_render;
	void *lock;

	/* XXX can we do this in refresh? */
#if 0
	what_image(sima);
	
	if (sima->image) {
		ED_image_aspect(sima->image, &xuser_asp, &yuser_asp);
		
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
	ED_space_image_zoom(sima, ar, &zoomx, &zoomy);
	ibuf = ED_space_image_acquire_buffer(sima, &lock);

	show_viewer = (ima && ima->source == IMA_SRC_VIEWER);
	show_render = (show_viewer && ima->type == IMA_TYPE_R_RESULT);

	/* draw the image or grid */
	if (ibuf == NULL)
		ED_region_grid_draw(ar, zoomx, zoomy);
	else if (sima->flag & SI_DRAW_TILE)
		draw_image_buffer_repeated(sima, ar, scene, ima, ibuf, zoomx, zoomy);
	else if (ima && (ima->tpageflag & IMA_TILES))
		draw_image_buffer_tiled(sima, ar, scene, ima, ibuf, 0.0f, 0.0, zoomx, zoomy);
	else
		draw_image_buffer(sima, ar, scene, ima, ibuf, 0.0f, 0.0f, zoomx, zoomy);

	/* paint helpers */
	if (sima->flag & SI_DRAWTOOL)
		draw_image_paint_helpers(ar, scene, zoomx, zoomy);


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

	ED_space_image_release_buffer(sima, lock);

	/* render info */
	if (ima && show_render)
		draw_render_info(scene, ima, ar);
}


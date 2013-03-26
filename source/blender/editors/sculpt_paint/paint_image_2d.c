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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/sculpt_paint/paint_image_2d.c
 *  \ingroup bke
 */
//#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_brush.h"
#include "BKE_main.h"
#include "BKE_image.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "ED_screen.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "RE_shader_ext.h"

#include "GPU_draw.h"

#include "paint_intern.h"

/* Brush Painting for 2D image editor */

/* Defines and Structs */
/* FTOCHAR as inline function */
BLI_INLINE unsigned char f_to_char(const float val)
{
	return FTOCHAR(val);
}
#define IMAPAINT_FLOAT_RGB_TO_CHAR(c, f)  {                                   \
	(c)[0] = f_to_char((f)[0]);                                               \
	(c)[1] = f_to_char((f)[1]);                                               \
	(c)[2] = f_to_char((f)[2]);                                               \
} (void)0

#define IMAPAINT_CHAR_RGB_TO_FLOAT(f, c)  {                                   \
	(f)[0] = IMAPAINT_CHAR_TO_FLOAT((c)[0]);                                  \
	(f)[1] = IMAPAINT_CHAR_TO_FLOAT((c)[1]);                                  \
	(f)[2] = IMAPAINT_CHAR_TO_FLOAT((c)[2]);                                  \
} (void)0

#define IMAPAINT_FLOAT_RGB_COPY(a, b) copy_v3_v3(a, b)

typedef struct BrushPainterCache {
	short enabled;

	int size;           /* size override, if 0 uses 2*BKE_brush_size_get(brush) */
	short flt;          /* need float imbuf? */
	short texonly;      /* no alpha, color or fallof, only texture in imbuf */

	int lastsize;
	float lastalpha;
	float lastjitter;
	float last_rotation;

	ImBuf *ibuf;
	ImBuf *texibuf;
	ImBuf *maskibuf;
} BrushPainterCache;

typedef struct BrushPainter {
	Scene *scene;
	Brush *brush;

	float lastpaintpos[2];  /* position of last paint op */
	float startpaintpos[2]; /* position of first paint */

	short firsttouch;       /* first paint op */

	BrushPainterCache cache;
} BrushPainter;

typedef struct ImagePaintRegion {
	int destx, desty;
	int srcx, srcy;
	int width, height;
} ImagePaintRegion;

typedef struct ImagePaintState {
	BrushPainter *painter;
	SpaceImage *sima;
	View2D *v2d;
	Scene *scene;
	bScreen *screen;

	Brush *brush;
	short tool, blend;
	Image *image;
	ImBuf *canvas;
	ImBuf *clonecanvas;
	char *warnpackedfile;
	char *warnmultifile;

	/* viewport texture paint only, but _not_ project paint */
	Object *ob;
	int faceindex;
	float uv[2];
	int do_facesel;
} ImagePaintState;


static BrushPainter *brush_painter_2d_new(Scene *scene, Brush *brush)
{
	BrushPainter *painter = MEM_callocN(sizeof(BrushPainter), "BrushPainter");

	painter->brush = brush;
	painter->scene = scene;
	painter->firsttouch = 1;
	painter->cache.lastsize = -1; /* force ibuf create in refresh */

	return painter;
}


static void brush_painter_2d_require_imbuf(BrushPainter *painter, short flt, short texonly, int size)
{
	if ((painter->cache.flt != flt) || (painter->cache.size != size) ||
	    ((painter->cache.texonly != texonly) && texonly))
	{
		if (painter->cache.ibuf) IMB_freeImBuf(painter->cache.ibuf);
		if (painter->cache.maskibuf) IMB_freeImBuf(painter->cache.maskibuf);
		painter->cache.ibuf = painter->cache.maskibuf = NULL;
		painter->cache.lastsize = -1; /* force ibuf create in refresh */
	}

	if (painter->cache.flt != flt) {
		if (painter->cache.texibuf) IMB_freeImBuf(painter->cache.texibuf);
		painter->cache.texibuf = NULL;
		painter->cache.lastsize = -1; /* force ibuf create in refresh */
	}

	painter->cache.size = size;
	painter->cache.flt = flt;
	painter->cache.texonly = texonly;
	painter->cache.enabled = 1;
}

static void brush_painter_2d_free(BrushPainter *painter)
{
	if (painter->cache.ibuf) IMB_freeImBuf(painter->cache.ibuf);
	if (painter->cache.texibuf) IMB_freeImBuf(painter->cache.texibuf);
	if (painter->cache.maskibuf) IMB_freeImBuf(painter->cache.maskibuf);
	MEM_freeN(painter);
}

static void brush_painter_2d_do_partial(BrushPainter *painter, ImBuf *oldtexibuf,
                                     int x, int y, int w, int h, int xt, int yt,
                                     const float pos[2])
{
	Scene *scene = painter->scene;
	Brush *brush = painter->brush;
	ImBuf *ibuf, *maskibuf, *texibuf;
	float *bf, *mf, *tf, *otf = NULL, xoff, yoff, xy[2], rgba[4];
	unsigned char *b, *m, *t, *ot = NULL;
	int dotexold, origx = x, origy = y;
	const int radius = BKE_brush_size_get(painter->scene, brush);

	xoff = -radius + 0.5f;
	yoff = -radius + 0.5f;
	xoff += (int)pos[0] - (int)painter->startpaintpos[0];
	yoff += (int)pos[1] - (int)painter->startpaintpos[1];

	ibuf = painter->cache.ibuf;
	texibuf = painter->cache.texibuf;
	maskibuf = painter->cache.maskibuf;

	dotexold = (oldtexibuf != NULL);

	/* not sure if it's actually needed or it's a mistake in coords/sizes
	 * calculation in brush_painter_fixed_tex_partial_update(), but without this
	 * limitation memory gets corrupted at fast strokes with quite big spacing (sergey) */
	w = min_ii(w, ibuf->x);
	h = min_ii(h, ibuf->y);

	if (painter->cache.flt) {
		for (; y < h; y++) {
			bf = ibuf->rect_float + (y * ibuf->x + origx) * 4;
			tf = texibuf->rect_float + (y * texibuf->x + origx) * 4;
			mf = maskibuf->rect_float + (y * maskibuf->x + origx) * 4;

			if (dotexold)
				otf = oldtexibuf->rect_float + ((y - origy + yt) * oldtexibuf->x + xt) * 4;

			for (x = origx; x < w; x++, bf += 4, mf += 4, tf += 4) {
				if (dotexold) {
					copy_v4_v4(tf, otf);
					otf += 4;
				}
				else {
					xy[0] = x + xoff;
					xy[1] = y + yoff;

					BKE_brush_sample_tex_2D(scene, brush, xy, tf);
				}

				bf[0] = tf[0] * mf[0];
				bf[1] = tf[1] * mf[1];
				bf[2] = tf[2] * mf[2];
				bf[3] = tf[3] * mf[3];
			}
		}
	}
	else {
		for (; y < h; y++) {
			b = (unsigned char *)ibuf->rect + (y * ibuf->x + origx) * 4;
			t = (unsigned char *)texibuf->rect + (y * texibuf->x + origx) * 4;
			m = (unsigned char *)maskibuf->rect + (y * maskibuf->x + origx) * 4;

			if (dotexold)
				ot = (unsigned char *)oldtexibuf->rect + ((y - origy + yt) * oldtexibuf->x + xt) * 4;

			for (x = origx; x < w; x++, b += 4, m += 4, t += 4) {
				if (dotexold) {
					t[0] = ot[0];
					t[1] = ot[1];
					t[2] = ot[2];
					t[3] = ot[3];
					ot += 4;
				}
				else {
					xy[0] = x + xoff;
					xy[1] = y + yoff;

					BKE_brush_sample_tex_2D(scene, brush, xy, rgba);
					rgba_float_to_uchar(t, rgba);
				}

				b[0] = t[0] * m[0] / 255;
				b[1] = t[1] * m[1] / 255;
				b[2] = t[2] * m[2] / 255;
				b[3] = t[3] * m[3] / 255;
			}
		}
	}
}

static void brush_painter_2d_tiled_tex_partial_update(BrushPainter *painter, const float pos[2])
{
	const Scene *scene = painter->scene;
	Brush *brush = painter->brush;
	BrushPainterCache *cache = &painter->cache;
	ImBuf *oldtexibuf, *ibuf;
	int imbflag, destx, desty, srcx, srcy, w, h, x1, y1, x2, y2;
	const int diameter = 2 * BKE_brush_size_get(scene, brush);

	imbflag = (cache->flt) ? IB_rectfloat : IB_rect;
	if (!cache->ibuf)
		cache->ibuf = IMB_allocImBuf(diameter, diameter, 32, imbflag);
	ibuf = cache->ibuf;

	oldtexibuf = cache->texibuf;
	cache->texibuf = IMB_allocImBuf(diameter, diameter, 32, imbflag);
	if (oldtexibuf) {
		srcx = srcy = 0;
		destx = (int)painter->lastpaintpos[0] - (int)pos[0];
		desty = (int)painter->lastpaintpos[1] - (int)pos[1];
		w = oldtexibuf->x;
		h = oldtexibuf->y;

		IMB_rectclip(cache->texibuf, oldtexibuf, &destx, &desty, &srcx, &srcy, &w, &h);
	}
	else {
		srcx = srcy = 0;
		destx = desty = 0;
		w = h = 0;
	}
	
	x1 = destx;
	y1 = desty;
	x2 = destx + w;
	y2 = desty + h;

	/* blend existing texture in new position */
	if ((x1 < x2) && (y1 < y2))
		brush_painter_2d_do_partial(painter, oldtexibuf, x1, y1, x2, y2, srcx, srcy, pos);

	if (oldtexibuf)
		IMB_freeImBuf(oldtexibuf);

	/* sample texture in new areas */
	if ((0 < x1) && (0 < ibuf->y))
		brush_painter_2d_do_partial(painter, NULL, 0, 0, x1, ibuf->y, 0, 0, pos);
	if ((x2 < ibuf->x) && (0 < ibuf->y))
		brush_painter_2d_do_partial(painter, NULL, x2, 0, ibuf->x, ibuf->y, 0, 0, pos);
	if ((x1 < x2) && (0 < y1))
		brush_painter_2d_do_partial(painter, NULL, x1, 0, x2, y1, 0, 0, pos);
	if ((x1 < x2) && (y2 < ibuf->y))
		brush_painter_2d_do_partial(painter, NULL, x1, y2, x2, ibuf->y, 0, 0, pos);
}

static void brush_painter_2d_refresh_cache(BrushPainter *painter, const float pos[2], int use_color_correction)
{
	const Scene *scene = painter->scene;
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
	Brush *brush = painter->brush;
	BrushPainterCache *cache = &painter->cache;
	MTex *mtex = &brush->mtex;
	int size;
	short flt;
	const int diameter = 2 * BKE_brush_size_get(scene, brush);
	const float alpha = BKE_brush_alpha_get(scene, brush);
	const bool do_tiled = ELEM(brush->mtex.brush_map_mode, MTEX_MAP_MODE_TILED, MTEX_MAP_MODE_3D);
	float rotation = -mtex->rot;

	if (mtex->brush_map_mode == MTEX_MAP_MODE_VIEW) {
		rotation += ups->brush_rotation;
	}

	if (diameter != cache->lastsize ||
	    alpha != cache->lastalpha ||
	    brush->jitter != cache->lastjitter ||
	    rotation != cache->last_rotation)
	{
		if (cache->ibuf) {
			IMB_freeImBuf(cache->ibuf);
			cache->ibuf = NULL;
		}
		if (cache->maskibuf) {
			IMB_freeImBuf(cache->maskibuf);
			cache->maskibuf = NULL;
		}

		flt = cache->flt;
		size = (cache->size) ? cache->size : diameter;

		if (do_tiled) {
			BKE_brush_imbuf_new(scene, brush, flt, 3, size, &cache->maskibuf, use_color_correction);
			brush_painter_2d_tiled_tex_partial_update(painter, pos);
		}
		else
			BKE_brush_imbuf_new(scene, brush, flt, 2, size, &cache->ibuf, use_color_correction);

		cache->lastsize = diameter;
		cache->lastalpha = alpha;
		cache->lastjitter = brush->jitter;
		cache->last_rotation = rotation;
	}
	else if (do_tiled && mtex && mtex->tex) {
		int dx = (int)painter->lastpaintpos[0] - (int)pos[0];
		int dy = (int)painter->lastpaintpos[1] - (int)pos[1];

		if ((dx != 0) || (dy != 0))
			brush_painter_2d_tiled_tex_partial_update(painter, pos);
	}
}

/* keep these functions in sync */
static void paint_2d_ibuf_rgb_get(ImBuf *ibuf, int x, int y, const short is_torus, float r_rgb[3])
{
	if (is_torus) {
		x %= ibuf->x;
		if (x < 0) x += ibuf->x;
		y %= ibuf->y;
		if (y < 0) y += ibuf->y;
	}

	if (ibuf->rect_float) {
		float *rrgbf = ibuf->rect_float + (ibuf->x * y + x) * 4;
		IMAPAINT_FLOAT_RGB_COPY(r_rgb, rrgbf);
	}
	else {
		char *rrgb = (char *)ibuf->rect + (ibuf->x * y + x) * 4;
		IMAPAINT_CHAR_RGB_TO_FLOAT(r_rgb, rrgb);
	}
}
static void paint_2d_ibuf_rgb_set(ImBuf *ibuf, int x, int y, const short is_torus, const float rgb[3])
{
	if (is_torus) {
		x %= ibuf->x;
		if (x < 0) x += ibuf->x;
		y %= ibuf->y;
		if (y < 0) y += ibuf->y;
	}

	if (ibuf->rect_float) {
		float *rrgbf = ibuf->rect_float + (ibuf->x * y + x) * 4;
		IMAPAINT_FLOAT_RGB_COPY(rrgbf, rgb);
	}
	else {
		char *rrgb = (char *)ibuf->rect + (ibuf->x * y + x) * 4;
		IMAPAINT_FLOAT_RGB_TO_CHAR(rrgb, rgb);
	}
}

static int paint_2d_ibuf_add_if(ImBuf *ibuf, unsigned int x, unsigned int y, float *outrgb, short torus)
{
	float inrgb[3];

	// XXX: signed unsigned mismatch
	if ((x >= (unsigned int)(ibuf->x)) || (y >= (unsigned int)(ibuf->y))) {
		if (torus) paint_2d_ibuf_rgb_get(ibuf, x, y, 1, inrgb);
		else return 0;
	}
	else {
		paint_2d_ibuf_rgb_get(ibuf, x, y, 0, inrgb);
	}

	outrgb[0] += inrgb[0];
	outrgb[1] += inrgb[1];
	outrgb[2] += inrgb[2];

	return 1;
}

static void paint_2d_lift_soften(ImBuf *ibuf, ImBuf *ibufb, int *pos, const short is_torus)
{
	int x, y, count, xi, yi, xo, yo;
	int out_off[2], in_off[2], dim[2];
	float outrgb[3];

	dim[0] = ibufb->x;
	dim[1] = ibufb->y;
	in_off[0] = pos[0];
	in_off[1] = pos[1];
	out_off[0] = out_off[1] = 0;

	if (!is_torus) {
		IMB_rectclip(ibuf, ibufb, &in_off[0], &in_off[1], &out_off[0],
		             &out_off[1], &dim[0], &dim[1]);

		if ((dim[0] == 0) || (dim[1] == 0))
			return;
	}

	for (y = 0; y < dim[1]; y++) {
		for (x = 0; x < dim[0]; x++) {
			/* get input pixel */
			xi = in_off[0] + x;
			yi = in_off[1] + y;

			count = 1;
			paint_2d_ibuf_rgb_get(ibuf, xi, yi, is_torus, outrgb);

			count += paint_2d_ibuf_add_if(ibuf, xi - 1, yi - 1, outrgb, is_torus);
			count += paint_2d_ibuf_add_if(ibuf, xi - 1, yi, outrgb, is_torus);
			count += paint_2d_ibuf_add_if(ibuf, xi - 1, yi + 1, outrgb, is_torus);

			count += paint_2d_ibuf_add_if(ibuf, xi, yi - 1, outrgb, is_torus);
			count += paint_2d_ibuf_add_if(ibuf, xi, yi + 1, outrgb, is_torus);

			count += paint_2d_ibuf_add_if(ibuf, xi + 1, yi - 1, outrgb, is_torus);
			count += paint_2d_ibuf_add_if(ibuf, xi + 1, yi, outrgb, is_torus);
			count += paint_2d_ibuf_add_if(ibuf, xi + 1, yi + 1, outrgb, is_torus);

			mul_v3_fl(outrgb, 1.0f / (float)count);

			/* write into brush buffer */
			xo = out_off[0] + x;
			yo = out_off[1] + y;
			paint_2d_ibuf_rgb_set(ibufb, xo, yo, 0, outrgb);
		}
	}
}

static void paint_2d_set_region(ImagePaintRegion *region, int destx, int desty, int srcx, int srcy, int width, int height)
{
	region->destx = destx;
	region->desty = desty;
	region->srcx = srcx;
	region->srcy = srcy;
	region->width = width;
	region->height = height;
}

static int paint_2d_torus_split_region(ImagePaintRegion region[4], ImBuf *dbuf, ImBuf *sbuf)
{
	int destx = region->destx;
	int desty = region->desty;
	int srcx = region->srcx;
	int srcy = region->srcy;
	int width = region->width;
	int height = region->height;
	int origw, origh, w, h, tot = 0;

	/* convert destination and source coordinates to be within image */
	destx = destx % dbuf->x;
	if (destx < 0) destx += dbuf->x;
	desty = desty % dbuf->y;
	if (desty < 0) desty += dbuf->y;
	srcx = srcx % sbuf->x;
	if (srcx < 0) srcx += sbuf->x;
	srcy = srcy % sbuf->y;
	if (srcy < 0) srcy += sbuf->y;

	/* clip width of blending area to destination imbuf, to avoid writing the
	 * same pixel twice */
	origw = w = (width > dbuf->x) ? dbuf->x : width;
	origh = h = (height > dbuf->y) ? dbuf->y : height;

	/* clip within image */
	IMB_rectclip(dbuf, sbuf, &destx, &desty, &srcx, &srcy, &w, &h);
	paint_2d_set_region(&region[tot++], destx, desty, srcx, srcy, w, h);

	/* do 3 other rects if needed */
	if (w < origw)
		paint_2d_set_region(&region[tot++], (destx + w) % dbuf->x, desty, (srcx + w) % sbuf->x, srcy, origw - w, h);
	if (h < origh)
		paint_2d_set_region(&region[tot++], destx, (desty + h) % dbuf->y, srcx, (srcy + h) % sbuf->y, w, origh - h);
	if ((w < origw) && (h < origh))
		paint_2d_set_region(&region[tot++], (destx + w) % dbuf->x, (desty + h) % dbuf->y, (srcx + w) % sbuf->x, (srcy + h) % sbuf->y, origw - w, origh - h);

	return tot;
}

static void paint_2d_lift_smear(ImBuf *ibuf, ImBuf *ibufb, int *pos)
{
	ImagePaintRegion region[4];
	int a, tot;

	paint_2d_set_region(region, 0, 0, pos[0], pos[1], ibufb->x, ibufb->y);
	tot = paint_2d_torus_split_region(region, ibufb, ibuf);

	for (a = 0; a < tot; a++)
		IMB_rectblend(ibufb, ibuf, region[a].destx, region[a].desty,
		              region[a].srcx, region[a].srcy,
		              region[a].width, region[a].height, IMB_BLEND_COPY_RGB);
}

static ImBuf *paint_2d_lift_clone(ImBuf *ibuf, ImBuf *ibufb, int *pos)
{
	/* note: allocImbuf returns zero'd memory, so regions outside image will
	 * have zero alpha, and hence not be blended onto the image */
	int w = ibufb->x, h = ibufb->y, destx = 0, desty = 0, srcx = pos[0], srcy = pos[1];
	ImBuf *clonebuf = IMB_allocImBuf(w, h, ibufb->planes, ibufb->flags);

	IMB_rectclip(clonebuf, ibuf, &destx, &desty, &srcx, &srcy, &w, &h);
	IMB_rectblend(clonebuf, ibuf, destx, desty, srcx, srcy, w, h,
	              IMB_BLEND_COPY_RGB);
	IMB_rectblend(clonebuf, ibufb, destx, desty, destx, desty, w, h,
	              IMB_BLEND_COPY_ALPHA);

	return clonebuf;
}

static void paint_2d_convert_brushco(ImBuf *ibufb, const float pos[2], int ipos[2])
{
	ipos[0] = (int)floorf((pos[0] - ibufb->x / 2) + 1.0f);
	ipos[1] = (int)floorf((pos[1] - ibufb->y / 2) + 1.0f);
}

static int paint_2d_op(void *state, ImBuf *ibufb, const float lastpos[2], const float pos[2])
{
	ImagePaintState *s = ((ImagePaintState *)state);
	ImBuf *clonebuf = NULL, *frombuf;
	ImagePaintRegion region[4];
	short torus = s->brush->flag & BRUSH_TORUS;
	short blend = s->blend;
	float *offset = s->brush->clone.offset;
	float liftpos[2];
	int bpos[2], blastpos[2], bliftpos[2];
	int a, tot;

	paint_2d_convert_brushco(ibufb, pos, bpos);

	/* lift from canvas */
	if (s->tool == PAINT_TOOL_SOFTEN) {
		paint_2d_lift_soften(s->canvas, ibufb, bpos, torus);
	}
	else if (s->tool == PAINT_TOOL_SMEAR) {
		if (lastpos[0] == pos[0] && lastpos[1] == pos[1])
			return 0;

		paint_2d_convert_brushco(ibufb, lastpos, blastpos);
		paint_2d_lift_smear(s->canvas, ibufb, blastpos);
	}
	else if (s->tool == PAINT_TOOL_CLONE && s->clonecanvas) {
		liftpos[0] = pos[0] - offset[0] * s->canvas->x;
		liftpos[1] = pos[1] - offset[1] * s->canvas->y;

		paint_2d_convert_brushco(ibufb, liftpos, bliftpos);
		clonebuf = paint_2d_lift_clone(s->clonecanvas, ibufb, bliftpos);
	}

	frombuf = (clonebuf) ? clonebuf : ibufb;

	if (torus) {
		paint_2d_set_region(region, bpos[0], bpos[1], 0, 0, frombuf->x, frombuf->y);
		tot = paint_2d_torus_split_region(region, s->canvas, frombuf);
	}
	else {
		paint_2d_set_region(region, bpos[0], bpos[1], 0, 0, frombuf->x, frombuf->y);
		tot = 1;
	}

	/* blend into canvas */
	for (a = 0; a < tot; a++) {
		imapaint_dirty_region(s->image, s->canvas,
		                      region[a].destx, region[a].desty,
		                      region[a].width, region[a].height);

		IMB_rectblend(s->canvas, frombuf,
		              region[a].destx, region[a].desty,
		              region[a].srcx, region[a].srcy,
		              region[a].width, region[a].height, blend);
	}

	if (clonebuf) IMB_freeImBuf(clonebuf);

	return 1;
}


static int paint_2d_canvas_set(ImagePaintState *s, Image *ima)
{
	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, s->sima ? &s->sima->iuser : NULL, NULL);

	/* verify that we can paint and set canvas */
	if (ima == NULL) {
		return 0;
	}
	else if (ima->packedfile && ima->rr) {
		s->warnpackedfile = ima->id.name + 2;
		return 0;
	}
	else if (ibuf && ibuf->channels != 4) {
		s->warnmultifile = ima->id.name + 2;
		return 0;
	}
	else if (!ibuf || !(ibuf->rect || ibuf->rect_float))
		return 0;

	s->image = ima;
	s->canvas = ibuf;

	/* set clone canvas */
	if (s->tool == PAINT_TOOL_CLONE) {
		ima = s->brush->clone.image;
		ibuf = BKE_image_acquire_ibuf(ima, s->sima ? &s->sima->iuser : NULL, NULL);

		if (!ima || !ibuf || !(ibuf->rect || ibuf->rect_float)) {
			BKE_image_release_ibuf(ima, ibuf, NULL);
			BKE_image_release_ibuf(s->image, s->canvas, NULL);
			return 0;
		}

		s->clonecanvas = ibuf;

		/* temporarily add float rect for cloning */
		if (s->canvas->rect_float && !s->clonecanvas->rect_float) {
			IMB_float_from_rect(s->clonecanvas);
		}
		else if (!s->canvas->rect_float && !s->clonecanvas->rect)
			IMB_rect_from_float(s->clonecanvas);
	}

	return 1;
}

static void paint_2d_canvas_free(ImagePaintState *s)
{
	BKE_image_release_ibuf(s->image, s->canvas, NULL);
	BKE_image_release_ibuf(s->brush->clone.image, s->clonecanvas, NULL);
}

int paint_2d_stroke(void *ps, const int prev_mval[2], const int mval[2], int eraser)
{
	float newuv[2], olduv[2];
	int redraw = 0;
	ImagePaintState *s = ps;
	BrushPainter *painter = s->painter;
	ImBuf *ibuf = BKE_image_acquire_ibuf(s->image, s->sima ? &s->sima->iuser : NULL, NULL);
	const bool is_data = (ibuf && ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA);

	if (!ibuf)
		return 0;

	s->blend = s->brush->blend;
	if (eraser)
		s->blend = IMB_BLEND_ERASE_ALPHA;

	UI_view2d_region_to_view(s->v2d, mval[0], mval[1], &newuv[0], &newuv[1]);
	UI_view2d_region_to_view(s->v2d, prev_mval[0], prev_mval[1], &olduv[0], &olduv[1]);

	newuv[0] *= ibuf->x;
	newuv[1] *= ibuf->y;

	olduv[0] *= ibuf->x;
	olduv[1] *= ibuf->y;

	if (painter->firsttouch) {
		/* paint exactly once on first touch */
		painter->startpaintpos[0] = newuv[0];
		painter->startpaintpos[1] = newuv[1];

		painter->firsttouch = 0;
		copy_v2_v2(painter->lastpaintpos, newuv);
	}
	else {
		copy_v2_v2(painter->lastpaintpos, olduv);
	}
	/* OCIO_TODO: float buffers are now always linear, so always use color correction
	 *            this should probably be changed when texture painting color space is supported
	 */
	brush_painter_2d_require_imbuf(painter, ((ibuf->rect_float) ? 1 : 0), 0, 0);

	if (painter->cache.enabled)
		brush_painter_2d_refresh_cache(painter, newuv, is_data == false);

	if (paint_2d_op(s, painter->cache.ibuf, olduv, newuv)) {
		imapaint_image_update(s->sima, s->image, ibuf, false);
		BKE_image_release_ibuf(s->image, ibuf, NULL);
		redraw |= 1;
	}
	else {
		BKE_image_release_ibuf(s->image, ibuf, NULL);
	}

	if (redraw)
		imapaint_clear_partial_redraw();

	return redraw;
}

void *paint_2d_new_stroke(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *settings = scene->toolsettings;
	Brush *brush = paint_brush(&settings->imapaint.paint);

	ImagePaintState *s = MEM_callocN(sizeof(ImagePaintState), "ImagePaintState");

	s->sima = CTX_wm_space_image(C);
	s->v2d = &CTX_wm_region(C)->v2d;
	s->scene = scene;
	s->screen = CTX_wm_screen(C);

	s->brush = brush;
	s->tool = brush->imagepaint_tool;
	s->blend = brush->blend;

	s->image = s->sima->image;

	if (!paint_2d_canvas_set(s, s->image)) {
		if (s->warnmultifile)
			BKE_report(op->reports, RPT_WARNING, "Image requires 4 color channels to paint");
		if (s->warnpackedfile)
			BKE_report(op->reports, RPT_WARNING, "Packed MultiLayer files cannot be painted");

		MEM_freeN(s);
		return NULL;
	}

	paint_brush_init_tex(s->brush);

	/* create painter */
	s->painter = brush_painter_2d_new(scene, s->brush);

	return s;
}

void paint_2d_redraw(const bContext *C, void *ps, int final)
{
	ImagePaintState *s = ps;

	if (final) {
		if (s->image && !(s->sima && s->sima->lock))
			GPU_free_image(s->image);

		/* compositor listener deals with updating */
		WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, s->image);
	}
	else {
		if (!s->sima || !s->sima->lock)
			ED_region_tag_redraw(CTX_wm_region(C));
		else
			WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, s->image);
	}
}

void paint_2d_stroke_done(void *ps)
{
	ImagePaintState *s = ps;

	paint_2d_canvas_free(s);
	brush_painter_2d_free(s->painter);
	paint_brush_exit_tex(s->brush);

	MEM_freeN(s);
}

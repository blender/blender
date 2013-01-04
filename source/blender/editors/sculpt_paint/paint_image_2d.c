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

#include "BKE_brush.h"

#include "BLI_math.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_shader_ext.h"

 /* Brush Painting for 2D image editor */

typedef struct BrushPainterCache {
	short enabled;

	int size;           /* size override, if 0 uses 2*BKE_brush_size_get(brush) */
	short flt;          /* need float imbuf? */
	short texonly;      /* no alpha, color or fallof, only texture in imbuf */

	int lastsize;
	float lastalpha;
	float lastjitter;

	ImBuf *ibuf;
	ImBuf *texibuf;
	ImBuf *maskibuf;
} BrushPainterCache;

struct BrushPainter {
	Scene *scene;
	Brush *brush;

	float lastmousepos[2];  /* mouse position of last paint call */

	float accumdistance;    /* accumulated distance of brush since last paint op */
	float lastpaintpos[2];  /* position of last paint op */
	float startpaintpos[2]; /* position of first paint */

	double accumtime;       /* accumulated time since last paint op (airbrush) */
	double lasttime;        /* time of last update */

	float lastpressure;

	short firsttouch;       /* first paint op */

	float startsize;
	float startalpha;
	float startjitter;
	float startspacing;

	BrushPainterCache cache;
};

BrushPainter *BKE_brush_painter_new(Scene *scene, Brush *brush)
{
	BrushPainter *painter = MEM_callocN(sizeof(BrushPainter), "BrushPainter");

	painter->brush = brush;
	painter->scene = scene;
	painter->firsttouch = 1;
	painter->cache.lastsize = -1; /* force ibuf create in refresh */

	painter->startsize = BKE_brush_size_get(scene, brush);
	painter->startalpha = BKE_brush_alpha_get(scene, brush);
	painter->startjitter = brush->jitter;
	painter->startspacing = brush->spacing;

	return painter;
}


static void brush_pressure_apply(BrushPainter *painter, Brush *brush, float pressure)
{
	if (BKE_brush_use_alpha_pressure(painter->scene, brush))
		BKE_brush_alpha_set(painter->scene, brush, max_ff(0.0f, painter->startalpha * pressure));
	if (BKE_brush_use_size_pressure(painter->scene, brush))
		BKE_brush_size_set(painter->scene, brush, max_ff(1.0f, painter->startsize * pressure));
	if (brush->flag & BRUSH_JITTER_PRESSURE)
		brush->jitter = max_ff(0.0f, painter->startjitter * pressure);
	if (brush->flag & BRUSH_SPACING_PRESSURE)
		brush->spacing = max_ff(1.0f, painter->startspacing * (1.5f - pressure));
}


void BKE_brush_painter_require_imbuf(BrushPainter *painter, short flt, short texonly, int size)
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

void BKE_brush_painter_free(BrushPainter *painter)
{
	Brush *brush = painter->brush;

	BKE_brush_size_set(painter->scene, brush, painter->startsize);
	BKE_brush_alpha_set(painter->scene, brush, painter->startalpha);
	brush->jitter = painter->startjitter;
	brush->spacing = painter->startspacing;

	if (painter->cache.ibuf) IMB_freeImBuf(painter->cache.ibuf);
	if (painter->cache.texibuf) IMB_freeImBuf(painter->cache.texibuf);
	if (painter->cache.maskibuf) IMB_freeImBuf(painter->cache.maskibuf);
	MEM_freeN(painter);
}

static void brush_painter_do_partial(BrushPainter *painter, ImBuf *oldtexibuf,
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
					copy_v3_v3(tf, otf);
					tf[3] = otf[3];
					otf += 4;
				}
				else {
					xy[0] = x + xoff;
					xy[1] = y + yoff;

					BKE_brush_sample_tex(scene, brush, xy, tf, 0);
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

					BKE_brush_sample_tex(scene, brush, xy, rgba, 0);
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

static void brush_painter_fixed_tex_partial_update(BrushPainter *painter, const float pos[2])
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
		brush_painter_do_partial(painter, oldtexibuf, x1, y1, x2, y2, srcx, srcy, pos);

	if (oldtexibuf)
		IMB_freeImBuf(oldtexibuf);

	/* sample texture in new areas */
	if ((0 < x1) && (0 < ibuf->y))
		brush_painter_do_partial(painter, NULL, 0, 0, x1, ibuf->y, 0, 0, pos);
	if ((x2 < ibuf->x) && (0 < ibuf->y))
		brush_painter_do_partial(painter, NULL, x2, 0, ibuf->x, ibuf->y, 0, 0, pos);
	if ((x1 < x2) && (0 < y1))
		brush_painter_do_partial(painter, NULL, x1, 0, x2, y1, 0, 0, pos);
	if ((x1 < x2) && (y2 < ibuf->y))
		brush_painter_do_partial(painter, NULL, x1, y2, x2, ibuf->y, 0, 0, pos);
}

static void brush_painter_refresh_cache(BrushPainter *painter, const float pos[2], int use_color_correction)
{
	const Scene *scene = painter->scene;
	Brush *brush = painter->brush;
	BrushPainterCache *cache = &painter->cache;
	MTex *mtex = &brush->mtex;
	int size;
	short flt;
	const int diameter = 2 * BKE_brush_size_get(scene, brush);
	const float alpha = BKE_brush_alpha_get(scene, brush);

	if (diameter != cache->lastsize ||
	    alpha != cache->lastalpha ||
	    brush->jitter != cache->lastjitter)
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

		if (brush->flag & BRUSH_FIXED_TEX) {
			BKE_brush_imbuf_new(scene, brush, flt, 3, size, &cache->maskibuf, use_color_correction);
			brush_painter_fixed_tex_partial_update(painter, pos);
		}
		else
			BKE_brush_imbuf_new(scene, brush, flt, 2, size, &cache->ibuf, use_color_correction);

		cache->lastsize = diameter;
		cache->lastalpha = alpha;
		cache->lastjitter = brush->jitter;
	}
	else if ((brush->flag & BRUSH_FIXED_TEX) && mtex && mtex->tex) {
		int dx = (int)painter->lastpaintpos[0] - (int)pos[0];
		int dy = (int)painter->lastpaintpos[1] - (int)pos[1];

		if ((dx != 0) || (dy != 0))
			brush_painter_fixed_tex_partial_update(painter, pos);
	}
}

void BKE_brush_painter_break_stroke(BrushPainter *painter)
{
	painter->firsttouch = 1;
}


int BKE_brush_painter_paint(BrushPainter *painter, BrushFunc func, const float pos[2], double time, float pressure,
                            void *user, int use_color_correction)
{
	Scene *scene = painter->scene;
	Brush *brush = painter->brush;
	int totpaintops = 0;

	if (pressure == 0.0f) {
		if (painter->lastpressure) // XXX - hack, operator misses
			pressure = painter->lastpressure;
		else
			pressure = 1.0f;    /* zero pressure == not using tablet */
	}
	if (painter->firsttouch) {
		/* paint exactly once on first touch */
		painter->startpaintpos[0] = pos[0];
		painter->startpaintpos[1] = pos[1];

		brush_pressure_apply(painter, brush, pressure);
		if (painter->cache.enabled)
			brush_painter_refresh_cache(painter, pos, use_color_correction);
		totpaintops += func(user, painter->cache.ibuf, pos, pos);
		
		painter->lasttime = time;
		painter->firsttouch = 0;
		painter->lastpaintpos[0] = pos[0];
		painter->lastpaintpos[1] = pos[1];
	}
#if 0
	else if (painter->brush->flag & BRUSH_AIRBRUSH) {
		float spacing, step, paintpos[2], dmousepos[2], len;
		double starttime, curtime = time;

		/* compute brush spacing adapted to brush size */
		spacing = brush->rate; //radius*brush->spacing*0.01f;

		/* setup starting time, direction vector and accumulated time */
		starttime = painter->accumtime;
		sub_v2_v2v2(dmousepos, pos, painter->lastmousepos);
		len = normalize_v2(dmousepos);
		painter->accumtime += curtime - painter->lasttime;

		/* do paint op over unpainted time distance */
		while (painter->accumtime >= spacing) {
			step = (spacing - starttime) * len;
			paintpos[0] = painter->lastmousepos[0] + dmousepos[0] * step;
			paintpos[1] = painter->lastmousepos[1] + dmousepos[1] * step;

			if (painter->cache.enabled)
				brush_painter_refresh_cache(painter);
			totpaintops += func(user, painter->cache.ibuf,
			                    painter->lastpaintpos, paintpos);

			painter->lastpaintpos[0] = paintpos[0];
			painter->lastpaintpos[1] = paintpos[1];
			painter->accumtime -= spacing;
			starttime -= spacing;
		}
		
		painter->lasttime = curtime;
	}
#endif
	else {
		float startdistance, spacing, step, paintpos[2], dmousepos[2], finalpos[2];
		float t, len, press;
		const int radius = BKE_brush_size_get(scene, brush);

		/* compute brush spacing adapted to brush radius, spacing may depend
		 * on pressure, so update it */
		brush_pressure_apply(painter, brush, painter->lastpressure);
		spacing = max_ff(1.0f, radius) * brush->spacing * 0.01f;

		/* setup starting distance, direction vector and accumulated distance */
		startdistance = painter->accumdistance;
		sub_v2_v2v2(dmousepos, pos, painter->lastmousepos);
		len = normalize_v2(dmousepos);
		painter->accumdistance += len;

		if (brush->flag & BRUSH_SPACE) {
			/* do paint op over unpainted distance */
			while ((len > 0.0f) && (painter->accumdistance >= spacing)) {
				step = spacing - startdistance;
				paintpos[0] = painter->lastmousepos[0] + dmousepos[0] * step;
				paintpos[1] = painter->lastmousepos[1] + dmousepos[1] * step;

				t = step / len;
				press = (1.0f - t) * painter->lastpressure + t * pressure;
				brush_pressure_apply(painter, brush, press);
				spacing = max_ff(1.0f, radius) * brush->spacing * 0.01f;

				BKE_brush_jitter_pos(scene, brush, paintpos, finalpos);

				if (painter->cache.enabled)
					brush_painter_refresh_cache(painter, finalpos, use_color_correction);

				totpaintops +=
				    func(user, painter->cache.ibuf, painter->lastpaintpos, finalpos);

				painter->lastpaintpos[0] = paintpos[0];
				painter->lastpaintpos[1] = paintpos[1];
				painter->accumdistance -= spacing;
				startdistance -= spacing;
			}
		}
		else {
			BKE_brush_jitter_pos(scene, brush, pos, finalpos);

			if (painter->cache.enabled)
				brush_painter_refresh_cache(painter, finalpos, use_color_correction);

			totpaintops += func(user, painter->cache.ibuf, pos, finalpos);

			painter->lastpaintpos[0] = pos[0];
			painter->lastpaintpos[1] = pos[1];
			painter->accumdistance = 0;
		}

		/* do airbrush paint ops, based on the number of paint ops left over
		 * from regular painting. this is a temporary solution until we have
		 * accurate time stamps for mouse move events */
		if (brush->flag & BRUSH_AIRBRUSH) {
			double curtime = time;
			double painttime = brush->rate * totpaintops;

			painter->accumtime += curtime - painter->lasttime;
			if (painter->accumtime <= painttime)
				painter->accumtime = 0.0;
			else
				painter->accumtime -= painttime;

			while (painter->accumtime >= (double)brush->rate) {
				brush_pressure_apply(painter, brush, pressure);

				BKE_brush_jitter_pos(scene, brush, pos, finalpos);

				if (painter->cache.enabled)
					brush_painter_refresh_cache(painter, finalpos, use_color_correction);

				totpaintops +=
				    func(user, painter->cache.ibuf, painter->lastmousepos, finalpos);
				painter->accumtime -= (double)brush->rate;
			}

			painter->lasttime = curtime;
		}
	}

	painter->lastmousepos[0] = pos[0];
	painter->lastmousepos[1] = pos[1];
	painter->lastpressure = pressure;

	BKE_brush_alpha_set(scene, brush, painter->startalpha);
	BKE_brush_size_set(scene, brush, painter->startsize);
	brush->jitter = painter->startjitter;
	brush->spacing = painter->startspacing;

	return totpaintops;
}


/* TODO: should probably be unified with BrushPainter stuff? */
unsigned int *BKE_brush_gen_texture_cache(Brush *br, int half_side)
{
	unsigned int *texcache = NULL;
	MTex *mtex = &br->mtex;
	TexResult texres = {0};
	int hasrgb, ix, iy;
	int side = half_side * 2;
	
	if (mtex->tex) {
		float x, y, step = 2.0 / side, co[3];

		texcache = MEM_callocN(sizeof(int) * side * side, "Brush texture cache");

		/*do normalized cannonical view coords for texture*/
		for (y = -1.0, iy = 0; iy < side; iy++, y += step) {
			for (x = -1.0, ix = 0; ix < side; ix++, x += step) {
				co[0] = x;
				co[1] = y;
				co[2] = 0.0f;
				
				/* This is copied from displace modifier code */
				hasrgb = multitex_ext(mtex->tex, co, NULL, NULL, 0, &texres);
			
				/* if the texture gave an RGB value, we assume it didn't give a valid
				 * intensity, so calculate one (formula from do_material_tex).
				 * if the texture didn't give an RGB value, copy the intensity across
				 */
				if (hasrgb & TEX_RGB)
					texres.tin = rgb_to_grayscale(&texres.tr);

				((char *)texcache)[(iy * side + ix) * 4] =
				((char *)texcache)[(iy * side + ix) * 4 + 1] =
				((char *)texcache)[(iy * side + ix) * 4 + 2] =
				((char *)texcache)[(iy * side + ix) * 4 + 3] = (char)(texres.tin * 255.0f);
			}
		}
	}

	return texcache;
}


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
#include "BLI_rect.h"

#include "BKE_context.h"
#include "BKE_brush.h"
#include "BKE_main.h"
#include "BKE_image.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "ED_screen.h"
#include "ED_sculpt.h"

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

typedef struct BrushPainterCache {
	int size;                    /* size override, if 0 uses 2*BKE_brush_size_get(brush) */

	bool use_float;              /* need float imbuf? */
	bool use_color_correction;   /* use color correction for float */
	bool use_masking;            /* use masking? */

	bool is_texbrush;
	bool is_maskbrush;

	int lastsize;
	float lastalpha;
	float lastjitter;
	float last_tex_rotation;
	float last_mask_rotation;

	ImBuf *ibuf;
	ImBuf *texibuf;
	unsigned short *mask;
} BrushPainterCache;

typedef struct BrushPainter {
	Scene *scene;
	Brush *brush;

	float lastpaintpos[2];  /* position of last paint op */
	float startpaintpos[2]; /* position of first paint */

	short firsttouch;       /* first paint op */

	struct ImagePool *pool;	/* image pool */
	rctf tex_mapping;		/* texture coordinate mapping */
	rctf mask_mapping;		/* mask texture coordinate mapping */

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
	struct ImagePool *image_pool;

	Brush *brush;
	short tool, blend;
	Image *image;
	ImBuf *canvas;
	ImBuf *clonecanvas;
	char *warnpackedfile;
	char *warnmultifile;

	bool do_masking;

	/* viewport texture paint only, but _not_ project paint */
	Object *ob;
	int faceindex;
	float uv[2];
	int do_facesel;

	bool need_redraw;
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


static void brush_painter_2d_require_imbuf(BrushPainter *painter, bool use_float, bool use_color_correction, bool use_masking)
{
	Brush *brush = painter->brush;

	if ((painter->cache.use_float != use_float)) {
		if (painter->cache.ibuf) IMB_freeImBuf(painter->cache.ibuf);
		if (painter->cache.mask) MEM_freeN(painter->cache.mask);
		painter->cache.ibuf = NULL;
		painter->cache.mask = NULL;
		painter->cache.lastsize = -1; /* force ibuf create in refresh */
	}

	if (painter->cache.use_float != use_float) {
		if (painter->cache.texibuf) IMB_freeImBuf(painter->cache.texibuf);
		painter->cache.texibuf = NULL;
		painter->cache.lastsize = -1; /* force ibuf create in refresh */
	}

	painter->cache.use_float = use_float;
	painter->cache.use_color_correction = use_float && use_color_correction;
	painter->cache.use_masking = use_masking;
	painter->cache.is_texbrush = (brush->mtex.tex && brush->imagepaint_tool == PAINT_TOOL_DRAW) ? true : false;
	painter->cache.is_maskbrush = (brush->mask_mtex.tex) ? true : false;
}

static void brush_painter_2d_free(BrushPainter *painter)
{
	if (painter->cache.ibuf) IMB_freeImBuf(painter->cache.ibuf);
	if (painter->cache.texibuf) IMB_freeImBuf(painter->cache.texibuf);
	if (painter->cache.mask) MEM_freeN(painter->cache.mask);
	MEM_freeN(painter);
}

static void brush_imbuf_tex_co(rctf *mapping, int x, int y, float texco[3])
{
	texco[0] = mapping->xmin + x * mapping->xmax;
	texco[1] = mapping->ymin + y * mapping->ymax;
	texco[2] = 0.0f;
}

/* create a mask with the falloff strength and optionally brush alpha */
static unsigned short *brush_painter_mask_new(BrushPainter *painter, int size)
{
	Scene *scene = painter->scene;
	Brush *brush = painter->brush;
	bool use_masking = painter->cache.use_masking;

	float alpha = (use_masking) ? 1.0f : BKE_brush_alpha_get(scene, brush);
	int radius = BKE_brush_size_get(scene, brush);
	int xoff = -size * 0.5f + 0.5f;
	int yoff = -size * 0.5f + 0.5f;

	unsigned short *mask, *m;
	int x, y;

	mask = MEM_callocN(sizeof(unsigned short) * size * size, "brush_painter_mask");
	m = mask;

	for (y = 0; y < size; y++) {
		for (x = 0; x < size; x++, m++) {
			float xy[2] = {x + xoff, y + yoff};
			float len = len_v2(xy);
			float strength = alpha;

			strength *= BKE_brush_curve_strength_clamp(brush, len, radius);

			*m = (unsigned short)(65535.0f * strength);
		}
	}

	return mask;
}

/* create imbuf with brush color */
static ImBuf *brush_painter_imbuf_new(BrushPainter *painter, int size)
{
	Scene *scene = painter->scene;
	Brush *brush = painter->brush;

	rctf tex_mapping = painter->tex_mapping;
	rctf mask_mapping = painter->mask_mapping;
	struct ImagePool *pool = painter->pool;

	bool use_masking = painter->cache.use_masking;
	bool use_color_correction = painter->cache.use_color_correction;
	bool use_float = painter->cache.use_float;
	bool is_texbrush = painter->cache.is_texbrush;
	bool is_maskbrush = painter->cache.is_maskbrush;

	float alpha = (use_masking) ? 1.0f : BKE_brush_alpha_get(scene, brush);
	int radius = BKE_brush_size_get(scene, brush);
	int xoff = -size * 0.5f + 0.5f;
	int yoff = -size * 0.5f + 0.5f;

	int x, y, thread = 0;
	float brush_rgb[3];

	/* allocate image buffer */
	ImBuf *ibuf = IMB_allocImBuf(size, size, 32, (use_float) ? IB_rectfloat : IB_rect);

	/* get brush color */
	if (brush->imagepaint_tool == PAINT_TOOL_DRAW) {
		copy_v3_v3(brush_rgb, brush->rgb);

		if (use_color_correction)
			srgb_to_linearrgb_v3_v3(brush_rgb, brush_rgb);
	}
	else {
		brush_rgb[0] = 1.0f;
		brush_rgb[1] = 1.0f;
		brush_rgb[2] = 1.0f;
	}

	/* fill image buffer */
	for (y = 0; y < size; y++) {
		for (x = 0; x < size; x++) {
			/* sample texture and multiply with brush color */
			float texco[3], rgba[4];

			if (is_texbrush) {
				brush_imbuf_tex_co(&tex_mapping, x, y, texco);
				BKE_brush_sample_tex_3D(scene, brush, texco, rgba, thread, pool);
				/* TODO(sergey): Support texture paint color space. */
				if (!use_float) {
					linearrgb_to_srgb_v3_v3(rgba, rgba);
				}
				mul_v3_v3(rgba, brush_rgb);
			}
			else {
				copy_v3_v3(rgba, brush_rgb);
				rgba[3] = 1.0f;
			}

			if (is_maskbrush) {
				brush_imbuf_tex_co(&mask_mapping, x, y, texco);
				rgba[3] *= BKE_brush_sample_masktex(scene, brush, texco, thread, pool);
			}

			/* when not using masking, multiply in falloff and strength */
			if (!use_masking) {
				float xy[2] = {x + xoff, y + yoff};
				float len = len_v2(xy);

				rgba[3] *= alpha * BKE_brush_curve_strength_clamp(brush, len, radius);
			}

			if (use_float) {
				/* write to float pixel */
				float *dstf = ibuf->rect_float + (y * size + x) * 4;
				mul_v3_v3fl(dstf, rgba, rgba[3]); /* premultiply */
				dstf[3] = rgba[3];
			}
			else {
				/* write to byte pixel */
				unsigned char *dst = (unsigned char *)ibuf->rect + (y * size + x) * 4;

				rgb_float_to_uchar(dst, rgba);
				dst[3] = FTOCHAR(rgba[3]);
			}
		}
	}

	return ibuf;
}

/* update rectangular section of the brush image */
static void brush_painter_imbuf_update(BrushPainter *painter, ImBuf *oldtexibuf,
                                       int origx, int origy, int w, int h, int xt, int yt)
{
	Scene *scene = painter->scene;
	Brush *brush = painter->brush;

	rctf tex_mapping = painter->tex_mapping;
	rctf mask_mapping = painter->mask_mapping;
	struct ImagePool *pool = painter->pool;

	bool use_masking = painter->cache.use_masking;
	bool use_color_correction = painter->cache.use_color_correction;
	bool use_float = painter->cache.use_float;
	bool is_texbrush = painter->cache.is_texbrush;
	bool is_maskbrush = painter->cache.is_maskbrush;
	bool use_texture_old = (oldtexibuf != NULL);

	int x, y, thread = 0;
	float brush_rgb[3];

	ImBuf *ibuf = painter->cache.ibuf;
	ImBuf *texibuf = painter->cache.texibuf;
	unsigned short *mask = painter->cache.mask;

	/* get brush color */
	if (brush->imagepaint_tool == PAINT_TOOL_DRAW) {
		copy_v3_v3(brush_rgb, brush->rgb);

		if (use_color_correction)
			srgb_to_linearrgb_v3_v3(brush_rgb, brush_rgb);
	}
	else {
		brush_rgb[0] = 1.0f;
		brush_rgb[1] = 1.0f;
		brush_rgb[2] = 1.0f;
	}

	/* fill pixes */
	for (y = origy; y < h; y++) {
		for (x = origx; x < w; x++) {
			/* sample texture and multiply with brush color */
			float texco[3], rgba[4];

			if (!use_texture_old) {
				if (is_texbrush) {
					brush_imbuf_tex_co(&tex_mapping, x, y, texco);
					BKE_brush_sample_tex_3D(scene, brush, texco, rgba, thread, pool);
					/* TODO(sergey): Support texture paint color space. */
					if (!use_float) {
						linearrgb_to_srgb_v3_v3(rgba, rgba);
					}
					mul_v3_v3(rgba, brush_rgb);
				}
				else {
					copy_v3_v3(rgba, brush_rgb);
					rgba[3] = 1.0f;
				}

				if (is_maskbrush) {
					brush_imbuf_tex_co(&mask_mapping, x, y, texco);
					rgba[3] *= BKE_brush_sample_masktex(scene, brush, texco, thread, pool);
				}
			}

			if (use_float) {
				/* handle float pixel */
				float *bf = ibuf->rect_float + (y * ibuf->x + x) * 4;
				float *tf = texibuf->rect_float + (y * texibuf->x + x) * 4;

				/* read from old texture buffer */
				if (use_texture_old) {
					float *otf = oldtexibuf->rect_float + ((y - origy + yt) * oldtexibuf->x + (x - origx + xt)) * 4;
					copy_v4_v4(rgba, otf);
				}

				/* write to new texture buffer */
				copy_v4_v4(tf, rgba);

				/* if not using masking, multiply in the mask now */
				if (!use_masking) {
					unsigned short *m = mask + (y * ibuf->x + x);
					rgba[3] *= *m * (1.0f / 65535.0f);
				}

				/* output premultiplied float image, mf was already premultiplied */
				mul_v3_v3fl(bf, rgba, rgba[3]);
				bf[3] = rgba[3];
			}
			else {
				unsigned char crgba[4];

				/* handle byte pixel */
				unsigned char *b = (unsigned char *)ibuf->rect + (y * ibuf->x + x) * 4;
				unsigned char *t = (unsigned char *)texibuf->rect + (y * texibuf->x + x) * 4;

				/* read from old texture buffer */
				if (use_texture_old) {
					unsigned char *ot = (unsigned char *)oldtexibuf->rect + ((y - origy + yt) * oldtexibuf->x + (x - origx + xt)) * 4;
					crgba[0] = ot[0];
					crgba[1] = ot[1];
					crgba[2] = ot[2];
					crgba[3] = ot[3];
				}
				else
					rgba_float_to_uchar(crgba, rgba);

				/* write to new texture buffer */
				t[0] = crgba[0];
				t[1] = crgba[1];
				t[2] = crgba[2];
				t[3] = crgba[3];

				/* if not using masking, multiply in the mask now */
				if (!use_masking) {
					unsigned short *m = mask + (y * ibuf->x + x);
					crgba[3] = (crgba[3] * (*m)) / 65535;
				}

				/* write to brush image buffer */
				b[0] = crgba[0];
				b[1] = crgba[1];
				b[2] = crgba[2];
				b[3] = crgba[3];
			}
		}
	}
}

/* update the brush image by trying to reuse the cached texture result. this
 * can be considerably faster for brushes that change size due to pressure or
 * textures that stick to the surface where only part of the pixels are new */
static void brush_painter_imbuf_partial_update(BrushPainter *painter, const float pos[2])
{
	const Scene *scene = painter->scene;
	Brush *brush = painter->brush;
	BrushPainterCache *cache = &painter->cache;
	ImBuf *oldtexibuf, *ibuf;
	int imbflag, destx, desty, srcx, srcy, w, h, x1, y1, x2, y2;
	int diameter = 2 * BKE_brush_size_get(scene, brush);

	/* create brush image buffer if it didn't exist yet */
	imbflag = (cache->use_float) ? IB_rectfloat : IB_rect;
	if (!cache->ibuf)
		cache->ibuf = IMB_allocImBuf(diameter, diameter, 32, imbflag);
	ibuf = cache->ibuf;

	/* create new texture image buffer with coordinates relative to old */
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
	x2 = min_ii(destx + w, ibuf->x);
	y2 = min_ii(desty + h, ibuf->y);

	/* blend existing texture in new position */
	if ((x1 < x2) && (y1 < y2))
		brush_painter_imbuf_update(painter, oldtexibuf, x1, y1, x2, y2, srcx, srcy);

	if (oldtexibuf)
		IMB_freeImBuf(oldtexibuf);

	/* sample texture in new areas */
	if ((0 < x1) && (0 < ibuf->y))
		brush_painter_imbuf_update(painter, NULL, 0, 0, x1, ibuf->y, 0, 0);
	if ((x2 < ibuf->x) && (0 < ibuf->y))
		brush_painter_imbuf_update(painter, NULL, x2, 0, ibuf->x, ibuf->y, 0, 0);
	if ((x1 < x2) && (0 < y1))
		brush_painter_imbuf_update(painter, NULL, x1, 0, x2, y1, 0, 0);
	if ((x1 < x2) && (y2 < ibuf->y))
		brush_painter_imbuf_update(painter, NULL, x1, y2, x2, ibuf->y, 0, 0);
}

static void brush_painter_2d_tex_mapping(ImagePaintState *s, int size, const float startpos[2], const float pos[2], const float mouse[2], int mapmode, rctf *mapping)
{
	float invw = 1.0f / (float)s->canvas->x;
	float invh = 1.0f / (float)s->canvas->y;
	int xmin, ymin, xmax, ymax;
	int ipos[2];

	/* find start coordinate of brush in canvas */
	ipos[0] = (int)floorf((pos[0] - size / 2) + 1.0f);
	ipos[1] = (int)floorf((pos[1] - size / 2) + 1.0f);

	if (mapmode == MTEX_MAP_MODE_STENCIL) {
		/* map from view coordinates of brush to region coordinates */
		UI_view2d_to_region_no_clip(s->v2d, ipos[0] * invw, ipos[1] * invh, &xmin, &ymin);
		UI_view2d_to_region_no_clip(s->v2d, (ipos[0] + size) * invw, (ipos[1] + size) * invh, &xmax, &ymax);

		/* output mapping from brush ibuf x/y to region coordinates */
		mapping->xmin = xmin;
		mapping->ymin = ymin;
		mapping->xmax = (xmax - xmin) / (float)size;
		mapping->ymax = (ymax - ymin) / (float)size;
	}
	else if (mapmode == MTEX_MAP_MODE_3D) {
		/* 3D mapping, just mapping to canvas 0..1  */
		mapping->xmin = 2.0f * (ipos[0] * invw - 0.5f);
		mapping->ymin = 2.0f * (ipos[1] * invh - 0.5f);
		mapping->xmax = 2.0f * invw;
		mapping->ymax = 2.0f * invh;
	}
	else if (ELEM(mapmode, MTEX_MAP_MODE_VIEW, MTEX_MAP_MODE_RANDOM)) {
		/* view mapping */
		mapping->xmin = mouse[0] - size * 0.5f + 0.5f;
		mapping->ymin = mouse[1] - size * 0.5f + 0.5f;
		mapping->xmax = 1.0f;
		mapping->ymax = 1.0f;
	}
	else /* if (mapmode == MTEX_MAP_MODE_TILED) */ {
		mapping->xmin = -size * 0.5f + 0.5f + (int)pos[0] - (int)startpos[0];
		mapping->ymin = -size * 0.5f + 0.5f + (int)pos[1] - (int)startpos[1];
		mapping->xmax = 1.0f;
		mapping->ymax = 1.0f;
	}
}

static void brush_painter_2d_refresh_cache(ImagePaintState *s, BrushPainter *painter, const float pos[2], const float mouse[2])
{
	const Scene *scene = painter->scene;
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
	Brush *brush = painter->brush;
	BrushPainterCache *cache = &painter->cache;
	const int diameter = 2 * BKE_brush_size_get(scene, brush);
	const int size = (cache->size) ? cache->size : diameter;
	const float alpha = BKE_brush_alpha_get(scene, brush);
	const bool use_masking = painter->cache.use_masking;

	bool do_random = false;
	bool do_partial_update = false;
	bool do_view = false;
	float tex_rotation = -brush->mtex.rot;
	float mask_rotation = -brush->mask_mtex.rot;

	/* determine how can update based on textures used */
	if (painter->cache.is_texbrush) {
		if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_VIEW) {
			do_view = true;
			tex_rotation += ups->brush_rotation;
		}
		else if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_RANDOM)
			do_random = true;
		else
			do_partial_update = true;

		brush_painter_2d_tex_mapping(s, size, painter->startpaintpos, pos, mouse,
		                             brush->mtex.brush_map_mode, &painter->tex_mapping);
	}

	if (painter->cache.is_maskbrush) {
		if (brush->mask_mtex.brush_map_mode == MTEX_MAP_MODE_VIEW) {
			do_view = true;
			mask_rotation += ups->brush_rotation;
		}
		else if (brush->mask_mtex.brush_map_mode == MTEX_MAP_MODE_RANDOM)
			do_random = true;
		else
			do_partial_update = true;

		brush_painter_2d_tex_mapping(s, size, painter->startpaintpos, pos, mouse,
		                             brush->mask_mtex.brush_map_mode, &painter->mask_mapping);
	}

	if (do_view || do_random)
		do_partial_update = false;

	painter->pool = BKE_image_pool_new();

	/* detect if we need to recreate image brush buffer */
	if (diameter != cache->lastsize ||
	    alpha != cache->lastalpha ||
	    brush->jitter != cache->lastjitter ||
	    tex_rotation != cache->last_tex_rotation ||
	    mask_rotation != cache->last_mask_rotation ||
	    do_random)
	{
		if (cache->ibuf) {
			IMB_freeImBuf(cache->ibuf);
			cache->ibuf = NULL;
		}
		if (cache->mask) {
			MEM_freeN(cache->mask);
			cache->mask = NULL;
		}

		if (do_partial_update) {
			/* do partial update of texture + recreate mask */
			cache->mask = brush_painter_mask_new(painter, size);
			brush_painter_imbuf_partial_update(painter, pos);
		}
		else {
			/* create brush and mask from scratch */
			if (use_masking)
				cache->mask = brush_painter_mask_new(painter, size);
			cache->ibuf = brush_painter_imbuf_new(painter, size);
		}

		cache->lastsize = diameter;
		cache->lastalpha = alpha;
		cache->lastjitter = brush->jitter;
		cache->last_tex_rotation = tex_rotation;
		cache->last_mask_rotation = mask_rotation;
	}
	else if (do_partial_update) {
		/* do only partial update of texture */
		int dx = (int)painter->lastpaintpos[0] - (int)pos[0];
		int dy = (int)painter->lastpaintpos[1] - (int)pos[1];

		if ((dx != 0) || (dy != 0)) {
			brush_painter_imbuf_partial_update(painter, pos);
		}
	}

	BKE_image_pool_free(painter->pool);
	painter->pool = NULL;
}

/* keep these functions in sync */
static void paint_2d_ibuf_rgb_get(ImBuf *ibuf, int x, int y, const bool is_torus, float r_rgb[4])
{
	if (is_torus) {
		x %= ibuf->x;
		if (x < 0) x += ibuf->x;
		y %= ibuf->y;
		if (y < 0) y += ibuf->y;
	}

	if (ibuf->rect_float) {
		float *rrgbf = ibuf->rect_float + (ibuf->x * y + x) * 4;
		copy_v4_v4(r_rgb, rrgbf);
	}
	else {
		unsigned char *rrgb = (unsigned char *)ibuf->rect + (ibuf->x * y + x) * 4;
		straight_uchar_to_premul_float(r_rgb, rrgb);
	}
}
static void paint_2d_ibuf_rgb_set(ImBuf *ibuf, int x, int y, const bool is_torus, const float rgb[4])
{
	if (is_torus) {
		x %= ibuf->x;
		if (x < 0) x += ibuf->x;
		y %= ibuf->y;
		if (y < 0) y += ibuf->y;
	}

	if (ibuf->rect_float) {
		float *rrgbf = ibuf->rect_float + (ibuf->x * y + x) * 4;
		float map_alpha = (rgb[3] == 0.0f) ? rrgbf[3] : rrgbf[3] / rgb[3];

		mul_v3_v3fl(rrgbf, rgb, map_alpha);
	}
	else {
		unsigned char straight[4];
		unsigned char *rrgb = (unsigned char *)ibuf->rect + (ibuf->x * y + x) * 4;

		premul_float_to_straight_uchar(straight, rgb);
		rrgb[0] = straight[0];
		rrgb[1] = straight[1];
		rrgb[2] = straight[2];
	}
}

static int paint_2d_ibuf_add_if(ImBuf *ibuf, unsigned int x, unsigned int y, float *outrgb, short torus)
{
	float inrgb[4];

	// XXX: signed unsigned mismatch
	if ((x >= (unsigned int)(ibuf->x)) || (y >= (unsigned int)(ibuf->y))) {
		if (torus) paint_2d_ibuf_rgb_get(ibuf, x, y, 1, inrgb);
		else return 0;
	}
	else {
		paint_2d_ibuf_rgb_get(ibuf, x, y, 0, inrgb);
	}

	add_v4_v4(outrgb, inrgb);

	return 1;
}

static void paint_2d_lift_soften(ImBuf *ibuf, ImBuf *ibufb, int *pos, const bool is_torus)
{
	int x, y, count, xi, yi, xo, yo;
	int out_off[2], in_off[2], dim[2];
	float outrgb[4];

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

			mul_v4_fl(outrgb, 1.0f / (float)count);

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
		IMB_rectblend(ibufb, ibufb, ibuf, NULL, NULL, 0, region[a].destx, region[a].desty,
		              region[a].destx, region[a].desty,
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
	IMB_rectblend(clonebuf, clonebuf, ibufb, NULL, NULL, 0, destx, desty, destx, desty, destx, desty, w, h,
	              IMB_BLEND_COPY_ALPHA);
	IMB_rectblend(clonebuf, clonebuf, ibuf, NULL, NULL, 0, destx, desty, destx, desty, srcx, srcy, w, h,
	              IMB_BLEND_COPY_RGB);

	return clonebuf;
}

static void paint_2d_convert_brushco(ImBuf *ibufb, const float pos[2], int ipos[2])
{
	ipos[0] = (int)floorf((pos[0] - ibufb->x / 2) + 1.0f);
	ipos[1] = (int)floorf((pos[1] - ibufb->y / 2) + 1.0f);
}

static int paint_2d_op(void *state, ImBuf *ibufb, unsigned short *maskb, const float lastpos[2], const float pos[2])
{
	ImagePaintState *s = ((ImagePaintState *)state);
	ImBuf *clonebuf = NULL, *frombuf, *tmpbuf = NULL;
	ImagePaintRegion region[4];
	short torus = s->brush->flag & BRUSH_TORUS;
	short blend = s->blend;
	float *offset = s->brush->clone.offset;
	float liftpos[2];
	float brush_alpha = BKE_brush_alpha_get(s->scene, s->brush);
	unsigned short mask_max = (unsigned short)(brush_alpha * 65535.0f);
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

	if (s->do_masking)
		tmpbuf = IMB_allocImBuf(IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32, 0);
	
	/* blend into canvas */
	for (a = 0; a < tot; a++) {
		ED_imapaint_dirty_region(s->image, s->canvas,
		                      region[a].destx, region[a].desty,
		                      region[a].width, region[a].height);
	
		if (s->do_masking) {
			/* masking, find original pixels tiles from undo buffer to composite over */
			int tilex, tiley, tilew, tileh, tx, ty;

			imapaint_region_tiles(s->canvas, region[a].destx, region[a].desty,
			                      region[a].width, region[a].height,
			                      &tilex, &tiley, &tilew, &tileh);
			
			for (ty = tiley; ty <= tileh; ty++) {
				for (tx = tilex; tx <= tilew; tx++) {
					/* retrieve original pixels + mask from undo buffer */
					unsigned short *mask;
					int origx = region[a].destx - tx * IMAPAINT_TILE_SIZE;
					int origy = region[a].desty - ty * IMAPAINT_TILE_SIZE;

					if (s->canvas->rect_float)
						tmpbuf->rect_float = image_undo_find_tile(s->image, s->canvas, tx, ty, &mask);
					else
						tmpbuf->rect = image_undo_find_tile(s->image, s->canvas, tx, ty, &mask);

					IMB_rectblend(s->canvas, tmpbuf, frombuf, mask,
					              maskb, mask_max,
					              region[a].destx, region[a].desty,
					              origx, origy,
					              region[a].srcx, region[a].srcy,
					              region[a].width, region[a].height, blend);
				}
			}
		}
		else {
			/* no masking, composite brush directly onto canvas */
			IMB_rectblend(s->canvas, s->canvas, frombuf, NULL, NULL, 0,
			              region[a].destx, region[a].desty,
			              region[a].destx, region[a].desty,
			              region[a].srcx, region[a].srcy,
			              region[a].width, region[a].height, blend);
		}
	}

	if (clonebuf) IMB_freeImBuf(clonebuf);
	if (tmpbuf) IMB_freeImBuf(tmpbuf);

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

	/* set masking */
	s->do_masking = (s->brush->flag & BRUSH_AIRBRUSH ||
	                 (s->brush->imagepaint_tool == PAINT_TOOL_SMEAR) ||
	                 (s->brush->mtex.tex && !ELEM3(s->brush->mtex.brush_map_mode, MTEX_MAP_MODE_TILED, MTEX_MAP_MODE_STENCIL, MTEX_MAP_MODE_3D)))
	                 ? false : true;
	
	return 1;
}

static void paint_2d_canvas_free(ImagePaintState *s)
{
	BKE_image_release_ibuf(s->image, s->canvas, NULL);
	BKE_image_release_ibuf(s->brush->clone.image, s->clonecanvas, NULL);

	if (s->do_masking)
		image_undo_remove_masks();
}

void paint_2d_stroke(void *ps, const float prev_mval[2], const float mval[2], int eraser)
{
	float newuv[2], olduv[2];
	ImagePaintState *s = ps;
	BrushPainter *painter = s->painter;
	ImBuf *ibuf = BKE_image_acquire_ibuf(s->image, s->sima ? &s->sima->iuser : NULL, NULL);
	const bool is_data = (ibuf && ibuf->colormanage_flag & IMB_COLORMANAGE_IS_DATA);

	if (!ibuf)
		return;

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
		float startuv[2];

		UI_view2d_region_to_view(s->v2d, 0, 0, &startuv[0], &startuv[1]);

		/* paint exactly once on first touch */
		painter->startpaintpos[0] = startuv[0] * ibuf->x;
		painter->startpaintpos[1] = startuv[1] * ibuf->y;

		painter->firsttouch = 0;
		copy_v2_v2(painter->lastpaintpos, newuv);
	}
	else {
		copy_v2_v2(painter->lastpaintpos, olduv);
	}

	/* OCIO_TODO: float buffers are now always linear, so always use color correction
	 *            this should probably be changed when texture painting color space is supported
	 */
	brush_painter_2d_require_imbuf(painter, (ibuf->rect_float != NULL), !is_data, s->do_masking);

	brush_painter_2d_refresh_cache(s, painter, newuv, mval);

	if (paint_2d_op(s, painter->cache.ibuf, painter->cache.mask, olduv, newuv))
		s->need_redraw = true;

	BKE_image_release_ibuf(s->image, ibuf, NULL);
}

void *paint_2d_new_stroke(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *settings = scene->toolsettings;
	Brush *brush = BKE_paint_brush(&settings->imapaint.paint);

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

void paint_2d_redraw(const bContext *C, void *ps, bool final)
{
	ImagePaintState *s = ps;

	if (s->need_redraw) {
		ImBuf *ibuf = BKE_image_acquire_ibuf(s->image, s->sima ? &s->sima->iuser : NULL, NULL);

		imapaint_image_update(s->sima, s->image, ibuf, false);
		ED_imapaint_clear_partial_redraw();

		BKE_image_release_ibuf(s->image, ibuf, NULL);

		s->need_redraw = false;
	}
	else if (!final) {
		return;
	}

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
			WM_event_add_notifier(C, NC_IMAGE | NA_PAINTING, s->image);
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

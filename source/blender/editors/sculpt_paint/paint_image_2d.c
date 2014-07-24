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
#include "BLI_math_color_blend.h"
#include "BLI_stack.h"
#include "BLI_bitmap.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_brush.h"
#include "BKE_image.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_texture.h"

#include "ED_paint.h"
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

typedef struct BrushPainterCache {
	bool use_float;              /* need float imbuf? */
	bool use_color_correction;   /* use color correction for float */
	bool invert;

	bool is_texbrush;
	bool is_maskbrush;

	int lastdiameter;
	float last_tex_rotation;
	float last_mask_rotation;
	float last_pressure;

	ImBuf *ibuf;
	ImBuf *texibuf;
	unsigned short *curve_mask;
	unsigned short *tex_mask;
	unsigned short *tex_mask_old;
	unsigned int tex_mask_old_w;
	unsigned int tex_mask_old_h;
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
	const char *warnpackedfile;
	const char *warnmultifile;

	bool do_masking;

	/* viewport texture paint only, but _not_ project paint */
	Object *ob;
	int faceindex;
	float uv[2];
	int do_facesel;

	bool need_redraw;

	BlurKernel *blurkernel;
} ImagePaintState;


static BrushPainter *brush_painter_2d_new(Scene *scene, Brush *brush, bool invert)
{
	BrushPainter *painter = MEM_callocN(sizeof(BrushPainter), "BrushPainter");

	painter->brush = brush;
	painter->scene = scene;
	painter->firsttouch = 1;
	painter->cache.lastdiameter = -1; /* force ibuf create in refresh */
	painter->cache.invert = invert;

	return painter;
}


static void brush_painter_2d_require_imbuf(BrushPainter *painter, bool use_float, bool use_color_correction)
{
	Brush *brush = painter->brush;

	if ((painter->cache.use_float != use_float)) {
		if (painter->cache.ibuf) IMB_freeImBuf(painter->cache.ibuf);
		if (painter->cache.curve_mask) MEM_freeN(painter->cache.curve_mask);
		if (painter->cache.tex_mask) MEM_freeN(painter->cache.tex_mask);
		if (painter->cache.tex_mask_old) MEM_freeN(painter->cache.tex_mask_old);
		painter->cache.ibuf = NULL;
		painter->cache.curve_mask = NULL;
		painter->cache.tex_mask = NULL;
		painter->cache.lastdiameter = -1; /* force ibuf create in refresh */
	}

	painter->cache.use_float = use_float;
	painter->cache.use_color_correction = use_float && use_color_correction;
	painter->cache.is_texbrush = (brush->mtex.tex && brush->imagepaint_tool == PAINT_TOOL_DRAW) ? true : false;
	painter->cache.is_maskbrush = (brush->mask_mtex.tex) ? true : false;
}

static void brush_painter_2d_free(BrushPainter *painter)
{
	if (painter->cache.ibuf) IMB_freeImBuf(painter->cache.ibuf);
	if (painter->cache.texibuf) IMB_freeImBuf(painter->cache.texibuf);
	if (painter->cache.curve_mask) MEM_freeN(painter->cache.curve_mask);
	if (painter->cache.tex_mask) MEM_freeN(painter->cache.tex_mask);
	if (painter->cache.tex_mask_old) MEM_freeN(painter->cache.tex_mask_old);
	MEM_freeN(painter);
}

static void brush_imbuf_tex_co(rctf *mapping, int x, int y, float texco[3])
{
	texco[0] = mapping->xmin + x * mapping->xmax;
	texco[1] = mapping->ymin + y * mapping->ymax;
	texco[2] = 0.0f;
}

/* create a mask with the mask texture */
static unsigned short *brush_painter_mask_ibuf_new(BrushPainter *painter, int size)
{
	Scene *scene = painter->scene;
	Brush *brush = painter->brush;
	rctf mask_mapping = painter->mask_mapping;
	struct ImagePool *pool = painter->pool;

	float texco[3];
	unsigned short *mask, *m;
	int x, y, thread = 0;

	mask = MEM_mallocN(sizeof(unsigned short) * size * size, "brush_painter_mask");
	m = mask;

	for (y = 0; y < size; y++) {
		for (x = 0; x < size; x++, m++) {
			float res;
			brush_imbuf_tex_co(&mask_mapping, x, y, texco);
			res = BKE_brush_sample_masktex(scene, brush, texco, thread, pool);
			*m = (unsigned short)(65535.0f * res);
		}
	}

	return mask;
}

/* update rectangular section of the brush image */
static void brush_painter_mask_imbuf_update(
        BrushPainter *painter, unsigned short *tex_mask_old,
        int origx, int origy, int w, int h, int xt, int yt, int diameter)
{
	Scene *scene = painter->scene;
	Brush *brush = painter->brush;
	rctf tex_mapping = painter->mask_mapping;
	struct ImagePool *pool = painter->pool;
	unsigned short res;

	bool use_texture_old = (tex_mask_old != NULL);

	int x, y, thread = 0;

	unsigned short *tex_mask = painter->cache.tex_mask;
	unsigned short *tex_mask_cur = painter->cache.tex_mask_old;

	/* fill pixels */
	for (y = origy; y < h; y++) {
		for (x = origx; x < w; x++) {
			/* sample texture */
			float texco[3];

			/* handle byte pixel */
			unsigned short *b = tex_mask + (y * diameter + x);
			unsigned short *t = tex_mask_cur + (y * diameter + x);

			if (!use_texture_old) {
				brush_imbuf_tex_co(&tex_mapping, x, y, texco);
				res = (unsigned short)(65535.0f * BKE_brush_sample_masktex(scene, brush, texco, thread, pool));
			}

			/* read from old texture buffer */
			if (use_texture_old) {
				res = *(tex_mask_old + ((y - origy + yt) * painter->cache.tex_mask_old_w + (x - origx + xt)));
			}

			/* write to new texture mask */
			*t = res;
			/* write to mask image buffer */
			*b = res;
		}
	}
}


/**
 * Update the brush mask image by trying to reuse the cached texture result.
 * This can be considerably faster for brushes that change size due to pressure or
 * textures that stick to the surface where only part of the pixels are new
 */
static void brush_painter_mask_imbuf_partial_update(BrushPainter *painter, const float pos[2], int diameter)
{
	BrushPainterCache *cache = &painter->cache;
	unsigned short *tex_mask_old;
	int destx, desty, srcx, srcy, w, h, x1, y1, x2, y2;

	/* create brush image buffer if it didn't exist yet */
	if (!cache->tex_mask)
		cache->tex_mask = MEM_mallocN(sizeof(unsigned short) * diameter * diameter, "brush_painter_mask");

	/* create new texture image buffer with coordinates relative to old */
	tex_mask_old = cache->tex_mask_old;
	cache->tex_mask_old = MEM_mallocN(sizeof(unsigned short) * diameter * diameter, "brush_painter_mask");

	if (tex_mask_old) {
		ImBuf maskibuf;
		ImBuf maskibuf_old;
		maskibuf.x = maskibuf.y = diameter;
		maskibuf_old.x = cache->tex_mask_old_w;
		maskibuf_old.y = cache->tex_mask_old_h;

		srcx = srcy = 0;
		w = cache->tex_mask_old_w;
		h = cache->tex_mask_old_h;
		destx = (int)painter->lastpaintpos[0] - (int)pos[0]  + (diameter / 2 - w / 2);
		desty = (int)painter->lastpaintpos[1] - (int)pos[1]  + (diameter / 2 - h / 2);

		/* hack, use temporary rects so that clipping works */
		IMB_rectclip(&maskibuf, &maskibuf_old, &destx, &desty, &srcx, &srcy, &w, &h);
	}
	else {
		srcx = srcy = 0;
		destx = desty = 0;
		w = h = 0;
	}

	x1 = min_ii(destx, diameter);
	y1 = min_ii(desty, diameter);
	x2 = min_ii(destx + w, diameter);
	y2 = min_ii(desty + h, diameter);

	/* blend existing texture in new position */
	if ((x1 < x2) && (y1 < y2))
		brush_painter_mask_imbuf_update(painter, tex_mask_old, x1, y1, x2, y2, srcx, srcy, diameter);

	if (tex_mask_old)
		MEM_freeN(tex_mask_old);

	/* sample texture in new areas */
	if ((0 < x1) && (0 < diameter))
		brush_painter_mask_imbuf_update(painter, NULL, 0, 0, x1, diameter, 0, 0, diameter);
	if ((x2 < diameter) && (0 < diameter))
		brush_painter_mask_imbuf_update(painter, NULL, x2, 0, diameter, diameter, 0, 0, diameter);
	if ((x1 < x2) && (0 < y1))
		brush_painter_mask_imbuf_update(painter, NULL, x1, 0, x2, y1, 0, 0, diameter);
	if ((x1 < x2) && (y2 < diameter))
		brush_painter_mask_imbuf_update(painter, NULL, x1, y2, x2, diameter, 0, 0, diameter);

	/* through with sampling, now update sizes */
	cache->tex_mask_old_w = diameter;
	cache->tex_mask_old_h = diameter;
}

/* create a mask with the falloff strength */
static unsigned short *brush_painter_curve_mask_new(BrushPainter *painter, int diameter, float radius)
{
	Brush *brush = painter->brush;

	int xoff = -diameter * 0.5f + 0.5f;
	int yoff = -diameter * 0.5f + 0.5f;

	unsigned short *mask, *m;
	int x, y;

	mask = MEM_mallocN(sizeof(unsigned short) * diameter * diameter, "brush_painter_mask");
	m = mask;

	for (y = 0; y < diameter; y++) {
		for (x = 0; x < diameter; x++, m++) {
			float xy[2] = {x + xoff, y + yoff};
			float len = len_v2(xy);

			*m = (unsigned short)(65535.0f * BKE_brush_curve_strength_clamp(brush, len, radius));
		}
	}

	return mask;
}


/* create imbuf with brush color */
static ImBuf *brush_painter_imbuf_new(BrushPainter *painter, int size, float pressure, float distance)
{
	Scene *scene = painter->scene;
	Brush *brush = painter->brush;

	const char *display_device = scene->display_settings.display_device;
	struct ColorManagedDisplay *display = IMB_colormanagement_display_get_named(display_device);

	rctf tex_mapping = painter->tex_mapping;
	struct ImagePool *pool = painter->pool;

	bool use_color_correction = painter->cache.use_color_correction;
	bool use_float = painter->cache.use_float;
	bool is_texbrush = painter->cache.is_texbrush;

	int x, y, thread = 0;
	float brush_rgb[3];

	/* allocate image buffer */
	ImBuf *ibuf = IMB_allocImBuf(size, size, 32, (use_float) ? IB_rectfloat : IB_rect);

	/* get brush color */
	if (brush->imagepaint_tool == PAINT_TOOL_DRAW) {
		paint_brush_color_get(scene, brush, use_color_correction, painter->cache.invert, distance, pressure, brush_rgb, display);
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
					IMB_colormanagement_scene_linear_to_display_v3(rgba, display);
				}
				mul_v3_v3(rgba, brush_rgb);
			}
			else {
				copy_v3_v3(rgba, brush_rgb);
				rgba[3] = 1.0f;
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

	const char *display_device = scene->display_settings.display_device;
	struct ColorManagedDisplay *display = IMB_colormanagement_display_get_named(display_device);

	rctf tex_mapping = painter->tex_mapping;
	struct ImagePool *pool = painter->pool;

	bool use_color_correction = painter->cache.use_color_correction;
	bool use_float = painter->cache.use_float;
	bool is_texbrush = painter->cache.is_texbrush;
	bool use_texture_old = (oldtexibuf != NULL);

	int x, y, thread = 0;
	float brush_rgb[3];

	ImBuf *ibuf = painter->cache.ibuf;
	ImBuf *texibuf = painter->cache.texibuf;

	/* get brush color */
	if (brush->imagepaint_tool == PAINT_TOOL_DRAW) {
		paint_brush_color_get(scene, brush, use_color_correction, painter->cache.invert, 0.0, 1.0, brush_rgb, display);
	}
	else {
		brush_rgb[0] = 1.0f;
		brush_rgb[1] = 1.0f;
		brush_rgb[2] = 1.0f;
	}

	/* fill pixels */
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
						IMB_colormanagement_scene_linear_to_display_v3(rgba, display);
					}
					mul_v3_v3(rgba, brush_rgb);
				}
				else {
					copy_v3_v3(rgba, brush_rgb);
					rgba[3] = 1.0f;
				}
			}

			if (use_float) {
				/* handle float pixel */
				float *bf = ibuf->rect_float + (y * ibuf->x + x) * 4;
				float *tf = texibuf->rect_float + (y * texibuf->x + x) * 4;

				/* read from old texture buffer */
				if (use_texture_old) {
					const float *otf = oldtexibuf->rect_float + ((y - origy + yt) * oldtexibuf->x + (x - origx + xt)) * 4;
					copy_v4_v4(rgba, otf);
				}

				/* write to new texture buffer */
				copy_v4_v4(tf, rgba);

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
static void brush_painter_imbuf_partial_update(BrushPainter *painter, const float pos[2], int diameter)
{
	BrushPainterCache *cache = &painter->cache;
	ImBuf *oldtexibuf, *ibuf;
	int imbflag, destx, desty, srcx, srcy, w, h, x1, y1, x2, y2;

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
		w = oldtexibuf->x;
		h = oldtexibuf->y;
		destx = (int)painter->lastpaintpos[0] - (int)pos[0] + (diameter / 2 - w / 2);
		desty = (int)painter->lastpaintpos[1] - (int)pos[1] + (diameter / 2 - h / 2);

		IMB_rectclip(cache->texibuf, oldtexibuf, &destx, &desty, &srcx, &srcy, &w, &h);
	}
	else {
		srcx = srcy = 0;
		destx = desty = 0;
		w = h = 0;
	}
	
	x1 = min_ii(destx, ibuf->x);
	y1 = min_ii(desty, ibuf->y);
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

static void brush_painter_2d_tex_mapping(ImagePaintState *s, int diameter, const float startpos[2], const float pos[2], const float mouse[2], int mapmode, rctf *mapping)
{
	float invw = 1.0f / (float)s->canvas->x;
	float invh = 1.0f / (float)s->canvas->y;
	int xmin, ymin, xmax, ymax;
	int ipos[2];

	/* find start coordinate of brush in canvas */
	ipos[0] = (int)floorf((pos[0] - diameter / 2) + 1.0f);
	ipos[1] = (int)floorf((pos[1] - diameter / 2) + 1.0f);

	if (mapmode == MTEX_MAP_MODE_STENCIL) {
		/* map from view coordinates of brush to region coordinates */
		UI_view2d_view_to_region(s->v2d, ipos[0] * invw, ipos[1] * invh, &xmin, &ymin);
		UI_view2d_view_to_region(s->v2d, (ipos[0] + diameter) * invw, (ipos[1] + diameter) * invh, &xmax, &ymax);

		/* output mapping from brush ibuf x/y to region coordinates */
		mapping->xmin = xmin;
		mapping->ymin = ymin;
		mapping->xmax = (xmax - xmin) / (float)diameter;
		mapping->ymax = (ymax - ymin) / (float)diameter;
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
		mapping->xmin = mouse[0] - diameter * 0.5f + 0.5f;
		mapping->ymin = mouse[1] - diameter * 0.5f + 0.5f;
		mapping->xmax = 1.0f;
		mapping->ymax = 1.0f;
	}
	else /* if (mapmode == MTEX_MAP_MODE_TILED) */ {
		mapping->xmin = (int)(-diameter * 0.5) + (int)pos[0] - (int)startpos[0];
		mapping->ymin = (int)(-diameter * 0.5) + (int)pos[1] - (int)startpos[1];
		mapping->xmax = 1.0f;
		mapping->ymax = 1.0f;
	}
}

static void brush_painter_2d_refresh_cache(ImagePaintState *s, BrushPainter *painter, const float pos[2], const float mouse[2], float pressure, float distance, float size)
{
	const Scene *scene = painter->scene;
	UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
	Brush *brush = painter->brush;
	BrushPainterCache *cache = &painter->cache;
	const int diameter = 2 * size;

	bool do_random = false;
	bool do_partial_update = false;
	bool update_color = (brush->flag & BRUSH_USE_GRADIENT) &&
	                    ((ELEM(brush->gradient_stroke_mode,
	                           BRUSH_GRADIENT_SPACING_REPEAT,
	                           BRUSH_GRADIENT_SPACING_CLAMP)) ||
	                     (cache->last_pressure != pressure));
	float tex_rotation = -brush->mtex.rot;
	float mask_rotation = -brush->mask_mtex.rot;

	painter->pool = BKE_image_pool_new();

	/* determine how can update based on textures used */
	if (painter->cache.is_texbrush) {
		if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_VIEW) {
			tex_rotation += ups->brush_rotation;
		}
		else if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_RANDOM)
			do_random = true;
		else if (!((brush->flag & BRUSH_ANCHORED) || update_color))
			do_partial_update = true;

		brush_painter_2d_tex_mapping(s, diameter, painter->startpaintpos, pos, mouse,
		                             brush->mtex.brush_map_mode, &painter->tex_mapping);
	}

	if (painter->cache.is_maskbrush) {
		bool renew_maxmask = false;
		bool do_partial_update_mask = false;
		/* invalidate case for all mapping modes */
		if (brush->mask_mtex.brush_map_mode == MTEX_MAP_MODE_VIEW) {
			mask_rotation += ups->brush_rotation;
		}
		else if (brush->mask_mtex.brush_map_mode == MTEX_MAP_MODE_RANDOM) {
			renew_maxmask = true;
		}
		else if (!(brush->flag & BRUSH_ANCHORED)) {
			do_partial_update_mask = true;
			renew_maxmask = true;
		}
		/* explicilty disable partial update even if it has been enabled above */
		if (brush->mask_pressure) {
			do_partial_update_mask = false;
			renew_maxmask = true;
		}

		if ((diameter != cache->lastdiameter) ||
		    (mask_rotation != cache->last_mask_rotation) ||
		    renew_maxmask)
		{
			if (cache->tex_mask) {
				MEM_freeN(cache->tex_mask);
				cache->tex_mask = NULL;
			}

			brush_painter_2d_tex_mapping(s, diameter, painter->startpaintpos, pos, mouse,
			                             brush->mask_mtex.brush_map_mode, &painter->mask_mapping);

			if (do_partial_update_mask)
				brush_painter_mask_imbuf_partial_update(painter, pos, diameter);
			else
				cache->tex_mask = brush_painter_mask_ibuf_new(painter, diameter);
			cache->last_mask_rotation = mask_rotation;
		}
	}

	/* curve mask can only change if the size changes */
	if (diameter != cache->lastdiameter) {
		if (cache->curve_mask) {
			MEM_freeN(cache->curve_mask);
			cache->curve_mask = NULL;
		}

		cache->curve_mask = brush_painter_curve_mask_new(painter, diameter, size);
	}

	/* detect if we need to recreate image brush buffer */
	if ((diameter != cache->lastdiameter) ||
	    (tex_rotation != cache->last_tex_rotation) ||
	    do_random ||
	    update_color)
	{
		if (cache->ibuf) {
			IMB_freeImBuf(cache->ibuf);
			cache->ibuf = NULL;
		}

		if (do_partial_update) {
			/* do partial update of texture */
			brush_painter_imbuf_partial_update(painter, pos, diameter);
		}
		else {
			/* create brush from scratch */
			cache->ibuf = brush_painter_imbuf_new(painter, diameter, pressure, distance);
		}

		cache->lastdiameter = diameter;
		cache->last_tex_rotation = tex_rotation;
		cache->last_pressure = pressure;
	}
	else if (do_partial_update) {
		/* do only partial update of texture */
		int dx = (int)painter->lastpaintpos[0] - (int)pos[0];
		int dy = (int)painter->lastpaintpos[1] - (int)pos[1];

		if ((dx != 0) || (dy != 0)) {
			brush_painter_imbuf_partial_update(painter, pos, diameter);
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
		const float *rrgbf = ibuf->rect_float + (ibuf->x * y + x) * 4;
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

static float paint_2d_ibuf_add_if(ImBuf *ibuf, unsigned int x, unsigned int y, float *outrgb, short torus, float w)
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

	mul_v4_fl(inrgb, w);
	add_v4_v4(outrgb, inrgb);

	return w;
}

static void paint_2d_lift_soften(ImagePaintState *s, ImBuf *ibuf, ImBuf *ibufb, int *pos, const short is_torus)
{
	bool sharpen = (s->painter->cache.invert ^ ((s->brush->flag & BRUSH_DIR_IN) != 0));
	float threshold = s->brush->sharp_threshold;
	int x, y, xi, yi, xo, yo, xk, yk;
	float count;
	int out_off[2], in_off[2], dim[2];
	int diff_pos[2];
	float outrgb[4];
	float rgba[4];
	BlurKernel *kernel = s->blurkernel;

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

	/* find offset inside mask buffers to sample them */
	sub_v2_v2v2_int(diff_pos, out_off, in_off);

	for (y = 0; y < dim[1]; y++) {
		for (x = 0; x < dim[0]; x++) {
			/* get input pixel */
			xi = in_off[0] + x;
			yi = in_off[1] + y;

			count = 0.0;
			paint_2d_ibuf_rgb_get(ibuf, xi, yi, is_torus, rgba);
			zero_v4(outrgb);

			for (yk = 0; yk < kernel->side; yk++) {
				for (xk = 0; xk < kernel->side; xk++) {
					count += paint_2d_ibuf_add_if(ibuf, xi + xk - kernel->pixel_len,
					                               yi + yk - kernel->pixel_len, outrgb, is_torus,
					                               kernel->wdata[xk + yk * kernel->side]);
				}
			}

			if (count > 0.0f) {
				mul_v4_fl(outrgb, 1.0f / (float)count);

				if (sharpen) {
					/* subtract blurred image from normal image gives high pass filter */
					sub_v3_v3v3(outrgb, rgba, outrgb);

					/* now rgba_ub contains the edge result, but this should be converted to luminance to avoid
					 * colored speckles appearing in final image, and also to check for threshhold */
					outrgb[0] = outrgb[1] = outrgb[2] = rgb_to_grayscale(outrgb);
					if (fabsf(outrgb[0]) > threshold) {
						float mask = BKE_brush_alpha_get(s->scene, s->brush);
						float alpha = rgba[3];
						rgba[3] = outrgb[3] = mask;

						/* add to enhance edges */
						blend_color_add_float(outrgb, rgba, outrgb);
						outrgb[3] = alpha;
					}
					else
						copy_v4_v4(outrgb, rgba);
				}
			}
			else
				copy_v4_v4(outrgb, rgba);
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
		IMB_rectblend(ibufb, ibufb, ibuf, NULL, NULL, NULL, 0, region[a].destx, region[a].desty,
		              region[a].destx, region[a].desty,
		              region[a].srcx, region[a].srcy,
		              region[a].width, region[a].height, IMB_BLEND_COPY_RGB, false);
}

static ImBuf *paint_2d_lift_clone(ImBuf *ibuf, ImBuf *ibufb, int *pos)
{
	/* note: allocImbuf returns zero'd memory, so regions outside image will
	 * have zero alpha, and hence not be blended onto the image */
	int w = ibufb->x, h = ibufb->y, destx = 0, desty = 0, srcx = pos[0], srcy = pos[1];
	ImBuf *clonebuf = IMB_allocImBuf(w, h, ibufb->planes, ibufb->flags);

	IMB_rectclip(clonebuf, ibuf, &destx, &desty, &srcx, &srcy, &w, &h);
	IMB_rectblend(clonebuf, clonebuf, ibufb, NULL, NULL, NULL, 0, destx, desty, destx, desty, destx, desty, w, h,
	              IMB_BLEND_COPY_ALPHA, false);
	IMB_rectblend(clonebuf, clonebuf, ibuf, NULL, NULL, NULL, 0, destx, desty, destx, desty, srcx, srcy, w, h,
	              IMB_BLEND_COPY_RGB, false);

	return clonebuf;
}

static void paint_2d_convert_brushco(ImBuf *ibufb, const float pos[2], int ipos[2])
{
	ipos[0] = (int)floorf((pos[0] - ibufb->x / 2) + 1.0f);
	ipos[1] = (int)floorf((pos[1] - ibufb->y / 2) + 1.0f);
}

static int paint_2d_op(void *state, ImBuf *ibufb, unsigned short *curveb, unsigned short *texmaskb, const float lastpos[2], const float pos[2])
{
	ImagePaintState *s = ((ImagePaintState *)state);
	ImBuf *clonebuf = NULL, *frombuf;
	ImagePaintRegion region[4];
	short torus = s->brush->flag & BRUSH_TORUS;
	short blend = s->blend;
	const float *offset = s->brush->clone.offset;
	float liftpos[2];
	float mask_max = BKE_brush_alpha_get(s->scene, s->brush);
	int bpos[2], blastpos[2], bliftpos[2];
	int a, tot;

	paint_2d_convert_brushco(ibufb, pos, bpos);

	/* lift from canvas */
	if (s->tool == PAINT_TOOL_SOFTEN) {
		paint_2d_lift_soften(s, s->canvas, ibufb, bpos, torus);
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
		ED_imapaint_dirty_region(s->image, s->canvas,
		                      region[a].destx, region[a].desty,
		                      region[a].width, region[a].height);
	
		if (s->do_masking) {
			/* masking, find original pixels tiles from undo buffer to composite over */
			int tilex, tiley, tilew, tileh, tx, ty;
			ImBuf *tmpbuf;

			imapaint_region_tiles(s->canvas, region[a].destx, region[a].desty,
			                      region[a].width, region[a].height,
			                      &tilex, &tiley, &tilew, &tileh);

			tmpbuf = IMB_allocImBuf(IMAPAINT_TILE_SIZE, IMAPAINT_TILE_SIZE, 32, 0);

			for (ty = tiley; ty <= tileh; ty++) {
				for (tx = tilex; tx <= tilew; tx++) {
					/* retrieve original pixels + mask from undo buffer */
					unsigned short *mask;
					int origx = region[a].destx - tx * IMAPAINT_TILE_SIZE;
					int origy = region[a].desty - ty * IMAPAINT_TILE_SIZE;

					if (s->canvas->rect_float)
						tmpbuf->rect_float = image_undo_find_tile(s->image, s->canvas, tx, ty, &mask, false);
					else
						tmpbuf->rect = image_undo_find_tile(s->image, s->canvas, tx, ty, &mask, false);

					IMB_rectblend(s->canvas, tmpbuf, frombuf, mask,
					              curveb, texmaskb, mask_max,
					              region[a].destx, region[a].desty,
					              origx, origy,
					              region[a].srcx, region[a].srcy,
					              region[a].width, region[a].height, blend, ((s->brush->flag & BRUSH_ACCUMULATE) != 0));
				}
			}

			IMB_freeImBuf(tmpbuf);
		}
		else {
			/* no masking, composite brush directly onto canvas */
			IMB_rectblend(s->canvas, s->canvas, frombuf, NULL, curveb, texmaskb, mask_max,
			              region[a].destx, region[a].desty,
			              region[a].destx, region[a].desty,
			              region[a].srcx, region[a].srcy,
			              region[a].width, region[a].height, blend, false);
		}
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

	/* set masking */
	s->do_masking = paint_use_opacity_masking(s->brush);
	
	return 1;
}

static void paint_2d_canvas_free(ImagePaintState *s)
{
	BKE_image_release_ibuf(s->image, s->canvas, NULL);
	BKE_image_release_ibuf(s->brush->clone.image, s->clonecanvas, NULL);

	if (s->blurkernel) {
		paint_delete_blur_kernel(s->blurkernel);
		MEM_freeN(s->blurkernel);
	}

	image_undo_remove_masks();
}

void paint_2d_stroke(void *ps, const float prev_mval[2], const float mval[2], int eraser, float pressure, float distance, float size)
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
	brush_painter_2d_require_imbuf(painter, (ibuf->rect_float != NULL), !is_data);

	brush_painter_2d_refresh_cache(s, painter, newuv, mval, pressure, distance, size);

	if (paint_2d_op(s, painter->cache.ibuf, painter->cache.curve_mask, painter->cache.tex_mask, olduv, newuv))
		s->need_redraw = true;

	BKE_image_release_ibuf(s->image, ibuf, NULL);
}

void *paint_2d_new_stroke(bContext *C, wmOperator *op, int mode)
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

	if (brush->imagepaint_tool == PAINT_TOOL_SOFTEN) {
		s->blurkernel = paint_new_blur_kernel(brush);
	}

	paint_brush_init_tex(s->brush);

	/* create painter */
	s->painter = brush_painter_2d_new(scene, s->brush, mode == BRUSH_STROKE_INVERT);

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
		DAG_id_tag_update(&s->image->id, 0);
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

static void paint_2d_fill_add_pixel_byte(
        const int x_px, const int y_px, ImBuf *ibuf, BLI_Stack *stack, BLI_bitmap *touched,
        const float color[4], float threshold_sq)
{
	int coordinate;

	if (x_px >= ibuf->x || x_px < 0 || y_px >= ibuf->y || y_px < 0)
		return;

	coordinate = y_px * ibuf->x + x_px;

	if (!BLI_BITMAP_TEST(touched, coordinate)) {
		float color_f[4];
		unsigned char *color_b = (unsigned char *)(ibuf->rect + coordinate);
		rgba_uchar_to_float(color_f, color_b);

		if (compare_len_squared_v3v3(color_f, color, threshold_sq)) {
			BLI_stack_push(stack, &coordinate);
		}
		BLI_BITMAP_SET(touched, coordinate, true);
	}
}

static void paint_2d_fill_add_pixel_float(
        const int x_px, const int y_px, ImBuf *ibuf, BLI_Stack *stack, BLI_bitmap *touched,
        const float color[4], float threshold_sq)
{
	int coordinate;

	if (x_px >= ibuf->x || x_px < 0 || y_px >= ibuf->y || y_px < 0)
		return;

	coordinate = y_px * ibuf->x + x_px;

	if (!BLI_BITMAP_TEST(touched, coordinate)) {
		if (compare_len_squared_v3v3(ibuf->rect_float + 4 * coordinate, color, threshold_sq)) {
			BLI_stack_push(stack, &coordinate);
		}
		BLI_BITMAP_SET(touched, coordinate, true);
	}
}

/* this function expects linear space color values */
void paint_2d_bucket_fill(
        const bContext *C, const float color[3], Brush *br,
        const float mouse_init[2],
        void *ps)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Image *ima = sima->image;

	ImagePaintState *s = ps;

	ImBuf *ibuf;
	int x_px, y_px;
	unsigned int color_b;
	float color_f[4];
	float strength = br ? br->alpha : 1.0f;

	bool do_float;

	if (!ima)
		return;

	ibuf = BKE_image_acquire_ibuf(ima, &sima->iuser, NULL);

	if (!ibuf)
		return;

	do_float = (ibuf->rect_float != NULL);
	/* first check if our image is float. If it is not we should correct the color to
	 * be in gamma space. strictly speaking this is not correct, but blender does not paint
	 * byte images in linear space */
	if (!do_float) {
		linearrgb_to_srgb_uchar3((unsigned char *)&color_b, color);
		*(((char *)&color_b) + 3) = strength * 255;
	}
	else {
		copy_v3_v3(color_f, color);
		color_f[3] = strength;
	}

	if (!mouse_init || !br) {
		/* first case, no image UV, fill the whole image */
		ED_imapaint_dirty_region(ima, ibuf, 0, 0, ibuf->x, ibuf->y);

		if (do_float) {
			for (x_px = 0; x_px < ibuf->x; x_px++) {
				for (y_px = 0; y_px < ibuf->y; y_px++) {
					blend_color_mix_float(ibuf->rect_float + 4 * (y_px * ibuf->x + x_px),
					                      ibuf->rect_float + 4 * (y_px * ibuf->x + x_px), color_f);
				}
			}
		}
		else {
			for (x_px = 0; x_px < ibuf->x; x_px++) {
				for (y_px = 0; y_px < ibuf->y; y_px++) {
					blend_color_mix_byte((unsigned char *)(ibuf->rect + y_px * ibuf->x + x_px),
					                     (unsigned char *)(ibuf->rect + y_px * ibuf->x + x_px), (unsigned char *)&color_b);
				}
			}
		}
	}
	else {
		/* second case, start sweeping the neighboring pixels, looking for pixels whose
		 * value is within the brush fill threshold from the fill color */
		BLI_Stack *stack;
		BLI_bitmap *touched;
		int coordinate;
		int width = ibuf->x;
		float image_init[2];
		int minx = ibuf->x, miny = ibuf->y, maxx = 0, maxy = 0;
		float pixel_color[4];
		float threshold_sq = br->fill_threshold * br->fill_threshold;

		UI_view2d_region_to_view(s->v2d, mouse_init[0], mouse_init[1], &image_init[0], &image_init[1]);

		x_px = image_init[0] * ibuf->x;
		y_px = image_init[1] * ibuf->y;

		if (x_px >= ibuf->x || x_px < 0 || y_px > ibuf->y || y_px < 0) {
			BKE_image_release_ibuf(ima, ibuf, NULL);
			return;
		}

		/* change image invalidation method later */
		ED_imapaint_dirty_region(ima, ibuf, 0, 0, ibuf->x, ibuf->y);

		stack = BLI_stack_new(sizeof(int), __func__);
		touched = BLI_BITMAP_NEW(ibuf->x * ibuf->y, "bucket_fill_bitmap");

		coordinate = (y_px * ibuf->x + x_px);

		if (do_float) {
			copy_v4_v4(pixel_color, ibuf->rect_float + 4 * coordinate);
		}
		else {
			int pixel_color_b = *(ibuf->rect + coordinate);
			rgba_uchar_to_float(pixel_color, (unsigned char *)&pixel_color_b);
		}

		BLI_stack_push(stack, &coordinate);
		BLI_BITMAP_SET(touched, coordinate, true);

		if (do_float) {
			while (!BLI_stack_is_empty(stack)) {
				BLI_stack_pop(stack, &coordinate);

				IMB_blend_color_float(ibuf->rect_float + 4 * (coordinate),
				                      ibuf->rect_float + 4 * (coordinate),
				                      color_f, br->blend);

				/* reconstruct the coordinates here */
				x_px = coordinate % width;
				y_px = coordinate / width;

				paint_2d_fill_add_pixel_float(x_px - 1, y_px - 1, ibuf, stack, touched, pixel_color, threshold_sq);
				paint_2d_fill_add_pixel_float(x_px - 1, y_px, ibuf, stack, touched, pixel_color, threshold_sq);
				paint_2d_fill_add_pixel_float(x_px - 1, y_px + 1, ibuf, stack, touched, pixel_color, threshold_sq);
				paint_2d_fill_add_pixel_float(x_px, y_px + 1, ibuf, stack, touched, pixel_color, threshold_sq);
				paint_2d_fill_add_pixel_float(x_px, y_px - 1, ibuf, stack, touched, pixel_color, threshold_sq);
				paint_2d_fill_add_pixel_float(x_px + 1, y_px - 1, ibuf, stack, touched, pixel_color, threshold_sq);
				paint_2d_fill_add_pixel_float(x_px + 1, y_px, ibuf, stack, touched, pixel_color, threshold_sq);
				paint_2d_fill_add_pixel_float(x_px + 1, y_px + 1, ibuf, stack, touched, pixel_color, threshold_sq);

				if (x_px > maxx)
					maxx = x_px;
				if (x_px < minx)
					minx = x_px;
				if (y_px > maxy)
					maxy = y_px;
				if (x_px > miny)
					miny = y_px;
			}
		}
		else {
			while (!BLI_stack_is_empty(stack)) {
				BLI_stack_pop(stack, &coordinate);

				IMB_blend_color_byte((unsigned char *)(ibuf->rect + coordinate),
				                     (unsigned char *)(ibuf->rect + coordinate),
				                     (unsigned char *)&color_b, br->blend);

				/* reconstruct the coordinates here */
				x_px = coordinate % width;
				y_px = coordinate / width;

				paint_2d_fill_add_pixel_byte(x_px - 1, y_px - 1, ibuf, stack, touched, pixel_color, threshold_sq);
				paint_2d_fill_add_pixel_byte(x_px - 1, y_px, ibuf, stack, touched, pixel_color, threshold_sq);
				paint_2d_fill_add_pixel_byte(x_px - 1, y_px + 1, ibuf, stack, touched, pixel_color, threshold_sq);
				paint_2d_fill_add_pixel_byte(x_px, y_px + 1, ibuf, stack, touched, pixel_color, threshold_sq);
				paint_2d_fill_add_pixel_byte(x_px, y_px - 1, ibuf, stack, touched, pixel_color, threshold_sq);
				paint_2d_fill_add_pixel_byte(x_px + 1, y_px - 1, ibuf, stack, touched, pixel_color, threshold_sq);
				paint_2d_fill_add_pixel_byte(x_px + 1, y_px, ibuf, stack, touched, pixel_color, threshold_sq);
				paint_2d_fill_add_pixel_byte(x_px + 1, y_px + 1, ibuf, stack, touched, pixel_color, threshold_sq);

				if (x_px > maxx)
					maxx = x_px;
				if (x_px < minx)
					minx = x_px;
				if (y_px > maxy)
					maxy = y_px;
				if (x_px > miny)
					miny = y_px;
			}
		}

		MEM_freeN(touched);
		BLI_stack_free(stack);
	}

	imapaint_image_update(sima, ima, ibuf, false);
	ED_imapaint_clear_partial_redraw();

	BKE_image_release_ibuf(ima, ibuf, NULL);

	WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);
}

void paint_2d_gradient_fill(
        const bContext *C, Brush *br,
        const float mouse_init[2], const float mouse_final[2],
        void *ps)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Image *ima = sima->image;
	ImagePaintState *s = ps;

	ImBuf *ibuf;
	int x_px, y_px;
	unsigned int color_b;
	float color_f[4];
	float image_init[2], image_final[2];
	float tangent[2];
	float line_len_sq_inv, line_len;

	bool do_float;

	if (!ima)
		return;

	ibuf = BKE_image_acquire_ibuf(ima, &sima->iuser, NULL);

	if (!ibuf)
		return;

	UI_view2d_region_to_view(s->v2d, mouse_final[0], mouse_final[1], &image_final[0], &image_final[1]);
	UI_view2d_region_to_view(s->v2d, mouse_init[0], mouse_init[1], &image_init[0], &image_init[1]);

	image_final[0] *= ibuf->x;
	image_final[1] *= ibuf->y;

	image_init[0] *= ibuf->x;
	image_init[1] *= ibuf->y;

	/* some math to get needed gradient variables */
	sub_v2_v2v2(tangent, image_final, image_init);
	line_len = len_squared_v2(tangent);
	line_len_sq_inv = 1.0f / line_len;
	line_len = sqrt(line_len);

	do_float = (ibuf->rect_float != NULL);

	/* this will be substituted by something else when selection is available */
	ED_imapaint_dirty_region(ima, ibuf, 0, 0, ibuf->x, ibuf->y);

	if (do_float) {
		for (x_px = 0; x_px < ibuf->x; x_px++) {
			for (y_px = 0; y_px < ibuf->y; y_px++) {
				float f;
				float p[2] = {x_px - image_init[0], y_px - image_init[1]};

				switch (br->gradient_fill_mode) {
					case BRUSH_GRADIENT_LINEAR:
					{
						f = dot_v2v2(p, tangent) * line_len_sq_inv;
						break;
					}
					case BRUSH_GRADIENT_RADIAL:
					{
						f = len_v2(p) / line_len;
						break;
					}
				}
				do_colorband(br->gradient, f, color_f);
				/* convert to premultiplied */
				mul_v3_fl(color_f, color_f[3]);
				color_f[3] *= br->alpha;
				IMB_blend_color_float(ibuf->rect_float + 4 * (y_px * ibuf->x + x_px),
				                      ibuf->rect_float + 4 * (y_px * ibuf->x + x_px),
				                      color_f, br->blend);
			}
		}
	}
	else {
		for (x_px = 0; x_px < ibuf->x; x_px++) {
			for (y_px = 0; y_px < ibuf->y; y_px++) {
				float f;
				float p[2] = {x_px - image_init[0], y_px - image_init[1]};

				switch (br->gradient_fill_mode) {
					case BRUSH_GRADIENT_LINEAR:
					{
						f = dot_v2v2(p, tangent) * line_len_sq_inv;
						break;
					}
					case BRUSH_GRADIENT_RADIAL:
					{
						f = len_v2(p) / line_len;
						break;
					}
				}

				do_colorband(br->gradient, f, color_f);
				rgba_float_to_uchar((unsigned char *)&color_b, color_f);
				((unsigned char *)&color_b)[3] *= br->alpha;
				IMB_blend_color_byte((unsigned char *)(ibuf->rect + y_px * ibuf->x + x_px),
				                     (unsigned char *)(ibuf->rect + y_px * ibuf->x + x_px),
				                     (unsigned char *)&color_b, br->blend);
			}
		}
	}

	imapaint_image_update(sima, ima, ibuf, false);
	ED_imapaint_clear_partial_redraw();

	BKE_image_release_ibuf(ima, ibuf, NULL);

	WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);
}

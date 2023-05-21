/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup bke
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BLI_bitmap.h"
#include "BLI_listbase.h"
#include "BLI_math_color_blend.h"
#include "BLI_stack.h"
#include "BLI_task.h"

#include "BKE_brush.h"
#include "BKE_colorband.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"

#include "ED_paint.h"
#include "ED_screen.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "paint_intern.hh"

/* Brush Painting for 2D image editor */

/* Defines and Structs */

struct BrushPainterCache {
  bool use_float;            /* need float imbuf? */
  bool use_color_correction; /* use color correction for float */
  bool invert;

  bool is_texbrush;
  bool is_maskbrush;

  int lastdiameter;
  float last_tex_rotation;
  float last_mask_rotation;
  float last_pressure;

  ImBuf *ibuf;
  ImBuf *texibuf;
  ushort *tex_mask;
  ushort *tex_mask_old;
  uint tex_mask_old_w;
  uint tex_mask_old_h;

  CurveMaskCache curve_mask_cache;

  int image_size[2];
};

struct BrushPainter {
  Scene *scene;
  Brush *brush;

  bool firsttouch; /* first paint op */

  ImagePool *pool;   /* image pool */
  rctf tex_mapping;  /* texture coordinate mapping */
  rctf mask_mapping; /* mask texture coordinate mapping */

  bool cache_invert;
};

struct ImagePaintRegion {
  int destx, desty;
  int srcx, srcy;
  int width, height;
};

enum ImagePaintTileState {
  PAINT2D_TILE_UNINITIALIZED = 0,
  PAINT2D_TILE_MISSING,
  PAINT2D_TILE_READY,
};

struct ImagePaintTile {
  ImageUser iuser;
  ImBuf *canvas;
  float radius_fac;
  int size[2];
  float uv_origin[2]; /* Stores the position of this tile in UV space. */
  bool need_redraw;
  BrushPainterCache cache;

  ImagePaintTileState state;

  float last_paintpos[2];  /* position of last paint op */
  float start_paintpos[2]; /* position of first paint */
};

struct ImagePaintState {
  BrushPainter *painter;
  SpaceImage *sima;
  View2D *v2d;
  Scene *scene;

  Brush *brush;
  short tool, blend;
  Image *image;
  ImBuf *clonecanvas;

  bool do_masking;

  int symmetry;

  ImagePaintTile *tiles;
  int num_tiles;

  BlurKernel *blurkernel;
};

static BrushPainter *brush_painter_2d_new(Scene *scene, Brush *brush, bool invert)
{
  BrushPainter *painter = MEM_cnew<BrushPainter>(__func__);

  painter->brush = brush;
  painter->scene = scene;
  painter->firsttouch = true;
  painter->cache_invert = invert;

  return painter;
}

static void brush_painter_2d_require_imbuf(
    Brush *brush, ImagePaintTile *tile, bool use_float, bool use_color_correction, bool invert)
{
  BrushPainterCache *cache = &tile->cache;

  if (cache->use_float != use_float) {
    if (cache->ibuf) {
      IMB_freeImBuf(cache->ibuf);
    }
    if (cache->tex_mask) {
      MEM_freeN(cache->tex_mask);
    }
    if (cache->tex_mask_old) {
      MEM_freeN(cache->tex_mask_old);
    }
    cache->ibuf = nullptr;
    cache->tex_mask = nullptr;
    cache->lastdiameter = -1; /* force ibuf create in refresh */
    cache->invert = invert;
  }

  cache->use_float = use_float;
  cache->use_color_correction = use_float && use_color_correction;
  cache->is_texbrush = (brush->mtex.tex && brush->imagepaint_tool == PAINT_TOOL_DRAW) ? true :
                                                                                        false;
  cache->is_maskbrush = (brush->mask_mtex.tex) ? true : false;
}

static void brush_painter_cache_2d_free(BrushPainterCache *cache)
{
  if (cache->ibuf) {
    IMB_freeImBuf(cache->ibuf);
  }
  if (cache->texibuf) {
    IMB_freeImBuf(cache->texibuf);
  }
  paint_curve_mask_cache_free_data(&cache->curve_mask_cache);
  if (cache->tex_mask) {
    MEM_freeN(cache->tex_mask);
  }
  if (cache->tex_mask_old) {
    MEM_freeN(cache->tex_mask_old);
  }
}

static void brush_imbuf_tex_co(rctf *mapping, int x, int y, float texco[3])
{
  texco[0] = mapping->xmin + x * mapping->xmax;
  texco[1] = mapping->ymin + y * mapping->ymax;
  texco[2] = 0.0f;
}

/* create a mask with the mask texture */
static ushort *brush_painter_mask_ibuf_new(BrushPainter *painter, const int size)
{
  Scene *scene = painter->scene;
  Brush *brush = painter->brush;
  rctf mask_mapping = painter->mask_mapping;
  ImagePool *pool = painter->pool;

  float texco[3];
  ushort *mask, *m;
  int x, y, thread = 0;

  mask = static_cast<ushort *>(MEM_mallocN(sizeof(ushort) * size * size, __func__));
  m = mask;

  for (y = 0; y < size; y++) {
    for (x = 0; x < size; x++, m++) {
      float res;
      brush_imbuf_tex_co(&mask_mapping, x, y, texco);
      res = BKE_brush_sample_masktex(scene, brush, texco, thread, pool);
      *m = ushort(65535.0f * res);
    }
  }

  return mask;
}

/* update rectangular section of the brush image */
static void brush_painter_mask_imbuf_update(BrushPainter *painter,
                                            ImagePaintTile *tile,
                                            const ushort *tex_mask_old,
                                            int origx,
                                            int origy,
                                            int w,
                                            int h,
                                            int xt,
                                            int yt,
                                            const int diameter)
{
  Scene *scene = painter->scene;
  Brush *brush = painter->brush;
  BrushPainterCache *cache = &tile->cache;
  rctf tex_mapping = painter->mask_mapping;
  ImagePool *pool = painter->pool;
  ushort res;

  bool use_texture_old = (tex_mask_old != nullptr);

  int x, y, thread = 0;

  ushort *tex_mask = cache->tex_mask;
  ushort *tex_mask_cur = cache->tex_mask_old;

  /* fill pixels */
  for (y = origy; y < h; y++) {
    for (x = origx; x < w; x++) {
      /* sample texture */
      float texco[3];

      /* handle byte pixel */
      ushort *b = tex_mask + (y * diameter + x);
      ushort *t = tex_mask_cur + (y * diameter + x);

      if (!use_texture_old) {
        brush_imbuf_tex_co(&tex_mapping, x, y, texco);
        res = ushort(65535.0f * BKE_brush_sample_masktex(scene, brush, texco, thread, pool));
      }

      /* read from old texture buffer */
      if (use_texture_old) {
        res = *(tex_mask_old + ((y - origy + yt) * cache->tex_mask_old_w + (x - origx + xt)));
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
static void brush_painter_mask_imbuf_partial_update(BrushPainter *painter,
                                                    ImagePaintTile *tile,
                                                    const float pos[2],
                                                    const int diameter)
{
  BrushPainterCache *cache = &tile->cache;
  ushort *tex_mask_old;
  int destx, desty, srcx, srcy, w, h, x1, y1, x2, y2;

  /* create brush image buffer if it didn't exist yet */
  if (!cache->tex_mask) {
    cache->tex_mask = static_cast<ushort *>(
        MEM_mallocN(sizeof(ushort) * diameter * diameter, __func__));
  }

  /* create new texture image buffer with coordinates relative to old */
  tex_mask_old = cache->tex_mask_old;
  cache->tex_mask_old = static_cast<ushort *>(
      MEM_mallocN(sizeof(ushort) * diameter * diameter, __func__));

  if (tex_mask_old) {
    ImBuf maskibuf;
    ImBuf maskibuf_old;
    maskibuf.x = diameter;
    maskibuf.y = diameter;
    maskibuf_old.x = cache->tex_mask_old_w;
    maskibuf_old.y = cache->tex_mask_old_h;

    srcx = srcy = 0;
    w = cache->tex_mask_old_w;
    h = cache->tex_mask_old_h;
    destx = int(floorf(tile->last_paintpos[0])) - int(floorf(pos[0])) + (diameter / 2 - w / 2);
    desty = int(floorf(tile->last_paintpos[1])) - int(floorf(pos[1])) + (diameter / 2 - h / 2);

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
  if ((x1 < x2) && (y1 < y2)) {
    brush_painter_mask_imbuf_update(
        painter, tile, tex_mask_old, x1, y1, x2, y2, srcx, srcy, diameter);
  }

  if (tex_mask_old) {
    MEM_freeN(tex_mask_old);
  }

  /* sample texture in new areas */
  if ((0 < x1) && (0 < diameter)) {
    brush_painter_mask_imbuf_update(painter, tile, nullptr, 0, 0, x1, diameter, 0, 0, diameter);
  }
  if ((x2 < diameter) && (0 < diameter)) {
    brush_painter_mask_imbuf_update(
        painter, tile, nullptr, x2, 0, diameter, diameter, 0, 0, diameter);
  }
  if ((x1 < x2) && (0 < y1)) {
    brush_painter_mask_imbuf_update(painter, tile, nullptr, x1, 0, x2, y1, 0, 0, diameter);
  }
  if ((x1 < x2) && (y2 < diameter)) {
    brush_painter_mask_imbuf_update(painter, tile, nullptr, x1, y2, x2, diameter, 0, 0, diameter);
  }

  /* through with sampling, now update sizes */
  cache->tex_mask_old_w = diameter;
  cache->tex_mask_old_h = diameter;
}

/* create imbuf with brush color */
static ImBuf *brush_painter_imbuf_new(
    BrushPainter *painter, ImagePaintTile *tile, const int size, float pressure, float distance)
{
  Scene *scene = painter->scene;
  Brush *brush = painter->brush;
  BrushPainterCache *cache = &tile->cache;

  const char *display_device = scene->display_settings.display_device;
  ColorManagedDisplay *display = IMB_colormanagement_display_get_named(display_device);

  rctf tex_mapping = painter->tex_mapping;
  ImagePool *pool = painter->pool;

  bool use_color_correction = cache->use_color_correction;
  bool use_float = cache->use_float;
  bool is_texbrush = cache->is_texbrush;

  int x, y, thread = 0;
  float brush_rgb[3];

  /* allocate image buffer */
  ImBuf *ibuf = IMB_allocImBuf(size, size, 32, (use_float) ? IB_rectfloat : IB_rect);

  /* get brush color */
  if (brush->imagepaint_tool == PAINT_TOOL_DRAW) {
    paint_brush_color_get(
        scene, brush, use_color_correction, cache->invert, distance, pressure, brush_rgb, display);
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
        const MTex *mtex = &brush->mtex;
        BKE_brush_sample_tex_3d(scene, brush, mtex, texco, rgba, thread, pool);
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
        float *dstf = ibuf->float_buffer.data + (y * size + x) * 4;
        mul_v3_v3fl(dstf, rgba, rgba[3]); /* premultiply */
        dstf[3] = rgba[3];
      }
      else {
        /* write to byte pixel */
        uchar *dst = ibuf->byte_buffer.data + (y * size + x) * 4;

        rgb_float_to_uchar(dst, rgba);
        dst[3] = unit_float_to_uchar_clamp(rgba[3]);
      }
    }
  }

  return ibuf;
}

/* update rectangular section of the brush image */
static void brush_painter_imbuf_update(BrushPainter *painter,
                                       ImagePaintTile *tile,
                                       ImBuf *oldtexibuf,
                                       int origx,
                                       int origy,
                                       int w,
                                       int h,
                                       int xt,
                                       int yt)
{
  Scene *scene = painter->scene;
  Brush *brush = painter->brush;
  const MTex *mtex = &brush->mtex;
  BrushPainterCache *cache = &tile->cache;

  const char *display_device = scene->display_settings.display_device;
  ColorManagedDisplay *display = IMB_colormanagement_display_get_named(display_device);

  rctf tex_mapping = painter->tex_mapping;
  ImagePool *pool = painter->pool;

  bool use_color_correction = cache->use_color_correction;
  bool use_float = cache->use_float;
  bool is_texbrush = cache->is_texbrush;
  bool use_texture_old = (oldtexibuf != nullptr);

  int x, y, thread = 0;
  float brush_rgb[3];

  ImBuf *ibuf = cache->ibuf;
  ImBuf *texibuf = cache->texibuf;

  /* get brush color */
  if (brush->imagepaint_tool == PAINT_TOOL_DRAW) {
    paint_brush_color_get(
        scene, brush, use_color_correction, cache->invert, 0.0f, 1.0f, brush_rgb, display);
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
          BKE_brush_sample_tex_3d(scene, brush, mtex, texco, rgba, thread, pool);
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
        float *bf = ibuf->float_buffer.data + (y * ibuf->x + x) * 4;
        float *tf = texibuf->float_buffer.data + (y * texibuf->x + x) * 4;

        /* read from old texture buffer */
        if (use_texture_old) {
          const float *otf = oldtexibuf->float_buffer.data +
                             ((y - origy + yt) * oldtexibuf->x + (x - origx + xt)) * 4;
          copy_v4_v4(rgba, otf);
        }

        /* write to new texture buffer */
        copy_v4_v4(tf, rgba);

        /* output premultiplied float image, mf was already premultiplied */
        mul_v3_v3fl(bf, rgba, rgba[3]);
        bf[3] = rgba[3];
      }
      else {
        uchar crgba[4];

        /* handle byte pixel */
        uchar *b = ibuf->byte_buffer.data + (y * ibuf->x + x) * 4;
        uchar *t = texibuf->byte_buffer.data + (y * texibuf->x + x) * 4;

        /* read from old texture buffer */
        if (use_texture_old) {
          uchar *ot = oldtexibuf->byte_buffer.data +
                      ((y - origy + yt) * oldtexibuf->x + (x - origx + xt)) * 4;
          crgba[0] = ot[0];
          crgba[1] = ot[1];
          crgba[2] = ot[2];
          crgba[3] = ot[3];
        }
        else {
          rgba_float_to_uchar(crgba, rgba);
        }

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
static void brush_painter_imbuf_partial_update(BrushPainter *painter,
                                               ImagePaintTile *tile,
                                               const float pos[2],
                                               const int diameter)
{
  BrushPainterCache *cache = &tile->cache;
  ImBuf *oldtexibuf, *ibuf;
  int imbflag, destx, desty, srcx, srcy, w, h, x1, y1, x2, y2;

  /* create brush image buffer if it didn't exist yet */
  imbflag = (cache->use_float) ? IB_rectfloat : IB_rect;
  if (!cache->ibuf) {
    cache->ibuf = IMB_allocImBuf(diameter, diameter, 32, imbflag);
  }
  ibuf = cache->ibuf;

  /* create new texture image buffer with coordinates relative to old */
  oldtexibuf = cache->texibuf;
  cache->texibuf = IMB_allocImBuf(diameter, diameter, 32, imbflag);

  if (oldtexibuf) {
    srcx = srcy = 0;
    w = oldtexibuf->x;
    h = oldtexibuf->y;
    destx = int(floorf(tile->last_paintpos[0])) - int(floorf(pos[0])) + (diameter / 2 - w / 2);
    desty = int(floorf(tile->last_paintpos[1])) - int(floorf(pos[1])) + (diameter / 2 - h / 2);

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
  if ((x1 < x2) && (y1 < y2)) {
    brush_painter_imbuf_update(painter, tile, oldtexibuf, x1, y1, x2, y2, srcx, srcy);
  }

  if (oldtexibuf) {
    IMB_freeImBuf(oldtexibuf);
  }

  /* sample texture in new areas */
  if ((0 < x1) && (0 < ibuf->y)) {
    brush_painter_imbuf_update(painter, tile, nullptr, 0, 0, x1, ibuf->y, 0, 0);
  }
  if ((x2 < ibuf->x) && (0 < ibuf->y)) {
    brush_painter_imbuf_update(painter, tile, nullptr, x2, 0, ibuf->x, ibuf->y, 0, 0);
  }
  if ((x1 < x2) && (0 < y1)) {
    brush_painter_imbuf_update(painter, tile, nullptr, x1, 0, x2, y1, 0, 0);
  }
  if ((x1 < x2) && (y2 < ibuf->y)) {
    brush_painter_imbuf_update(painter, tile, nullptr, x1, y2, x2, ibuf->y, 0, 0);
  }
}

static void brush_painter_2d_tex_mapping(ImagePaintState *s,
                                         ImagePaintTile *tile,
                                         const int diameter,
                                         const float pos[2],
                                         const float mouse[2],
                                         int mapmode,
                                         rctf *mapping)
{
  float invw = 1.0f / float(tile->canvas->x);
  float invh = 1.0f / float(tile->canvas->y);
  float start[2];

  /* find start coordinate of brush in canvas */
  start[0] = pos[0] - diameter / 2.0f;
  start[1] = pos[1] - diameter / 2.0f;

  if (mapmode == MTEX_MAP_MODE_STENCIL) {
    /* map from view coordinates of brush to region coordinates */
    float xmin, ymin, xmax, ymax;
    UI_view2d_view_to_region_fl(s->v2d, start[0] * invw, start[1] * invh, &xmin, &ymin);
    UI_view2d_view_to_region_fl(
        s->v2d, (start[0] + diameter) * invw, (start[1] + diameter) * invh, &xmax, &ymax);

    /* output mapping from brush ibuf x/y to region coordinates */
    mapping->xmax = (xmax - xmin) / float(diameter);
    mapping->ymax = (ymax - ymin) / float(diameter);
    mapping->xmin = xmin + (tile->uv_origin[0] * tile->size[0] * mapping->xmax);
    mapping->ymin = ymin + (tile->uv_origin[1] * tile->size[1] * mapping->ymax);
  }
  else if (mapmode == MTEX_MAP_MODE_3D) {
    /* 3D mapping, just mapping to canvas 0..1. */
    mapping->xmin = 2.0f * (start[0] * invw - 0.5f);
    mapping->ymin = 2.0f * (start[1] * invh - 0.5f);
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
    mapping->xmin = int(-diameter * 0.5) + int(floorf(pos[0])) -
                    int(floorf(tile->start_paintpos[0]));
    mapping->ymin = int(-diameter * 0.5) + int(floorf(pos[1])) -
                    int(floorf(tile->start_paintpos[1]));
    mapping->xmax = 1.0f;
    mapping->ymax = 1.0f;
  }
}

static void brush_painter_2d_refresh_cache(ImagePaintState *s,
                                           BrushPainter *painter,
                                           ImagePaintTile *tile,
                                           const float pos[2],
                                           const float mouse[2],
                                           float pressure,
                                           float distance,
                                           float size)
{
  const Scene *scene = painter->scene;
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  Brush *brush = painter->brush;
  BrushPainterCache *cache = &tile->cache;
  /* Adding 4 pixels of padding for brush antialiasing */
  const int diameter = MAX2(1, size * 2) + 4;

  bool do_random = false;
  bool do_partial_update = false;
  bool update_color = ((brush->flag & BRUSH_USE_GRADIENT) && (ELEM(brush->gradient_stroke_mode,
                                                                   BRUSH_GRADIENT_SPACING_REPEAT,
                                                                   BRUSH_GRADIENT_SPACING_CLAMP) ||
                                                              (cache->last_pressure != pressure)));
  float tex_rotation = -brush->mtex.rot;
  float mask_rotation = -brush->mask_mtex.rot;

  painter->pool = BKE_image_pool_new();

  /* determine how can update based on textures used */
  if (cache->is_texbrush) {
    if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_VIEW) {
      tex_rotation += ups->brush_rotation;
    }
    else if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_RANDOM) {
      do_random = true;
    }
    else if (!((brush->flag & BRUSH_ANCHORED) || update_color)) {
      do_partial_update = true;
    }

    brush_painter_2d_tex_mapping(
        s, tile, diameter, pos, mouse, brush->mtex.brush_map_mode, &painter->tex_mapping);
  }

  if (cache->is_maskbrush) {
    bool renew_maxmask = false;
    bool do_partial_update_mask = false;
    /* invalidate case for all mapping modes */
    if (brush->mask_mtex.brush_map_mode == MTEX_MAP_MODE_VIEW) {
      mask_rotation += ups->brush_rotation_sec;
    }
    else if (brush->mask_mtex.brush_map_mode == MTEX_MAP_MODE_RANDOM) {
      renew_maxmask = true;
    }
    else if (!(brush->flag & BRUSH_ANCHORED)) {
      do_partial_update_mask = true;
      renew_maxmask = true;
    }
    /* explicitly disable partial update even if it has been enabled above */
    if (brush->mask_pressure) {
      do_partial_update_mask = false;
      renew_maxmask = true;
    }

    if (diameter != cache->lastdiameter || (mask_rotation != cache->last_mask_rotation) ||
        renew_maxmask)
    {
      MEM_SAFE_FREE(cache->tex_mask);

      brush_painter_2d_tex_mapping(
          s, tile, diameter, pos, mouse, brush->mask_mtex.brush_map_mode, &painter->mask_mapping);

      if (do_partial_update_mask) {
        brush_painter_mask_imbuf_partial_update(painter, tile, pos, diameter);
      }
      else {
        cache->tex_mask = brush_painter_mask_ibuf_new(painter, diameter);
      }
      cache->last_mask_rotation = mask_rotation;
    }
  }

  /* Re-initialize the curve mask. Mask is always recreated due to the change of position. */
  paint_curve_mask_cache_update(&cache->curve_mask_cache, brush, diameter, size, pos);

  /* detect if we need to recreate image brush buffer */
  if (diameter != cache->lastdiameter || (tex_rotation != cache->last_tex_rotation) || do_random ||
      update_color)
  {
    if (cache->ibuf) {
      IMB_freeImBuf(cache->ibuf);
      cache->ibuf = nullptr;
    }

    if (do_partial_update) {
      /* do partial update of texture */
      brush_painter_imbuf_partial_update(painter, tile, pos, diameter);
    }
    else {
      /* create brush from scratch */
      cache->ibuf = brush_painter_imbuf_new(painter, tile, diameter, pressure, distance);
    }

    cache->lastdiameter = diameter;
    cache->last_tex_rotation = tex_rotation;
    cache->last_pressure = pressure;
  }
  else if (do_partial_update) {
    /* do only partial update of texture */
    int dx = int(floorf(tile->last_paintpos[0])) - int(floorf(pos[0]));
    int dy = int(floorf(tile->last_paintpos[1])) - int(floorf(pos[1]));

    if ((dx != 0) || (dy != 0)) {
      brush_painter_imbuf_partial_update(painter, tile, pos, diameter);
    }
  }

  BKE_image_pool_free(painter->pool);
  painter->pool = nullptr;
}

static bool paint_2d_ensure_tile_canvas(ImagePaintState *s, int i)
{
  if (i == 0) {
    return true;
  }
  if (i >= s->num_tiles) {
    return false;
  }

  if (s->tiles[i].state == PAINT2D_TILE_READY) {
    return true;
  }
  if (s->tiles[i].state == PAINT2D_TILE_MISSING) {
    return false;
  }

  s->tiles[i].cache.lastdiameter = -1;

  ImBuf *ibuf = BKE_image_acquire_ibuf(s->image, &s->tiles[i].iuser, nullptr);
  if (ibuf != nullptr) {
    if (ibuf->channels != 4) {
      s->tiles[i].state = PAINT2D_TILE_MISSING;
    }
    else if ((s->tiles[0].canvas->byte_buffer.data && !ibuf->byte_buffer.data) ||
             (s->tiles[0].canvas->float_buffer.data && !ibuf->float_buffer.data))
    {
      s->tiles[i].state = PAINT2D_TILE_MISSING;
    }
    else {
      s->tiles[i].size[0] = ibuf->x;
      s->tiles[i].size[1] = ibuf->y;
      s->tiles[i].radius_fac = sqrtf((float(ibuf->x) * float(ibuf->y)) /
                                     (s->tiles[0].size[0] * s->tiles[0].size[1]));
      s->tiles[i].state = PAINT2D_TILE_READY;
    }
  }
  else {
    s->tiles[i].state = PAINT2D_TILE_MISSING;
  }

  if (s->tiles[i].state == PAINT2D_TILE_MISSING) {
    BKE_image_release_ibuf(s->image, ibuf, nullptr);
    return false;
  }

  s->tiles[i].canvas = ibuf;
  return true;
}

/* keep these functions in sync */
static void paint_2d_ibuf_rgb_get(ImBuf *ibuf, int x, int y, float r_rgb[4])
{
  if (ibuf->float_buffer.data) {
    const float *rrgbf = ibuf->float_buffer.data + (ibuf->x * y + x) * 4;
    copy_v4_v4(r_rgb, rrgbf);
  }
  else {
    uchar *rrgb = ibuf->byte_buffer.data + (ibuf->x * y + x) * 4;
    straight_uchar_to_premul_float(r_rgb, rrgb);
  }
}
static void paint_2d_ibuf_rgb_set(
    ImBuf *ibuf, int x, int y, const bool is_torus, const float rgb[4])
{
  if (is_torus) {
    x %= ibuf->x;
    if (x < 0) {
      x += ibuf->x;
    }
    y %= ibuf->y;
    if (y < 0) {
      y += ibuf->y;
    }
  }

  if (ibuf->float_buffer.data) {
    float *rrgbf = ibuf->float_buffer.data + (ibuf->x * y + x) * 4;
    float map_alpha = (rgb[3] == 0.0f) ? rrgbf[3] : rrgbf[3] / rgb[3];

    mul_v3_v3fl(rrgbf, rgb, map_alpha);
    rrgbf[3] = rgb[3];
  }
  else {
    uchar straight[4];
    uchar *rrgb = ibuf->byte_buffer.data + (ibuf->x * y + x) * 4;

    premul_float_to_straight_uchar(straight, rgb);
    rrgb[0] = straight[0];
    rrgb[1] = straight[1];
    rrgb[2] = straight[2];
    rrgb[3] = straight[3];
  }
}

static void paint_2d_ibuf_tile_convert(ImBuf *ibuf, int *x, int *y, short paint_tile)
{
  if (paint_tile & PAINT_TILE_X) {
    *x %= ibuf->x;
    if (*x < 0) {
      *x += ibuf->x;
    }
  }
  if (paint_tile & PAINT_TILE_Y) {
    *y %= ibuf->y;
    if (*y < 0) {
      *y += ibuf->y;
    }
  }
}

static float paint_2d_ibuf_add_if(
    ImBuf *ibuf, int x, int y, float *outrgb, short paint_tile, float w)
{
  float inrgb[4];

  if (paint_tile) {
    paint_2d_ibuf_tile_convert(ibuf, &x, &y, paint_tile);
  }
  /* need to also do clipping here always since tiled coordinates
   * are not always within bounds */
  if (x < ibuf->x && x >= 0 && y < ibuf->y && y >= 0) {
    paint_2d_ibuf_rgb_get(ibuf, x, y, inrgb);
  }
  else {
    return 0.0f;
  }

  mul_v4_fl(inrgb, w);
  add_v4_v4(outrgb, inrgb);

  return w;
}

static void paint_2d_lift_soften(ImagePaintState *s,
                                 ImagePaintTile *tile,
                                 ImBuf *ibuf,
                                 ImBuf *ibufb,
                                 const int *pos,
                                 const short paint_tile)
{
  bool sharpen = (tile->cache.invert ^ ((s->brush->flag & BRUSH_DIR_IN) != 0));
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

  if (!paint_tile) {
    IMB_rectclip(ibuf, ibufb, &in_off[0], &in_off[1], &out_off[0], &out_off[1], &dim[0], &dim[1]);

    if ((dim[0] == 0) || (dim[1] == 0)) {
      return;
    }
  }

  /* find offset inside mask buffers to sample them */
  sub_v2_v2v2_int(diff_pos, out_off, in_off);

  for (y = 0; y < dim[1]; y++) {
    for (x = 0; x < dim[0]; x++) {
      /* get input pixel */
      xi = in_off[0] + x;
      yi = in_off[1] + y;

      count = 0.0;
      if (paint_tile) {
        paint_2d_ibuf_tile_convert(ibuf, &xi, &yi, paint_tile);
        if (xi < ibuf->x && xi >= 0 && yi < ibuf->y && yi >= 0) {
          paint_2d_ibuf_rgb_get(ibuf, xi, yi, rgba);
        }
        else {
          zero_v4(rgba);
        }
      }
      else {
        /* coordinates have been clipped properly here, it should be safe to do this */
        paint_2d_ibuf_rgb_get(ibuf, xi, yi, rgba);
      }
      zero_v4(outrgb);

      for (yk = 0; yk < kernel->side; yk++) {
        for (xk = 0; xk < kernel->side; xk++) {
          count += paint_2d_ibuf_add_if(ibuf,
                                        xi + xk - kernel->pixel_len,
                                        yi + yk - kernel->pixel_len,
                                        outrgb,
                                        paint_tile,
                                        kernel->wdata[xk + yk * kernel->side]);
        }
      }

      if (count > 0.0f) {
        mul_v4_fl(outrgb, 1.0f / float(count));

        if (sharpen) {
          /* subtract blurred image from normal image gives high pass filter */
          sub_v3_v3v3(outrgb, rgba, outrgb);

          /* Now rgba_ub contains the edge result, but this should be converted to luminance to
           * avoid colored speckles appearing in final image, and also to check for threshold. */
          outrgb[0] = outrgb[1] = outrgb[2] = IMB_colormanagement_get_luminance(outrgb);
          if (fabsf(outrgb[0]) > threshold) {
            float mask = BKE_brush_alpha_get(s->scene, s->brush);
            float alpha = rgba[3];
            rgba[3] = outrgb[3] = mask;

            /* add to enhance edges */
            blend_color_add_float(outrgb, rgba, outrgb);
            outrgb[3] = alpha;
          }
          else {
            copy_v4_v4(outrgb, rgba);
          }
        }
      }
      else {
        copy_v4_v4(outrgb, rgba);
      }
      /* write into brush buffer */
      xo = out_off[0] + x;
      yo = out_off[1] + y;
      paint_2d_ibuf_rgb_set(ibufb, xo, yo, 0, outrgb);
    }
  }
}

static void paint_2d_set_region(
    ImagePaintRegion *region, int destx, int desty, int srcx, int srcy, int width, int height)
{
  region->destx = destx;
  region->desty = desty;
  region->srcx = srcx;
  region->srcy = srcy;
  region->width = width;
  region->height = height;
}

static int paint_2d_torus_split_region(ImagePaintRegion region[4],
                                       ImBuf *dbuf,
                                       ImBuf *sbuf,
                                       short paint_tile)
{
  int destx = region->destx;
  int desty = region->desty;
  int srcx = region->srcx;
  int srcy = region->srcy;
  int width = region->width;
  int height = region->height;
  int origw, origh, w, h, tot = 0;

  /* convert destination and source coordinates to be within image */
  if (paint_tile & PAINT_TILE_X) {
    destx = destx % dbuf->x;
    if (destx < 0) {
      destx += dbuf->x;
    }
    srcx = srcx % sbuf->x;
    if (srcx < 0) {
      srcx += sbuf->x;
    }
  }
  if (paint_tile & PAINT_TILE_Y) {
    desty = desty % dbuf->y;
    if (desty < 0) {
      desty += dbuf->y;
    }
    srcy = srcy % sbuf->y;
    if (srcy < 0) {
      srcy += sbuf->y;
    }
  }
  /* clip width of blending area to destination imbuf, to avoid writing the
   * same pixel twice */
  origw = w = (width > dbuf->x) ? dbuf->x : width;
  origh = h = (height > dbuf->y) ? dbuf->y : height;

  /* clip within image */
  IMB_rectclip(dbuf, sbuf, &destx, &desty, &srcx, &srcy, &w, &h);
  paint_2d_set_region(&region[tot++], destx, desty, srcx, srcy, w, h);

  /* do 3 other rects if needed */
  if ((paint_tile & PAINT_TILE_X) && w < origw) {
    paint_2d_set_region(
        &region[tot++], (destx + w) % dbuf->x, desty, (srcx + w) % sbuf->x, srcy, origw - w, h);
  }
  if ((paint_tile & PAINT_TILE_Y) && h < origh) {
    paint_2d_set_region(
        &region[tot++], destx, (desty + h) % dbuf->y, srcx, (srcy + h) % sbuf->y, w, origh - h);
  }
  if ((paint_tile & PAINT_TILE_X) && (paint_tile & PAINT_TILE_Y) && (w < origw) && (h < origh)) {
    paint_2d_set_region(&region[tot++],
                        (destx + w) % dbuf->x,
                        (desty + h) % dbuf->y,
                        (srcx + w) % sbuf->x,
                        (srcy + h) % sbuf->y,
                        origw - w,
                        origh - h);
  }

  return tot;
}

static void paint_2d_lift_smear(ImBuf *ibuf, ImBuf *ibufb, int *pos, short paint_tile)
{
  ImagePaintRegion region[4];
  int a, tot;

  paint_2d_set_region(region, 0, 0, pos[0], pos[1], ibufb->x, ibufb->y);
  tot = paint_2d_torus_split_region(region, ibufb, ibuf, paint_tile);

  for (a = 0; a < tot; a++) {
    IMB_rectblend(ibufb,
                  ibufb,
                  ibuf,
                  nullptr,
                  nullptr,
                  nullptr,
                  0,
                  region[a].destx,
                  region[a].desty,
                  region[a].destx,
                  region[a].desty,
                  region[a].srcx,
                  region[a].srcy,
                  region[a].width,
                  region[a].height,
                  IMB_BLEND_COPY,
                  false);
  }
}

static ImBuf *paint_2d_lift_clone(ImBuf *ibuf, ImBuf *ibufb, const int *pos)
{
  /* NOTE: allocImbuf returns zero'd memory, so regions outside image will
   * have zero alpha, and hence not be blended onto the image */
  int w = ibufb->x, h = ibufb->y, destx = 0, desty = 0, srcx = pos[0], srcy = pos[1];
  ImBuf *clonebuf = IMB_allocImBuf(w, h, ibufb->planes, ibufb->flags);

  IMB_rectclip(clonebuf, ibuf, &destx, &desty, &srcx, &srcy, &w, &h);
  IMB_rectblend(clonebuf,
                clonebuf,
                ibufb,
                nullptr,
                nullptr,
                nullptr,
                0,
                destx,
                desty,
                destx,
                desty,
                destx,
                desty,
                w,
                h,
                IMB_BLEND_COPY_ALPHA,
                false);
  IMB_rectblend(clonebuf,
                clonebuf,
                ibuf,
                nullptr,
                nullptr,
                nullptr,
                0,
                destx,
                desty,
                destx,
                desty,
                srcx,
                srcy,
                w,
                h,
                IMB_BLEND_COPY_RGB,
                false);

  return clonebuf;
}

static void paint_2d_convert_brushco(ImBuf *ibufb, const float pos[2], int ipos[2])
{
  ipos[0] = int(floorf(pos[0] - ibufb->x / 2));
  ipos[1] = int(floorf(pos[1] - ibufb->y / 2));
}

static void paint_2d_do_making_brush(ImagePaintState *s,
                                     ImagePaintTile *tile,
                                     ImagePaintRegion *region,
                                     ImBuf *frombuf,
                                     float mask_max,
                                     short blend,
                                     int tilex,
                                     int tiley,
                                     int tilew,
                                     int tileh)
{
  ImBuf tmpbuf;
  IMB_initImBuf(&tmpbuf, ED_IMAGE_UNDO_TILE_SIZE, ED_IMAGE_UNDO_TILE_SIZE, 32, 0);

  PaintTileMap *undo_tiles = ED_image_paint_tile_map_get();

  for (int ty = tiley; ty <= tileh; ty++) {
    for (int tx = tilex; tx <= tilew; tx++) {
      /* retrieve original pixels + mask from undo buffer */
      ushort *mask;
      int origx = region->destx - tx * ED_IMAGE_UNDO_TILE_SIZE;
      int origy = region->desty - ty * ED_IMAGE_UNDO_TILE_SIZE;

      if (tile->canvas->float_buffer.data) {
        IMB_assign_float_buffer(
            &tmpbuf,
            static_cast<float *>(ED_image_paint_tile_find(
                undo_tiles, s->image, tile->canvas, &tile->iuser, tx, ty, &mask, false)),
            IB_DO_NOT_TAKE_OWNERSHIP);
      }
      else {
        IMB_assign_byte_buffer(
            &tmpbuf,
            static_cast<uchar *>(ED_image_paint_tile_find(
                undo_tiles, s->image, tile->canvas, &tile->iuser, tx, ty, &mask, false)),
            IB_DO_NOT_TAKE_OWNERSHIP);
      }

      IMB_rectblend(tile->canvas,
                    &tmpbuf,
                    frombuf,
                    mask,
                    tile->cache.curve_mask_cache.curve_mask,
                    tile->cache.tex_mask,
                    mask_max,
                    region->destx,
                    region->desty,
                    origx,
                    origy,
                    region->srcx,
                    region->srcy,
                    region->width,
                    region->height,
                    IMB_BlendMode(blend),
                    ((s->brush->flag & BRUSH_ACCUMULATE) != 0));
    }
  }
}

struct Paint2DForeachData {
  ImagePaintState *s;
  ImagePaintTile *tile;
  ImagePaintRegion *region;
  ImBuf *frombuf;
  float mask_max;
  short blend;
  int tilex;
  int tilew;
};

static void paint_2d_op_foreach_do(void *__restrict data_v,
                                   const int iter,
                                   const TaskParallelTLS *__restrict /*tls*/)
{
  Paint2DForeachData *data = (Paint2DForeachData *)data_v;
  paint_2d_do_making_brush(data->s,
                           data->tile,
                           data->region,
                           data->frombuf,
                           data->mask_max,
                           data->blend,
                           data->tilex,
                           iter,
                           data->tilew,
                           iter);
}

static int paint_2d_op(void *state,
                       ImagePaintTile *tile,
                       const float lastpos[2],
                       const float pos[2])
{
  ImagePaintState *s = ((ImagePaintState *)state);
  ImBuf *clonebuf = nullptr, *frombuf;
  ImBuf *canvas = tile->canvas;
  ImBuf *ibufb = tile->cache.ibuf;
  ImagePaintRegion region[4];
  short paint_tile = s->symmetry & (PAINT_TILE_X | PAINT_TILE_Y);
  short blend = s->blend;
  const float *offset = s->brush->clone.offset;
  float liftpos[2];
  float mask_max = BKE_brush_alpha_get(s->scene, s->brush);
  int bpos[2], blastpos[2], bliftpos[2];
  int a, tot;

  paint_2d_convert_brushco(ibufb, pos, bpos);

  /* lift from canvas */
  if (s->tool == PAINT_TOOL_SOFTEN) {
    paint_2d_lift_soften(s, tile, canvas, ibufb, bpos, paint_tile);
    blend = IMB_BLEND_INTERPOLATE;
  }
  else if (s->tool == PAINT_TOOL_SMEAR) {
    if (lastpos[0] == pos[0] && lastpos[1] == pos[1]) {
      return 0;
    }

    paint_2d_convert_brushco(ibufb, lastpos, blastpos);
    paint_2d_lift_smear(canvas, ibufb, blastpos, paint_tile);
    blend = IMB_BLEND_INTERPOLATE;
  }
  else if (s->tool == PAINT_TOOL_CLONE && s->clonecanvas) {
    liftpos[0] = pos[0] - offset[0] * canvas->x;
    liftpos[1] = pos[1] - offset[1] * canvas->y;

    paint_2d_convert_brushco(ibufb, liftpos, bliftpos);
    clonebuf = paint_2d_lift_clone(s->clonecanvas, ibufb, bliftpos);
  }

  frombuf = (clonebuf) ? clonebuf : ibufb;

  if (paint_tile) {
    paint_2d_set_region(region, bpos[0], bpos[1], 0, 0, frombuf->x, frombuf->y);
    tot = paint_2d_torus_split_region(region, canvas, frombuf, paint_tile);
  }
  else {
    paint_2d_set_region(region, bpos[0], bpos[1], 0, 0, frombuf->x, frombuf->y);
    tot = 1;
  }

  /* blend into canvas */
  for (a = 0; a < tot; a++) {
    ED_imapaint_dirty_region(s->image,
                             canvas,
                             &tile->iuser,
                             region[a].destx,
                             region[a].desty,
                             region[a].width,
                             region[a].height,
                             true);

    if (s->do_masking) {
      /* masking, find original pixels tiles from undo buffer to composite over */
      int tilex, tiley, tilew, tileh;

      imapaint_region_tiles(canvas,
                            region[a].destx,
                            region[a].desty,
                            region[a].width,
                            region[a].height,
                            &tilex,
                            &tiley,
                            &tilew,
                            &tileh);

      if (tiley == tileh) {
        paint_2d_do_making_brush(
            s, tile, &region[a], frombuf, mask_max, blend, tilex, tiley, tilew, tileh);
      }
      else {
        Paint2DForeachData data;
        data.s = s;
        data.tile = tile;
        data.region = &region[a];
        data.frombuf = frombuf;
        data.mask_max = mask_max;
        data.blend = blend;
        data.tilex = tilex;
        data.tilew = tilew;

        TaskParallelSettings settings;
        BLI_parallel_range_settings_defaults(&settings);
        BLI_task_parallel_range(tiley, tileh + 1, &data, paint_2d_op_foreach_do, &settings);
      }
    }
    else {
      /* no masking, composite brush directly onto canvas */
      IMB_rectblend_threaded(canvas,
                             canvas,
                             frombuf,
                             nullptr,
                             tile->cache.curve_mask_cache.curve_mask,
                             tile->cache.tex_mask,
                             mask_max,
                             region[a].destx,
                             region[a].desty,
                             region[a].destx,
                             region[a].desty,
                             region[a].srcx,
                             region[a].srcy,
                             region[a].width,
                             region[a].height,
                             IMB_BlendMode(blend),
                             false);
    }
  }

  if (clonebuf) {
    IMB_freeImBuf(clonebuf);
  }

  return 1;
}

static int paint_2d_canvas_set(ImagePaintState *s)
{
  /* set clone canvas */
  if (s->tool == PAINT_TOOL_CLONE) {
    Image *ima = s->brush->clone.image;
    ImBuf *ibuf = BKE_image_acquire_ibuf(ima, nullptr, nullptr);

    if (!ima || !ibuf || !(ibuf->byte_buffer.data || ibuf->float_buffer.data)) {
      BKE_image_release_ibuf(ima, ibuf, nullptr);
      return 0;
    }

    s->clonecanvas = ibuf;

    /* temporarily add float rect for cloning */
    if (s->tiles[0].canvas->float_buffer.data && !s->clonecanvas->float_buffer.data) {
      IMB_float_from_rect(s->clonecanvas);
    }
    else if (!s->tiles[0].canvas->float_buffer.data && !s->clonecanvas->byte_buffer.data) {
      IMB_rect_from_float(s->clonecanvas);
    }
  }

  /* set masking */
  s->do_masking = paint_use_opacity_masking(s->brush);

  return 1;
}

static void paint_2d_canvas_free(ImagePaintState *s)
{
  for (int i = 0; i < s->num_tiles; i++) {
    BKE_image_release_ibuf(s->image, s->tiles[i].canvas, nullptr);
  }
  BKE_image_release_ibuf(s->brush->clone.image, s->clonecanvas, nullptr);

  if (s->blurkernel) {
    paint_delete_blur_kernel(s->blurkernel);
    MEM_freeN(s->blurkernel);
  }
}

static void paint_2d_transform_mouse(View2D *v2d, const float in[2], float out[2])
{
  UI_view2d_region_to_view(v2d, in[0], in[1], &out[0], &out[1]);
}

static bool is_inside_tile(const int size[2], const float pos[2], const float brush[2])
{
  return (pos[0] >= -brush[0]) && (pos[0] < size[0] + brush[0]) && (pos[1] >= -brush[1]) &&
         (pos[1] < size[1] + brush[1]);
}

static void paint_2d_uv_to_coord(ImagePaintTile *tile, const float uv[2], float coord[2])
{
  coord[0] = (uv[0] - tile->uv_origin[0]) * tile->size[0];
  coord[1] = (uv[1] - tile->uv_origin[1]) * tile->size[1];
}

void paint_2d_stroke(void *ps,
                     const float prev_mval[2],
                     const float mval[2],
                     const bool eraser,
                     float pressure,
                     float distance,
                     float base_size)
{
  float new_uv[2], old_uv[2];
  ImagePaintState *s = static_cast<ImagePaintState *>(ps);
  BrushPainter *painter = s->painter;

  const bool is_data = s->tiles[0].canvas->colormanage_flag & IMB_COLORMANAGE_IS_DATA;

  s->blend = s->brush->blend;
  if (eraser) {
    s->blend = IMB_BLEND_ERASE_ALPHA;
  }

  UI_view2d_region_to_view(s->v2d, mval[0], mval[1], &new_uv[0], &new_uv[1]);
  UI_view2d_region_to_view(s->v2d, prev_mval[0], prev_mval[1], &old_uv[0], &old_uv[1]);

  float last_uv[2], start_uv[2];
  UI_view2d_region_to_view(s->v2d, 0.0f, 0.0f, &start_uv[0], &start_uv[1]);
  if (painter->firsttouch) {
    /* paint exactly once on first touch */
    copy_v2_v2(last_uv, new_uv);
  }
  else {
    copy_v2_v2(last_uv, old_uv);
  }

  const float uv_brush_size[2] = {
      (s->symmetry & PAINT_TILE_X) ? FLT_MAX : base_size / s->tiles[0].size[0],
      (s->symmetry & PAINT_TILE_Y) ? FLT_MAX : base_size / s->tiles[0].size[1]};

  for (int i = 0; i < s->num_tiles; i++) {
    ImagePaintTile *tile = &s->tiles[i];

    /* First test: Project brush into UV space, clip against tile. */
    const int uv_size[2] = {1, 1};
    float local_new_uv[2], local_old_uv[2];
    sub_v2_v2v2(local_new_uv, new_uv, tile->uv_origin);
    sub_v2_v2v2(local_old_uv, old_uv, tile->uv_origin);
    if (!(is_inside_tile(uv_size, local_new_uv, uv_brush_size) ||
          is_inside_tile(uv_size, local_old_uv, uv_brush_size)))
    {
      continue;
    }

    /* Lazy tile loading to get size in pixels. */
    if (!paint_2d_ensure_tile_canvas(s, i)) {
      continue;
    }

    float size = base_size * tile->radius_fac;

    float new_coord[2], old_coord[2];
    paint_2d_uv_to_coord(tile, new_uv, new_coord);
    paint_2d_uv_to_coord(tile, old_uv, old_coord);
    if (painter->firsttouch) {
      paint_2d_uv_to_coord(tile, start_uv, tile->start_paintpos);
    }
    paint_2d_uv_to_coord(tile, last_uv, tile->last_paintpos);

    /* Second check in pixel coordinates. */
    const float pixel_brush_size[] = {(s->symmetry & PAINT_TILE_X) ? FLT_MAX : size,
                                      (s->symmetry & PAINT_TILE_Y) ? FLT_MAX : size};
    if (!(is_inside_tile(tile->size, new_coord, pixel_brush_size) ||
          is_inside_tile(tile->size, old_coord, pixel_brush_size)))
    {
      continue;
    }

    ImBuf *ibuf = tile->canvas;

    /* OCIO_TODO: float buffers are now always linear, so always use color correction
     *            this should probably be changed when texture painting color space is supported
     */
    brush_painter_2d_require_imbuf(painter->brush,
                                   tile,
                                   (ibuf->float_buffer.data != nullptr),
                                   !is_data,
                                   painter->cache_invert);

    brush_painter_2d_refresh_cache(s, painter, tile, new_coord, mval, pressure, distance, size);

    if (paint_2d_op(s, tile, old_coord, new_coord)) {
      tile->need_redraw = true;
    }
  }

  painter->firsttouch = false;
}

void *paint_2d_new_stroke(bContext *C, wmOperator *op, int mode)
{
  Scene *scene = CTX_data_scene(C);
  SpaceImage *sima = CTX_wm_space_image(C);
  ToolSettings *settings = scene->toolsettings;
  Brush *brush = BKE_paint_brush(&settings->imapaint.paint);

  ImagePaintState *s = MEM_cnew<ImagePaintState>(__func__);

  s->sima = CTX_wm_space_image(C);
  s->v2d = &CTX_wm_region(C)->v2d;
  s->scene = scene;

  s->brush = brush;
  s->tool = brush->imagepaint_tool;
  s->blend = brush->blend;

  s->image = s->sima->image;
  s->symmetry = settings->imapaint.paint.symmetry_flags;

  if (s->image == nullptr) {
    MEM_freeN(s);
    return nullptr;
  }
  if (BKE_image_has_packedfile(s->image) && s->image->rr != nullptr) {
    BKE_report(op->reports, RPT_WARNING, "Packed MultiLayer files cannot be painted");
    MEM_freeN(s);
    return nullptr;
  }

  s->num_tiles = BLI_listbase_count(&s->image->tiles);
  s->tiles = MEM_cnew_array<ImagePaintTile>(s->num_tiles, __func__);
  for (int i = 0; i < s->num_tiles; i++) {
    s->tiles[i].iuser = sima->iuser;
  }

  zero_v2(s->tiles[0].uv_origin);

  ImBuf *ibuf = BKE_image_acquire_ibuf(s->image, &s->tiles[0].iuser, nullptr);
  if (ibuf == nullptr) {
    MEM_freeN(s->tiles);
    MEM_freeN(s);
    return nullptr;
  }

  if (ibuf->channels != 4) {
    BKE_image_release_ibuf(s->image, ibuf, nullptr);
    BKE_report(op->reports, RPT_WARNING, "Image requires 4 color channels to paint");
    MEM_freeN(s->tiles);
    MEM_freeN(s);
    return nullptr;
  }

  s->tiles[0].size[0] = ibuf->x;
  s->tiles[0].size[1] = ibuf->y;
  s->tiles[0].radius_fac = 1.0f;

  s->tiles[0].canvas = ibuf;
  s->tiles[0].state = PAINT2D_TILE_READY;

  /* Initialize offsets here, they're needed for the uv space clip test before lazy-loading the
   * tile properly. */
  int tile_idx = 0;
  for (ImageTile *tile = static_cast<ImageTile *>(s->image->tiles.first); tile;
       tile = tile->next, tile_idx++)
  {
    s->tiles[tile_idx].iuser.tile = tile->tile_number;
    s->tiles[tile_idx].uv_origin[0] = ((tile->tile_number - 1001) % 10);
    s->tiles[tile_idx].uv_origin[1] = ((tile->tile_number - 1001) / 10);
  }

  if (!paint_2d_canvas_set(s)) {
    MEM_freeN(s->tiles);

    MEM_freeN(s);
    return nullptr;
  }

  if (brush->imagepaint_tool == PAINT_TOOL_SOFTEN) {
    s->blurkernel = paint_new_blur_kernel(brush, false);
  }

  paint_brush_init_tex(s->brush);

  /* create painter */
  s->painter = brush_painter_2d_new(scene, s->brush, mode == BRUSH_STROKE_INVERT);

  return s;
}

void paint_2d_redraw(const bContext *C, void *ps, bool final)
{
  ImagePaintState *s = static_cast<ImagePaintState *>(ps);

  bool had_redraw = false;
  for (int i = 0; i < s->num_tiles; i++) {
    if (s->tiles[i].need_redraw) {
      ImBuf *ibuf = BKE_image_acquire_ibuf(s->image, &s->tiles[i].iuser, nullptr);

      imapaint_image_update(s->sima, s->image, ibuf, &s->tiles[i].iuser, false);

      BKE_image_release_ibuf(s->image, ibuf, nullptr);

      s->tiles[i].need_redraw = false;
      had_redraw = true;
    }
  }

  if (had_redraw) {
    ED_imapaint_clear_partial_redraw();
    if (s->sima == nullptr || !s->sima->lock) {
      ED_region_tag_redraw(CTX_wm_region(C));
    }
    else {
      WM_event_add_notifier(C, NC_IMAGE | NA_PAINTING, s->image);
    }
  }

  if (final) {
    if (s->image && !(s->sima && s->sima->lock)) {
      BKE_image_free_gputextures(s->image);
    }

    /* compositor listener deals with updating */
    WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, s->image);
    DEG_id_tag_update(&s->image->id, 0);
  }
}

void paint_2d_stroke_done(void *ps)
{
  ImagePaintState *s = static_cast<ImagePaintState *>(ps);

  paint_2d_canvas_free(s);
  for (int i = 0; i < s->num_tiles; i++) {
    brush_painter_cache_2d_free(&s->tiles[i].cache);
  }
  MEM_freeN(s->painter);
  MEM_freeN(s->tiles);
  paint_brush_exit_tex(s->brush);

  MEM_freeN(s);
}

static void paint_2d_fill_add_pixel_byte(const int x_px,
                                         const int y_px,
                                         ImBuf *ibuf,
                                         BLI_Stack *stack,
                                         BLI_bitmap *touched,
                                         const float color[4],
                                         float threshold_sq)
{
  size_t coordinate;

  if (x_px >= ibuf->x || x_px < 0 || y_px >= ibuf->y || y_px < 0) {
    return;
  }

  coordinate = size_t(y_px) * ibuf->x + x_px;

  if (!BLI_BITMAP_TEST(touched, coordinate)) {
    float color_f[4];
    uchar *color_b = ibuf->byte_buffer.data + 4 * coordinate;
    rgba_uchar_to_float(color_f, color_b);
    straight_to_premul_v4(color_f);

    if (len_squared_v4v4(color_f, color) <= threshold_sq) {
      BLI_stack_push(stack, &coordinate);
    }
    BLI_BITMAP_SET(touched, coordinate, true);
  }
}

static void paint_2d_fill_add_pixel_float(const int x_px,
                                          const int y_px,
                                          ImBuf *ibuf,
                                          BLI_Stack *stack,
                                          BLI_bitmap *touched,
                                          const float color[4],
                                          float threshold_sq)
{
  size_t coordinate;

  if (x_px >= ibuf->x || x_px < 0 || y_px >= ibuf->y || y_px < 0) {
    return;
  }

  coordinate = size_t(y_px) * ibuf->x + x_px;

  if (!BLI_BITMAP_TEST(touched, coordinate)) {
    if (len_squared_v4v4(ibuf->float_buffer.data + 4 * coordinate, color) <= threshold_sq) {
      BLI_stack_push(stack, &coordinate);
    }
    BLI_BITMAP_SET(touched, coordinate, true);
  }
}

static ImageUser *paint_2d_get_tile_iuser(ImagePaintState *s, int tile_number)
{
  ImageUser *iuser = &s->tiles[0].iuser;
  for (int i = 0; i < s->num_tiles; i++) {
    if (s->tiles[i].iuser.tile == tile_number) {
      if (!paint_2d_ensure_tile_canvas(s, i)) {
        return nullptr;
      }
      iuser = &s->tiles[i].iuser;
      break;
    }
  }

  return iuser;
}

void paint_2d_bucket_fill(const bContext *C,
                          const float color[3],
                          Brush *br,
                          const float mouse_init[2],
                          const float mouse_final[2],
                          void *ps)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Image *ima = sima->image;

  ImagePaintState *s = static_cast<ImagePaintState *>(ps);

  ImBuf *ibuf;
  int x_px, y_px;
  uint color_b;
  float color_f[4];
  float strength = (s && br) ? BKE_brush_alpha_get(s->scene, br) : 1.0f;

  bool do_float;

  if (!ima) {
    return;
  }

  View2D *v2d = s ? s->v2d : &CTX_wm_region(C)->v2d;
  float uv_origin[2];
  float image_init[2];
  paint_2d_transform_mouse(v2d, mouse_init, image_init);

  int tile_number = BKE_image_get_tile_from_pos(ima, image_init, image_init, uv_origin);

  ImageUser local_iuser, *iuser;
  if (s != nullptr) {
    iuser = paint_2d_get_tile_iuser(s, tile_number);
    if (iuser == nullptr) {
      return;
    }
  }
  else {
    iuser = &local_iuser;
    BKE_imageuser_default(iuser);
    iuser->tile = tile_number;
  }

  ibuf = BKE_image_acquire_ibuf(ima, iuser, nullptr);
  if (!ibuf) {
    return;
  }

  do_float = (ibuf->float_buffer.data != nullptr);
  /* first check if our image is float. If it is not we should correct the color to
   * be in gamma space. strictly speaking this is not correct, but blender does not paint
   * byte images in linear space */
  if (!do_float) {
    linearrgb_to_srgb_uchar3((uchar *)&color_b, color);
    *(((char *)&color_b) + 3) = strength * 255;
  }
  else {
    copy_v3_v3(color_f, color);
    color_f[3] = strength;
  }

  if (!mouse_final || !br) {
    /* first case, no image UV, fill the whole image */
    ED_imapaint_dirty_region(ima, ibuf, iuser, 0, 0, ibuf->x, ibuf->y, false);

    if (do_float) {
      for (x_px = 0; x_px < ibuf->x; x_px++) {
        for (y_px = 0; y_px < ibuf->y; y_px++) {
          blend_color_mix_float(ibuf->float_buffer.data + 4 * (size_t(y_px) * ibuf->x + x_px),
                                ibuf->float_buffer.data + 4 * (size_t(y_px) * ibuf->x + x_px),
                                color_f);
        }
      }
    }
    else {
      for (x_px = 0; x_px < ibuf->x; x_px++) {
        for (y_px = 0; y_px < ibuf->y; y_px++) {
          blend_color_mix_byte(ibuf->byte_buffer.data + 4 * (size_t(y_px) * ibuf->x + x_px),
                               ibuf->byte_buffer.data + 4 * (size_t(y_px) * ibuf->x + x_px),
                               (uchar *)&color_b);
        }
      }
    }
  }
  else {
    /* second case, start sweeping the neighboring pixels, looking for pixels whose
     * value is within the brush fill threshold from the fill color */
    BLI_Stack *stack;
    BLI_bitmap *touched;
    size_t coordinate;
    int width = ibuf->x;
    int minx = ibuf->x, miny = ibuf->y, maxx = 0, maxy = 0;
    float pixel_color[4];
    /* We are comparing to sum of three squared values
     * (assumed in range [0,1]), so need to multiply... */
    float threshold_sq = br->fill_threshold * br->fill_threshold * 3;

    x_px = image_init[0] * ibuf->x;
    y_px = image_init[1] * ibuf->y;

    if (x_px >= ibuf->x || x_px < 0 || y_px > ibuf->y || y_px < 0) {
      BKE_image_release_ibuf(ima, ibuf, nullptr);
      return;
    }

    /* change image invalidation method later */
    ED_imapaint_dirty_region(ima, ibuf, iuser, 0, 0, ibuf->x, ibuf->y, false);

    stack = BLI_stack_new(sizeof(size_t), __func__);
    touched = BLI_BITMAP_NEW(size_t(ibuf->x) * ibuf->y, "bucket_fill_bitmap");

    coordinate = (size_t(y_px) * ibuf->x + x_px);

    if (do_float) {
      copy_v4_v4(pixel_color, ibuf->float_buffer.data + 4 * coordinate);
    }
    else {
      int pixel_color_b = *ibuf->byte_buffer.data + 4 * coordinate;
      rgba_uchar_to_float(pixel_color, (uchar *)&pixel_color_b);
      straight_to_premul_v4(pixel_color);
    }

    BLI_stack_push(stack, &coordinate);
    BLI_BITMAP_SET(touched, coordinate, true);

    if (do_float) {
      while (!BLI_stack_is_empty(stack)) {
        BLI_stack_pop(stack, &coordinate);

        IMB_blend_color_float(ibuf->float_buffer.data + 4 * (coordinate),
                              ibuf->float_buffer.data + 4 * (coordinate),
                              color_f,
                              IMB_BlendMode(br->blend));

        /* reconstruct the coordinates here */
        x_px = coordinate % width;
        y_px = coordinate / width;

        paint_2d_fill_add_pixel_float(
            x_px - 1, y_px - 1, ibuf, stack, touched, pixel_color, threshold_sq);
        paint_2d_fill_add_pixel_float(
            x_px - 1, y_px, ibuf, stack, touched, pixel_color, threshold_sq);
        paint_2d_fill_add_pixel_float(
            x_px - 1, y_px + 1, ibuf, stack, touched, pixel_color, threshold_sq);
        paint_2d_fill_add_pixel_float(
            x_px, y_px + 1, ibuf, stack, touched, pixel_color, threshold_sq);
        paint_2d_fill_add_pixel_float(
            x_px, y_px - 1, ibuf, stack, touched, pixel_color, threshold_sq);
        paint_2d_fill_add_pixel_float(
            x_px + 1, y_px - 1, ibuf, stack, touched, pixel_color, threshold_sq);
        paint_2d_fill_add_pixel_float(
            x_px + 1, y_px, ibuf, stack, touched, pixel_color, threshold_sq);
        paint_2d_fill_add_pixel_float(
            x_px + 1, y_px + 1, ibuf, stack, touched, pixel_color, threshold_sq);

        if (x_px > maxx) {
          maxx = x_px;
        }
        if (x_px < minx) {
          minx = x_px;
        }
        if (y_px > maxy) {
          maxy = y_px;
        }
        if (x_px > miny) {
          miny = y_px;
        }
      }
    }
    else {
      while (!BLI_stack_is_empty(stack)) {
        BLI_stack_pop(stack, &coordinate);

        IMB_blend_color_byte(ibuf->byte_buffer.data + 4 * coordinate,
                             ibuf->byte_buffer.data + 4 * coordinate,
                             (uchar *)&color_b,
                             IMB_BlendMode(br->blend));

        /* reconstruct the coordinates here */
        x_px = coordinate % width;
        y_px = coordinate / width;

        paint_2d_fill_add_pixel_byte(
            x_px - 1, y_px - 1, ibuf, stack, touched, pixel_color, threshold_sq);
        paint_2d_fill_add_pixel_byte(
            x_px - 1, y_px, ibuf, stack, touched, pixel_color, threshold_sq);
        paint_2d_fill_add_pixel_byte(
            x_px - 1, y_px + 1, ibuf, stack, touched, pixel_color, threshold_sq);
        paint_2d_fill_add_pixel_byte(
            x_px, y_px + 1, ibuf, stack, touched, pixel_color, threshold_sq);
        paint_2d_fill_add_pixel_byte(
            x_px, y_px - 1, ibuf, stack, touched, pixel_color, threshold_sq);
        paint_2d_fill_add_pixel_byte(
            x_px + 1, y_px - 1, ibuf, stack, touched, pixel_color, threshold_sq);
        paint_2d_fill_add_pixel_byte(
            x_px + 1, y_px, ibuf, stack, touched, pixel_color, threshold_sq);
        paint_2d_fill_add_pixel_byte(
            x_px + 1, y_px + 1, ibuf, stack, touched, pixel_color, threshold_sq);

        if (x_px > maxx) {
          maxx = x_px;
        }
        if (x_px < minx) {
          minx = x_px;
        }
        if (y_px > maxy) {
          maxy = y_px;
        }
        if (x_px > miny) {
          miny = y_px;
        }
      }
    }

    MEM_freeN(touched);
    BLI_stack_free(stack);
  }

  imapaint_image_update(sima, ima, ibuf, iuser, false);
  ED_imapaint_clear_partial_redraw();

  BKE_image_release_ibuf(ima, ibuf, nullptr);

  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);
}

void paint_2d_gradient_fill(
    const bContext *C, Brush *br, const float mouse_init[2], const float mouse_final[2], void *ps)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Image *ima = sima->image;
  ImagePaintState *s = static_cast<ImagePaintState *>(ps);

  ImBuf *ibuf;
  int x_px, y_px;
  uint color_b;
  float color_f[4];
  float image_init[2], image_final[2];
  float tangent[2];
  float line_len_sq_inv, line_len;
  const float brush_alpha = BKE_brush_alpha_get(s->scene, br);

  bool do_float;

  if (ima == nullptr) {
    return;
  }

  float uv_origin[2];
  int tile_number = BKE_image_get_tile_from_pos(ima, image_init, image_init, uv_origin);
  ImageUser *iuser = paint_2d_get_tile_iuser(s, tile_number);
  if (!iuser) {
    return;
  }

  ibuf = BKE_image_acquire_ibuf(ima, iuser, nullptr);
  if (ibuf == nullptr) {
    return;
  }

  paint_2d_transform_mouse(s->v2d, mouse_final, image_final);
  paint_2d_transform_mouse(s->v2d, mouse_init, image_init);
  sub_v2_v2(image_init, uv_origin);
  sub_v2_v2(image_final, uv_origin);

  image_final[0] *= ibuf->x;
  image_final[1] *= ibuf->y;

  image_init[0] *= ibuf->x;
  image_init[1] *= ibuf->y;

  /* some math to get needed gradient variables */
  sub_v2_v2v2(tangent, image_final, image_init);
  line_len = len_squared_v2(tangent);
  line_len_sq_inv = 1.0f / line_len;
  line_len = sqrtf(line_len);

  do_float = (ibuf->float_buffer.data != nullptr);

  /* this will be substituted by something else when selection is available */
  ED_imapaint_dirty_region(ima, ibuf, iuser, 0, 0, ibuf->x, ibuf->y, false);

  if (do_float) {
    for (x_px = 0; x_px < ibuf->x; x_px++) {
      for (y_px = 0; y_px < ibuf->y; y_px++) {
        float f;
        const float p[2] = {x_px - image_init[0], y_px - image_init[1]};

        switch (br->gradient_fill_mode) {
          case BRUSH_GRADIENT_LINEAR: {
            f = dot_v2v2(p, tangent) * line_len_sq_inv;
            break;
          }
          case BRUSH_GRADIENT_RADIAL:
          default: {
            f = len_v2(p) / line_len;
            break;
          }
        }
        BKE_colorband_evaluate(br->gradient, f, color_f);
        /* convert to premultiplied */
        mul_v3_fl(color_f, color_f[3]);
        color_f[3] *= brush_alpha;
        IMB_blend_color_float(ibuf->float_buffer.data + 4 * (size_t(y_px) * ibuf->x + x_px),
                              ibuf->float_buffer.data + 4 * (size_t(y_px) * ibuf->x + x_px),
                              color_f,
                              IMB_BlendMode(br->blend));
      }
    }
  }
  else {
    for (x_px = 0; x_px < ibuf->x; x_px++) {
      for (y_px = 0; y_px < ibuf->y; y_px++) {
        float f;
        const float p[2] = {x_px - image_init[0], y_px - image_init[1]};

        switch (br->gradient_fill_mode) {
          case BRUSH_GRADIENT_LINEAR: {
            f = dot_v2v2(p, tangent) * line_len_sq_inv;
            break;
          }
          case BRUSH_GRADIENT_RADIAL:
          default: {
            f = len_v2(p) / line_len;
            break;
          }
        }

        BKE_colorband_evaluate(br->gradient, f, color_f);
        linearrgb_to_srgb_v3_v3(color_f, color_f);
        rgba_float_to_uchar((uchar *)&color_b, color_f);
        ((uchar *)&color_b)[3] *= brush_alpha;
        IMB_blend_color_byte(ibuf->byte_buffer.data + 4 * (size_t(y_px) * ibuf->x + x_px),
                             ibuf->byte_buffer.data + 4 * (size_t(y_px) * ibuf->x + x_px),
                             (uchar *)&color_b,
                             IMB_BlendMode(br->blend));
      }
    }
  }

  imapaint_image_update(sima, ima, ibuf, iuser, false);
  ED_imapaint_clear_partial_redraw();

  BKE_image_release_ibuf(ima, ibuf, nullptr);

  WM_event_add_notifier(C, NC_IMAGE | NA_EDITED, ima);
}

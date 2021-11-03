/*
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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup blf
 *
 * Glyph rendering, texturing and caching. Wraps Freetype and OpenGL functions.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_OUTLINE_H
#include FT_BITMAP_H

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_threads.h"

#include "BLF_api.h"

#include "GPU_capabilities.h"
#include "GPU_immediate.h"

#include "blf_internal.h"
#include "blf_internal_types.h"

#include "BLI_math_vector.h"
#include "BLI_strict_flags.h"

GlyphCacheBLF *blf_glyph_cache_find(FontBLF *font, unsigned int size, unsigned int dpi)
{
  GlyphCacheBLF *p;

  p = (GlyphCacheBLF *)font->cache.first;
  while (p) {
    if (p->size == size && p->dpi == dpi && (p->bold == ((font->flags & BLF_BOLD) != 0)) &&
        (p->italic == ((font->flags & BLF_ITALIC) != 0))) {
      return p;
    }
    p = p->next;
  }
  return NULL;
}

/* Create a new glyph cache for the current size, dpi, bold, italic. */
GlyphCacheBLF *blf_glyph_cache_new(FontBLF *font)
{
  GlyphCacheBLF *gc;

  gc = (GlyphCacheBLF *)MEM_callocN(sizeof(GlyphCacheBLF), "blf_glyph_cache_new");
  gc->next = NULL;
  gc->prev = NULL;
  gc->size = font->size;
  gc->dpi = font->dpi;
  gc->bold = ((font->flags & BLF_BOLD) != 0);
  gc->italic = ((font->flags & BLF_ITALIC) != 0);

  memset(gc->glyph_ascii_table, 0, sizeof(gc->glyph_ascii_table));
  memset(gc->bucket, 0, sizeof(gc->bucket));

  BLI_addhead(&font->cache, gc);
  return gc;
}

GlyphCacheBLF *blf_glyph_cache_acquire(FontBLF *font)
{
  BLI_spin_lock(font->glyph_cache_mutex);

  GlyphCacheBLF *gc = blf_glyph_cache_find(font, font->size, font->dpi);

  if (!gc) {
    gc = blf_glyph_cache_new(font);
  }

  return gc;
}

void blf_glyph_cache_release(FontBLF *font)
{
  BLI_spin_unlock(font->glyph_cache_mutex);
}

void blf_glyph_cache_clear(FontBLF *font)
{
  GlyphCacheBLF *gc;

  BLI_spin_lock(font->glyph_cache_mutex);

  while ((gc = BLI_pophead(&font->cache))) {
    blf_glyph_cache_free(gc);
  }

  BLI_spin_unlock(font->glyph_cache_mutex);
}

void blf_glyph_cache_free(GlyphCacheBLF *gc)
{
  GlyphBLF *g;
  for (uint i = 0; i < ARRAY_SIZE(gc->bucket); i++) {
    while ((g = BLI_pophead(&gc->bucket[i]))) {
      blf_glyph_free(g);
    }
  }
  if (gc->texture) {
    GPU_texture_free(gc->texture);
  }
  if (gc->bitmap_result) {
    MEM_freeN(gc->bitmap_result);
  }
  MEM_freeN(gc);
}

static GlyphBLF *blf_glyph_search(GlyphCacheBLF *gc, unsigned int c)
{
  GlyphBLF *p;
  unsigned int key;

  key = blf_hash(c);
  p = gc->bucket[key].first;
  while (p) {
    if (p->c == c) {
      return p;
    }
    p = p->next;
  }
  return NULL;
}

static bool blf_glyph_render(FontBLF *font, FT_UInt glyph_index)
{
  int load_flags;
  int render_mode;

  if (font->flags & BLF_MONOCHROME) {
    load_flags = FT_LOAD_TARGET_MONO;
    render_mode = FT_RENDER_MODE_MONO;
  }
  else {
    load_flags = FT_LOAD_NO_BITMAP;
    render_mode = FT_RENDER_MODE_NORMAL;
    if (font->flags & BLF_HINTING_NONE) {
      load_flags |= FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_HINTING;
    }
    else if (font->flags & BLF_HINTING_SLIGHT) {
      load_flags |= FT_LOAD_TARGET_LIGHT;
    }
    else if (font->flags & BLF_HINTING_FULL) {
      load_flags |= FT_LOAD_TARGET_NORMAL;
    }
    else {
      /* Default, hinting disabled until FreeType has been upgraded
       * to give good results on all platforms. */
      load_flags |= FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_HINTING;
    }
  }

  FT_Error err = FT_Load_Glyph(font->face, glyph_index, load_flags);
  if (err != 0) {
    return false;
  }

  /* Do not oblique a font that is designed to be italic! */
  if (((font->flags & BLF_ITALIC) != 0) && !(font->face->style_flags & FT_STYLE_FLAG_ITALIC) &&
      (font->face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)) {
    /* For (fake) italic: a shear transform with a 6 degree angle. */
    FT_Matrix transform;
    transform.xx = 0x10000L;
    transform.yx = 0x00000L;
    transform.xy = 0x03000L;
    transform.yy = 0x10000L;
    FT_Outline_Transform(&font->face->glyph->outline, &transform);
  }

  /* Do not embolden an already bold font! */
  if (((font->flags & BLF_BOLD) != 0) && !(font->face->style_flags & FT_STYLE_FLAG_BOLD) &&
      (font->face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)) {
    /* Strengthen the width more than the height. */
    const FT_Pos extra_x = FT_MulFix(font->face->units_per_EM, font->face->size->metrics.x_scale) /
                           14;
    const FT_Pos extra_y = FT_MulFix(font->face->units_per_EM, font->face->size->metrics.y_scale) /
                           28;
    FT_Outline_EmboldenXY(&font->face->glyph->outline, extra_x, extra_y);
    if ((font->face->face_flags & FT_FACE_FLAG_FIXED_WIDTH) == 0) {
      /* Need to increase advance, but not for fixed-width fonts. */
      font->face->glyph->advance.x += (FT_Pos)(((float)extra_x) * 1.05f);
      font->face->glyph->advance.y += extra_y;
    }
    else {
      /* Widened fixed-pitch font gets a nudge left. */
      FT_Outline_Translate(&font->face->glyph->outline, (extra_x / -2), 0);
    }
  }

  /* get the glyph. */
  FT_GlyphSlot slot = font->face->glyph;
  err = FT_Render_Glyph(slot, render_mode);

  FT_Bitmap tempbitmap;

  if (font->flags & BLF_MONOCHROME) {
    /* Convert result from 1 bit per pixel to 8 bit per pixel */
    /* Accum errors for later, fine if not interested beyond "ok vs any error" */
    FT_Bitmap_New(&tempbitmap);

    /* Does Blender use Pitch 1 always? It works so far */
    err += FT_Bitmap_Convert(font->ft_lib, &slot->bitmap, &tempbitmap, 1);
    err += FT_Bitmap_Copy(font->ft_lib, &tempbitmap, &slot->bitmap);
    err += FT_Bitmap_Done(font->ft_lib, &tempbitmap);
  }

  if (err || slot->format != FT_GLYPH_FORMAT_BITMAP) {
    return false;
  }

  return true;
}

GlyphBLF *blf_glyph_ensure(FontBLF *font, GlyphCacheBLF *gc, uint charcode)
{
  GlyphBLF *g = (charcode < GLYPH_ASCII_TABLE_SIZE) ? (gc->glyph_ascii_table)[charcode] :
                                                      blf_glyph_search(gc, charcode);
  if (g) {
    return g;
  }

  FT_UInt glyph_index = FT_Get_Char_Index(font->face, charcode);

  if (!blf_glyph_render(font, glyph_index)) {
    return NULL;
  }

  FT_GlyphSlot slot = font->face->glyph;

  /* glyphs are dynamically created as needed by font rendering. this means that
   * to make font rendering thread safe we have to do locking here. note that this
   * must be a lock for the whole library and not just per font, because the font
   * renderer uses a shared buffer internally */
  BLI_spin_lock(font->ft_lib_mutex);

  g = (GlyphBLF *)MEM_callocN(sizeof(GlyphBLF), "blf_glyph_get");
  g->c = charcode;
  g->idx = glyph_index;
  g->advance = ((float)slot->advance.x) / 64.0f;
  g->advance_i = (int)g->advance;
  g->pos[0] = slot->bitmap_left;
  g->pos[1] = slot->bitmap_top;
  g->dims[0] = (int)slot->bitmap.width;
  g->dims[1] = (int)slot->bitmap.rows;
  g->pitch = slot->bitmap.pitch;

  FT_BBox bbox;
  FT_Outline_Get_CBox(&(slot->outline), &bbox);
  g->box.xmin = ((float)bbox.xMin) / 64.0f;
  g->box.xmax = ((float)bbox.xMax) / 64.0f;
  g->box.ymin = ((float)bbox.yMin) / 64.0f;
  g->box.ymax = ((float)bbox.yMax) / 64.0f;

  const int buffer_size = (int)(slot->bitmap.width * slot->bitmap.rows);
  if (buffer_size != 0) {
    if (font->flags & BLF_MONOCHROME) {
      /* Font buffer uses only 0 or 1 values, Blender expects full 0..255 range */
      for (int i = 0; i < buffer_size; i++) {
        slot->bitmap.buffer[i] = slot->bitmap.buffer[i] ? 255 : 0;
      }
    }
    g->bitmap = MEM_mallocN((size_t)buffer_size, "glyph bitmap");
    memcpy(g->bitmap, slot->bitmap.buffer, (size_t)buffer_size);
  }

  unsigned int key = blf_hash(g->c);
  BLI_addhead(&(gc->bucket[key]), g);
  if (charcode < GLYPH_ASCII_TABLE_SIZE) {
    gc->glyph_ascii_table[charcode] = g;
  }

  BLI_spin_unlock(font->ft_lib_mutex);

  return g;
}

void blf_glyph_free(GlyphBLF *g)
{
  if (g->bitmap) {
    MEM_freeN(g->bitmap);
  }
  MEM_freeN(g);
}

static void blf_texture_draw(const unsigned char color[4],
                             const int glyph_size[2],
                             const int offset,
                             float x1,
                             float y1,
                             float x2,
                             float y2)
{
  /* Only one vertex per glyph, geometry shader expand it into a quad. */
  /* TODO: Get rid of Geom Shader because it's not optimal AT ALL for the GPU. */
  copy_v4_fl4(GPU_vertbuf_raw_step(&g_batch.pos_step),
              x1 + g_batch.ofs[0],
              y1 + g_batch.ofs[1],
              x2 + g_batch.ofs[0],
              y2 + g_batch.ofs[1]);
  copy_v4_v4_uchar(GPU_vertbuf_raw_step(&g_batch.col_step), color);
  copy_v2_v2_int(GPU_vertbuf_raw_step(&g_batch.glyph_size_step), glyph_size);
  *((int *)GPU_vertbuf_raw_step(&g_batch.offset_step)) = offset;

  g_batch.glyph_len++;
  /* Flush cache if it's full. */
  if (g_batch.glyph_len == BLF_BATCH_DRAW_LEN_MAX) {
    blf_batch_draw();
  }
}

static void blf_texture5_draw(const unsigned char color_in[4],
                              const int glyph_size[2],
                              const int offset,
                              float x1,
                              float y1,
                              float x2,
                              float y2)
{
  int glyph_size_flag[2];
  /* flag the x and y component signs for 5x5 blurring */
  glyph_size_flag[0] = -glyph_size[0];
  glyph_size_flag[1] = -glyph_size[1];

  blf_texture_draw(color_in, glyph_size_flag, offset, x1, y1, x2, y2);
}

static void blf_texture3_draw(const unsigned char color_in[4],
                              const int glyph_size[2],
                              const int offset,
                              float x1,
                              float y1,
                              float x2,
                              float y2)
{
  int glyph_size_flag[2];
  /* flag the x component sign for 3x3 blurring */
  glyph_size_flag[0] = -glyph_size[0];
  glyph_size_flag[1] = glyph_size[1];

  blf_texture_draw(color_in, glyph_size_flag, offset, x1, y1, x2, y2);
}

static void blf_glyph_calc_rect(rctf *rect, GlyphBLF *g, float x, float y)
{
  rect->xmin = floorf(x + (float)g->pos[0]);
  rect->xmax = rect->xmin + (float)g->dims[0];
  rect->ymin = floorf(y + (float)g->pos[1]);
  rect->ymax = rect->ymin - (float)g->dims[1];
}

static void blf_glyph_calc_rect_test(rctf *rect, GlyphBLF *g, float x, float y)
{
  /* Intentionally check with g->advance, because this is the
   * width used by BLF_width. This allows that the text slightly
   * overlaps the clipping border to achieve better alignment. */
  rect->xmin = floorf(x);
  rect->xmax = rect->xmin + MIN2(g->advance, (float)g->dims[0]);
  rect->ymin = floorf(y);
  rect->ymax = rect->ymin - (float)g->dims[1];
}

static void blf_glyph_calc_rect_shadow(rctf *rect, GlyphBLF *g, float x, float y, FontBLF *font)
{
  blf_glyph_calc_rect(rect, g, x + (float)font->shadow_x, y + (float)font->shadow_y);
}

void blf_glyph_draw(FontBLF *font, GlyphCacheBLF *gc, GlyphBLF *g, float x, float y)
{
  if ((!g->dims[0]) || (!g->dims[1])) {
    return;
  }

  if (g->glyph_cache == NULL) {
    if (font->tex_size_max == -1) {
      font->tex_size_max = GPU_max_texture_size();
    }

    g->offset = gc->bitmap_len;

    int buff_size = g->dims[0] * g->dims[1];
    int bitmap_len = gc->bitmap_len + buff_size;

    if (bitmap_len > gc->bitmap_len_alloc) {
      int w = font->tex_size_max;
      int h = bitmap_len / w + 1;

      gc->bitmap_len_alloc = w * h;
      gc->bitmap_result = MEM_reallocN(gc->bitmap_result, (size_t)gc->bitmap_len_alloc);

      /* Keep in sync with the texture. */
      if (gc->texture) {
        GPU_texture_free(gc->texture);
      }
      gc->texture = GPU_texture_create_2d(__func__, w, h, 1, GPU_R8, NULL);

      gc->bitmap_len_landed = 0;
    }

    memcpy(&gc->bitmap_result[gc->bitmap_len], g->bitmap, (size_t)buff_size);
    gc->bitmap_len = bitmap_len;

    g->glyph_cache = gc;
  }

  if (font->flags & BLF_CLIPPING) {
    rctf rect_test;
    blf_glyph_calc_rect_test(&rect_test, g, x, y);
    BLI_rctf_translate(&rect_test, font->pos[0], font->pos[1]);

    if (!BLI_rctf_inside_rctf(&font->clip_rec, &rect_test)) {
      return;
    }
  }

  if (g_batch.glyph_cache != g->glyph_cache) {
    blf_batch_draw();
    g_batch.glyph_cache = g->glyph_cache;
  }

  if (font->flags & BLF_SHADOW) {
    rctf rect_ofs;
    blf_glyph_calc_rect_shadow(&rect_ofs, g, x, y, font);

    if (font->shadow == 0) {
      blf_texture_draw(font->shadow_color,
                       g->dims,
                       g->offset,
                       rect_ofs.xmin,
                       rect_ofs.ymin,
                       rect_ofs.xmax,
                       rect_ofs.ymax);
    }
    else if (font->shadow <= 4) {
      blf_texture3_draw(font->shadow_color,
                        g->dims,
                        g->offset,
                        rect_ofs.xmin,
                        rect_ofs.ymin,
                        rect_ofs.xmax,
                        rect_ofs.ymax);
    }
    else {
      blf_texture5_draw(font->shadow_color,
                        g->dims,
                        g->offset,
                        rect_ofs.xmin,
                        rect_ofs.ymin,
                        rect_ofs.xmax,
                        rect_ofs.ymax);
    }
  }

  rctf rect;
  blf_glyph_calc_rect(&rect, g, x, y);

#if BLF_BLUR_ENABLE
  switch (font->blur) {
    case 3:
      blf_texture3_draw(
          font->color, g->dims, g->offset, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
      break;
    case 5:
      blf_texture5_draw(
          font->color, g->dims, g->offset, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
      break;
    default:
      blf_texture_draw(
          font->color, g->dims, g->offset, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
  }
#else
  blf_texture_draw(font->color, g->dims, g->offset, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
#endif
}

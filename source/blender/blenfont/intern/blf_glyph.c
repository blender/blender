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

/* -------------------------------------------------------------------- */
/** \name Internal Utilities
 * \{ */

/**
 * Convert a floating point value to a FreeType 16.16 fixed point value.
 */
static FT_Fixed to_16dot16(double val)
{
  return (FT_Fixed)(lround(val * 65536.0));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Glyph Cache
 * \{ */

/**
 * Find a glyph cache that matches a size, DPI & styles.
 */
GlyphCacheBLF *blf_glyph_cache_find(FontBLF *font, unsigned int size, unsigned int dpi)
{
  GlyphCacheBLF *gc = (GlyphCacheBLF *)font->cache.first;
  while (gc) {
    if (gc->size == size && gc->dpi == dpi && (gc->bold == ((font->flags & BLF_BOLD) != 0)) &&
        (gc->italic == ((font->flags & BLF_ITALIC) != 0))) {
      return gc;
    }
    gc = gc->next;
  }
  return NULL;
}

/**
 * Create a new glyph cache for the current size, DPI & styles.
 */
GlyphCacheBLF *blf_glyph_cache_new(FontBLF *font)
{
  GlyphCacheBLF *gc = (GlyphCacheBLF *)MEM_callocN(sizeof(GlyphCacheBLF), "blf_glyph_cache_new");

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

/**
 * Try to find a glyph in cache.
 *
 * \return NULL if not found.
 */
static GlyphBLF *blf_glyph_cache_find_glyph(GlyphCacheBLF *gc, uint charcode)
{
  if (charcode < GLYPH_ASCII_TABLE_SIZE) {
    return gc->glyph_ascii_table[charcode];
  }

  GlyphBLF *g = gc->bucket[blf_hash(charcode)].first;
  while (g) {
    if (g->c == charcode) {
      return g;
    }
    g = g->next;
  }
  return NULL;
}

/**
 * Add a rendered glyph to a cache.
 */
static GlyphBLF *blf_glyph_cache_add_glyph(
    FontBLF *font, GlyphCacheBLF *gc, FT_GlyphSlot glyph, uint charcode, FT_UInt glyph_index)
{
  GlyphBLF *g = (GlyphBLF *)MEM_callocN(sizeof(GlyphBLF), "blf_glyph_get");
  g->c = charcode;
  g->idx = glyph_index;
  g->advance = ((float)glyph->advance.x) / 64.0f;
  g->advance_i = (int)g->advance;
  g->pos[0] = glyph->bitmap_left;
  g->pos[1] = glyph->bitmap_top;
  g->dims[0] = (int)glyph->bitmap.width;
  g->dims[1] = (int)glyph->bitmap.rows;
  g->pitch = glyph->bitmap.pitch;

  FT_BBox bbox;
  FT_Outline_Get_CBox(&(glyph->outline), &bbox);
  g->box.xmin = ((float)bbox.xMin) / 64.0f;
  g->box.xmax = ((float)bbox.xMax) / 64.0f;
  g->box.ymin = ((float)bbox.yMin) / 64.0f;
  g->box.ymax = ((float)bbox.yMax) / 64.0f;

  const int buffer_size = (int)(glyph->bitmap.width * glyph->bitmap.rows);
  if (buffer_size != 0) {
    if (font->flags & BLF_MONOCHROME) {
      /* Font buffer uses only 0 or 1 values, Blender expects full 0..255 range. */
      for (int i = 0; i < buffer_size; i++) {
        glyph->bitmap.buffer[i] = glyph->bitmap.buffer[i] ? 255 : 0;
      }
    }
    g->bitmap = MEM_mallocN((size_t)buffer_size, "glyph bitmap");
    memcpy(g->bitmap, glyph->bitmap.buffer, (size_t)buffer_size);
  }

  unsigned int key = blf_hash(g->c);
  BLI_addhead(&(gc->bucket[key]), g);
  if (charcode < GLYPH_ASCII_TABLE_SIZE) {
    gc->glyph_ascii_table[charcode] = g;
  }

  return g;
}

/**
 * Return a glyph index from `charcode`. Not found returns zero, which is a valid
 * printable character (`.notdef` or `tofu`). Font is allowed to change here.
 */
static FT_UInt blf_glyph_index_from_charcode(FontBLF **font, const uint charcode)
{
  FT_UInt glyph_index = FT_Get_Char_Index((*font)->face, charcode);
  /* TODO: If not found in this font, check others, update font pointer. */
  return glyph_index;
}

/**
 * Load a glyph into the glyph slot of a font's face object.
 */
static FT_GlyphSlot blf_glyph_load(FontBLF *font, FT_UInt glyph_index)
{
  int load_flags;

  if (font->flags & BLF_MONOCHROME) {
    load_flags = FT_LOAD_TARGET_MONO;
  }
  else {
    load_flags = FT_LOAD_NO_BITMAP;
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

  if (FT_Load_Glyph(font->face, glyph_index, load_flags) == 0) {
    return font->face->glyph;
  }
  return NULL;
}

/**
 * Convert a glyph from outlines to a bitmap that we can display.
 */
static bool blf_glyph_render_bitmap(FontBLF *font, FT_GlyphSlot glyph)
{
  int render_mode;

  if (font->flags & BLF_MONOCHROME) {
    render_mode = FT_RENDER_MODE_MONO;
  }
  else {
    render_mode = FT_RENDER_MODE_NORMAL;
  }

  /* Render the glyph curves to a bitmap. */
  FT_Error err = FT_Render_Glyph(glyph, render_mode);
  if (err != 0) {
    return false;
  }

  FT_Bitmap tempbitmap;

  if (font->flags & BLF_MONOCHROME) {
    /* Convert result from 1 bit per pixel to 8 bit per pixel */
    /* Accumulate errors for later, fine if not interested beyond "ok vs any error" */
    FT_Bitmap_New(&tempbitmap);

    /* Does Blender use Pitch 1 always? It works so far */
    err += FT_Bitmap_Convert(font->ft_lib, &glyph->bitmap, &tempbitmap, 1);
    err += FT_Bitmap_Copy(font->ft_lib, &tempbitmap, &glyph->bitmap);
    err += FT_Bitmap_Done(font->ft_lib, &tempbitmap);
  }

  if (err || glyph->format != FT_GLYPH_FORMAT_BITMAP) {
    return false;
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Glyph Transformations
 * \{ */

/**
 * Adjust the glyphs weight by a factor.
 *
 * \param factor: -1 (min stroke width) <= 0 (normal) => 1 (max boldness).
 */
static bool blf_glyph_transform_weight(FT_GlyphSlot glyph, float factor, bool monospaced)
{
  if (glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
    /* Fake bold if the font does not have this variable axis. */
    const FT_Pos average_width = FT_MulFix(glyph->face->units_per_EM,
                                           glyph->face->size->metrics.x_scale);
    FT_Pos change = (FT_Pos)((float)average_width * factor * 0.1f);
    FT_Outline_EmboldenXY(&glyph->outline, change, change / 2);
    if (monospaced) {
      /* Widened fixed-pitch font needs a nudge left. */
      FT_Outline_Translate(&glyph->outline, change / -2, 0);
    }
    else {
      /* Need to increase advance. */
      glyph->advance.x += change;
      glyph->advance.y += change / 2;
    }
    return true;
  }
  return false;
}

/**
 * Adjust the glyphs slant by a factor (making it oblique).
 *
 * \param factor: -1 (max negative) <= 0 (no slant) => 1 (max positive).
 *
 * \note that left-leaning italics are possible in some RTL writing systems.
 */
static bool blf_glyph_transform_slant(FT_GlyphSlot glyph, float factor)
{
  if (glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
    FT_Matrix transform = {to_16dot16(1), to_16dot16(factor / 2.0f), 0, to_16dot16(1)};
    FT_Outline_Transform(&glyph->outline, &transform);
    return true;
  }
  return false;
}

/**
 * Adjust the glyph width by factor.
 *
 * \param factor: -1 (min width) <= 0 (normal) => 1 (max width).
 */
static bool UNUSED_FUNCTION(blf_glyph_transform_width)(FT_GlyphSlot glyph, float factor)
{
  if (glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
    float scale = (factor * 0.4f) + 1.0f; /* 0.6f - 1.4f */
    FT_Matrix matrix = {to_16dot16(scale), 0, 0, to_16dot16(1)};
    FT_Outline_Transform(&glyph->outline, &matrix);
    glyph->advance.x = (FT_Pos)((double)glyph->advance.x * scale);
    return true;
  }
  return false;
}

/**
 * Transform glyph to fit nicely within a fixed column width.
 */
static bool UNUSED_FUNCTION(blf_glyph_transform_monospace)(FT_GlyphSlot glyph, int width)
{
  if (glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
    int gwidth = (int)(glyph->linearHoriAdvance >> 16);
    if (gwidth > width) {
      float scale = (float)width / (float)gwidth;
      FT_Matrix matrix = {to_16dot16(scale), 0, 0, to_16dot16(1)};
      /* Narrowing all points also thins vertical strokes. */
      FT_Outline_Transform(&glyph->outline, &matrix);
      const FT_Pos extra_x = (int)((float)(gwidth - width) * 5.65f);
      /* Horizontally widen strokes to counteract narrowing. */
      FT_Outline_EmboldenXY(&glyph->outline, extra_x, 0);
    }
    else if (gwidth < width) {
      /* Narrow glyphs only need to be centered. */
      int nudge = (width - gwidth) / 2;
      FT_Outline_Translate(&glyph->outline, (FT_Pos)nudge * 64, 0);
    }
    return true;
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Glyph Access (Ensure/Free)
 * \{ */

/**
 * Create and return a fully-rendered bitmap glyph.
 */
static FT_GlyphSlot blf_glyph_render(FontBLF *settings_font,
                                     FontBLF *glyph_font,
                                     FT_UInt glyph_index)
{
  if (glyph_font != settings_font) {
    FT_Set_Char_Size(glyph_font->face,
                     0,
                     ((FT_F26Dot6)(settings_font->size)) * 64,
                     settings_font->dpi,
                     settings_font->dpi);
    glyph_font->size = settings_font->size;
    glyph_font->dpi = settings_font->dpi;
  }

  FT_GlyphSlot glyph = blf_glyph_load(glyph_font, glyph_index);
  if (!glyph) {
    return NULL;
  }

  if ((settings_font->flags & BLF_ITALIC) != 0) {
    /* 37.5% of maximum rightward slant results in 6 degree slope, matching italic
     * version (`DejaVuSans-Oblique.ttf`) of our current font. But a nice median when
     * checking others. Worth reevaluating if we change default font. We could also
     * narrow the glyph slightly as most italics do, but this one does not. */
    blf_glyph_transform_slant(glyph, 0.375f);
  }

  if ((settings_font->flags & BLF_BOLD) != 0) {
    /* 70% of maximum weight results in the same amount of boldness and horizontal
     * expansion as the bold version (`DejaVuSans-Bold.ttf`) of our default font.
     * Worth reevaluating if we change default font. */
    blf_glyph_transform_weight(glyph, 0.7f, glyph->face->face_flags & FT_FACE_FLAG_FIXED_WIDTH);
  }

  if (blf_glyph_render_bitmap(glyph_font, glyph)) {
    return glyph;
  }
  return NULL;
}

/**
 * Create (or load from cache) a fully-rendered bitmap glyph.
 */
GlyphBLF *blf_glyph_ensure(FontBLF *font, GlyphCacheBLF *gc, uint charcode)
{
  GlyphBLF *g = blf_glyph_cache_find_glyph(gc, charcode);
  if (g) {
    return g;
  }

  /* Glyph might not come from the initial font. */
  FontBLF *font_with_glyph = font;
  FT_UInt glyph_index = blf_glyph_index_from_charcode(&font_with_glyph, charcode);

  /* Glyphs are dynamically created as needed by font rendering. this means that
   * to make font rendering thread safe we have to do locking here. note that this
   * must be a lock for the whole library and not just per font, because the font
   * renderer uses a shared buffer internally. */
  BLI_spin_lock(font_with_glyph->ft_lib_mutex);

  FT_GlyphSlot glyph = blf_glyph_render(font, font_with_glyph, glyph_index);

  if (glyph) {
    /* Save this glyph in the initial font's cache. */
    g = blf_glyph_cache_add_glyph(font, gc, glyph, charcode, glyph_index);
  }

  BLI_spin_unlock(font_with_glyph->ft_lib_mutex);
  return g;
}

void blf_glyph_free(GlyphBLF *g)
{
  if (g->bitmap) {
    MEM_freeN(g->bitmap);
  }
  MEM_freeN(g);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Glyph Bounds Calculation
 * \{ */

static void blf_glyph_calc_rect(rctf *rect, GlyphBLF *g, float x, float y)
{
  rect->xmin = floorf(x + (float)g->pos[0]);
  rect->xmax = rect->xmin + (float)g->dims[0];
  rect->ymin = floorf(y + (float)g->pos[1]);
  rect->ymax = rect->ymin - (float)g->dims[1];
}

static void blf_glyph_calc_rect_test(rctf *rect, GlyphBLF *g, float x, float y)
{
  /* Intentionally check with `g->advance`, because this is the
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Glyph Drawing
 * \{ */

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

/** \} */

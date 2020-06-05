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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup blf
 */

#ifndef __BLF_INTERNAL_TYPES_H__
#define __BLF_INTERNAL_TYPES_H__

#include "GPU_texture.h"
#include "GPU_vertex_buffer.h"

#define BLF_BATCH_DRAW_LEN_MAX 2048 /* in glyph */

typedef struct BatchBLF {
  struct FontBLF *font; /* can only batch glyph from the same font */
  struct GPUBatch *batch;
  struct GPUVertBuf *verts;
  struct GPUVertBufRaw pos_step, col_step, offset_step, glyph_size_step;
  unsigned int pos_loc, col_loc, offset_loc, glyph_size_loc;
  unsigned int glyph_len;
  float ofs[2];    /* copy of font->pos */
  float mat[4][4]; /* previous call modelmatrix. */
  bool enabled, active, simple_shader;
  struct GlyphCacheBLF *glyph_cache;
} BatchBLF;

extern BatchBLF g_batch;

typedef struct KerningCacheBLF {
  struct KerningCacheBLF *next, *prev;

  /* kerning mode. */
  FT_UInt mode;

  /* only cache a ascii glyph pairs. Only store the x
   * offset we are interested in, instead of the full FT_Vector. */
  int table[0x80][0x80];
} KerningCacheBLF;

typedef struct GlyphCacheBLF {
  struct GlyphCacheBLF *next;
  struct GlyphCacheBLF *prev;

  /* font size. */
  unsigned int size;

  /* and dpi. */
  unsigned int dpi;

  bool bold;
  bool italic;

  /* and the glyphs. */
  ListBase bucket[257];

  /* fast ascii lookup */
  struct GlyphBLF *glyph_ascii_table[256];

  /* texture array, to draw the glyphs. */
  GPUTexture *texture;
  char *bitmap_result;
  int bitmap_len;
  int bitmap_len_landed;
  int bitmap_len_alloc;

  /* and the bigger glyph in the font. */
  int glyph_width_max;
  int glyph_height_max;

  /* number of glyphs in the font. */
  int glyphs_len_max;

  /* number of glyphs not yet loaded (decreases every glyph loaded). */
  int glyphs_len_free;

  /* ascender and descender value. */
  float ascender;
  float descender;
} GlyphCacheBLF;

typedef struct GlyphBLF {
  struct GlyphBLF *next;
  struct GlyphBLF *prev;

  /* and the character, as UTF8 */
  unsigned int c;

  /* freetype2 index, to speed-up the search. */
  FT_UInt idx;

  /* glyph box. */
  rctf box;

  /* advance size. */
  float advance;
  /* avoid conversion to int while drawing */
  int advance_i;

  /* position inside the texture where this glyph is store. */
  int offset;

  /* Bitmap data, from freetype. Take care that this
   * can be NULL.
   */
  unsigned char *bitmap;

  /* Glyph width and height. */
  int dims[2];
  int pitch;

  /**
   * X and Y bearing of the glyph.
   * The X bearing is from the origin to the glyph left bbox edge.
   * The Y bearing is from the baseline to the top of the glyph edge.
   */
  int pos[2];

  struct GlyphCacheBLF *glyph_cache;
} GlyphBLF;

typedef struct FontBufInfoBLF {
  /* for draw to buffer, always set this to NULL after finish! */
  float *fbuf;

  /* the same but unsigned char */
  unsigned char *cbuf;

  /** Buffer size, keep signed so comparisons with negative values work. */
  int dims[2];

  /* number of channels. */
  int ch;

  /* display device used for color management */
  struct ColorManagedDisplay *display;

  /* and the color, the alphas is get from the glyph!
   * color is srgb space */
  float col_init[4];
  /* cached conversion from 'col_init' */
  unsigned char col_char[4];
  float col_float[4];

} FontBufInfoBLF;

typedef struct FontBLF {
  /* font name. */
  char *name;

  /* # of times this font was loaded */
  unsigned int reference_count;

  /* filename or NULL. */
  char *filename;

  /* aspect ratio or scale. */
  float aspect[3];

  /* initial position for draw the text. */
  float pos[3];

  /* angle in radians. */
  float angle;

#if 0 /* BLF_BLUR_ENABLE */
  /* blur: 3 or 5 large kernel */
  int blur;
#endif

  /* shadow level. */
  int shadow;

  /* and shadow offset. */
  int shadow_x;
  int shadow_y;

  /* shadow color. */
  unsigned char shadow_color[4];

  /* main text color. */
  unsigned char color[4];

  /* Multiplied this matrix with the current one before
   * draw the text! see blf_draw__start.
   */
  float m[16];

  /* clipping rectangle. */
  rctf clip_rec;

  /* the width to wrap the text, see BLF_WORD_WRAP */
  int wrap_width;

  /* font dpi (default 72). */
  unsigned int dpi;

  /* font size. */
  unsigned int size;

  /* max texture size. */
  int tex_size_max;

  /* font options. */
  int flags;

  /* List of glyph caches (GlyphCacheBLF) for this font for size, dpi, bold, italic.
   * Use blf_glyph_cache_acquire(font) and blf_glyph_cache_release(font) to access cache!
   */
  ListBase cache;

  /* list of kerning cache for this font. */
  ListBase kerning_caches;

  /* current kerning cache for this font and kerning mode. */
  KerningCacheBLF *kerning_cache;

  /* freetype2 lib handle. */
  FT_Library ft_lib;

  /* Mutex lock for library */
  SpinLock *ft_lib_mutex;

  /* freetype2 face. */
  FT_Face face;

  /* freetype kerning */
  FT_UInt kerning_mode;

  /* data for buffer usage (drawing into a texture buffer) */
  FontBufInfoBLF buf_info;

  /* Mutex lock for glyph cache. */
  SpinLock *glyph_cache_mutex;
} FontBLF;

typedef struct DirBLF {
  struct DirBLF *next;
  struct DirBLF *prev;

  /* full path where search fonts. */
  char *path;
} DirBLF;

#endif /* __BLF_INTERNAL_TYPES_H__ */

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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenfont/intern/blf_internal_types.h
 *  \ingroup blf
 */


#ifndef __BLF_INTERNAL_TYPES_H__
#define __BLF_INTERNAL_TYPES_H__

#include "GPU_vertex_buffer.h"
#include "GPU_texture.h"

#define BLF_BATCH_DRAW_LEN_MAX 2048 /* in glyph */

typedef struct BatchBLF {
	struct FontBLF *font; /* can only batch glyph from the same font */
	struct GPUBatch *batch;
	struct GPUVertBuf *verts;
	struct GPUVertBufRaw pos_step, tex_step, col_step;
	unsigned int pos_loc, tex_loc, col_loc;
	unsigned int glyph_len;
	float ofs[2];    /* copy of font->pos */
	float mat[4][4]; /* previous call modelmatrix. */
	bool enabled, active, simple_shader;
	GPUTexture *tex_bind_state;
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

	/* and the glyphs. */
	ListBase bucket[257];

	/* fast ascii lookup */
	struct GlyphBLF *glyph_ascii_table[256];

	/* texture array, to draw the glyphs. */
	GPUTexture **textures;

	/* size of the array. */
	unsigned int textures_len;

	/* and the last texture, aka. the current texture. */
	unsigned int texture_current;

	/* like bftgl, we draw every glyph in a big texture, so this is the
	 * current position inside the texture.
	 */
	int offset_x;
	int offset_y;

	/* and the space from one to other. */
	int pad;

	/* and the bigger glyph in the font. */
	int glyph_width_max;
	int glyph_height_max;

	/* next two integer power of two, to build the texture. */
	int p2_width;
	int p2_height;

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

	/* texture id where this glyph is store. */
	GPUTexture *tex;

	/* position inside the texture where this glyph is store. */
	int offset_x;
	int offset_y;

	/* Bitmap data, from freetype. Take care that this
	 * can be NULL.
	 */
	unsigned char *bitmap;

	/* glyph width and height. */
	int width;
	int height;
	int pitch;

	/* uv coords. */
	float uv[2][2];

	/* X and Y bearing of the glyph.
	 * The X bearing is from the origin to the glyph left bbox edge.
	 * The Y bearing is from the baseline to the top of the glyph edge.
	 */
	float pos_x;
	float pos_y;

	/* with value of zero mean that we need build the texture. */
	char build_tex;
} GlyphBLF;

typedef struct FontBufInfoBLF {
	/* for draw to buffer, always set this to NULL after finish! */
	float *fbuf;

	/* the same but unsigned char */
	unsigned char *cbuf;

	/* buffer size, keep signed so comparisons with negative values work */
	int w;
	int h;

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

	/* cache current OpenGL texture to save calls into the API */
	GPUTexture *tex_bind_state;

	/* font options. */
	int flags;

	/* list of glyph cache for this font. */
	ListBase cache;

	/* current glyph cache, size and dpi. */
	GlyphCacheBLF *glyph_cache;

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
} FontBLF;

typedef struct DirBLF {
	struct DirBLF *next;
	struct DirBLF *prev;

	/* full path where search fonts. */
	char *path;
} DirBLF;

#define BLF_TEXTURE_UNSET ((unsigned int)-1)

#endif /* __BLF_INTERNAL_TYPES_H__ */

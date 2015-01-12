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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenfont/intern/blf_font.c
 *  \ingroup blf
 *
 * Deals with drawing text to OpenGL or bitmap buffers.
 *
 * Also low level functions for managing \a FontBLF.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "MEM_guardedalloc.h"

#include "DNA_vec_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_threads.h"
#include "BLI_alloca.h"

#include "BIF_gl.h"
#include "BLF_api.h"

#include "IMB_colormanagement.h"

#include "blf_internal_types.h"
#include "blf_internal.h"

#include "BLI_strict_flags.h"

#ifdef WIN32
#  define FT_New_Face FT_New_Face__win32_compat
#endif

/* freetype2 handle ONLY for this file!. */
static FT_Library ft_lib;
static SpinLock ft_lib_mutex;

int blf_font_init(void)
{
	BLI_spin_init(&ft_lib_mutex);
	return FT_Init_FreeType(&ft_lib);
}

void blf_font_exit(void)
{
	FT_Done_FreeType(ft_lib);
	BLI_spin_end(&ft_lib_mutex);
}

void blf_font_size(FontBLF *font, unsigned int size, unsigned int dpi)
{
	GlyphCacheBLF *gc;
	FT_Error err;

	err = FT_Set_Char_Size(font->face, 0, (FT_F26Dot6)(size * 64), dpi, dpi);
	if (err) {
		/* FIXME: here we can go through the fixed size and choice a close one */
		printf("The current font don't support the size, %u and dpi, %u\n", size, dpi);
		return;
	}

	font->size = size;
	font->dpi = dpi;

	gc = blf_glyph_cache_find(font, size, dpi);
	if (gc)
		font->glyph_cache = gc;
	else {
		gc = blf_glyph_cache_new(font);
		if (gc)
			font->glyph_cache = gc;
		else
			font->glyph_cache = NULL;
	}
}

static void blf_font_ensure_ascii_table(FontBLF *font)
{
	GlyphBLF **glyph_ascii_table = font->glyph_cache->glyph_ascii_table;

	/* build ascii on demand */
	if (glyph_ascii_table['0'] == NULL) {
		GlyphBLF *g;
		unsigned int i;
		for (i = 0; i < 256; i++) {
			g = blf_glyph_search(font->glyph_cache, i);
			if (!g) {
				FT_UInt glyph_index = FT_Get_Char_Index(font->face, i);
				g = blf_glyph_add(font, glyph_index, i);
			}
			glyph_ascii_table[i] = g;
		}
	}
}

/* Fast path for runs of ASCII characters. Given that common UTF-8
 * input will consist of an overwhelming majority of ASCII
 * characters.
 */

/* Note,
 * blf_font_ensure_ascii_table(font); must be called before this macro */

#define BLF_UTF8_NEXT_FAST(_font, _g, _str, _i, _c, _glyph_ascii_table)          \
	if (((_c) = (_str)[_i]) < 0x80) {                                            \
		_g = (_glyph_ascii_table)[_c];                                           \
		_i++;                                                                    \
	}                                                                            \
	else if ((_c = BLI_str_utf8_as_unicode_step(_str, &(_i))) != BLI_UTF8_ERR) { \
		if ((_g = blf_glyph_search((_font)->glyph_cache, _c)) == NULL) {         \
			_g = blf_glyph_add(_font,                                            \
			                  FT_Get_Char_Index((_font)->face, _c), _c);         \
		}                                                                        \
	} (void)0


#define BLF_KERNING_VARS(_font, _has_kerning, _kern_mode)                        \
	const bool _has_kerning = FT_HAS_KERNING((_font)->face);                     \
	const FT_UInt _kern_mode = (_has_kerning == 0) ? 0 :                         \
	                         (((_font)->flags & BLF_KERNING_DEFAULT) ?           \
	                          ft_kerning_default : (FT_UInt)FT_KERNING_UNFITTED) \


#define BLF_KERNING_STEP(_font, _kern_mode, _g_prev, _g, _delta, _pen_x)         \
{                                                                                \
	if (_g_prev) {                                                               \
		_delta.x = _delta.y = 0;                                                 \
		if (FT_Get_Kerning((_font)->face,                                        \
		                   (_g_prev)->idx,                                       \
		                   (_g)->idx,                                            \
		                   _kern_mode,                                           \
		                   &(_delta)) == 0)                                      \
		{                                                                        \
			_pen_x += (int)_delta.x >> 6;                                        \
		}                                                                        \
	}                                                                            \
} (void)0

void blf_font_draw(FontBLF *font, const char *str, size_t len)
{
	unsigned int c;
	GlyphBLF *g, *g_prev = NULL;
	FT_Vector delta;
	int pen_x = 0, pen_y = 0;
	size_t i = 0;
	GlyphBLF **glyph_ascii_table = font->glyph_cache->glyph_ascii_table;

	BLF_KERNING_VARS(font, has_kerning, kern_mode);

	blf_font_ensure_ascii_table(font);

	while ((i < len) && str[i]) {
		BLF_UTF8_NEXT_FAST(font, g, str, i, c, glyph_ascii_table);

		if (UNLIKELY(c == BLI_UTF8_ERR))
			break;
		if (UNLIKELY(g == NULL))
			continue;
		if (has_kerning)
			BLF_KERNING_STEP(font, kern_mode, g_prev, g, delta, pen_x);

		/* do not return this loop if clipped, we want every character tested */
		blf_glyph_render(font, g, (float)pen_x, (float)pen_y);

		pen_x += g->advance_i;
		g_prev = g;
	}
}

/* faster version of blf_font_draw, ascii only for view dimensions */
void blf_font_draw_ascii(FontBLF *font, const char *str, size_t len)
{
	unsigned char c;
	GlyphBLF *g, *g_prev = NULL;
	FT_Vector delta;
	int pen_x = 0, pen_y = 0;
	GlyphBLF **glyph_ascii_table = font->glyph_cache->glyph_ascii_table;

	BLF_KERNING_VARS(font, has_kerning, kern_mode);

	blf_font_ensure_ascii_table(font);

	while ((c = *(str++)) && len--) {
		BLI_assert(c < 128);
		if ((g = glyph_ascii_table[c]) == NULL)
			continue;
		if (has_kerning)
			BLF_KERNING_STEP(font, kern_mode, g_prev, g, delta, pen_x);

		/* do not return this loop if clipped, we want every character tested */
		blf_glyph_render(font, g, (float)pen_x, (float)pen_y);

		pen_x += g->advance_i;
		g_prev = g;
	}
}

/* use fixed column width, but an utf8 character may occupy multiple columns */
int blf_font_draw_mono(FontBLF *font, const char *str, size_t len, int cwidth)
{
	unsigned int c;
	GlyphBLF *g;
	int col, columns = 0;
	int pen_x = 0, pen_y = 0;
	size_t i = 0;
	GlyphBLF **glyph_ascii_table = font->glyph_cache->glyph_ascii_table;

	blf_font_ensure_ascii_table(font);

	while ((i < len) && str[i]) {
		BLF_UTF8_NEXT_FAST(font, g, str, i, c, glyph_ascii_table);

		if (UNLIKELY(c == BLI_UTF8_ERR))
			break;
		if (UNLIKELY(g == NULL))
			continue;

		/* do not return this loop if clipped, we want every character tested */
		blf_glyph_render(font, g, (float)pen_x, (float)pen_y);

		col = BLI_wcwidth((wchar_t)c);
		if (col < 0)
			col = 1;

		columns += col;
		pen_x += cwidth * col;
	}

	return columns;
}

/* Sanity checks are done by BLF_draw_buffer() */
void blf_font_buffer(FontBLF *font, const char *str)
{
	unsigned int c;
	GlyphBLF *g, *g_prev = NULL;
	FT_Vector delta;
	int pen_x = (int)font->pos[0], pen_y = 0;
	size_t i = 0;
	GlyphBLF **glyph_ascii_table = font->glyph_cache->glyph_ascii_table;

	/* buffer specific vars */
	FontBufInfoBLF *buf_info = &font->buf_info;
	float b_col_float[4];
	const unsigned char b_col_char[4] = {
	    (unsigned char)(buf_info->col[0] * 255),
	    (unsigned char)(buf_info->col[1] * 255),
	    (unsigned char)(buf_info->col[2] * 255),
	    (unsigned char)(buf_info->col[3] * 255)};

	unsigned char *cbuf;
	int chx, chy;
	int y, x;
	float a, *fbuf;

	BLF_KERNING_VARS(font, has_kerning, kern_mode);

	blf_font_ensure_ascii_table(font);

	/* another buffer specific call for color conversion */
	if (buf_info->display) {
		copy_v4_v4(b_col_float, buf_info->col);
		IMB_colormanagement_display_to_scene_linear_v3(b_col_float, buf_info->display);
	}
	else {
		srgb_to_linearrgb_v4(b_col_float, buf_info->col);
	}

	while (str[i]) {
		BLF_UTF8_NEXT_FAST(font, g, str, i, c, glyph_ascii_table);

		if (UNLIKELY(c == BLI_UTF8_ERR))
			break;
		if (UNLIKELY(g == NULL))
			continue;
		if (has_kerning)
			BLF_KERNING_STEP(font, kern_mode, g_prev, g, delta, pen_x);

		chx = pen_x + ((int)g->pos_x);
		chy = (int)font->pos[1] + g->height;

		if (g->pitch < 0) {
			pen_y = (int)font->pos[1] + (g->height - (int)g->pos_y);
		}
		else {
			pen_y = (int)font->pos[1] - (g->height - (int)g->pos_y);
		}

		if ((chx + g->width) >= 0 && chx < buf_info->w && (pen_y + g->height) >= 0 && pen_y < buf_info->h) {
			/* don't draw beyond the buffer bounds */
			int width_clip = g->width;
			int height_clip = g->height;
			int yb_start = g->pitch < 0 ? 0 : g->height - 1;

			if (width_clip + chx > buf_info->w)
				width_clip -= chx + width_clip - buf_info->w;
			if (height_clip + pen_y > buf_info->h)
				height_clip -= pen_y + height_clip - buf_info->h;
			
			/* drawing below the image? */
			if (pen_y < 0) {
				yb_start += (g->pitch < 0) ? -pen_y : pen_y;
				height_clip += pen_y;
				pen_y = 0;
			}

			if (buf_info->fbuf) {
				int yb = yb_start;
				for (y = ((chy >= 0) ? 0 : -chy); y < height_clip; y++) {
					for (x = ((chx >= 0) ? 0 : -chx); x < width_clip; x++) {
						a = *(g->bitmap + x + (yb * g->pitch)) / 255.0f;

						if (a > 0.0f) {
							float alphatest;
							fbuf = buf_info->fbuf + buf_info->ch * ((chx + x) + ((pen_y + y) * buf_info->w));
							if (a >= 1.0f) {
								fbuf[0] = b_col_float[0];
								fbuf[1] = b_col_float[1];
								fbuf[2] = b_col_float[2];
								fbuf[3] = (alphatest = (fbuf[3] + (b_col_float[3]))) < 1.0f ? alphatest : 1.0f;
							}
							else {
								fbuf[0] = (b_col_float[0] * a) + (fbuf[0] * (1.0f - a));
								fbuf[1] = (b_col_float[1] * a) + (fbuf[1] * (1.0f - a));
								fbuf[2] = (b_col_float[2] * a) + (fbuf[2] * (1.0f - a));
								fbuf[3] = (alphatest = (fbuf[3] + (b_col_float[3] * a))) < 1.0f ? alphatest : 1.0f;
							}
						}
					}

					if (g->pitch < 0)
						yb++;
					else
						yb--;
				}
			}

			if (buf_info->cbuf) {
				int yb = yb_start;
				for (y = 0; y < height_clip; y++) {
					for (x = 0; x < width_clip; x++) {
						a = *(g->bitmap + x + (yb * g->pitch)) / 255.0f;

						if (a > 0.0f) {
							int alphatest;
							cbuf = buf_info->cbuf + buf_info->ch * ((chx + x) + ((pen_y + y) * buf_info->w));
							if (a >= 1.0f) {
								cbuf[0] = b_col_char[0];
								cbuf[1] = b_col_char[1];
								cbuf[2] = b_col_char[2];
								
								alphatest = (int)cbuf[3] + (int)b_col_char[3];
								if (alphatest < 255) {
									cbuf[3] = (unsigned char)(alphatest);
								}
								else {
									cbuf[3] = 255;
								}
							}
							else {
								cbuf[0] = (unsigned char)((b_col_char[0] * a) + (cbuf[0] * (1.0f - a)));
								cbuf[1] = (unsigned char)((b_col_char[1] * a) + (cbuf[1] * (1.0f - a)));
								cbuf[2] = (unsigned char)((b_col_char[2] * a) + (cbuf[2] * (1.0f - a)));
								
								alphatest = ((int)cbuf[3] + (int)((b_col_float[3] * a) * 255.0f));
								if (alphatest < 255) {
									cbuf[3] = (unsigned char)(alphatest);
								}
								else {
									cbuf[3] = 255;
								}
							}
						}
					}

					if (g->pitch < 0)
						yb++;
					else
						yb--;
				}
			}
		}

		pen_x += g->advance_i;
		g_prev = g;
	}
}

size_t blf_font_width_to_strlen(FontBLF *font, const char *str, size_t len, float width, float *r_width)
{
	unsigned int c;
	GlyphBLF *g, *g_prev = NULL;
	FT_Vector delta;
	int pen_x = 0;
	size_t i = 0, i_prev;
	GlyphBLF **glyph_ascii_table = font->glyph_cache->glyph_ascii_table;
	const int width_i = (int)width + 1;
	int width_new;

	BLF_KERNING_VARS(font, has_kerning, kern_mode);

	blf_font_ensure_ascii_table(font);

	while ((i_prev = i), (width_new = pen_x), ((i < len) && str[i])) {
		BLF_UTF8_NEXT_FAST(font, g, str, i, c, glyph_ascii_table);

		if (UNLIKELY(c == BLI_UTF8_ERR))
			break;
		if (UNLIKELY(g == NULL))
			continue;
		if (has_kerning)
			BLF_KERNING_STEP(font, kern_mode, g_prev, g, delta, pen_x);

		pen_x += g->advance_i;

		if (width_i < pen_x) {
			break;
		}

		g_prev = g;
	}

	if (r_width) {
		*r_width = (float)width_new;
	}

	return i_prev;
}

size_t blf_font_width_to_rstrlen(FontBLF *font, const char *str, size_t len, float width, float *r_width)
{
	unsigned int c;
	GlyphBLF *g, *g_prev = NULL;
	FT_Vector delta;
	int pen_x = 0;
	size_t i = 0, i_prev;
	GlyphBLF **glyph_ascii_table = font->glyph_cache->glyph_ascii_table;
	const int width_i = (int)width + 1;
	int width_new;

	bool is_malloc;
	int (*width_accum)[2];
	int width_accum_ofs = 0;

	BLF_KERNING_VARS(font, has_kerning, kern_mode);

	/* skip allocs in simple cases */
	len = BLI_strnlen(str, len);
	if (width_i <= 1 || len == 0) {
		if (r_width) {
			*r_width = 0.0f;
		}
		return len;
	}

	if (len < 2048) {
		width_accum = BLI_array_alloca(width_accum, len);
		is_malloc = false;
	}
	else {
		width_accum = MEM_mallocN(sizeof(*width_accum) * len, __func__);
		is_malloc = true;
	}

	blf_font_ensure_ascii_table(font);

	while ((i < len) && str[i]) {
		BLF_UTF8_NEXT_FAST(font, g, str, i, c, glyph_ascii_table);

		if (UNLIKELY(c == BLI_UTF8_ERR))
			break;
		if (UNLIKELY(g == NULL))
			continue;
		if (has_kerning)
			BLF_KERNING_STEP(font, kern_mode, g_prev, g, delta, pen_x);

		pen_x += g->advance_i;

		width_accum[width_accum_ofs][0] = (int)i;
		width_accum[width_accum_ofs][1] = pen_x;
		width_accum_ofs++;

		g_prev = g;
	}

	if (pen_x > width_i && width_accum_ofs != 0) {
		const int min_x = pen_x - width_i;

		/* search backwards */
		width_new = pen_x;
		while (width_accum_ofs-- > 0) {
			if (min_x > width_accum[width_accum_ofs][1]) {
				break;
			}
		}
		width_accum_ofs++;
		width_new = pen_x - width_accum[width_accum_ofs][1];
		i_prev = (size_t)width_accum[width_accum_ofs][0];
	}
	else {
		width_new = pen_x;
		i_prev = 0;
	}

	if (is_malloc) {
		MEM_freeN(width_accum);
	}

	if (r_width) {
		*r_width = (float)width_new;
	}

	return i_prev;
}

void blf_font_boundbox(FontBLF *font, const char *str, size_t len, rctf *box)
{
	unsigned int c;
	GlyphBLF *g, *g_prev = NULL;
	FT_Vector delta;
	int pen_x = 0, pen_y = 0;
	size_t i = 0;
	GlyphBLF **glyph_ascii_table = font->glyph_cache->glyph_ascii_table;

	rctf gbox;

	BLF_KERNING_VARS(font, has_kerning, kern_mode);

	box->xmin = 32000.0f;
	box->xmax = -32000.0f;
	box->ymin = 32000.0f;
	box->ymax = -32000.0f;

	blf_font_ensure_ascii_table(font);

	while ((i < len) && str[i]) {
		BLF_UTF8_NEXT_FAST(font, g, str, i, c, glyph_ascii_table);

		if (UNLIKELY(c == BLI_UTF8_ERR))
			break;
		if (UNLIKELY(g == NULL))
			continue;
		if (has_kerning)
			BLF_KERNING_STEP(font, kern_mode, g_prev, g, delta, pen_x);

		gbox.xmin = (float)pen_x;
		gbox.xmax = (float)pen_x + g->advance;
		gbox.ymin = g->box.ymin + (float)pen_y;
		gbox.ymax = g->box.ymax + (float)pen_y;

		if (gbox.xmin < box->xmin) box->xmin = gbox.xmin;
		if (gbox.ymin < box->ymin) box->ymin = gbox.ymin;

		if (gbox.xmax > box->xmax) box->xmax = gbox.xmax;
		if (gbox.ymax > box->ymax) box->ymax = gbox.ymax;

		pen_x += g->advance_i;
		g_prev = g;
	}

	if (box->xmin > box->xmax) {
		box->xmin = 0.0f;
		box->ymin = 0.0f;
		box->xmax = 0.0f;
		box->ymax = 0.0f;
	}
}

void blf_font_width_and_height(FontBLF *font, const char *str, size_t len, float *width, float *height)
{
	float xa, ya;
	rctf box;

	if (font->flags & BLF_ASPECT) {
		xa = font->aspect[0];
		ya = font->aspect[1];
	}
	else {
		xa = 1.0f;
		ya = 1.0f;
	}

	blf_font_boundbox(font, str, len, &box);
	*width  = (BLI_rctf_size_x(&box) * xa);
	*height = (BLI_rctf_size_y(&box) * ya);
}

float blf_font_width(FontBLF *font, const char *str, size_t len)
{
	float xa;
	rctf box;

	if (font->flags & BLF_ASPECT)
		xa = font->aspect[0];
	else
		xa = 1.0f;

	blf_font_boundbox(font, str, len, &box);
	return BLI_rctf_size_x(&box) * xa;
}

float blf_font_height(FontBLF *font, const char *str, size_t len)
{
	float ya;
	rctf box;

	if (font->flags & BLF_ASPECT)
		ya = font->aspect[1];
	else
		ya = 1.0f;

	blf_font_boundbox(font, str, len, &box);
	return BLI_rctf_size_y(&box) * ya;
}

float blf_font_fixed_width(FontBLF *font)
{
	const unsigned int c = ' ';
	GlyphBLF *g = blf_glyph_search(font->glyph_cache, c);
	if (!g) {
		g = blf_glyph_add(font, FT_Get_Char_Index(font->face, c), c);

		/* if we don't find the glyph. */
		if (!g) {
			return 0.0f;
		}
	}

	return g->advance;
}

void blf_font_free(FontBLF *font)
{
	GlyphCacheBLF *gc;

	font->glyph_cache = NULL;
	while ((gc = BLI_pophead(&font->cache))) {
		blf_glyph_cache_free(gc);
	}

	FT_Done_Face(font->face);
	if (font->filename)
		MEM_freeN(font->filename);
	if (font->name)
		MEM_freeN(font->name);
	MEM_freeN(font);
}

static void blf_font_fill(FontBLF *font)
{
	unsigned int i;

	font->aspect[0] = 1.0f;
	font->aspect[1] = 1.0f;
	font->aspect[2] = 1.0f;
	font->pos[0] = 0.0f;
	font->pos[1] = 0.0f;
	font->angle = 0.0f;

	for (i = 0; i < 16; i++)
		font->m[i] = 0;

	font->clip_rec.xmin = 0.0f;
	font->clip_rec.xmax = 0.0f;
	font->clip_rec.ymin = 0.0f;
	font->clip_rec.ymax = 0.0f;
	font->flags = 0;
	font->dpi = 0;
	font->size = 0;
	BLI_listbase_clear(&font->cache);
	font->glyph_cache = NULL;
	font->blur = 0;
	font->max_tex_size = -1;

	font->buf_info.fbuf = NULL;
	font->buf_info.cbuf = NULL;
	font->buf_info.w = 0;
	font->buf_info.h = 0;
	font->buf_info.ch = 0;
	font->buf_info.col[0] = 0;
	font->buf_info.col[1] = 0;
	font->buf_info.col[2] = 0;
	font->buf_info.col[3] = 0;

	font->ft_lib = ft_lib;
	font->ft_lib_mutex = &ft_lib_mutex;
}

FontBLF *blf_font_new(const char *name, const char *filename)
{
	FontBLF *font;
	FT_Error err;
	char *mfile;

	font = (FontBLF *)MEM_callocN(sizeof(FontBLF), "blf_font_new");
	err = FT_New_Face(ft_lib, filename, 0, &font->face);
	if (err) {
		MEM_freeN(font);
		return NULL;
	}

	err = FT_Select_Charmap(font->face, ft_encoding_unicode);
	if (err) {
		printf("Can't set the unicode character map!\n");
		FT_Done_Face(font->face);
		MEM_freeN(font);
		return NULL;
	}

	mfile = blf_dir_metrics_search(filename);
	if (mfile) {
		err = FT_Attach_File(font->face, mfile);
		if (err) {
			fprintf(stderr, "FT_Attach_File failed to load '%s' with error %d\n", filename, (int)err);
		}
		MEM_freeN(mfile);
	}

	font->name = BLI_strdup(name);
	font->filename = BLI_strdup(filename);
	blf_font_fill(font);
	return font;
}

void blf_font_attach_from_mem(FontBLF *font, const unsigned char *mem, int mem_size)
{
	FT_Open_Args open;

	open.flags = FT_OPEN_MEMORY;
	open.memory_base = (const FT_Byte *)mem;
	open.memory_size = mem_size;
	FT_Attach_Stream(font->face, &open);
}

FontBLF *blf_font_new_from_mem(const char *name, const unsigned char *mem, int mem_size)
{
	FontBLF *font;
	FT_Error err;

	font = (FontBLF *)MEM_callocN(sizeof(FontBLF), "blf_font_new_from_mem");
	err = FT_New_Memory_Face(ft_lib, mem, mem_size, 0, &font->face);
	if (err) {
		MEM_freeN(font);
		return NULL;
	}

	err = FT_Select_Charmap(font->face, ft_encoding_unicode);
	if (err) {
		printf("Can't set the unicode character map!\n");
		FT_Done_Face(font->face);
		MEM_freeN(font);
		return NULL;
	}

	font->name = BLI_strdup(name);
	font->filename = NULL;
	blf_font_fill(font);
	return font;
}

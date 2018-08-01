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

#include "UI_interface.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_batch.h"

#include "blf_internal_types.h"
#include "blf_internal.h"

#include "BLI_strict_flags.h"

#ifdef WIN32
#  define FT_New_Face FT_New_Face__win32_compat
#endif

/* Batching buffer for drawing. */
BatchBLF g_batch;

/* freetype2 handle ONLY for this file!. */
static FT_Library ft_lib;
static SpinLock ft_lib_mutex;

/* -------------------------------------------------------------------- */
/** \name Glyph Batching
 * \{ */
/**
 * Drawcalls are precious! make them count!
 * Since most of the Text elems are not covered by other UI elements, we can
 * group some strings together and render them in one drawcall. This behaviour
 * is on demand only, between BLF_batch_start() and BLF_batch_end().
 **/
static void blf_batch_draw_init(void)
{
	GPUVertFormat format = {0};
	g_batch.pos_loc = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
	g_batch.tex_loc = GPU_vertformat_attr_add(&format, "tex", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
	g_batch.col_loc = GPU_vertformat_attr_add(&format, "col", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

	g_batch.verts = GPU_vertbuf_create_with_format_ex(&format, GPU_USAGE_STREAM);
	GPU_vertbuf_data_alloc(g_batch.verts, BLF_BATCH_DRAW_LEN_MAX);

	GPU_vertbuf_attr_get_raw_data(g_batch.verts, g_batch.pos_loc, &g_batch.pos_step);
	GPU_vertbuf_attr_get_raw_data(g_batch.verts, g_batch.tex_loc, &g_batch.tex_step);
	GPU_vertbuf_attr_get_raw_data(g_batch.verts, g_batch.col_loc, &g_batch.col_step);
	g_batch.glyph_len = 0;

	g_batch.batch = GPU_batch_create_ex(GPU_PRIM_POINTS, g_batch.verts, NULL, GPU_BATCH_OWNS_VBO);
}

static void blf_batch_draw_exit(void)
{
	GPU_BATCH_DISCARD_SAFE(g_batch.batch);
}

void blf_batch_draw_vao_clear(void)
{
	if (g_batch.batch) {
		GPU_batch_vao_cache_clear(g_batch.batch);
	}
}

void blf_batch_draw_begin(FontBLF *font)
{
	if (g_batch.batch == NULL) {
		blf_batch_draw_init();
	}

	const bool font_changed = (g_batch.font != font);
	const bool simple_shader = ((font->flags & (BLF_ROTATION | BLF_MATRIX | BLF_ASPECT)) == 0);
	const bool shader_changed = (simple_shader != g_batch.simple_shader);

	g_batch.active = g_batch.enabled && simple_shader;

	if (simple_shader) {
		/* Offset is applied to each glyph. */
		g_batch.ofs[0] = floorf(font->pos[0]);
		g_batch.ofs[1] = floorf(font->pos[1]);
	}
	else {
		/* Offset is baked in modelview mat. */
		zero_v2(g_batch.ofs);
	}

	if (g_batch.active) {
		float gpumat[4][4];
		GPU_matrix_model_view_get(gpumat);

		bool mat_changed = (memcmp(gpumat, g_batch.mat, sizeof(g_batch.mat)) != 0);

		if (mat_changed) {
			/* Modelviewmat is no longer the same.
			 * Flush cache but with the previous mat. */
			GPU_matrix_push();
			GPU_matrix_set(g_batch.mat);
		}

		/* flush cache if config is not the same. */
		if (mat_changed || font_changed || shader_changed) {
			blf_batch_draw();
			g_batch.simple_shader = simple_shader;
			g_batch.font = font;
		}
		else {
			/* Nothing changed continue batching. */
			return;
		}

		if (mat_changed) {
			GPU_matrix_pop();
			/* Save for next memcmp. */
			memcpy(g_batch.mat, gpumat, sizeof(g_batch.mat));
		}
	}
	else {
		/* flush cache */
		blf_batch_draw();
		g_batch.font = font;
		g_batch.simple_shader = simple_shader;
	}
}

void blf_batch_draw(void)
{
	if (g_batch.glyph_len == 0)
		return;

	GPU_blend(true);
	GPU_blend_set_func_separate(GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

	/* We need to flush widget base first to ensure correct ordering. */
	UI_widgetbase_draw_cache_flush();

	GPU_texture_bind(g_batch.tex_bind_state, 0);
	GPU_vertbuf_vertex_count_set(g_batch.verts, g_batch.glyph_len);
	GPU_vertbuf_use(g_batch.verts); /* send data */

	GPUBuiltinShader shader = (g_batch.simple_shader) ? GPU_SHADER_TEXT_SIMPLE : GPU_SHADER_TEXT;
	GPU_batch_program_set_builtin(g_batch.batch, shader);
	GPU_batch_uniform_1i(g_batch.batch, "glyph", 0);
	GPU_batch_draw(g_batch.batch);

	GPU_blend(false);

	/* restart to 1st vertex data pointers */
	GPU_vertbuf_attr_get_raw_data(g_batch.verts, g_batch.pos_loc, &g_batch.pos_step);
	GPU_vertbuf_attr_get_raw_data(g_batch.verts, g_batch.tex_loc, &g_batch.tex_step);
	GPU_vertbuf_attr_get_raw_data(g_batch.verts, g_batch.col_loc, &g_batch.col_step);
	g_batch.glyph_len = 0;
}

static void blf_batch_draw_end(void)
{
	if (!g_batch.active) {
		blf_batch_draw();
	}
}

/** \} */

/* -------------------------------------------------------------------- */

int blf_font_init(void)
{
	memset(&g_batch, 0, sizeof(g_batch));
	BLI_spin_init(&ft_lib_mutex);
	return FT_Init_FreeType(&ft_lib);
}

void blf_font_exit(void)
{
	FT_Done_FreeType(ft_lib);
	BLI_spin_end(&ft_lib_mutex);
	blf_batch_draw_exit();
}

void blf_font_size(FontBLF *font, unsigned int size, unsigned int dpi)
{
	GlyphCacheBLF *gc;
	FT_Error err;

	gc = blf_glyph_cache_find(font, size, dpi);
	if (gc) {
		font->glyph_cache = gc;
		/* Optimization: do not call FT_Set_Char_Size if size did not change. */
		if (font->size == size && font->dpi == dpi)
			return;
	}

	err = FT_Set_Char_Size(font->face, 0, (FT_F26Dot6)(size * 64), dpi, dpi);
	if (err) {
		/* FIXME: here we can go through the fixed size and choice a close one */
		printf("The current font don't support the size, %u and dpi, %u\n", size, dpi);
		return;
	}

	font->size = size;
	font->dpi = dpi;

	if (!gc) {
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

static void blf_font_ensure_ascii_kerning(FontBLF *font, const FT_UInt kern_mode)
{
	KerningCacheBLF *kc = font->kerning_cache;

	font->kerning_mode = kern_mode;

	if (!kc || kc->mode != kern_mode) {
		font->kerning_cache = kc = blf_kerning_cache_find(font);
		if (!kc) {
			font->kerning_cache = kc = blf_kerning_cache_new(font);
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
	const bool _has_kerning = FT_HAS_KERNING((_font)->face) != 0;                \
	const FT_UInt _kern_mode = (_has_kerning == 0) ? 0 :                         \
	                         (((_font)->flags & BLF_KERNING_DEFAULT) ?           \
	                          ft_kerning_default : (FT_UInt)FT_KERNING_UNFITTED) \

/* Note,
 * blf_font_ensure_ascii_kerning(font, kern_mode); must be called before this macro */

#define BLF_KERNING_STEP_FAST(_font, _kern_mode, _g_prev, _g, _c_prev, _c, _pen_x) \
{                                                                                \
	if (_g_prev) {                                                               \
		FT_Vector _delta;                                                        \
		if (_c_prev < 0x80 && _c < 0x80) {                                       \
			_pen_x += (_font)->kerning_cache->table[_c][_c_prev];                 \
		}                                                                        \
		else if (FT_Get_Kerning((_font)->face,                                   \
		                        (_g_prev)->idx,                                  \
		                        (_g)->idx,                                       \
		                        _kern_mode,                                      \
		                        &(_delta)) == 0)                                 \
		{                                                                        \
			_pen_x += (int)_delta.x >> 6;                                        \
		}                                                                        \
	}                                                                            \
} (void)0

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

static void blf_font_draw_ex(
        FontBLF *font, const char *str, size_t len, struct ResultBLF *r_info,
        int pen_y)
{
	unsigned int c, c_prev = BLI_UTF8_ERR;
	GlyphBLF *g, *g_prev = NULL;
	int pen_x = 0;
	size_t i = 0;
	GlyphBLF **glyph_ascii_table = font->glyph_cache->glyph_ascii_table;

	if (len == 0) {
		/* early output, don't do any IMM OpenGL. */
		return;
	}

	BLF_KERNING_VARS(font, has_kerning, kern_mode);

	blf_font_ensure_ascii_table(font);
	blf_font_ensure_ascii_kerning(font, kern_mode);

	blf_batch_draw_begin(font);

	while ((i < len) && str[i]) {
		BLF_UTF8_NEXT_FAST(font, g, str, i, c, glyph_ascii_table);

		if (UNLIKELY(c == BLI_UTF8_ERR))
			break;
		if (UNLIKELY(g == NULL))
			continue;
		if (has_kerning)
			BLF_KERNING_STEP_FAST(font, kern_mode, g_prev, g, c_prev, c, pen_x);

		/* do not return this loop if clipped, we want every character tested */
		blf_glyph_render(font, g, (float)pen_x, (float)pen_y);

		pen_x += g->advance_i;
		g_prev = g;
		c_prev = c;
	}

	blf_batch_draw_end();

	if (r_info) {
		r_info->lines = 1;
		r_info->width = pen_x;
	}
}
void blf_font_draw(FontBLF *font, const char *str, size_t len, struct ResultBLF *r_info)
{
	blf_font_draw_ex(font, str, len, r_info, 0);
}

/* faster version of blf_font_draw, ascii only for view dimensions */
static void blf_font_draw_ascii_ex(
        FontBLF *font, const char *str, size_t len, struct ResultBLF *r_info,
        int pen_y)
{
	unsigned int c, c_prev = BLI_UTF8_ERR;
	GlyphBLF *g, *g_prev = NULL;
	int pen_x = 0;
	GlyphBLF **glyph_ascii_table = font->glyph_cache->glyph_ascii_table;

	BLF_KERNING_VARS(font, has_kerning, kern_mode);

	blf_font_ensure_ascii_table(font);
	blf_font_ensure_ascii_kerning(font, kern_mode);

	blf_batch_draw_begin(font);

	while ((c = *(str++)) && len--) {
		BLI_assert(c < 128);
		if ((g = glyph_ascii_table[c]) == NULL)
			continue;
		if (has_kerning)
			BLF_KERNING_STEP_FAST(font, kern_mode, g_prev, g, c_prev, c, pen_x);

		/* do not return this loop if clipped, we want every character tested */
		blf_glyph_render(font, g, (float)pen_x, (float)pen_y);

		pen_x += g->advance_i;
		g_prev = g;
		c_prev = c;
	}

	blf_batch_draw_end();

	if (r_info) {
		r_info->lines = 1;
		r_info->width = pen_x;
	}
}
void blf_font_draw_ascii(FontBLF *font, const char *str, size_t len, struct ResultBLF *r_info)
{
	blf_font_draw_ascii_ex(font, str, len, r_info, 0);
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

	blf_batch_draw_begin(font);

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

	blf_batch_draw_end();

	return columns;
}

/* Sanity checks are done by BLF_draw_buffer() */
static void blf_font_draw_buffer_ex(
        FontBLF *font, const char *str, size_t len, struct ResultBLF *r_info,
        int pen_y)
{
	unsigned int c, c_prev = BLI_UTF8_ERR;
	GlyphBLF *g, *g_prev = NULL;
	int pen_x = (int)font->pos[0];
	int pen_y_basis = (int)font->pos[1] + pen_y;
	size_t i = 0;
	GlyphBLF **glyph_ascii_table = font->glyph_cache->glyph_ascii_table;

	/* buffer specific vars */
	FontBufInfoBLF *buf_info = &font->buf_info;
	const float *b_col_float = buf_info->col_float;
	const unsigned char *b_col_char = buf_info->col_char;
	int chx, chy;
	int y, x;

	BLF_KERNING_VARS(font, has_kerning, kern_mode);

	blf_font_ensure_ascii_table(font);
	blf_font_ensure_ascii_kerning(font, kern_mode);

	/* another buffer specific call for color conversion */

	while ((i < len) && str[i]) {
		BLF_UTF8_NEXT_FAST(font, g, str, i, c, glyph_ascii_table);

		if (UNLIKELY(c == BLI_UTF8_ERR))
			break;
		if (UNLIKELY(g == NULL))
			continue;
		if (has_kerning)
			BLF_KERNING_STEP_FAST(font, kern_mode, g_prev, g, c_prev, c, pen_x);

		chx = pen_x + ((int)g->pos_x);
		chy = pen_y_basis + g->height;

		if (g->pitch < 0) {
			pen_y = pen_y_basis + (g->height - (int)g->pos_y);
		}
		else {
			pen_y = pen_y_basis - (g->height - (int)g->pos_y);
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
						const char a_byte = *(g->bitmap + x + (yb * g->pitch));
						if (a_byte) {
							const float a = (a_byte / 255.0f) * b_col_float[3];
							const size_t buf_ofs = (
							        ((size_t)(chx + x) + ((size_t)(pen_y + y) * (size_t)buf_info->w)) *
							        (size_t)buf_info->ch);
							float *fbuf = buf_info->fbuf + buf_ofs;

							if (a >= 1.0f) {
								fbuf[0] = b_col_float[0];
								fbuf[1] = b_col_float[1];
								fbuf[2] = b_col_float[2];
								fbuf[3] = 1.0f;
							}
							else {
								fbuf[0] = (b_col_float[0] * a) + (fbuf[0] * (1.0f - a));
								fbuf[1] = (b_col_float[1] * a) + (fbuf[1] * (1.0f - a));
								fbuf[2] = (b_col_float[2] * a) + (fbuf[2] * (1.0f - a));
								fbuf[3] = MIN2(fbuf[3] + a, 1.0f); /* clamp to 1.0 */
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
				for (y = ((chy >= 0) ? 0 : -chy); y < height_clip; y++) {
					for (x = ((chx >= 0) ? 0 : -chx); x < width_clip; x++) {
						const char a_byte = *(g->bitmap + x + (yb * g->pitch));

						if (a_byte) {
							const float a = (a_byte / 255.0f) * b_col_float[3];
							const size_t buf_ofs = (
							        ((size_t)(chx + x) + ((size_t)(pen_y + y) * (size_t)buf_info->w)) *
							        (size_t)buf_info->ch);
							unsigned char *cbuf = buf_info->cbuf + buf_ofs;

							if (a >= 1.0f) {
								cbuf[0] = b_col_char[0];
								cbuf[1] = b_col_char[1];
								cbuf[2] = b_col_char[2];
								cbuf[3] = 255;
							}
							else {
								cbuf[0] = (unsigned char)((b_col_char[0] * a) + (cbuf[0] * (1.0f - a)));
								cbuf[1] = (unsigned char)((b_col_char[1] * a) + (cbuf[1] * (1.0f - a)));
								cbuf[2] = (unsigned char)((b_col_char[2] * a) + (cbuf[2] * (1.0f - a)));
								cbuf[3] = (unsigned char)MIN2((int)cbuf[3] + (int)(a * 255), 255); /* clamp to 255 */
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
		c_prev = c;
	}

	if (r_info) {
		r_info->lines = 1;
		r_info->width = pen_x;
	}
}
void blf_font_draw_buffer(
        FontBLF *font, const char *str, size_t len, struct ResultBLF *r_info)
{
	blf_font_draw_buffer_ex(font, str, len, r_info, 0);
}

size_t blf_font_width_to_strlen(FontBLF *font, const char *str, size_t len, float width, float *r_width)
{
	unsigned int c;
	GlyphBLF *g, *g_prev = NULL;
	FT_Vector delta;
	int pen_x = 0;
	size_t i = 0, i_prev;
	GlyphBLF **glyph_ascii_table = font->glyph_cache->glyph_ascii_table;
	const int width_i = (int)width;
	int width_new;

	BLF_KERNING_VARS(font, has_kerning, kern_mode);

	blf_font_ensure_ascii_table(font);

	while ((void)(i_prev = i),
	       (void)(width_new = pen_x),
	       ((i < len) && str[i]))
	{
		BLF_UTF8_NEXT_FAST(font, g, str, i, c, glyph_ascii_table);

		if (UNLIKELY(c == BLI_UTF8_ERR))
			break;
		if (UNLIKELY(g == NULL))
			continue;
		if (has_kerning)
			BLF_KERNING_STEP(font, kern_mode, g_prev, g, delta, pen_x);

		pen_x += g->advance_i;

		if (width_i <= pen_x) {
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

static void blf_font_boundbox_ex(
        FontBLF *font, const char *str, size_t len, rctf *box, struct ResultBLF *r_info,
        int pen_y)
{
	unsigned int c, c_prev = BLI_UTF8_ERR;
	GlyphBLF *g, *g_prev = NULL;
	int pen_x = 0;
	size_t i = 0;
	GlyphBLF **glyph_ascii_table = font->glyph_cache->glyph_ascii_table;

	rctf gbox;

	BLF_KERNING_VARS(font, has_kerning, kern_mode);

	box->xmin = 32000.0f;
	box->xmax = -32000.0f;
	box->ymin = 32000.0f;
	box->ymax = -32000.0f;

	blf_font_ensure_ascii_table(font);
	blf_font_ensure_ascii_kerning(font, kern_mode);

	while ((i < len) && str[i]) {
		BLF_UTF8_NEXT_FAST(font, g, str, i, c, glyph_ascii_table);

		if (UNLIKELY(c == BLI_UTF8_ERR))
			break;
		if (UNLIKELY(g == NULL))
			continue;
		if (has_kerning)
			BLF_KERNING_STEP_FAST(font, kern_mode, g_prev, g, c_prev, c, pen_x);

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
		c_prev = c;
	}

	if (box->xmin > box->xmax) {
		box->xmin = 0.0f;
		box->ymin = 0.0f;
		box->xmax = 0.0f;
		box->ymax = 0.0f;
	}

	if (r_info) {
		r_info->lines = 1;
		r_info->width = pen_x;
	}
}
void blf_font_boundbox(FontBLF *font, const char *str, size_t len, rctf *r_box, struct ResultBLF *r_info)
{
	blf_font_boundbox_ex(font, str, len, r_box, r_info, 0);
}


/* -------------------------------------------------------------------- */
/** \name Word-Wrap Support
 * \{ */


/**
 * Generic function to add word-wrap support for other existing functions.
 *
 * Wraps on spaces and respects newlines.
 * Intentionally ignores non-unix newlines, tabs and more advanced text formatting.
 *
 * \note If we want rich text - we better have a higher level API to handle that
 * (color, bold, switching fonts... etc).
 */
static void blf_font_wrap_apply(
        FontBLF *font, const char *str, size_t len, struct ResultBLF *r_info,
        void (*callback)(FontBLF *font, const char *str, size_t len, int pen_y, void *userdata),
        void *userdata)
{
	unsigned int c;
	GlyphBLF *g, *g_prev = NULL;
	FT_Vector delta;
	int pen_x = 0, pen_y = 0;
	size_t i = 0;
	GlyphBLF **glyph_ascii_table = font->glyph_cache->glyph_ascii_table;
	int lines = 0;
	int pen_x_next = 0;

	BLF_KERNING_VARS(font, has_kerning, kern_mode);

	struct WordWrapVars {
		int wrap_width;
		size_t start, last[2];
	} wrap = {font->wrap_width != -1 ? font->wrap_width : INT_MAX, 0, {0, 0}};

	blf_font_ensure_ascii_table(font);
	// printf("%s wrapping (%d, %d) `%s`:\n", __func__, len, strlen(str), str);
	while ((i < len) && str[i]) {

		/* wrap vars */
		size_t i_curr = i;
		bool do_draw = false;

		BLF_UTF8_NEXT_FAST(font, g, str, i, c, glyph_ascii_table);

		if (UNLIKELY(c == BLI_UTF8_ERR))
			break;
		if (UNLIKELY(g == NULL))
			continue;
		if (has_kerning)
			BLF_KERNING_STEP(font, kern_mode, g_prev, g, delta, pen_x);

		/**
		 * Implementation Detail (utf8).
		 *
		 * Take care with single byte offsets here,
		 * since this is utf8 we can't be sure a single byte is a single character.
		 *
		 * This is _only_ done when we know for sure the character is ascii (newline or a space).
		 */
		pen_x_next = pen_x + g->advance_i;
		if (UNLIKELY((pen_x_next >= wrap.wrap_width) && (wrap.start != wrap.last[0]))) {
			do_draw = true;
		}
		else if (UNLIKELY(((i < len) && str[i]) == 0)) {
			/* need check here for trailing newline, else we draw it */
			wrap.last[0] = i + ((g->c != '\n') ? 1 : 0);
			wrap.last[1] = i;
			do_draw = true;
		}
		else if (UNLIKELY(g->c == '\n')) {
			wrap.last[0] = i_curr + 1;
			wrap.last[1] = i;
			do_draw = true;
		}
		else if (UNLIKELY(g->c != ' ' && (g_prev ? g_prev->c == ' ' : false))) {
			wrap.last[0] = i_curr;
			wrap.last[1] = i_curr;
		}

		if (UNLIKELY(do_draw)) {
			// printf("(%03d..%03d)  `%.*s`\n",
			//        wrap.start, wrap.last[0], (wrap.last[0] - wrap.start) - 1, &str[wrap.start]);

			callback(font, &str[wrap.start], (wrap.last[0] - wrap.start) - 1, pen_y, userdata);
			wrap.start = wrap.last[0];
			i = wrap.last[1];
			pen_x = 0;
			pen_y -= font->glyph_cache->glyph_height_max;
			g_prev = NULL;
			lines += 1;
			continue;
		}

		pen_x = pen_x_next;
		g_prev = g;
	}

	// printf("done! lines: %d, width, %d\n", lines, pen_x_next);

	if (r_info) {
		r_info->lines = lines;
		/* width of last line only (with wrapped lines) */
		r_info->width = pen_x_next;
	}
}

/* blf_font_draw__wrap */
static void blf_font_draw__wrap_cb(FontBLF *font, const char *str, size_t len, int pen_y, void *UNUSED(userdata))
{
	blf_font_draw_ex(font, str, len, NULL, pen_y);
}
void blf_font_draw__wrap(FontBLF *font, const char *str, size_t len, struct ResultBLF *r_info)
{
	blf_font_wrap_apply(font, str, len, r_info, blf_font_draw__wrap_cb, NULL);
}

/* blf_font_boundbox__wrap */
static void blf_font_boundbox_wrap_cb(FontBLF *font, const char *str, size_t len, int pen_y, void *userdata)
{
	rctf *box = userdata;
	rctf box_single;

	blf_font_boundbox_ex(font, str, len, &box_single, NULL, pen_y);
	BLI_rctf_union(box, &box_single);
}
void blf_font_boundbox__wrap(FontBLF *font, const char *str, size_t len, rctf *box, struct ResultBLF *r_info)
{
	box->xmin = 32000.0f;
	box->xmax = -32000.0f;
	box->ymin = 32000.0f;
	box->ymax = -32000.0f;

	blf_font_wrap_apply(font, str, len, r_info, blf_font_boundbox_wrap_cb, box);
}

/* blf_font_draw_buffer__wrap */
static void blf_font_draw_buffer__wrap_cb(FontBLF *font, const char *str, size_t len, int pen_y, void *UNUSED(userdata))
{
	blf_font_draw_buffer_ex(font, str, len, NULL, pen_y);
}
void blf_font_draw_buffer__wrap(FontBLF *font, const char *str, size_t len, struct ResultBLF *r_info)
{
	blf_font_wrap_apply(font, str, len, r_info, blf_font_draw_buffer__wrap_cb, NULL);
}

/** \} */


void blf_font_width_and_height(
        FontBLF *font, const char *str, size_t len,
        float *r_width, float *r_height, struct ResultBLF *r_info)
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

	if (font->flags & BLF_WORD_WRAP) {
		blf_font_boundbox__wrap(font, str, len, &box, r_info);
	}
	else {
		blf_font_boundbox(font, str, len, &box, r_info);
	}
	*r_width  = (BLI_rctf_size_x(&box) * xa);
	*r_height = (BLI_rctf_size_y(&box) * ya);
}

float blf_font_width(FontBLF *font, const char *str, size_t len, struct ResultBLF *r_info)
{
	float xa;
	rctf box;

	if (font->flags & BLF_ASPECT)
		xa = font->aspect[0];
	else
		xa = 1.0f;

	if (font->flags & BLF_WORD_WRAP) {
		blf_font_boundbox__wrap(font, str, len, &box, r_info);
	}
	else {
		blf_font_boundbox(font, str, len, &box, r_info);
	}
	return BLI_rctf_size_x(&box) * xa;
}

float blf_font_height(FontBLF *font, const char *str, size_t len, struct ResultBLF *r_info)
{
	float ya;
	rctf box;

	if (font->flags & BLF_ASPECT)
		ya = font->aspect[1];
	else
		ya = 1.0f;

	if (font->flags & BLF_WORD_WRAP) {
		blf_font_boundbox__wrap(font, str, len, &box, r_info);
	}
	else {
		blf_font_boundbox(font, str, len, &box, r_info);
	}
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

int blf_font_count_missing_chars(FontBLF *font, const char *str, const size_t len, int *r_tot_chars)
{
	int missing = 0;
	size_t i = 0;

	*r_tot_chars = 0;
	while (i < len) {
		unsigned int c;

		if ((c = str[i]) < 0x80) {
			i++;
		}
		else if ((c = BLI_str_utf8_as_unicode_step(str, &i)) != BLI_UTF8_ERR) {
			if (FT_Get_Char_Index((font)->face, c) == 0) {
				missing++;
			}
		}
		(*r_tot_chars)++;
	}
	return missing;
}

void blf_font_free(FontBLF *font)
{
	GlyphCacheBLF *gc;

	font->glyph_cache = NULL;
	while ((gc = BLI_pophead(&font->cache))) {
		blf_glyph_cache_free(gc);
	}

	blf_kerning_cache_clear(font);

	FT_Done_Face(font->face);
	if (font->filename)
		MEM_freeN(font->filename);
	if (font->name)
		MEM_freeN(font->name);
	MEM_freeN(font);
}

static void blf_font_fill(FontBLF *font)
{
	font->aspect[0] = 1.0f;
	font->aspect[1] = 1.0f;
	font->aspect[2] = 1.0f;
	font->pos[0] = 0.0f;
	font->pos[1] = 0.0f;
	font->angle = 0.0f;

	for (int i = 0; i < 16; i++)
		font->m[i] = 0;

	/* annoying bright color so we can see where to add BLF_color calls */
	font->color[0] = 255;
	font->color[1] = 255;
	font->color[2] = 0;
	font->color[3] = 255;

	font->clip_rec.xmin = 0.0f;
	font->clip_rec.xmax = 0.0f;
	font->clip_rec.ymin = 0.0f;
	font->clip_rec.ymax = 0.0f;
	font->flags = 0;
	font->dpi = 0;
	font->size = 0;
	BLI_listbase_clear(&font->cache);
	BLI_listbase_clear(&font->kerning_caches);
	font->glyph_cache = NULL;
	font->kerning_cache = NULL;
#if BLF_BLUR_ENABLE
	font->blur = 0;
#endif
	font->tex_size_max = -1;

	font->buf_info.fbuf = NULL;
	font->buf_info.cbuf = NULL;
	font->buf_info.w = 0;
	font->buf_info.h = 0;
	font->buf_info.ch = 0;
	font->buf_info.col_init[0] = 0;
	font->buf_info.col_init[1] = 0;
	font->buf_info.col_init[2] = 0;
	font->buf_info.col_init[3] = 0;

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

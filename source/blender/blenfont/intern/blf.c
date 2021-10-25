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

/** \file blender/blenfont/intern/blf.c
 *  \ingroup blf
 *
 * Main BlenFont (BLF) API, public functions for font handling.
 *
 * Wraps OpenGL and FreeType.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_vec_types.h"

#include "BLI_math.h"
#include "BLI_threads.h"

#include "BIF_gl.h"
#include "BLF_api.h"

#include "IMB_colormanagement.h"

#ifndef BLF_STANDALONE
#include "GPU_basic_shader.h"
#endif

#include "blf_internal_types.h"
#include "blf_internal.h"

/* Max number of font in memory.
 * Take care that now every font have a glyph cache per size/dpi,
 * so we don't need load the same font with different size, just
 * load one and call BLF_size.
 */
#define BLF_MAX_FONT 16

/* call BLF_default_set first! */
#define ASSERT_DEFAULT_SET BLI_assert(global_font_default != -1)

#define BLF_RESULT_CHECK_INIT(r_info) \
if (r_info) { \
	memset(r_info, 0, sizeof(*(r_info))); \
} ((void)0)

/* Font array. */
static FontBLF *global_font[BLF_MAX_FONT] = {NULL};

/* Default size and dpi, for BLF_draw_default. */
static int global_font_default = -1;
static int global_font_points = 11;
static int global_font_dpi = 72;

/* XXX, should these be made into global_font_'s too? */
int blf_mono_font = -1;
int blf_mono_font_render = -1;

static FontBLF *blf_get(int fontid)
{
	if (fontid >= 0 && fontid < BLF_MAX_FONT)
		return global_font[fontid];
	return NULL;
}

int BLF_init(void)
{
	int i;

	for (i = 0; i < BLF_MAX_FONT; i++)
		global_font[i] = NULL;

	global_font_points = 11;
	global_font_dpi = 72;

	return blf_font_init();
}

void BLF_default_dpi(int dpi)
{
	global_font_dpi = dpi;
}

void BLF_exit(void)
{
	FontBLF *font;
	int i;

	for (i = 0; i < BLF_MAX_FONT; i++) {
		font = global_font[i];
		if (font) {
			blf_font_free(font);
			global_font[i] = NULL;
		}
	}

	blf_font_exit();
}

void BLF_cache_clear(void)
{
	FontBLF *font;
	int i;

	for (i = 0; i < BLF_MAX_FONT; i++) {
		font = global_font[i];
		if (font)
			blf_glyph_cache_clear(font);
	}
}

static int blf_search(const char *name)
{
	FontBLF *font;
	int i;

	for (i = 0; i < BLF_MAX_FONT; i++) {
		font = global_font[i];
		if (font && (STREQ(font->name, name)))
			return i;
	}

	return -1;
}

static int blf_search_available(void)
{
	int i;

	for (i = 0; i < BLF_MAX_FONT; i++)
		if (!global_font[i])
			return i;
	
	return -1;
}

void BLF_default_set(int fontid)
{
	FontBLF *font = blf_get(fontid);
	if (font || fontid == -1) {
		global_font_default = fontid;
	}
}

int BLF_load(const char *name)
{
	FontBLF *font;
	char *filename;
	int i;

	/* check if we already load this font. */
	i = blf_search(name);
	if (i >= 0) {
		/*font = global_font[i];*/ /*UNUSED*/
		return i;
	}

	i = blf_search_available();
	if (i == -1) {
		printf("Too many fonts!!!\n");
		return -1;
	}

	filename = blf_dir_search(name);
	if (!filename) {
		printf("Can't find font: %s\n", name);
		return -1;
	}

	font = blf_font_new(name, filename);
	MEM_freeN(filename);

	if (!font) {
		printf("Can't load font: %s\n", name);
		return -1;
	}

	global_font[i] = font;
	return i;
}

int BLF_load_unique(const char *name)
{
	FontBLF *font;
	char *filename;
	int i;

	/* Don't search in the cache!! make a new
	 * object font, this is for keep fonts threads safe.
	 */
	i = blf_search_available();
	if (i == -1) {
		printf("Too many fonts!!!\n");
		return -1;
	}

	filename = blf_dir_search(name);
	if (!filename) {
		printf("Can't find font: %s\n", name);
		return -1;
	}

	font = blf_font_new(name, filename);
	MEM_freeN(filename);

	if (!font) {
		printf("Can't load font: %s\n", name);
		return -1;
	}

	global_font[i] = font;
	return i;
}

void BLF_metrics_attach(int fontid, unsigned char *mem, int mem_size)
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		blf_font_attach_from_mem(font, mem, mem_size);
	}
}

int BLF_load_mem(const char *name, const unsigned char *mem, int mem_size)
{
	FontBLF *font;
	int i;

	i = blf_search(name);
	if (i >= 0) {
		/*font = global_font[i];*/ /*UNUSED*/
		return i;
	}

	i = blf_search_available();
	if (i == -1) {
		printf("Too many fonts!!!\n");
		return -1;
	}

	if (!mem_size) {
		printf("Can't load font: %s from memory!!\n", name);
		return -1;
	}

	font = blf_font_new_from_mem(name, mem, mem_size);
	if (!font) {
		printf("Can't load font: %s from memory!!\n", name);
		return -1;
	}

	global_font[i] = font;
	return i;
}

int BLF_load_mem_unique(const char *name, const unsigned char *mem, int mem_size)
{
	FontBLF *font;
	int i;

	/*
	 * Don't search in the cache, make a new object font!
	 * this is to keep the font thread safe.
	 */
	i = blf_search_available();
	if (i == -1) {
		printf("Too many fonts!!!\n");
		return -1;
	}

	if (!mem_size) {
		printf("Can't load font: %s from memory!!\n", name);
		return -1;
	}

	font = blf_font_new_from_mem(name, mem, mem_size);
	if (!font) {
		printf("Can't load font: %s from memory!!\n", name);
		return -1;
	}

	global_font[i] = font;
	return i;
}

void BLF_unload(const char *name)
{
	FontBLF *font;
	int i;

	for (i = 0; i < BLF_MAX_FONT; i++) {
		font = global_font[i];

		if (font && (STREQ(font->name, name))) {
			blf_font_free(font);
			global_font[i] = NULL;
		}
	}
}

void BLF_unload_id(int fontid)
{
	FontBLF *font = blf_get(fontid);
	if (font) {
		blf_font_free(font);
		global_font[fontid] = NULL;
	}
}

void BLF_enable(int fontid, int option)
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		font->flags |= option;
	}
}

void BLF_disable(int fontid, int option)
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		font->flags &= ~option;
	}
}

void BLF_enable_default(int option)
{
	FontBLF *font = blf_get(global_font_default);

	if (font) {
		font->flags |= option;
	}
}

void BLF_disable_default(int option)
{
	FontBLF *font = blf_get(global_font_default);

	if (font) {
		font->flags &= ~option;
	}
}

void BLF_aspect(int fontid, float x, float y, float z)
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		font->aspect[0] = x;
		font->aspect[1] = y;
		font->aspect[2] = z;
	}
}

void BLF_matrix(int fontid, const float m[16])
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		memcpy(font->m, m, sizeof(font->m));
	}
}

void BLF_position(int fontid, float x, float y, float z)
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		float xa, ya, za;
		float remainder;

		if (font->flags & BLF_ASPECT) {
			xa = font->aspect[0];
			ya = font->aspect[1];
			za = font->aspect[2];
		}
		else {
			xa = 1.0f;
			ya = 1.0f;
			za = 1.0f;
		}

		remainder = x - floorf(x);
		if (remainder > 0.4f && remainder < 0.6f) {
			if (remainder < 0.5f)
				x -= 0.1f * xa;
			else
				x += 0.1f * xa;
		}

		remainder = y - floorf(y);
		if (remainder > 0.4f && remainder < 0.6f) {
			if (remainder < 0.5f)
				y -= 0.1f * ya;
			else
				y += 0.1f * ya;
		}

		remainder = z - floorf(z);
		if (remainder > 0.4f && remainder < 0.6f) {
			if (remainder < 0.5f)
				z -= 0.1f * za;
			else
				z += 0.1f * za;
		}

		font->pos[0] = x;
		font->pos[1] = y;
		font->pos[2] = z;
	}
}

void BLF_size(int fontid, int size, int dpi)
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		blf_font_size(font, size, dpi);
	}
}

void BLF_blur(int fontid, int size)
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		font->blur = size;
	}
}

void BLF_draw_default(float x, float y, float z, const char *str, size_t len)
{
	ASSERT_DEFAULT_SET;

	BLF_size(global_font_default, global_font_points, global_font_dpi);
	BLF_position(global_font_default, x, y, z);
	BLF_draw(global_font_default, str, len);
}

/* same as above but call 'BLF_draw_ascii' */
void BLF_draw_default_ascii(float x, float y, float z, const char *str, size_t len)
{
	ASSERT_DEFAULT_SET;

	BLF_size(global_font_default, global_font_points, global_font_dpi);
	BLF_position(global_font_default, x, y, z);
	BLF_draw_ascii(global_font_default, str, len); /* XXX, use real length */
}

void BLF_rotation_default(float angle)
{
	FontBLF *font = blf_get(global_font_default);

	if (font) {
		font->angle = angle;
	}
}

static void blf_draw_gl__start(FontBLF *font, GLint *mode)
{
	/*
	 * The pixmap alignment hack is handle
	 * in BLF_position (old ui_rasterpos_safe).
	 */

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifndef BLF_STANDALONE
	GPU_basic_shader_bind(GPU_SHADER_TEXTURE_2D | GPU_SHADER_USE_COLOR);
#endif

	/* Save the current matrix mode. */
	glGetIntegerv(GL_MATRIX_MODE, mode);

	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	if (font->flags & BLF_MATRIX)
		glMultMatrixf(font->m);

	glTranslate3fv(font->pos);

	if (font->flags & BLF_ASPECT)
		glScalef(font->aspect[0], font->aspect[1], font->aspect[2]);

	if (font->flags & BLF_ROTATION)  /* radians -> degrees */
		glRotatef(font->angle * (float)(180.0 / M_PI), 0.0f, 0.0f, 1.0f);

	if (font->shadow || font->blur)
		glGetFloatv(GL_CURRENT_COLOR, font->orig_col);

	/* always bind the texture for the first glyph */
	font->tex_bind_state = -1;
}

static void blf_draw_gl__end(GLint mode)
{
	glMatrixMode(GL_TEXTURE);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if (mode != GL_MODELVIEW)
		glMatrixMode(mode);

#ifndef BLF_STANDALONE
	GPU_basic_shader_bind(GPU_SHADER_USE_COLOR);
#endif
	glDisable(GL_BLEND);
}

void BLF_draw_ex(
        int fontid, const char *str, size_t len,
        struct ResultBLF *r_info)
{
	FontBLF *font = blf_get(fontid);
	GLint mode;

	BLF_RESULT_CHECK_INIT(r_info);

	if (font && font->glyph_cache) {
		blf_draw_gl__start(font, &mode);
		if (font->flags & BLF_WORD_WRAP) {
			blf_font_draw__wrap(font, str, len, r_info);
		}
		else {
			blf_font_draw(font, str, len, r_info);
		}
		blf_draw_gl__end(mode);
	}
}
void BLF_draw(int fontid, const char *str, size_t len)
{
	BLF_draw_ex(fontid, str, len, NULL);
}

void BLF_draw_ascii_ex(
        int fontid, const char *str, size_t len,
        struct ResultBLF *r_info)
{
	FontBLF *font = blf_get(fontid);
	GLint mode;

	BLF_RESULT_CHECK_INIT(r_info);

	if (font && font->glyph_cache) {
		blf_draw_gl__start(font, &mode);
		if (font->flags & BLF_WORD_WRAP) {
			/* use non-ascii draw function for word-wrap */
			blf_font_draw__wrap(font, str, len, r_info);
		}
		else {
			blf_font_draw_ascii(font, str, len, r_info);
		}
		blf_draw_gl__end(mode);
	}
}
void BLF_draw_ascii(int fontid, const char *str, size_t len)
{
	BLF_draw_ascii_ex(fontid, str, len, NULL);
}

int BLF_draw_mono(int fontid, const char *str, size_t len, int cwidth)
{
	FontBLF *font = blf_get(fontid);
	GLint mode;
	int columns = 0;

	if (font && font->glyph_cache) {
		blf_draw_gl__start(font, &mode);
		columns = blf_font_draw_mono(font, str, len, cwidth);
		blf_draw_gl__end(mode);
	}

	return columns;
}

size_t BLF_width_to_strlen(int fontid, const char *str, size_t len, float width, float *r_width)
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		const float xa = (font->flags & BLF_ASPECT) ? font->aspect[0] : 1.0f;
		size_t ret;
		ret = blf_font_width_to_strlen(font, str, len, width / xa, r_width);
		if (r_width) {
			*r_width *= xa;
		}
		return ret;
	}

	if (r_width) {
		*r_width = 0.0f;
	}
	return 0;
}

size_t BLF_width_to_rstrlen(int fontid, const char *str, size_t len, float width, float *r_width)
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		const float xa = (font->flags & BLF_ASPECT) ? font->aspect[0] : 1.0f;
		size_t ret;
		ret = blf_font_width_to_rstrlen(font, str, len, width / xa, r_width);
		if (r_width) {
			*r_width *= xa;
		}
		return ret;
	}

	if (r_width) {
		*r_width = 0.0f;
	}
	return 0;
}

void BLF_boundbox_ex(
        int fontid, const char *str, size_t len, rctf *r_box,
        struct ResultBLF *r_info)
{
	FontBLF *font = blf_get(fontid);

	BLF_RESULT_CHECK_INIT(r_info);

	if (font) {
		if (font->flags & BLF_WORD_WRAP) {
			blf_font_boundbox__wrap(font, str, len, r_box, r_info);
		}
		else {
			blf_font_boundbox(font, str, len, r_box, r_info);
		}
	}
}
void BLF_boundbox(int fontid, const char *str, size_t len, rctf *r_box)
{
	BLF_boundbox_ex(fontid, str, len, r_box, NULL);
}

void BLF_width_and_height(int fontid, const char *str, size_t len, float *r_width, float *r_height)
{
	FontBLF *font = blf_get(fontid);

	if (font && font->glyph_cache) {
		blf_font_width_and_height(font, str, len, r_width, r_height, NULL);
	}
	else {
		*r_width = *r_height = 0.0f;
	}
}

void BLF_width_and_height_default(const char *str, size_t len, float *r_width, float *r_height)
{
	ASSERT_DEFAULT_SET;

	BLF_size(global_font_default, global_font_points, global_font_dpi);
	BLF_width_and_height(global_font_default, str, len, r_width, r_height);
}

float BLF_width_ex(
        int fontid, const char *str, size_t len,
        struct ResultBLF *r_info)
{
	FontBLF *font = blf_get(fontid);

	BLF_RESULT_CHECK_INIT(r_info);

	if (font && font->glyph_cache) {
		return blf_font_width(font, str, len, r_info);
	}

	return 0.0f;
}
float BLF_width(int fontid, const char *str, size_t len)
{
	return BLF_width_ex(fontid, str, len, NULL);
}

float BLF_fixed_width(int fontid)
{
	FontBLF *font = blf_get(fontid);

	if (font && font->glyph_cache) {
		return blf_font_fixed_width(font);
	}

	return 0.0f;
}

float BLF_width_default(const char *str, size_t len)
{
	ASSERT_DEFAULT_SET;

	BLF_size(global_font_default, global_font_points, global_font_dpi);
	return BLF_width(global_font_default, str, len);
}

float BLF_height_ex(
        int fontid, const char *str, size_t len,
        struct ResultBLF *r_info)
{
	FontBLF *font = blf_get(fontid);

	BLF_RESULT_CHECK_INIT(r_info);

	if (font && font->glyph_cache) {
		return blf_font_height(font, str, len, r_info);
	}

	return 0.0f;
}
float BLF_height(int fontid, const char *str, size_t len)
{
	return BLF_height_ex(fontid, str, len, NULL);
}

int BLF_height_max(int fontid)
{
	FontBLF *font = blf_get(fontid);

	if (font && font->glyph_cache) {
		return font->glyph_cache->max_glyph_height;
	}

	return 0;
}

float BLF_width_max(int fontid)
{
	FontBLF *font = blf_get(fontid);

	if (font && font->glyph_cache) {
		return font->glyph_cache->max_glyph_width;
	}

	return 0.0f;
}

float BLF_descender(int fontid)
{
	FontBLF *font = blf_get(fontid);

	if (font && font->glyph_cache) {
		return font->glyph_cache->descender;
	}

	return 0.0f;
}

float BLF_ascender(int fontid)
{
	FontBLF *font = blf_get(fontid);

	if (font && font->glyph_cache) {
		return font->glyph_cache->ascender;
	}

	return 0.0f;
}

float BLF_height_default(const char *str, size_t len)
{
	ASSERT_DEFAULT_SET;

	BLF_size(global_font_default, global_font_points, global_font_dpi);

	return BLF_height(global_font_default, str, len);
}

void BLF_rotation(int fontid, float angle)
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		font->angle = angle;
	}
}

void BLF_clipping(int fontid, float xmin, float ymin, float xmax, float ymax)
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		font->clip_rec.xmin = xmin;
		font->clip_rec.ymin = ymin;
		font->clip_rec.xmax = xmax;
		font->clip_rec.ymax = ymax;
	}
}

void BLF_clipping_default(float xmin, float ymin, float xmax, float ymax)
{
	FontBLF *font = blf_get(global_font_default);

	if (font) {
		font->clip_rec.xmin = xmin;
		font->clip_rec.ymin = ymin;
		font->clip_rec.xmax = xmax;
		font->clip_rec.ymax = ymax;
	}
}

void BLF_wordwrap(int fontid, int wrap_width)
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		font->wrap_width = wrap_width;
	}
}

void BLF_shadow(int fontid, int level, const float rgba[4])
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		font->shadow = level;
		copy_v4_v4(font->shadow_col, rgba);
	}
}

void BLF_shadow_offset(int fontid, int x, int y)
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		font->shadow_x = x;
		font->shadow_y = y;
	}
}

void BLF_buffer(int fontid, float *fbuf, unsigned char *cbuf, int w, int h, int nch, struct ColorManagedDisplay *display)
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		font->buf_info.fbuf = fbuf;
		font->buf_info.cbuf = cbuf;
		font->buf_info.w = w;
		font->buf_info.h = h;
		font->buf_info.ch = nch;
		font->buf_info.display = display;
	}
}

void BLF_buffer_col(int fontid, const float rgba[4])
{
	FontBLF *font = blf_get(fontid);

	if (font) {
		copy_v4_v4(font->buf_info.col_init, rgba);
	}
}


void blf_draw_buffer__start(FontBLF *font)
{
	FontBufInfoBLF *buf_info = &font->buf_info;

	buf_info->col_char[0] = buf_info->col_init[0] * 255;
	buf_info->col_char[1] = buf_info->col_init[1] * 255;
	buf_info->col_char[2] = buf_info->col_init[2] * 255;
	buf_info->col_char[3] = buf_info->col_init[3] * 255;

	if (buf_info->display) {
		copy_v4_v4(buf_info->col_float, buf_info->col_init);
		IMB_colormanagement_display_to_scene_linear_v3(buf_info->col_float, buf_info->display);
	}
	else {
		srgb_to_linearrgb_v4(buf_info->col_float, buf_info->col_init);
	}
}
void blf_draw_buffer__end(void) {}

void BLF_draw_buffer_ex(
        int fontid, const char *str, size_t len,
        struct ResultBLF *r_info)
{
	FontBLF *font = blf_get(fontid);

	if (font && font->glyph_cache && (font->buf_info.fbuf || font->buf_info.cbuf)) {
		blf_draw_buffer__start(font);
		if (font->flags & BLF_WORD_WRAP) {
			blf_font_draw_buffer__wrap(font, str, len, r_info);
		}
		else {
			blf_font_draw_buffer(font, str, len, r_info);
		}
		blf_draw_buffer__end();
	}
}
void BLF_draw_buffer(
        int fontid, const char *str, size_t len)
{
	BLF_draw_buffer_ex(fontid, str, len, NULL);
}

#ifdef DEBUG
void BLF_state_print(int fontid)
{
	FontBLF *font = blf_get(fontid);
	if (font) {
		printf("fontid %d %p\n", fontid, (void *)font);
		printf("  name:    '%s'\n", font->name);
		printf("  size:     %u\n", font->size);
		printf("  dpi:      %u\n", font->dpi);
		printf("  pos:      %.6f %.6f %.6f\n", UNPACK3(font->pos));
		printf("  aspect:   (%d) %.6f %.6f %.6f\n", (font->flags & BLF_ROTATION) != 0, UNPACK3(font->aspect));
		printf("  angle:    (%d) %.6f\n", (font->flags & BLF_ASPECT) != 0, font->angle);
		printf("  flag:     %d\n", font->flags);
	}
	else {
		printf("fontid %d (NULL)\n", fontid);
	}
	fflush(stdout);
}
#endif

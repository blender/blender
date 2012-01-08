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

#include "BIF_gl.h"
#include "BLF_api.h"

#include "blf_internal_types.h"
#include "blf_internal.h"


/* Max number of font in memory.
 * Take care that now every font have a glyph cache per size/dpi,
 * so we don't need load the same font with different size, just
 * load one and call BLF_size.
 */
#define BLF_MAX_FONT 16

/* Font array. */
static FontBLF *global_font[BLF_MAX_FONT] = {0};

/* Default size and dpi, for BLF_draw_default. */
static int global_font_default= -1;
static int global_font_points= 11;
static int global_font_dpi= 72;

// XXX, should these be made into global_font_'s too?
int blf_mono_font= -1;
int blf_mono_font_render= -1;

static FontBLF *BLF_get(int fontid)
{
	if (fontid >= 0 && fontid < BLF_MAX_FONT)
		return global_font[fontid];
	return NULL;
}

int BLF_init(int points, int dpi)
{
	int i;

	for (i= 0; i < BLF_MAX_FONT; i++)
		global_font[i]= NULL;

	global_font_points= points;
	global_font_dpi= dpi;
	return blf_font_init();
}

void BLF_exit(void)
{
	FontBLF *font;
	int i;

	for (i= 0; i < BLF_MAX_FONT; i++) {
		font= global_font[i];
		if (font) {
			blf_font_free(font);
			global_font[i]= NULL;
		}
	}

	blf_font_exit();
}

void BLF_cache_clear(void)
{
	FontBLF *font;
	int i;

	for (i= 0; i < BLF_MAX_FONT; i++) {
		font= global_font[i];
		if (font)
			blf_glyph_cache_clear(font);
	}
}

static int blf_search(const char *name)
{
	FontBLF *font;
	int i;

	for (i= 0; i < BLF_MAX_FONT; i++) {
		font= global_font[i];
		if (font && (!strcmp(font->name, name)))
			return i;
	}

	return -1;
}

static int blf_search_available(void)
{
	int i;

	for (i= 0; i < BLF_MAX_FONT; i++)
		if(!global_font[i])
			return i;
	
	return -1;
}

int BLF_load(const char *name)
{
	FontBLF *font;
	char *filename;
	int i;

	if (!name)
		return -1;

	/* check if we already load this font. */
	i= blf_search(name);
	if (i >= 0) {
		/*font= global_font[i];*/ /*UNUSED*/
		return i;
	}

	i = blf_search_available();
	if (i == -1) {
		printf("Too many fonts!!!\n");
		return -1;
	}

	filename= blf_dir_search(name);
	if (!filename) {
		printf("Can't find font: %s\n", name);
		return -1;
	}

	font= blf_font_new(name, filename);
	MEM_freeN(filename);

	if (!font) {
		printf("Can't load font: %s\n", name);
		return -1;
	}

	global_font[i]= font;
	return i;
}

int BLF_load_unique(const char *name)
{
	FontBLF *font;
	char *filename;
	int i;

	if (!name)
		return -1;

	/* Don't search in the cache!! make a new
	 * object font, this is for keep fonts threads safe.
	 */
	i = blf_search_available();
	if (i == -1) {
		printf("Too many fonts!!!\n");
		return -1;
	}

	filename= blf_dir_search(name);
	if (!filename) {
		printf("Can't find font: %s\n", name);
		return -1;
	}

	font= blf_font_new(name, filename);
	MEM_freeN(filename);

	if (!font) {
		printf("Can't load font: %s\n", name);
		return -1;
	}

	global_font[i]= font;
	return i;
}

void BLF_metrics_attach(int fontid, unsigned char *mem, int mem_size)
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		blf_font_attach_from_mem(font, mem, mem_size);
	}
}

int BLF_load_mem(const char *name, unsigned char *mem, int mem_size)
{
	FontBLF *font;
	int i;

	if (!name)
		return -1;

	i= blf_search(name);
	if (i >= 0) {
		/*font= global_font[i];*/ /*UNUSED*/
		return i;
	}

	i = blf_search_available();
	if (i == -1) {
		printf("Too many fonts!!!\n");
		return -1;
	}

	if (!mem || !mem_size) {
		printf("Can't load font: %s from memory!!\n", name);
		return -1;
	}

	font= blf_font_new_from_mem(name, mem, mem_size);
	if (!font) {
		printf("Can't load font: %s from memory!!\n", name);
		return -1;
	}

	global_font[i]= font;
	return i;
}

int BLF_load_mem_unique(const char *name, unsigned char *mem, int mem_size)
{
	FontBLF *font;
	int i;

	if (!name)
		return -1;

	/*
	 * Don't search in the cache, make a new object font!
	 * this is to keep the font thread safe.
	 */
	i = blf_search_available();
	if (i == -1) {
		printf("Too many fonts!!!\n");
		return -1;
	}

	if (!mem || !mem_size) {
		printf("Can't load font: %s from memory!!\n", name);
		return -1;
	}

	font= blf_font_new_from_mem(name, mem, mem_size);
	if (!font) {
		printf("Can't load font: %s from memory!!\n", name);
		return -1;
	}

	global_font[i]= font;
	return i;
}

void BLF_unload(const char *name)
{
	FontBLF *font;
	int i;

	for (i= 0; i < BLF_MAX_FONT; i++) {
		font= global_font[i];

		if (font && (!strcmp(font->name, name))) {
			blf_font_free(font);
			global_font[i]= NULL;
		}
	}
}

void BLF_enable(int fontid, int option)
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		font->flags |= option;
	}
}

void BLF_disable(int fontid, int option)
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		font->flags &= ~option;
	}
}

void BLF_enable_default(int option)
{
	FontBLF *font= BLF_get(global_font_default);

	if (font) {
		font->flags |= option;
	}
}

void BLF_disable_default(int option)
{
	FontBLF *font= BLF_get(global_font_default);

	if (font) {
		font->flags &= ~option;
	}
}

void BLF_aspect(int fontid, float x, float y, float z)
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		font->aspect[0]= x;
		font->aspect[1]= y;
		font->aspect[2]= z;
	}
}

void BLF_matrix(int fontid, const double m[16])
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		memcpy(font->m, m, sizeof(font->m));
	}
}

void BLF_position(int fontid, float x, float y, float z)
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		float xa, ya, za;
		float remainder;

		if (font->flags & BLF_ASPECT) {
			xa= font->aspect[0];
			ya= font->aspect[1];
			za= font->aspect[2];
		}
		else {
			xa= 1.0f;
			ya= 1.0f;
			za= 1.0f;
		}

		remainder= x - floorf(x);
		if (remainder > 0.4f && remainder < 0.6f) {
			if (remainder < 0.5f)
				x -= 0.1f * xa;
			else
				x += 0.1f * xa;
		}

		remainder= y - floorf(y);
		if (remainder > 0.4f && remainder < 0.6f) {
			if (remainder < 0.5f)
				y -= 0.1f * ya;
			else
				y += 0.1f * ya;
		}

		remainder= z - floorf(z);
		if (remainder > 0.4f && remainder < 0.6f) {
			if (remainder < 0.5f)
				z -= 0.1f * za;
			else
				z += 0.1f * za;
		}

		font->pos[0]= x;
		font->pos[1]= y;
		font->pos[2]= z;
	}
}

void BLF_size(int fontid, int size, int dpi)
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		blf_font_size(font, size, dpi);
	}
}

void BLF_blur(int fontid, int size)
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		font->blur= size;
	}
}

void BLF_draw_default(float x, float y, float z, const char *str, size_t len)
{
	if (!str)
		return;

	if (global_font_default == -1)
		global_font_default= blf_search("default");

	if (global_font_default == -1) {
		printf("Warning: Can't found default font!!\n");
		return;
	}

	BLF_size(global_font_default, global_font_points, global_font_dpi);
	BLF_position(global_font_default, x, y, z);
	BLF_draw(global_font_default, str, len);
}

/* same as above but call 'BLF_draw_ascii' */
void BLF_draw_default_ascii(float x, float y, float z, const char *str, size_t len)
{
	if (!str)
		return;

	if (global_font_default == -1)
		global_font_default= blf_search("default");

	if (global_font_default == -1) {
		printf("Warning: Can't found default font!!\n");
		return;
	}

	BLF_size(global_font_default, global_font_points, global_font_dpi);
	BLF_position(global_font_default, x, y, z);
	BLF_draw_ascii(global_font_default, str, len); /* XXX, use real length */
}

void BLF_rotation_default(float angle)
{
	FontBLF *font= BLF_get(global_font_default);

	if (font) {
		font->angle= angle;
	}
}

static void blf_draw__start(FontBLF *font, GLint *mode, GLint *param)
{
	/*
	 * The pixmap alignment hack is handle
	 * in BLF_position (old ui_rasterpos_safe).
	 */

	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* Save the current matrix mode. */
	glGetIntegerv(GL_MATRIX_MODE, mode);

	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	if (font->flags & BLF_MATRIX)
		glMultMatrixd((GLdouble *)&font->m);

	glTranslatef(font->pos[0], font->pos[1], font->pos[2]);

	if (font->flags & BLF_ASPECT)
		glScalef(font->aspect[0], font->aspect[1], font->aspect[2]);

	if (font->flags & BLF_ROTATION)
		glRotatef(font->angle, 0.0f, 0.0f, 1.0f);

	if(font->shadow || font->blur)
		glGetFloatv(GL_CURRENT_COLOR, font->orig_col);

	/* always bind the texture for the first glyph */
	font->tex_bind_state= -1;

	/* Save the current parameter to restore it later. */
	glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, param);
	if (*param != GL_MODULATE)
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

static void blf_draw__end(GLint mode, GLint param)
{
	/* and restore the original value. */
	if (param != GL_MODULATE)
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, param);

	glMatrixMode(GL_TEXTURE);
	glPopMatrix();

	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if (mode != GL_MODELVIEW)
		glMatrixMode(mode);

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
}

void BLF_draw(int fontid, const char *str, size_t len)
{
	FontBLF *font= BLF_get(fontid);
	GLint mode, param;

	if (font && font->glyph_cache) {
		blf_draw__start(font, &mode, &param);
		blf_font_draw(font, str, len);
		blf_draw__end(mode, param);
	}
}

void BLF_draw_ascii(int fontid, const char *str, size_t len)
{
	FontBLF *font= BLF_get(fontid);
	GLint mode, param;

	if (font && font->glyph_cache) {
		blf_draw__start(font, &mode, &param);
		blf_font_draw_ascii(font, str, len);
		blf_draw__end(mode, param);
	}
}

void BLF_boundbox(int fontid, const char *str, rctf *box)
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		blf_font_boundbox(font, str, box);
	}
}

void BLF_width_and_height(int fontid, const char *str, float *width, float *height)
{
	FontBLF *font= BLF_get(fontid);

	if (font && font->glyph_cache) {
		blf_font_width_and_height(font, str, width, height);
	}
}

float BLF_width(int fontid, const char *str)
{
	FontBLF *font= BLF_get(fontid);

	if (font && font->glyph_cache) {
		return blf_font_width(font, str);
	}

	return 0.0f;
}

float BLF_fixed_width(int fontid)
{
	FontBLF *font= BLF_get(fontid);

	if (font && font->glyph_cache) {
		return blf_font_fixed_width(font);
	}

	return 0.0f;
}

float BLF_width_default(const char *str)
{
	if (global_font_default == -1)
		global_font_default= blf_search("default");

	if (global_font_default == -1) {
		printf("Error: Can't found default font!!\n");
		return 0.0f;
	}

	BLF_size(global_font_default, global_font_points, global_font_dpi);
	return BLF_width(global_font_default, str);
}

float BLF_height(int fontid, const char *str)
{
	FontBLF *font= BLF_get(fontid);

	if (font && font->glyph_cache) {
		return blf_font_height(font, str);
	}

	return 0.0f;
}

float BLF_height_max(int fontid)
{
	FontBLF *font= BLF_get(fontid);

	if (font && font->glyph_cache) {
		return font->glyph_cache->max_glyph_height;
	}

	return 0.0f;
}

float BLF_width_max(int fontid)
{
	FontBLF *font= BLF_get(fontid);

	if (font && font->glyph_cache) {
		return font->glyph_cache->max_glyph_width;
	}

	return 0.0f;
}

float BLF_descender(int fontid)
{
	FontBLF *font= BLF_get(fontid);

	if (font && font->glyph_cache) {
		return font->glyph_cache->descender;
	}

	return 0.0f;
}

float BLF_ascender(int fontid)
{
	FontBLF *font= BLF_get(fontid);

	if (font && font->glyph_cache) {
		return font->glyph_cache->ascender;
	}

	return 0.0f;
}

float BLF_height_default(const char *str)
{
	if (global_font_default == -1)
		global_font_default= blf_search("default");

	if (global_font_default == -1) {
		printf("Error: Can't found default font!!\n");
		return 0.0f;
	}

	BLF_size(global_font_default, global_font_points, global_font_dpi);

	return BLF_height(global_font_default, str);
}

void BLF_rotation(int fontid, float angle)
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		font->angle= angle;
	}
}

void BLF_clipping(int fontid, float xmin, float ymin, float xmax, float ymax)
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		font->clip_rec.xmin= xmin;
		font->clip_rec.ymin= ymin;
		font->clip_rec.xmax= xmax;
		font->clip_rec.ymax= ymax;
	}
}

void BLF_clipping_default(float xmin, float ymin, float xmax, float ymax)
{
	FontBLF *font= BLF_get(global_font_default);

	if (font) {
		font->clip_rec.xmin= xmin;
		font->clip_rec.ymin= ymin;
		font->clip_rec.xmax= xmax;
		font->clip_rec.ymax= ymax;
	}
}

void BLF_shadow(int fontid, int level, float r, float g, float b, float a)
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		font->shadow= level;
		font->shadow_col[0]= r;
		font->shadow_col[1]= g;
		font->shadow_col[2]= b;
		font->shadow_col[3]= a;
	}
}

void BLF_shadow_offset(int fontid, int x, int y)
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		font->shadow_x= x;
		font->shadow_y= y;
	}
}

void BLF_buffer(int fontid, float *fbuf, unsigned char *cbuf, int w, int h, int nch)
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		font->b_fbuf= fbuf;
		font->b_cbuf= cbuf;
		font->bw= w;
		font->bh= h;
		font->bch= nch;
	}
}

void BLF_buffer_col(int fontid, float r, float g, float b, float a)
{
	FontBLF *font= BLF_get(fontid);

	if (font) {
		font->b_col[0]= r;
		font->b_col[1]= g;
		font->b_col[2]= b;
		font->b_col[3]= a;
	}
}

void BLF_draw_buffer(int fontid, const char *str)
{
	FontBLF *font= BLF_get(fontid);

	if (font && font->glyph_cache && (font->b_fbuf || font->b_cbuf)) {
		blf_font_buffer(font, str);
	}
}

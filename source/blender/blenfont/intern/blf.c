/**
 * $Id$
 *
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
FontBLF *global_font[BLF_MAX_FONT];

/* Number of font. */
int global_font_num= 0;

/* Default size and dpi, for BLF_draw_default. */
int global_font_default= -1;
int global_font_points= 11;
int global_font_dpi= 72;

static FontBLF *BLF_get(int fontid)
{
	if (fontid >= 0 && fontid < BLF_MAX_FONT)
		return(global_font[fontid]);
	return(NULL);
}

int BLF_init(int points, int dpi)
{
	int i;

	for (i= 0; i < BLF_MAX_FONT; i++)
		global_font[i]= NULL;

	global_font_points= points;
	global_font_dpi= dpi;
	return(blf_font_init());
}

void BLF_exit(void)
{
	FontBLF *font;
	int i;

	for (i= 0; i < global_font_num; i++) {
		font= global_font[i];
		if (font)
			blf_font_free(font);
	}

	blf_font_exit();
}

static int blf_search(char *name)
{
	FontBLF *font;
	int i;

	for (i= 0; i < BLF_MAX_FONT; i++) {
		font= global_font[i];
		if (font && (!strcmp(font->name, name)))
			return(i);
	}
	return(-1);
}

int BLF_load(char *name)
{
	FontBLF *font;
	char *filename;
	int i;

	if (!name)
		return(-1);

	/* check if we already load this font. */
	i= blf_search(name);
	if (i >= 0) {
		font= global_font[i];
		return(i);
	}

	if (global_font_num+1 >= BLF_MAX_FONT) {
		printf("Too many fonts!!!\n");
		return(-1);
	}

	filename= blf_dir_search(name);
	if (!filename) {
		printf("Can't find font: %s\n", name);
		return(-1);
	}

	font= blf_font_new(name, filename);
	MEM_freeN(filename);

	if (!font) {
		printf("Can't load font: %s\n", name);
		return(-1);
	}

	global_font[global_font_num]= font;
	i= global_font_num;
	global_font_num++;
	return(i);
}

int BLF_load_unique(char *name)
{
	FontBLF *font;
	char *filename;
	int i;

	if (!name)
		return(-1);

	/* Don't search in the cache!! make a new
	 * object font, this is for keep fonts threads safe.
	 */
	if (global_font_num+1 >= BLF_MAX_FONT) {
		printf("Too many fonts!!!\n");
		return(-1);
	}

	filename= blf_dir_search(name);
	if (!filename) {
		printf("Can't find font: %s\n", name);
		return(-1);
	}

	font= blf_font_new(name, filename);
	MEM_freeN(filename);

	if (!font) {
		printf("Can't load font: %s\n", name);
		return(-1);
	}

	global_font[global_font_num]= font;
	i= global_font_num;
	global_font_num++;
	return(i);
}

void BLF_metrics_attach(int fontid, unsigned char *mem, int mem_size)
{
	FontBLF *font;

	font= BLF_get(fontid);
	if (font)
		blf_font_attach_from_mem(font, mem, mem_size);
}

int BLF_load_mem(char *name, unsigned char *mem, int mem_size)
{
	FontBLF *font;
	int i;

	if (!name)
		return(-1);

	i= blf_search(name);
	if (i >= 0) {
		font= global_font[i];
		return(i);
	}

	if (global_font_num+1 >= BLF_MAX_FONT) {
		printf("Too many fonts!!!\n");
		return(-1);
	}

	if (!mem || !mem_size) {
		printf("Can't load font: %s from memory!!\n", name);
		return(-1);
	}

	font= blf_font_new_from_mem(name, mem, mem_size);
	if (!font) {
		printf("Can't load font: %s from memory!!\n", name);
		return(-1);
	}

	global_font[global_font_num]= font;
	i= global_font_num;
	global_font_num++;
	return(i);
}

int BLF_load_mem_unique(char *name, unsigned char *mem, int mem_size)
{
	FontBLF *font;
	int i;

	if (!name)
		return(-1);

	/*
	 * Don't search in the cache, make a new object font!
	 * this is to keep the font thread safe.
	 */
	if (global_font_num+1 >= BLF_MAX_FONT) {
		printf("Too many fonts!!!\n");
		return(-1);
	}

	if (!mem || !mem_size) {
		printf("Can't load font: %s from memory!!\n", name);
		return(-1);
	}

	font= blf_font_new_from_mem(name, mem, mem_size);
	if (!font) {
		printf("Can't load font: %s from memory!!\n", name);
		return(-1);
	}

	global_font[global_font_num]= font;
	i= global_font_num;
	global_font_num++;
	return(i);
}

void BLF_enable(int fontid, int option)
{
	FontBLF *font;

	font= BLF_get(fontid);
	if (font)
		font->flags |= option;
}

void BLF_disable(int fontid, int option)
{
	FontBLF *font;

	font= BLF_get(fontid);
	if (font)
		font->flags &= ~option;
}

void BLF_enable_default(int option)
{
	FontBLF *font;

	font= BLF_get(global_font_default);
	if (font)
		font->flags |= option;
}

void BLF_disable_default(int option)
{
	FontBLF *font;

	font= BLF_get(global_font_default);
	if (font)
		font->flags &= ~option;
}

void BLF_aspect(int fontid, float aspect)
{
	FontBLF *font;

	font= BLF_get(fontid);
	if (font)
		font->aspect= aspect;
}

void BLF_position(int fontid, float x, float y, float z)
{
	FontBLF *font;
	float remainder;

	font= BLF_get(fontid);
	if (font) {
		remainder= x - floor(x);
		if (remainder > 0.4 && remainder < 0.6) {
			if (remainder < 0.5)
				x -= 0.1 * font->aspect;
			else
				x += 0.1 * font->aspect;
		}

		remainder= y - floor(y);
		if (remainder > 0.4 && remainder < 0.6) {
			if (remainder < 0.5)
				y -= 0.1 * font->aspect;
			else
				y += 0.1 * font->aspect;
		}

		font->pos[0]= x;
		font->pos[1]= y;
		font->pos[2]= z;
	}
}

void BLF_size(int fontid, int size, int dpi)
{
	FontBLF *font;

	font= BLF_get(fontid);
	if (font)
		blf_font_size(font, size, dpi);
}

void BLF_blur(int fontid, int size)
{
	FontBLF *font;
	
	font= BLF_get(fontid);
	if (font)
		font->blur= size;
}

void BLF_draw_default(float x, float y, float z, char *str)
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
	BLF_draw(global_font_default, str);
}

void BLF_rotation_default(float angle)
{
	FontBLF *font;

	font= BLF_get(global_font_default);
	if (font)
		font->angle= angle;
}

void BLF_draw(int fontid, char *str)
{
	FontBLF *font;

	/*
	 * The pixmap alignment hack is handle
	 * in BLF_position (old ui_rasterpos_safe).
	 */
	font= BLF_get(fontid);
	if (font) {
		glEnable(GL_BLEND);
		glEnable(GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glPushMatrix();
		glTranslatef(font->pos[0], font->pos[1], font->pos[2]);
		glScalef(font->aspect, font->aspect, 1.0);

		if (font->flags & BLF_ROTATION)
			glRotatef(font->angle, 0.0f, 0.0f, 1.0f);

		blf_font_draw(font, str);

		glPopMatrix();
		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
	}
}

void BLF_boundbox(int fontid, char *str, rctf *box)
{
	FontBLF *font;

	font= BLF_get(fontid);
	if (font)
		blf_font_boundbox(font, str, box);
}

void BLF_width_and_height(int fontid, char *str, float *width, float *height)
{
	FontBLF *font;

	font= BLF_get(fontid);
	if (font)
		blf_font_width_and_height(font, str, width, height);
}

float BLF_width(int fontid, char *str)
{
	FontBLF *font;

	font= BLF_get(fontid);
	if (font)
		return(blf_font_width(font, str));
	return(0.0f);
}

float BLF_fixed_width(int fontid)
{
	FontBLF *font;

	font= BLF_get(fontid);
	if (font)
		return(blf_font_fixed_width(font));
	return(0.0f);
}

float BLF_width_default(char *str)
{
	float width;

	if (global_font_default == -1)
		global_font_default= blf_search("default");

	if (global_font_default == -1) {
		printf("Error: Can't found default font!!\n");
		return(0.0f);
	}

	BLF_size(global_font_default, global_font_points, global_font_dpi);
	width= BLF_width(global_font_default, str);
	return(width);
}

float BLF_height(int fontid, char *str)
{
	FontBLF *font;

	font= BLF_get(fontid);
	if (font)
		return(blf_font_height(font, str));
	return(0.0f);
}

float BLF_height_default(char *str)
{
	float height;

	if (global_font_default == -1)
		global_font_default= blf_search("default");

	if (global_font_default == -1) {
		printf("Error: Can't found default font!!\n");
		return(0.0f);
	}

	BLF_size(global_font_default, global_font_points, global_font_dpi);
	height= BLF_height(global_font_default, str);
	return(height);
}

void BLF_rotation(int fontid, float angle)
{
	FontBLF *font;

	font= BLF_get(fontid);
	if (font)
		font->angle= angle;
}

void BLF_clipping(int fontid, float xmin, float ymin, float xmax, float ymax)
{
	FontBLF *font;

	font= BLF_get(fontid);
	if (font) {
		font->clip_rec.xmin= xmin;
		font->clip_rec.ymin= ymin;
		font->clip_rec.xmax= xmax;
		font->clip_rec.ymax= ymax;
	}
}

void BLF_clipping_default(float xmin, float ymin, float xmax, float ymax)
{
	FontBLF *font;

	font= BLF_get(global_font_default);
	if (font) {
		font->clip_rec.xmin= xmin;
		font->clip_rec.ymin= ymin;
		font->clip_rec.xmax= xmax;
		font->clip_rec.ymax= ymax;
	}
}

void BLF_shadow(int fontid, int level, float r, float g, float b, float a)
{
	FontBLF *font;

	font= BLF_get(fontid);
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
	FontBLF *font;

	font= BLF_get(fontid);
	if (font) {
		font->shadow_x= x;
		font->shadow_y= y;
	}
}

void BLF_buffer(int fontid, float *fbuf, unsigned char *cbuf, unsigned int w, unsigned int h, int nch)
{
	FontBLF *font;

	font= BLF_get(fontid);
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
	FontBLF *font;

	font= BLF_get(fontid);
	if (font) {
		font->b_col[0]= r;
		font->b_col[1]= g;
		font->b_col[2]= b;
		font->b_col[3]= a;
	}
}

void BLF_draw_buffer(int fontid, char *str)
{
	FontBLF *font;

	font= BLF_get(fontid);
	if (font)
		blf_font_buffer(font, str);
}

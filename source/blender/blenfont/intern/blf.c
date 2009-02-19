/**
 * $Id:
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifdef WITH_FREETYPE2

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H

#endif /* WITH_FREETYPE2 */

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_vec_types.h"

#include "BKE_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"	/* linknode */
#include "BLI_string.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "blf_internal_types.h"
#include "blf_internal.h"


#ifdef WITH_FREETYPE2

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

/* Current font. */
int global_font_cur= 0;

#endif /* WITH_FREETYPE2 */

int BLF_init(void)
{
#ifdef WITH_FREETYPE2
	int i;

	for (i= 0; i < BLF_MAX_FONT; i++)
		global_font[i]= NULL;

	return(blf_font_init());
#else
	return(0);
#endif
}

void BLF_exit(void)
{
#ifdef WITH_FREETYPE2
	FontBLF *font;
	int i;

	for (i= 0; i < global_font_num; i++) {
		font= global_font[i];
		if(font)
			blf_font_free(font);
	}

	blf_font_exit();
#endif
}

int blf_search(char *name)
{
#ifdef WITH_FREETYPE2
	FontBLF *font;
	int i;

	for (i= 0; i < BLF_MAX_FONT; i++) {
		font= global_font[i];
		if (font && (!strcmp(font->name, name)))
			return(i);
	}
#endif
	return(-1);
}

int BLF_load(char *name)
{
#ifdef WITH_FREETYPE2
	FontBLF *font;
	char *filename;
	int i;

	if (!name)
		return(-1);

	/* check if we already load this font. */
	i= blf_search(name);
	if (i >= 0) {
		font= global_font[i];
		font->ref++;
		printf("Increment reference (%d): %s\n", font->ref, name);
		return(i);
	}

	if (global_font_num+1 >= BLF_MAX_FONT) {
		printf("Too many fonts!!!\n");
		return(-1);
	}

	filename= blf_dir_search(name);
	if (!filename) {
		printf("Can't found font: %s\n", name);
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
#else
	return(-1);
#endif /* WITH_FREETYPE2 */
}

int BLF_load_mem(char *name, unsigned char *mem, int mem_size)
{
#ifdef WITH_FREETYPE2
	FontBLF *font;
	int i;

	if (!name || !mem || !mem_size)
		return(-1);

	i= blf_search(name);
	if (i >= 0) {
		font= global_font[i];
		font->ref++;
		printf("Increment reference (%d): %s\n", font->ref, name);
		return(i);
	}

	if (global_font_num+1 >= BLF_MAX_FONT) {
		printf("Too many fonts!!!\n");
		return(-1);
	}

	font= blf_font_new_from_mem(name, mem, mem_size);
	if (!font) {
		printf("Can't load font, %s from memory!!\n", name);
		return(-1);
	}

	global_font[global_font_num]= font;
	i= global_font_num;
	global_font_num++;
	return(i);
#else
	return(-1);
#endif /* WITH_FREETYPE2 */
}

void BLF_set(int fontid)
{
#ifdef WITH_FREETYPE2
	if (fontid >= 0 && fontid < BLF_MAX_FONT)
		global_font_cur= fontid;
#endif
}

void BLF_aspect(float aspect)
{
#ifdef WITH_FREETYPE2
	FontBLF *font;

	font= global_font[global_font_cur];
	if (font)
		font->aspect= aspect;
#endif
}

void BLF_position(float x, float y, float z)
{
#ifdef WITH_FREETYPE2
	FontBLF *font;
	float remainder;

	font= global_font[global_font_cur];
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
#endif
}

void BLF_size(int size, int dpi)
{
#ifdef WITH_FREETYPE2
	FontBLF *font;

	font= global_font[global_font_cur];
	if (font)
		blf_font_size(font, size, dpi);
#endif
}

void BLF_draw(char *str)
{
#ifdef WITH_FREETYPE2
	FontBLF *font;

	font= global_font[global_font_cur];
	if (font && font->glyph_cache) {
		glEnable(GL_BLEND);
		glEnable(GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glPushMatrix();
		glTranslatef(font->pos[0], font->pos[1], font->pos[2]);
		glScalef(font->aspect, font->aspect, 1.0);
		glRotatef(font->angle, 0.0f, 0.0f, 1.0f);

		blf_font_draw(font, str);

		glPopMatrix();
		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
	}
#endif /* WITH_FREETYPE2 */
}

void BLF_boundbox(char *str, rctf *box)
{
#ifdef WITH_FREETYPE2
	FontBLF *font;

	font= global_font[global_font_cur];
	if (font && font->glyph_cache)
		blf_font_boundbox(font, str, box);
#endif
}

float BLF_width(char *str)
{
#ifdef WITH_FREETYPE2
	FontBLF *font;

	font= global_font[global_font_cur];
	if (font && font->glyph_cache)
		return(blf_font_width(font, str));
#endif
	return(0.0f);
}

float BLF_height(char *str)
{
#ifdef WITH_FREETYPE2
	FontBLF *font;

	font= global_font[global_font_cur];
	if (font && font->glyph_cache)
		return(blf_font_height(font, str));
#endif
	return(0.0f);
}

void BLF_rotation(float angle)
{
#ifdef WITH_FREETYPE2
	FontBLF *font;

	font= global_font[global_font_cur];
	if (font)
		font->angle= angle;
#endif
}

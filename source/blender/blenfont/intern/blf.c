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

#if 0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BKE_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"	/* linknode */
#include "BLI_string.h"

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

/* Current font. */
int global_font_cur= 0;

int BLF_init(void)
{
	int i;

	for (i= 0; i < BLF_MAX_FONT; i++)
		global_font[i]= NULL;

	return(blf_font_init());
}

int blf_search(char *name)
{
	FontBLF *font;
	int i;

	for (i= 0; i < global_font_num; i++) {
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
	if (i >= 0)
		return(i);

	if (global_font_num+1 >= BLF_MAX_FONT)
		return(-1);

	filename= blf_dir_search(name);
	if (!filename)
		return(-1);

	font= blf_font_new(name, filename);
	MEM_freeN(filename);

	if (!font)
		return(-1);

	global_font[global_font_num]= font;
	i= global_font_num;
	global_font_num++;
	return(i);
}

int BLF_load_mem(char *name, unsigned char *mem, int mem_size)
{
	FontBLF *font;
	int i;

	if (!name || !mem || !mem_size)
		return(-1);

	i= blf_search(name);
	if (i >= 0)
		return(i);

	if (global_font_num+1 >= BLF_MAX_FONT)
		return(-1);

	font= blf_font_new_from_mem(name, mem, size);
	if (!font)
		return(-1);

	global_font[global_font_num]= font;
	i= global_font_num;
	global_font_num++;
	return(i);
}

void BLF_set(int fontid)
{
	if (fontid >= 0 && fontid < global_font_num)
		global_font_cur= fontid;
}

void BLF_aspect(float aspect)
{
	FontBLF *font;

	font= global_font[global_font_cur];
	if (font)
		font->aspect= aspect;
}

void BLF_position(float x, float y, float z)
{
	FontBLF *font;

	font= global_font[global_font_cur];
	if (font) {
		font->pos[0]= x;
		font->pos[1]= y;
		font->pos[2]= z;
	}
}

void BLF_size(int size, int dpi)
{
	FontBLF *font;

	font= global_font[global_font_cur];
	if (font)
		blf_font_size(font, size, dpi);
}

void BLF_draw(char *str)
{
	FontBLF *font;

	font= global_font[global_font_cur];
	if (font) {
		glEnable(GL_BLEND);
		glEnable(GL_TEXTURE_2D);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glPushMatrix();
		glTranslatef(font->pos[0], font->pos[1], font->pos[2]);

		blf_font_draw(font, str);

		glPopMatrix();
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
	}
}

#endif

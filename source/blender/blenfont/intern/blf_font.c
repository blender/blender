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

#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_vec_types.h"

#include "BKE_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"	/* linknode */
#include "BLI_string.h"
#include "BLI_arithb.h"

#include "BIF_gl.h"
#include "BLF_api.h"

#include "blf_internal_types.h"
#include "blf_internal.h"


/* freetype2 handle. */
FT_Library global_ft_lib;

int blf_font_init(void)
{
	return(FT_Init_FreeType(&global_ft_lib));
}

void blf_font_exit(void)
{
	FT_Done_FreeType(global_ft_lib);
}

void blf_font_size(FontBLF *font, int size, int dpi)
{
	GlyphCacheBLF *gc;
	FT_Error err;

	err= FT_Set_Char_Size(font->face, 0, (size * 64), dpi, dpi);
	if (err) {
		/* FIXME: here we can go through the fixed size and choice a close one */
		printf("The current font don't support the size, %d and dpi, %d\n", size, dpi);
		return;
	}

	font->size= size;
	font->dpi= dpi;

	gc= blf_glyph_cache_find(font, size, dpi);
	if (gc)
		font->glyph_cache= gc;
	else {
		gc= blf_glyph_cache_new(font);
		if (gc)
			font->glyph_cache= gc;
		else
			font->glyph_cache= NULL;
	}
}

void blf_font_draw(FontBLF *font, char *str)
{
	unsigned int c;
	GlyphBLF *g, *g_prev;
	FT_Vector delta;
	FT_UInt glyph_index, g_prev_index;
	float pen_x, pen_y, old_pen_x;
	int i, has_kerning;

	if (!font->glyph_cache)
		return;

	i= 0;
	pen_x= 0;
	pen_y= 0;
	has_kerning= FT_HAS_KERNING(font->face);
	g_prev= NULL;
	g_prev_index= 0;

	while (str[i]) {
		c= blf_utf8_next((unsigned char *)str, &i);
		if (c == 0)
			break;

		glyph_index= FT_Get_Char_Index(font->face, c);
		g= blf_glyph_search(font->glyph_cache, c);
		if (!g)
			g= blf_glyph_add(font, glyph_index, c);

		/* if we don't found a glyph, skip it. */
		if (!g)
			continue;

		/*
		 * This happen if we change the mode of the
		 * font, we don't drop the glyph cache, so it's
		 * possible that some glyph don't have the
		 * bitmap or texture information.
		 */
		if (font->mode == BLF_MODE_BITMAP && (!g->bitmap_data))
			g= blf_glyph_add(font, glyph_index, c);
		else if (font->mode == BLF_MODE_TEXTURE && (!g->tex_data))
			g= blf_glyph_add(font, glyph_index, c);

		if ((font->flags & BLF_FONT_KERNING) && has_kerning && g_prev) {
			old_pen_x= pen_x;
			delta.x= 0;
			delta.y= 0;

			if (FT_Get_Kerning(font->face, g_prev_index, glyph_index, FT_KERNING_UNFITTED, &delta) == 0) {
				pen_x += delta.x >> 6;
/*
				if (pen_x < old_pen_x)
					pen_x= old_pen_x;
*/
			}
		}

		if (font->flags & BLF_USER_KERNING) {
			old_pen_x= pen_x;
			pen_x += font->kerning;
/*
			if (pen_x < old_pen_x)
				pen_x= old_pen_x;
*/
		}

		/* do not return this loop if clipped, we want every character tested */
		blf_glyph_render(font, g, pen_x, pen_y);

		pen_x += g->advance;
		g_prev= g;
		g_prev_index= glyph_index;
	}
}

void blf_font_boundbox(FontBLF *font, char *str, rctf *box)
{
	unsigned int c;
	GlyphBLF *g, *g_prev;
	FT_Vector delta;
	FT_UInt glyph_index, g_prev_index;
	rctf gbox;
	float pen_x, pen_y, old_pen_x;
	int i, has_kerning;

	if (!font->glyph_cache)
		return;

	box->xmin= 32000.0f;
	box->xmax= -32000.0f;
	box->ymin= 32000.0f;
	box->ymax= -32000.0f;

	i= 0;
	pen_x= 0;
	pen_y= 0;
	has_kerning= FT_HAS_KERNING(font->face);
	g_prev= NULL;
	g_prev_index= 0;

	while (str[i]) {
		c= blf_utf8_next((unsigned char *)str, &i);
		if (c == 0)
			break;

		glyph_index= FT_Get_Char_Index(font->face, c);
		g= blf_glyph_search(font->glyph_cache, c);
		if (!g)
			g= blf_glyph_add(font, glyph_index, c);

		/* if we don't found a glyph, skip it. */
		if (!g)
			continue;

		/*
		 * This happen if we change the mode of the
		 * font, we don't drop the glyph cache, so it's
		 * possible that some glyph don't have the
		 * bitmap or texture information.
		 */
		if (font->mode == BLF_MODE_BITMAP && (!g->bitmap_data))
			g= blf_glyph_add(font, glyph_index, c);
		else if (font->mode == BLF_MODE_TEXTURE && (!g->tex_data))
			g= blf_glyph_add(font, glyph_index, c);

		if ((font->flags & BLF_FONT_KERNING) && has_kerning && g_prev) {
			old_pen_x= pen_x;
			delta.x= 0;
			delta.y= 0;

			if (FT_Get_Kerning(font->face, g_prev_index, glyph_index, FT_KERNING_UNFITTED, &delta) == 0) {
				pen_x += delta.x >> 6;
/*
				if (pen_x < old_pen_x)
					old_pen_x= pen_x;
*/
			}
		}

		if (font->flags & BLF_USER_KERNING) {
			old_pen_x= pen_x;
			pen_x += font->kerning;
/*
			if (pen_x < old_pen_x)
				old_pen_x= pen_x;
*/
		}

		gbox.xmin= g->box.xmin + pen_x;
		gbox.xmax= g->box.xmax + pen_x;
		gbox.ymin= g->box.ymin + pen_y;
		gbox.ymax= g->box.ymax + pen_y;

		if (gbox.xmin < box->xmin)
			box->xmin= gbox.xmin;
		if (gbox.ymin < box->ymin)
			box->ymin= gbox.ymin;

		if (gbox.xmax > box->xmax)
			box->xmax= gbox.xmax;
		if (gbox.ymax > box->ymax)
			box->ymax= gbox.ymax;

		pen_x += g->advance;
		g_prev= g;
		g_prev_index= glyph_index;
	}

	if (box->xmin > box->xmax) {
		box->xmin= 0.0f;
		box->ymin= 0.0f;
		box->xmax= 0.0f;
		box->ymax= 0.0f;
	}
}

float blf_font_width(FontBLF *font, char *str)
{
	rctf box;

	if (!font->glyph_cache)
		return(0.0f);

	blf_font_boundbox(font, str, &box);
	return((box.xmax - box.xmin) * font->aspect);
}

float blf_font_height(FontBLF *font, char *str)
{
	rctf box;

	if (!font->glyph_cache)
		return(0.0f);

	blf_font_boundbox(font, str, &box);
	return((box.ymax - box.ymin) * font->aspect);
}

void blf_font_free(FontBLF *font)
{
	GlyphCacheBLF *gc;

	font->glyph_cache= NULL;
	while (font->cache.first) {
		gc= font->cache.first;
		BLI_remlink(&font->cache, gc);
		blf_glyph_cache_free(gc);
	}

	FT_Done_Face(font->face);
	if (font->filename)
		MEM_freeN(font->filename);
	if (font->name)
		MEM_freeN(font->name);
	MEM_freeN(font);
}

void blf_font_fill(FontBLF *font)
{
	font->mode= BLF_MODE_TEXTURE;
	font->aspect= 1.0f;
	font->pos[0]= 0.0f;
	font->pos[1]= 0.0f;
	font->angle= 0.0f;
	Mat4One(font->mat);
	font->clip_rec.xmin= 0.0f;
	font->clip_rec.xmax= 0.0f;
	font->clip_rec.ymin= 0.0f;
	font->clip_rec.ymax= 0.0f;
	font->flags= BLF_USER_KERNING | BLF_FONT_KERNING;
	font->dpi= 0;
	font->size= 0;
	font->kerning= 0.0f;
	font->cache.first= NULL;
	font->cache.last= NULL;
	font->glyph_cache= NULL;
	font->blur= 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&font->max_tex_size);
}

FontBLF *blf_font_new(char *name, char *filename)
{
	FontBLF *font;
	FT_Error err;
	char *mfile;

	font= (FontBLF *)MEM_mallocN(sizeof(FontBLF), "blf_font_new");
	err= FT_New_Face(global_ft_lib, filename, 0, &font->face);
	if (err) {
		MEM_freeN(font);
		return(NULL);
	}

	err= FT_Select_Charmap(font->face, ft_encoding_unicode);
	if (err) {
		printf("Can't set the unicode character map!\n");
		FT_Done_Face(font->face);
		MEM_freeN(font);
		return(NULL);
	}

	mfile= blf_dir_metrics_search(filename);
	if (mfile) {
		err= FT_Attach_File(font->face, mfile);
		MEM_freeN(mfile);
	}

	font->name= BLI_strdup(name);
	font->filename= BLI_strdup(filename);
	blf_font_fill(font);
	return(font);
}

void blf_font_attach_from_mem(FontBLF *font, const unsigned char *mem, int mem_size)
{
	FT_Open_Args open;

	open.flags= FT_OPEN_MEMORY;
	open.memory_base= (FT_Byte *)mem;
	open.memory_size= mem_size;
	FT_Attach_Stream(font->face, &open);
}

FontBLF *blf_font_new_from_mem(char *name, unsigned char *mem, int mem_size)
{
	FontBLF *font;
	FT_Error err;

	font= (FontBLF *)MEM_mallocN(sizeof(FontBLF), "blf_font_new_from_mem");
	err= FT_New_Memory_Face(global_ft_lib, mem, mem_size, 0, &font->face);
	if (err) {
		MEM_freeN(font);
		return(NULL);
	}

	err= FT_Select_Charmap(font->face, ft_encoding_unicode);
	if (err) {
		printf("Can't set the unicode character map!\n");
		FT_Done_Face(font->face);
		MEM_freeN(font);
		return(NULL);
	}

	font->name= BLI_strdup(name);
	font->filename= NULL;
	blf_font_fill(font);
	return(font);
}

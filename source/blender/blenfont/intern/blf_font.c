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

#include "BKE_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"	/* linknode */
#include "BLI_string.h"
#include "BLI_math.h"

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
	FT_UInt glyph_index;
	int pen_x, pen_y;
	int i, has_kerning, st;

	if (!font->glyph_cache)
		return;

	i= 0;
	pen_x= 0;
	pen_y= 0;
	has_kerning= FT_HAS_KERNING(font->face);
	g_prev= NULL;

	while (str[i]) {
		c= blf_utf8_next((unsigned char *)str, &i);
		if (c == 0)
			break;

		g= blf_glyph_search(font->glyph_cache, c);
		if (!g) {
			glyph_index= FT_Get_Char_Index(font->face, c);
			g= blf_glyph_add(font, glyph_index, c);
		}

		/* if we don't found a glyph, skip it. */
		if (!g)
			continue;

		if (has_kerning && g_prev) {
			delta.x= 0;
			delta.y= 0;

			if (font->flags & BLF_KERNING_DEFAULT)
				st= FT_Get_Kerning(font->face, g_prev->idx, g->idx, ft_kerning_default, &delta);
			else
				st= FT_Get_Kerning(font->face, g_prev->idx, g->idx, FT_KERNING_UNFITTED, &delta);

			if (st == 0)
				pen_x += delta.x >> 6;
		}

		/* do not return this loop if clipped, we want every character tested */
		blf_glyph_render(font, g, (float)pen_x, (float)pen_y);

		pen_x += g->advance;
		g_prev= g;
	}
}

void blf_font_buffer(FontBLF *font, char *str)
{
	unsigned char *data, *cbuf;
	unsigned int c;
	GlyphBLF *g, *g_prev;
	FT_Vector delta;
	FT_UInt glyph_index;
	float a, *fbuf;
	int pen_x, pen_y, y, x, yb, diff;
	int i, has_kerning, st, chx, chy;

	if (!font->glyph_cache)
		return;

	i= 0;
	pen_x= (int)font->pos[0];
	pen_y= (int)font->pos[1];
	has_kerning= FT_HAS_KERNING(font->face);
	g_prev= NULL;

	while (str[i]) {
		c= blf_utf8_next((unsigned char *)str, &i);
		if (c == 0)
			break;

		g= blf_glyph_search(font->glyph_cache, c);
		if (!g) {
			glyph_index= FT_Get_Char_Index(font->face, c);
			g= blf_glyph_add(font, glyph_index, c);
		}

		/* if we don't found a glyph, skip it. */
		if (!g)
			continue;

		if (has_kerning && g_prev) {
			delta.x= 0;
			delta.y= 0;

			if (font->flags & BLF_KERNING_DEFAULT)
				st= FT_Get_Kerning(font->face, g_prev->idx, g->idx, ft_kerning_default, &delta);
			else
				st= FT_Get_Kerning(font->face, g_prev->idx, g->idx, FT_KERNING_UNFITTED, &delta);

			if (st == 0)
				pen_x += delta.x >> 6;
		}

		chx= pen_x + ((int)g->pos_x);
		diff= g->height - ((int)g->pos_y);
		if (diff > 0) {
			if (g->pitch < 0)
				pen_y += diff;
			else
				pen_y -= diff;
		}
		else if (diff < 0) {
			if (g->pitch < 0)
				pen_y -= diff;
			else
				pen_y += diff;
		}

		if (g->pitch < 0)
			chy= pen_y - ((int)g->pos_y);
		else
			chy= pen_y + ((int)g->pos_y);

		if (font->b_fbuf) {
			if (chx >= 0 && chx < font->bw && pen_y >= 0 && pen_y < font->bh) {
				if (g->pitch < 0)
					yb= 0;
				else
					yb= g->height-1;

				for (y= 0; y < g->height; y++) {
					for (x= 0; x < g->width; x++) {
						
						a= *(g->bitmap + x + (yb * g->pitch)) / 255.0f;

						if(a > 0.0f) {
							fbuf= font->b_fbuf + font->bch * ((chx + x) + ((pen_y + y)*font->bw));
							if (a >= 1.0f) {
								fbuf[0]= font->b_col[0];
								fbuf[1]= font->b_col[1];
								fbuf[2]= font->b_col[2];
							}
							else {
								fbuf[0]= (font->b_col[0]*a) + (fbuf[0] * (1-a));
								fbuf[1]= (font->b_col[1]*a) + (fbuf[1] * (1-a));
								fbuf[2]= (font->b_col[2]*a) + (fbuf[2] * (1-a));
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

		if (font->b_cbuf) {
			if (chx >= 0 && chx < font->bw && pen_y >= 0 && pen_y < font->bh) {
				char b_col_char[3];
				b_col_char[0]= font->b_col[0] * 255;
				b_col_char[1]= font->b_col[1] * 255;
				b_col_char[2]= font->b_col[2] * 255;

				if (g->pitch < 0)
					yb= 0;
				else
					yb= g->height-1;

				for (y= 0; y < g->height; y++) {
					for (x= 0; x < g->width; x++) {
						a= *(g->bitmap + x + (yb * g->pitch)) / 255.0f;

						if(a > 0.0f) {
							cbuf= font->b_cbuf + font->bch * ((chx + x) + ((pen_y + y)*font->bw));
							if (a >= 1.0f) {
								cbuf[0]= b_col_char[0];
								cbuf[1]= b_col_char[1];
								cbuf[2]= b_col_char[2];
							}
							else {
								cbuf[0]= (b_col_char[0]*a) + (cbuf[0] * (1-a));
								cbuf[1]= (b_col_char[1]*a) + (cbuf[1] * (1-a));
								cbuf[2]= (b_col_char[2]*a) + (cbuf[2] * (1-a));
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

		if (diff > 0) {
			if (g->pitch < 0)
				pen_x -= diff;
			else
				pen_y += diff;
		}
		else if (diff < 0) {
			if (g->pitch < 0)
				pen_x += diff;
			else
				pen_y -= diff;
		}

		pen_x += g->advance;
		g_prev= g;
	}
}

void blf_font_boundbox(FontBLF *font, char *str, rctf *box)
{
	unsigned int c;
	GlyphBLF *g, *g_prev;
	FT_Vector delta;
	FT_UInt glyph_index;
	rctf gbox;
	int pen_x, pen_y;
	int i, has_kerning, st;

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

	while (str[i]) {
		c= blf_utf8_next((unsigned char *)str, &i);
		if (c == 0)
			break;

		g= blf_glyph_search(font->glyph_cache, c);
		if (!g) {
			glyph_index= FT_Get_Char_Index(font->face, c);
			g= blf_glyph_add(font, glyph_index, c);
		}

		/* if we don't found a glyph, skip it. */
		if (!g)
			continue;

		if (has_kerning && g_prev) {
			delta.x= 0;
			delta.y= 0;

			if (font->flags & BLF_KERNING_DEFAULT)
				st= FT_Get_Kerning(font->face, g_prev->idx, g->idx, ft_kerning_default, &delta);
			else
				st= FT_Get_Kerning(font->face, g_prev->idx, g->idx, FT_KERNING_UNFITTED, &delta);

			if (st == 0)
				pen_x += delta.x >> 6;
		}

		gbox.xmin= pen_x;
		gbox.xmax= pen_x + g->advance;
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
	}

	if (box->xmin > box->xmax) {
		box->xmin= 0.0f;
		box->ymin= 0.0f;
		box->xmax= 0.0f;
		box->ymax= 0.0f;
	}
}

void blf_font_width_and_height(FontBLF *font, char *str, float *width, float *height)
{
	rctf box;

	if (font->glyph_cache) {
		blf_font_boundbox(font, str, &box);
		*width= ((box.xmax - box.xmin) * font->aspect);
		*height= ((box.ymax - box.ymin) * font->aspect);
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

float blf_font_fixed_width(FontBLF *font)
{
	GlyphBLF *g;
	FT_UInt glyph_index;
	unsigned int c = ' ';

	if (!font->glyph_cache)
		return 0.0f;

	glyph_index= FT_Get_Char_Index(font->face, c);
	g= blf_glyph_search(font->glyph_cache, c);
	if (!g)
		g= blf_glyph_add(font, glyph_index, c);

	/* if we don't find the glyph. */
	if (!g)
		return 0.0f;
	
	return g->advance;
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

static void blf_font_fill(FontBLF *font)
{
	font->aspect= 1.0f;
	font->pos[0]= 0.0f;
	font->pos[1]= 0.0f;
	font->angle= 0.0f;
	unit_m4(font->mat);
	font->clip_rec.xmin= 0.0f;
	font->clip_rec.xmax= 0.0f;
	font->clip_rec.ymin= 0.0f;
	font->clip_rec.ymax= 0.0f;
	font->flags= 0;
	font->dpi= 0;
	font->size= 0;
	font->cache.first= NULL;
	font->cache.last= NULL;
	font->glyph_cache= NULL;
	font->blur= 0;
	font->max_tex_size= -1;
	font->b_fbuf= NULL;
	font->b_cbuf= NULL;
	font->bw= 0;
	font->bh= 0;
	font->bch= 0;
	font->b_col[0]= 0;
	font->b_col[1]= 0;
	font->b_col[2]= 0;
	font->b_col[3]= 0;
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

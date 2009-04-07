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
#include "BLI_arithb.h"

#include "BIF_gl.h"
#include "BLF_api.h"

#include "blf_internal_types.h"
#include "blf_internal.h"
#include "blf_font_helv10.h"

#ifndef BLF_INTERNAL_MINIMAL
#include "blf_font_helv12.h"
#include "blf_font_helvb8.h"
#include "blf_font_helvb10.h"
#include "blf_font_helvb12.h"
#include "blf_font_scr12.h"
#include "blf_font_scr14.h"
#include "blf_font_scr15.h"
#endif

int blf_internal_get_texture(FontBLF *font)
{
	FontDataBLF *data;
	CharDataBLF *cd;
	int width;
	int height;
	int c_rows, c_cols, c_width, c_height;
	int i_width, i_height;
	GLubyte *img, *img_row, *chr_row, *img_pxl;
	int base_line, i, cell_x, cell_y, y, x;
	int byte_idx, bit_idx;

	data= (FontDataBLF *)font->engine;
	if (data->texid != 0)
		return(0);

	width= data->xmax - data->xmin;
	height= data->ymax - data->ymin;
	c_rows= 16;
	c_cols= 16;
	c_width= 16;
	c_height= 16;
	i_width= c_cols * c_width;
	i_height= c_rows * c_height;
	base_line= -(data->ymin);
	img= (GLubyte *)malloc(i_height * i_width);
	memset((void *)img, 0, i_height * i_width);

	if (width >= 16 || height >= 16) {
		printf("Warning: Bad font size for: %s\n", font->name);
		return(-1);
	}

	for (i= 0; i < 256; i++) {
		cd= &data->chars[i];

		if (cd->data_offset != -1) {
			cell_x= i%16;
			cell_y= i/16;

			for (y= 0; y < cd->height; y++) {
				img_row = &img[(cell_y*c_height + y + base_line - cd->yorig)*i_width];
				chr_row = &data->bitmap_data[cd->data_offset + ((cd->width+7)/8)*y];

				for (x= 0; x < cd->width; x++) {
					img_pxl= &img_row[(cell_x*c_width + x - cd->xorig)];
					byte_idx= x/8;
					bit_idx= 7 - (x%8);

					if (chr_row[byte_idx]&(1<<bit_idx)) {
						img_pxl[0]= 255;
					}
				}
			}
		}
	}

	glGenTextures(1, &data->texid);
	glBindTexture(GL_TEXTURE_2D, data->texid);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA4, i_width, i_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, img);
	if (glGetError()) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE4_ALPHA4, i_width, i_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, img);
	}
	
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	free((void *)img);
	return(0);
}

void blf_internal_size(FontBLF *font, int size, int dpi)
{
	return;
}

void blf_internal_texture_draw(FontBLF *font, char *str)
{
	FontDataBLF *data;
	CharDataBLF *cd;
	unsigned char c;
	float pos, cell_x, cell_y, x, y, z;
	int base_line;
	GLint cur_tex;
	float dx, dx1, dy, dy1;

	data= (FontDataBLF *)font->engine;
	base_line= -(data->ymin);
	pos= 0;
	x= 0.0f;
	y= 0.0f;
	z= 0.0f;

	glGetIntegerv(GL_TEXTURE_2D_BINDING_EXT, &cur_tex);
	if (cur_tex != data->texid)
		glBindTexture(GL_TEXTURE_2D, data->texid);

	while ((c= (unsigned char) *str++)) {
		cd= &data->chars[c];

		if (cd->data_offset != -1) {
			cell_x= (c%16)/16.0;
			cell_y= (c/16)/16.0;

			dx= x + pos + 16.0;
			dx1= x + pos + 0.0;
			dy= -base_line + y + 0.0;
			dy1= -base_line + y + 16.0;

			if (font->flags & BLF_CLIPPING) {
				if (!BLI_in_rctf(&font->clip_rec, dx + font->pos[0], dy + font->pos[1]))
					return;
				if (!BLI_in_rctf(&font->clip_rec, dx + font->pos[0], dy1 + font->pos[1]))
					return;
				if (!BLI_in_rctf(&font->clip_rec, dx1 + font->pos[0], dy1 + font->pos[1]))
					return;
				if (!BLI_in_rctf(&font->clip_rec, dx1 + font->pos[0], dy + font->pos[1]))
					return;
			}

			glBegin(GL_QUADS);
			glTexCoord2f(cell_x + 1.0/16.0, cell_y);
			glVertex3f(dx, dy, z);

			glTexCoord2f(cell_x + 1.0/16.0, cell_y + 1.0/16.0);
			glVertex3f(dx, dy1, z);

			glTexCoord2f(cell_x, cell_y + 1.0/16.0);
			glVertex3f(dx1, dy1, z);

			glTexCoord2f(cell_x, cell_y);
			glVertex3f(dx1, dy, z);
			glEnd();
		}
		
		pos += cd->advance;
	}
}

void blf_internal_bitmap_draw(FontBLF *font, char *str)
{
	FontDataBLF *data;
	CharDataBLF *cd;
	unsigned char c;
	GLint alignment;

	data= (FontDataBLF *)font->engine;

	glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	
	while ((c= (unsigned char) *str++)) {
		cd= &data->chars[c];

		if (cd->data_offset==-1) {
			GLubyte nullBitmap= 0;
			glBitmap(1, 1, 0, 0, cd->advance, 0, &nullBitmap);	
		} else {
			GLubyte *bitmap= &data->bitmap_data[cd->data_offset];
			glBitmap(cd->width, cd->height, cd->xorig, cd->yorig, cd->advance, 0, bitmap);
		}
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
}

void blf_internal_draw(FontBLF *font, char *str)
{
	if (font->mode == BLF_MODE_BITMAP)
		blf_internal_bitmap_draw(font, str);
	else
		blf_internal_texture_draw(font, str);
}

void blf_internal_boundbox(FontBLF *font, char *str, rctf *box)
{
	FontDataBLF *data;
	unsigned char c;
	int length= 0;
	int ascent= 0;
	int descent= 0;
	int a=0, d=0;

	data= (FontDataBLF *)font->engine;
	while ((c= (unsigned char) *str++)) {
		length += data->chars[c].advance;
		d = data->chars[c].yorig;
		a = data->chars[c].height - data->chars[c].yorig;
		if (a > ascent)
			ascent= a;
		if (d > descent)
			descent= d;
	}
	box->xmin = (float)0;
	box->ymin = (float)-descent;
	box->xmax = (float)length;
	box->ymax = (float)ascent;
}

float blf_internal_width(FontBLF *font, char *str)
{
	FontDataBLF *data;
	unsigned char c;
	int length= 0;

	data= (FontDataBLF *)font->engine;
	while ((c= (unsigned char) *str++)) {
		length += data->chars[c].advance;
	}

	return((float)(length * font->aspect));
}

float blf_internal_height(FontBLF *font, char *str)
{
	FontDataBLF *data;

	data= (FontDataBLF *)font->engine;
	return(((float)(data->ymax - data->ymin)) * font->aspect);
}

void blf_internal_free(FontBLF *font)
{
	FontDataBLF *data;

	data= (FontDataBLF *)font->engine;
	if (data->texid != 0) {
		glDeleteTextures(1, &data->texid);
		data->texid= 0;
	}

	MEM_freeN(font->name);
	MEM_freeN(font);
}

FontBLF *blf_internal_new(char *name)
{
	FontBLF *font;

	font= (FontBLF *)MEM_mallocN(sizeof(FontBLF), "blf_internal_new");
	font->name= BLI_strdup(name);
	font->filename= NULL;

	if (!strcmp(name, "helv10")) {
		font->engine= (void *)&blf_font_helv10;
		font->size= 10;
	}
#ifndef BLF_INTERNAL_MINIMAL
	else if (!strcmp(name, "helv12")) {
		font->engine= (void *)&blf_font_helv12;
		font->size= 12;
	}
	else if (!strcmp(name, "helvb8")) {
		font->engine= (void *)&blf_font_helvb8;
		font->size= 8;
	}
	else if (!strcmp(name, "helvb10")) {
		font->engine= (void *)&blf_font_helvb10;
		font->size= 10;
	}
	else if (!strcmp(name, "helvb12")) {
		font->engine= (void *)&blf_font_helvb12;
		font->size= 12;
	}
	else if (!strcmp(name, "scr12")) {
		font->engine= (void *)&blf_font_scr12;
		font->size= 12;
	}
	else if (!strcmp(name, "scr14")) {
		font->engine= (void *)&blf_font_scr14;
		font->size= 14;
	}
	else if (!strcmp(name, "scr15")) {
		font->engine= (void *)&blf_font_scr15;
		font->size= 15;
	}
#endif
	else
		font->engine= NULL;

	if (!font->engine) {
		MEM_freeN(font->name);
		MEM_freeN(font);
		return(NULL);
	}

	font->type= BLF_FONT_INTERNAL;
	font->ref= 1;
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
	font->flags= 0;
	font->dpi= 72;
	font->cache.first= NULL;
	font->cache.last= NULL;
	font->glyph_cache= NULL;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&font->max_tex_size);

	font->size_set= blf_internal_size;
	font->draw= blf_internal_draw;
	font->boundbox_get= blf_internal_boundbox;
	font->width_get= blf_internal_width;
	font->height_get= blf_internal_height;
	font->free= blf_internal_free;

	if (font->mode == BLF_MODE_TEXTURE) {
		if (blf_internal_get_texture(font) != 0) {
			MEM_freeN(font->name);
			MEM_freeN(font);
			return(NULL);
		}
	}

	return(font);
}

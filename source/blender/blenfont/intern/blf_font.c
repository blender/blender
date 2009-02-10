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


/* freetype2 handle. */
FT_Library global_ft_lib;

int blf_font_init(void)
{
	return(FT_Init_FreeType(&global_ft_lib));
}

void blf_font_exit(void)
{
	FT_Done_Freetype(global_ft_lib);
}

FontBLF *blf_font_new(char *name)
{
	FontBLF *font;
	FT_Error err;

	font= (FontBLF *)MEM_mallocN(sizeof(FontBLF), "blf_font_new");
	err= FT_New_Face(global_ft_lib, name, 0, &font->face);
	if (err) {
		MEM_freeN(font);
		return(NULL);
	}

	err= FT_Select_Charmap(font->face, ft_encoding_unicode);
	if (err) {
		printf("Warning: FT_Select_Charmap fail!!\n");
		FT_Done_Face(font->face);
		MEM_freeN(font);
		return(NULL);
	}

	font->name= MEM_strdup(name);
	font->ref= 1;
	font->aspect= 1.0f;
	font->pos[0]= 0.0f;
	font->pos[1]= 0.0f;
	font->angle[0]= 0.0f;
	font->angle[1]= 0.0f;
	font->angle[2]= 0.0f;
	Mat4One(font->mat);
	font->clip_rec.xmin= 0.0f;
	font->clip_rec.xmax= 0.0f;
	font->clip_rec.ymin= 0.0f;
	font->clip_rec.ymax= 0.0f;
	font->clip_mode= BLF_CLIP_DISABLE;
	font->dpi= 0;
	font->size= 0;
	font->cache.first= NULL;
	font->cache.last= NULL;
	font->glyph_cache= NULL;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&font->max_tex_size);
	return(font);
}

void blf_font_size(FontBLF *font, int size, int dpi)
{
	GlyphCacheBLF *gc;
	FT_Error err;
	
	err= FT_Set_Char_Size(font->face, 0, (size * 64), dpi, dpi);
	if (err) {
		/* FIXME: here we can go through the fixed size and choice a close one */
		printf("Warning: The current face don't support the size (%d) and dpi (%d)\n", size, dpi);
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
	}
}

#endif /* zero!! */

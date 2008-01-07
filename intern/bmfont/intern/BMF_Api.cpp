/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 *
 * Implementation of the API of the OpenGL bitmap font library.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BMF_Api.h"

#include "BMF_BitmapFont.h"


#if BMF_INCLUDE_HELV10
extern BMF_FontData BMF_font_helv10;
static BMF_BitmapFont bmfHelv10(&BMF_font_helv10);
#endif // BMF_INCLUDE_HELV10
#if BMF_INCLUDE_HELV12
extern BMF_FontData BMF_font_helv12;
static BMF_BitmapFont bmfHelv12(&BMF_font_helv12);
#endif // BMF_INCLUDE_HELV12
#if BMF_INCLUDE_HELVB8
extern BMF_FontData BMF_font_helvb8;
static BMF_BitmapFont bmfHelvb8(&BMF_font_helvb8);
#endif // BMF_INCLUDE_HELVB8
#if BMF_INCLUDE_HELVB10
extern BMF_FontData BMF_font_helvb10;
static BMF_BitmapFont bmfHelvb10(&BMF_font_helvb10);
#endif // BMF_INCLUDE_HELVB10
#if BMF_INCLUDE_HELVB12
extern BMF_FontData BMF_font_helvb12;
static BMF_BitmapFont bmfHelvb12(&BMF_font_helvb12);
#endif // BMF_INCLUDE_HELVB12
#if BMF_INCLUDE_HELVB14
extern BMF_FontData BMF_font_helvb14;
static BMF_BitmapFont bmfHelvb14(&BMF_font_helvb14);
#endif // BMF_INCLUDE_HELVB14
#if BMF_INCLUDE_SCR12
extern BMF_FontData BMF_font_scr12;
static BMF_BitmapFont bmfScreen12(&BMF_font_scr12);
#endif // BMF_INCLUDE_SCR12
#if BMF_INCLUDE_SCR14
extern BMF_FontData BMF_font_scr14;
static BMF_BitmapFont bmfScreen14(&BMF_font_scr14);
#endif // BMF_INCLUDE_SCR14
#if BMF_INCLUDE_SCR15
extern BMF_FontData BMF_font_scr15;
static BMF_BitmapFont bmfScreen15(&BMF_font_scr15);
#endif // BMF_INCLUDE_SCR15


BMF_Font* BMF_GetFont(BMF_FontType font)
{
	switch (font)
	{
#if BMF_INCLUDE_HELV10
	case BMF_kHelvetica10:	return (BMF_Font*) &bmfHelv10;
#endif // BMF_INCLUDE_HELV10
#if BMF_INCLUDE_HELV12
	case BMF_kHelvetica12:	return (BMF_Font*) &bmfHelv12;
#endif // BMF_INCLUDE_HELV12
#if BMF_INCLUDE_HELVB8
	case BMF_kHelveticaBold8:	return (BMF_Font*) &bmfHelvb8;
#endif // BMF_INCLUDE_HELVB8
#if BMF_INCLUDE_HELVB10
	case BMF_kHelveticaBold10:	return (BMF_Font*) &bmfHelvb10;
#endif // BMF_INCLUDE_HELVB10
#if BMF_INCLUDE_HELVB12
	case BMF_kHelveticaBold12:	return (BMF_Font*) &bmfHelvb12;
#endif // BMF_INCLUDE_HELVB12
#if BMF_INCLUDE_HELVB14
	case BMF_kHelveticaBold14:	return (BMF_Font*) &bmfHelvb14;
#endif // BMF_INCLUDE_HELVB12
#if BMF_INCLUDE_SCR12
	case BMF_kScreen12:	return (BMF_Font*) &bmfScreen12;
#endif // BMF_INCLUDE_SCR12
#if BMF_INCLUDE_SCR14
	case BMF_kScreen14:	return (BMF_Font*) &bmfScreen14;
#endif // BMF_INCLUDE_SCR14
#if BMF_INCLUDE_SCR15
	case BMF_kScreen15:	return (BMF_Font*) &bmfScreen15;
#endif // BMF_INCLUDE_SCR15
	default:
		break;
	}
	return 0;
}


int BMF_DrawCharacter(BMF_Font* font, char c)
{
	char str[2] = {c, '\0'};
	return BMF_DrawString(font, str);
}


int BMF_DrawString(BMF_Font* font, char* str)
{
	if (!font) return 0;
	((BMF_BitmapFont*)font)->DrawString(str);
	return 1;
}


int BMF_GetCharacterWidth(BMF_Font* font, char c)
{
	char str[2] = {c, '\0'};
	return BMF_GetStringWidth(font, str);
}


int BMF_GetStringWidth(BMF_Font* font, char* str)
{
	if (!font) return 0;
	return ((BMF_BitmapFont*)font)->GetStringWidth(str);
}

void BMF_GetStringBoundingBox(BMF_Font* font, char* str, float*llx, float *lly, float *urx, float *ury){
	if (!font){
		*llx = *lly = *urx = *ury = 0;
	}else{
		((BMF_BitmapFont*)font)->GetStringBoundingBox(str, llx, lly, urx, ury);
	}
}

void BMF_GetFontBoundingBox(BMF_Font* font, int *xmin_r, int *ymin_r, int *xmax_r, int *ymax_r)
{
	if (!font) return;
	((BMF_BitmapFont*)font)->GetFontBoundingBox(*xmin_r, *ymin_r, *xmax_r, *ymax_r);
}

int BMF_GetFontHeight(BMF_Font* font)
{
	if (!font) return -1;
	return ((BMF_BitmapFont*)font)->GetFontHeight();
}

int BMF_GetFontTexture(BMF_Font* font) {
	if (!font) return -1;
	return ((BMF_BitmapFont*)font)->GetTexture();
}

void BMF_DrawStringTexture(BMF_Font* font, char *string, float x, float y, float z) {
	if (!font) return;
	((BMF_BitmapFont*)font)->DrawStringTexture(string, x, y, z);
}

void BMF_DrawStringBuf(BMF_Font* font, char *str, int posx, int posy, float *col, unsigned char *buf, float *fbuf, int w, int h) {
	if (!font) return;
	((BMF_BitmapFont*)font)->DrawStringBuf(str, posx, posy, col, buf, fbuf, w, h);
}

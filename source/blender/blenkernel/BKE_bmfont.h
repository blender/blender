/**
 * blenlib/BKE_bmfont.h (mar-2001 nzc)
 *
 *
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_BMFONT_H
#define BKE_BMFONT_H

#ifdef __cplusplus
extern "C" {
#endif

struct bmGlyph;
struct ImBuf;
struct bmFont;

void printfGlyph(struct bmGlyph * glyph);
void calcAlpha(struct ImBuf * ibuf);
void readBitmapFontVersion0(struct ImBuf * ibuf,
								   unsigned char * rect,
								   int step);
void detectBitmapFont(struct ImBuf *ibuf);
int locateGlyph(struct bmFont *bmfont, unsigned short unicode);
void matrixGlyph(struct ImBuf * ibuf, unsigned short unicode,
				 float *centerx, float *centery,
				 float *sizex,   float *sizey,
				 float *transx,  float *transy,
				 float *movex,   float *movey, float *advance); 

#ifdef __cplusplus
}
#endif
	
#endif


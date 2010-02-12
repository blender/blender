/**
 * blenlib/BKE_bmfont_types.h (mar-2001 nzc)
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
#ifndef BKE_BMFONT_TYPES_H
#define BKE_BMFONT_TYPES_H

#define is_power_of_two(N) ((N ^ (N - 1)) == (2 * N - 1))
/*
Moved to IMB_imbuf_types.h where it will live close to the ImBuf type.
It is used as a userflag bit mask.
#define IB_BITMAPFONT 1
*/
typedef struct bmGlyph {
	unsigned short unicode;
	short locx, locy;
	signed char ofsx, ofsy;
	unsigned char sizex, sizey;
	unsigned char advance, reserved;
} bmGlyph;

typedef struct bmFont {
	char magic[4];
	short version;
	short glyphcount;
	short xsize, ysize;
	bmGlyph glyphs[1];
} bmFont;

#endif


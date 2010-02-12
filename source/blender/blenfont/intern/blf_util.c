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

#include "BIF_gl.h"


unsigned int blf_next_p2(unsigned int x)
{
	x -= 1;
	x |= (x >> 16);
	x |= (x >> 8);
	x |= (x >> 4);
	x |= (x >> 2);
	x |= (x >> 1);
	x += 1;
	return(x);
}

unsigned int blf_hash(unsigned int val)
{
	unsigned int key;

	key= val;
	key += ~(key << 16);
	key ^= (key >> 5);
	key += (key << 3);
	key ^= (key >> 13);
	key += ~(key << 9);
	key ^= (key >> 17);
	return(key % 257);
}

/*
 * This function is from Imlib2 library (font_main.c), a
 * library that does image file loading and saving as well
 * as rendering, manipulation, arbitrary polygon support, etc.
 *
 * Copyright (C) 2000 Carsten Haitzler and various contributors
 * The original name: imlib_font_utf8_get_next
 * more info here: http://docs.enlightenment.org/api/imlib2/html/
 */
int blf_utf8_next(unsigned char *buf, int *iindex)
{
	/* Reads UTF8 bytes from 'buf', starting at 'index' and
	 * returns the code point of the next valid code point.
	 * 'index' is updated ready for the next call.
	 *
	 * Returns 0 to indicate an error (e.g. invalid UTF8)
	 */
	int index= *iindex, len, r;
	unsigned char d, d2, d3, d4;

	d= buf[index++];
	if (!d)
		return(0);

	while (buf[index] && ((buf[index] & 0xc0) == 0x80))
		index++;

	len= index - *iindex;
	if (len == 1)
		r= d;
	else if (len == 2) {
		/* 2 byte */
		d2= buf[*iindex + 1];
		r= d & 0x1f; /* copy lower 5 */
		r <<= 6;
		r |= (d2 & 0x3f); /* copy lower 6 */
	}
	else if (len == 3) {
		/* 3 byte */
		d2= buf[*iindex + 1];
		d3= buf[*iindex + 2];
		r= d & 0x0f; /* copy lower 4 */
		r <<= 6;
		r |= (d2 & 0x3f);
		r <<= 6;
		r |= (d3 & 0x3f);
	}
	else {
		/* 4 byte */
		d2= buf[*iindex + 1];
		d3= buf[*iindex + 2];
		d4= buf[*iindex + 3];
		r= d & 0x0f; /* copy lower 4 */
		r <<= 6;
		r |= (d2 & 0x3f);
		r <<= 6;
		r |= (d3 & 0x3f);
		r <<= 6;
		r |= (d4 & 0x3f);
	}
	*iindex= index;
	return(r);
}

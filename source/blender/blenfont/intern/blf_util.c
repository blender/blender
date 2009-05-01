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
	int index= *iindex, r;
	unsigned char d= buf[index++], d2, d3, d4;

	if (!d)
		return(0);

	if (d < 0x80) {
		*iindex= index;
		return(d);
	}

	if ((d & 0xe0) == 0xc0) {
		/* 2 byte */
		d2= buf[index++];
		if ((d2 & 0xc0) != 0x80)
			return(0);
		r= d & 0x1f; /* copy lower 5 */
		r <<= 6;
		r |= (d2 & 0x3f); /* copy lower 6 */
	}
	else if ((d & 0xf0) == 0xe0) {
		/* 3 byte */
		d2= buf[index++];
		d3= buf[index++];

		if ((d2 & 0xc0) != 0x80 || (d3 & 0xc0) != 0x80)
			return(0);

		r= d & 0x0f; /* copy lower 4 */
		r <<= 6;
		r |= (d2 & 0x3f);
		r <<= 6;
		r |= (d3 & 0x3f);
	}
	else {
		/* 4 byte */
		d2= buf[index++];
		d3= buf[index++];
		d4= buf[index++];

		if ((d2 & 0xc0) != 0x80 || (d3 & 0xc0) != 0x80 ||
		    (d4 & 0xc0) != 0x80)
			return(0);

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

void blf_texture_draw(float uv[2][2], float dx, float y1, float dx1, float y2)
{
	
	glBegin(GL_QUADS);
	glTexCoord2f(uv[0][0], uv[0][1]);
	glVertex2f(dx, y1);
	
	glTexCoord2f(uv[0][0], uv[1][1]);
	glVertex2f(dx, y2);
	
	glTexCoord2f(uv[1][0], uv[1][1]);
	glVertex2f(dx1, y2);
	
	glTexCoord2f(uv[1][0], uv[0][1]);
	glVertex2f(dx1, y1);
	glEnd();
	
}

void blf_texture5_draw(float uv[2][2], float x1, float y1, float x2, float y2)
{
	float soft[25]= {
		1/60.0f, 1/60.0f, 2/60.0f, 1/60.0f, 1/60.0f, 
		1/60.0f, 3/60.0f, 5/60.0f, 3/60.0f, 1/60.0f, 
		2/60.0f, 5/60.0f, 8/60.0f, 5/60.0f, 2/60.0f, 
		1/60.0f, 3/60.0f, 5/60.0f, 3/60.0f, 1/60.0f, 
		1/60.0f, 1/60.0f, 2/60.0f, 1/60.0f, 1/60.0f};
	
	float color[4], *fp= soft;
	int dx, dy;
	
	glGetFloatv(GL_CURRENT_COLOR, color);
	
	for(dx=-2; dx<3; dx++) {
		for(dy=-2; dy<3; dy++, fp++) {
			glColor4f(color[0], color[1], color[2], fp[0]*color[3]);
			blf_texture_draw(uv, x1+dx, y1+dy, x2+dx, y2+dy);
		}
	}
	
	glColor4fv(color);
}

void blf_texture3_draw(float uv[2][2], float x1, float y1, float x2, float y2)
{
	float soft[9]= {1/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 4/16.0f, 2/16.0f, 1/16.0f, 2/16.0f, 1/16.0f};
	float color[4], *fp= soft;
	int dx, dy;
	
	glGetFloatv(GL_CURRENT_COLOR, color);
	
	for(dx=-1; dx<2; dx++) {
		for(dy=-1; dy<2; dy++, fp++) {
			glColor4f(color[0], color[1], color[2], fp[0]*color[3]);
			blf_texture_draw(uv, x1+dx, y1+dy, x2+dx, y2+dy);
		}
	}
	
	glColor4fv(color);
}

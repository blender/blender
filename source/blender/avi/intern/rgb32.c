/*
 *
 * This is external code. Converts between rgb32 and avi.
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
 *
 */

/** \file blender/avi/intern/rgb32.c
 *  \ingroup avi
 */


#include "AVI_avi.h"
#include <stdlib.h>
#include <string.h>
#include "MEM_guardedalloc.h"
#include "rgb32.h"

void *avi_converter_from_rgb32 (AviMovie *movie, int stream, unsigned char *buffer, int *size)
{
	int y, x, rowstridea, rowstrideb;
	unsigned char *buf;

	(void)stream; /* unused */

	buf = MEM_mallocN (movie->header->Height * movie->header->Width * 3, "fromrgb32buf");
	*size = movie->header->Height * movie->header->Width * 3;

	rowstridea = movie->header->Width*3;
	rowstrideb = movie->header->Width*4;

	for (y=0; y < movie->header->Height; y++) {
		for (x=0; x < movie->header->Width; x++) {
			buf[y*rowstridea + x*3 + 0] = buffer[y*rowstrideb + x*4 + 3];
			buf[y*rowstridea + x*3 + 1] = buffer[y*rowstrideb + x*4 + 2];
			buf[y*rowstridea + x*3 + 2] = buffer[y*rowstrideb + x*4 + 1];
		}
	}

	MEM_freeN (buffer);

	return buf;
}

void *avi_converter_to_rgb32 (AviMovie *movie, int stream, unsigned char *buffer, int *size)
{
	int i;
	unsigned char *buf;
	unsigned char *to, *from;

	(void)stream; /* unused */

	buf= MEM_mallocN (movie->header->Height * movie->header->Width * 4, "torgb32buf");
	*size= movie->header->Height * movie->header->Width * 4;

	memset(buf, 255, *size);

	to= buf; from= buffer;
	i=movie->header->Height*movie->header->Width;
	
	while(i--) {
		memcpy(to, from, 3);
		to+=4; from+=3;
	}

	MEM_freeN (buffer);

	return buf;
}

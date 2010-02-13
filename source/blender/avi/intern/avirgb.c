/**
 * avirgb.c
 *
 * This is external code. Converts rgb-type avi-s.
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "AVI_avi.h"
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "avirgb.h"

#if defined(__sgi) || defined (__sparc) || defined (__sparc__) || defined (__PPC__) || defined (__ppc__) || defined (__hppa__) || defined (__BIG_ENDIAN__)
#define WORDS_BIGENDIAN
#endif


/* implementation */

void *avi_converter_from_avi_rgb (AviMovie *movie, int stream, unsigned char *buffer, int *size) {
	int x, y,i, rowstride;
	unsigned char *buf;
	AviBitmapInfoHeader *bi;
	short bits= 32;

	bi= (AviBitmapInfoHeader *) movie->streams[stream].sf;
	if (bi) bits= bi->BitCount;

	if (bits==16) {
		unsigned short *pxl;
		unsigned char *to;
		#ifdef WORDS_BIGENDIAN
		unsigned char  *pxla;
		#endif		  
		
		buf = MEM_mallocN (movie->header->Height * movie->header->Width * 3, "fromavirgbbuf");

		y= movie->header->Height;
		to= buf;
				
		while (y--) {
			pxl= (unsigned short *) (buffer + y * movie->header->Width * 2);
			
			#ifdef WORDS_BIGENDIAN
			pxla= (unsigned char *)pxl;
			#endif

			x= movie->header->Width;
			while (x--) {
				#ifdef WORDS_BIGENDIAN
				i= pxla[0];
				pxla[0]= pxla[1];
				pxla[1]= i;
	
				pxla+=2;
				#endif
			
				*(to++)= ((*pxl>>10)&0x1f)*8;
				*(to++)= ((*pxl>>5)&0x1f)*8;
				*(to++)= (*pxl&0x1f)*8;
				pxl++;	
			}
		}

		MEM_freeN (buffer);
		
		return buf;
	} else {
		buf = MEM_mallocN (movie->header->Height * movie->header->Width * 3, "fromavirgbbuf");
	
		rowstride = movie->header->Width*3;
		if (bits!=16) if (movie->header->Width%2) rowstride++;
	
		for (y=0; y < movie->header->Height; y++) {
			memcpy (&buf[y*movie->header->Width*3], &buffer[((movie->header->Height-1)-y)*rowstride], movie->header->Width*3);
		}
	
		for (y=0; y < movie->header->Height*movie->header->Width*3; y+=3) {
			i = buf[y];
			buf[y] = buf[y+2];
			buf[y+2] = i;
		}
	
		MEM_freeN (buffer);
	
		return buf;
	}
}

void *avi_converter_to_avi_rgb (AviMovie *movie, int stream, unsigned char *buffer, int *size) {
	int y, x, i, rowstride;
	unsigned char *buf;

	*size= movie->header->Height * movie->header->Width * 3;
	if (movie->header->Width%2) *size+= movie->header->Height;
	
	buf = MEM_mallocN (*size,"toavirgbbuf");

	rowstride = movie->header->Width*3;
	if (movie->header->Width%2) rowstride++;

	for (y=0; y < movie->header->Height; y++) {
		memcpy (&buf[y*rowstride], &buffer[((movie->header->Height-1)-y)*movie->header->Width*3], movie->header->Width*3);
	}

	for (y=0; y < movie->header->Height; y++) {
		for (x=0; x < movie->header->Width*3; x+=3) {
			i = buf[y*rowstride+x];
			buf[y*rowstride+x] = buf[y*rowstride+x+2];
			buf[y*rowstride+x+2] = i;
		}
	}

	MEM_freeN (buffer);

	return buf;
}

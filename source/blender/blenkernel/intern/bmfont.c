/*
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
 * Contributor(s): 04-10-2000 frank.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/blenkernel/intern/bmfont.c
 *  \ingroup bke
 */


/**
 * Two external functions:
 *
 * void detectBitmapFont(ImBuf *ibuf)
 *   detects if an image buffer contains a bitmap font. It makes the
 *   specific bitmap data which is stored in the bitmap invisible to blender.
 *
 * void matrixGlyph(ImBuf * ibuf, unsigned short unicode, *float x 7)
 *   returns all the information about the character (unicode) in the floats
 *
 * Room for improvement:
 *   add kerning data in the bitmap
 *   all calculations in matrixGlyph() are static and could be done during
 *     initialization
 */

#include <stdio.h>

#include "MEM_guardedalloc.h"
#include "BKE_global.h"
#include "IMB_imbuf_types.h"

#include "BKE_bmfont.h"
#include "BKE_bmfont_types.h"

void printfGlyph(bmGlyph * glyph)
{
	printf("unicode: %d '%c'\n", glyph->unicode, glyph->unicode);
	printf(" locx: %4d locy: %4d\n", glyph->locx, glyph->locy);
	printf(" sizex: %3d sizey: %3d\n", glyph->sizex, glyph->sizey);
	printf(" ofsx:  %3d ofsy:  %3d\n", glyph->ofsx, glyph->ofsy);
	printf(" advan: %3d reser: %3d\n", glyph->advance, glyph->reserved);
}

#define MAX2(x,y)          ( (x)>(y) ? (x) : (y) )
#define MAX3(x,y,z)                MAX2( MAX2((x),(y)) , (z) )  

void calcAlpha(ImBuf * ibuf)
{
	int i;
	char * rect;
	
	if (ibuf) {
		rect = (char *) ibuf->rect;
		for (i = ibuf->x * ibuf->y ; i > 0 ; i--) {
			rect[3] = MAX3(rect[0], rect[1], rect[2]);
			rect += 4;
		}
	}
}

void readBitmapFontVersion0(ImBuf * ibuf, unsigned char * rect, int step)
{
	int glyphcount, bytes, i, index, linelength, ysize;
	unsigned char * buffer;
	bmFont * bmfont;
	
	linelength = ibuf->x * step;
	
	glyphcount = (rect[6 * step] << 8) | rect[7 * step];
	bytes = ((glyphcount - 1) * sizeof(bmGlyph)) + sizeof(bmFont);
	
	ysize = (bytes + (ibuf->x - 1)) / ibuf->x;
	
	if (ysize < ibuf->y) {
		// we're first going to copy all data into a liniar buffer.
		// step can be 4 or 1 bytes, and the data is not sequential because
		// the bitmap was flipped vertically.
		
		buffer = MEM_mallocN(bytes, "readBitmapFontVersion0:buffer");
		
		index = 0;	
		for (i = 0; i < bytes; i++) {
			buffer[i] = rect[index];
			index += step;
			if (index >= linelength) {
				// we've read one line, no skip to the line *before* that
				rect -= linelength;
				index -= linelength;
			}
		}
		
		// we're now going to endian convert the data
		
		bmfont = MEM_mallocN(bytes, "readBitmapFontVersion0:bmfont");
		index = 0;
		
		// first read the header
		bmfont->magic[0]    = buffer[index++];
		bmfont->magic[1]    = buffer[index++];
		bmfont->magic[2]    = buffer[index++];
		bmfont->magic[3]    = buffer[index++];
		bmfont->version     = (buffer[index] << 8) | buffer[index + 1]; index += 2;
		bmfont->glyphcount  = (buffer[index] << 8) | buffer[index + 1]; index += 2;
		bmfont->xsize       = (buffer[index] << 8) | buffer[index + 1]; index += 2;
		bmfont->ysize       = (buffer[index] << 8) | buffer[index + 1]; index += 2;
		
		for (i = 0; i < bmfont->glyphcount; i++) {
			bmfont->glyphs[i].unicode  = (buffer[index] << 8) | buffer[index + 1]; index += 2;
			bmfont->glyphs[i].locx     = (buffer[index] << 8) | buffer[index + 1]; index += 2;
			bmfont->glyphs[i].locy     = (buffer[index] << 8) | buffer[index + 1]; index += 2;
			bmfont->glyphs[i].ofsx     = buffer[index++];
			bmfont->glyphs[i].ofsy     = buffer[index++];
			bmfont->glyphs[i].sizex    = buffer[index++];
			bmfont->glyphs[i].sizey    = buffer[index++];
			bmfont->glyphs[i].advance  = buffer[index++];
			bmfont->glyphs[i].reserved = buffer[index++];
			if (G.f & G_DEBUG) {
				printfGlyph(&bmfont->glyphs[i]);
			}
		}
		
		MEM_freeN(buffer);
		
		if (G.f & G_DEBUG) {
			printf("Oldy = %d Newy = %d\n", ibuf->y, ibuf->y - ysize);
			printf("glyphcount = %d\n", glyphcount);
			printf("bytes = %d\n", bytes);
		}

		// we've read the data from the image. Now we're going
		// to crop the image vertically so only the bitmap data
		// remains visible
		
		ibuf->y -= ysize;
		ibuf->userdata = bmfont;
		ibuf->userflags |= IB_BITMAPFONT;

		if (ibuf->planes < 32) {
			// we're going to fake alpha here:
			calcAlpha(ibuf);
		}
	}
	else {
		printf("readBitmapFontVersion0: corrupted bitmapfont\n");
	}
}

void detectBitmapFont(ImBuf *ibuf)
{
	unsigned char * rect;
	unsigned short version;
	int i;
	
	if (ibuf != NULL && ibuf->rect != NULL) {
			// bitmap must have an x size that is a power of two
		if (is_power_of_two(ibuf->x)) {
			rect = (unsigned char *) (ibuf->rect + (ibuf->x * (ibuf->y - 1)));
			// printf ("starts with: %s %c %c %c %c\n", rect, rect[0], rect[1], rect[2], rect[3]);
			if (rect[0] == 'B' && rect[1] == 'F' && rect[2] == 'N' && rect[3] == 'T') {
				// printf("found 8bit font !\n");
				// round y size down
				// do the 8 bit font stuff. (not yet)
			}
			else {
				// we try all 4 possible combinations
				for (i = 0; i < 4; i++) {
					if (rect[0] == 'B' && rect[4] == 'F' && rect[8] == 'N' && rect[12] == 'T') {
						// printf("found 24bit font !\n");
						// We're going to parse the file:
						
						version = (rect[16] << 8) | rect[20];
						
						if (version == 0) {
							readBitmapFontVersion0(ibuf, rect, 4);
						}
						else {
							printf("detectBitmapFont :Unsupported version %d\n", version);
						}
						
						// on succes ibuf->userdata points to the bitmapfont
						if (ibuf->userdata) {
							break;
						}
					}
					rect++;
				}
			}
		}
	}
}

int locateGlyph(bmFont *bmfont, unsigned short unicode)
{
	int min, max, current = 0;
	
	if (bmfont) {
		min = 0;
		max = bmfont->glyphcount;
		while (1) {
			// look halfway for glyph
			current = (min + max) >> 1;

			if (bmfont->glyphs[current].unicode == unicode) {
				break;
			}
			else if (bmfont->glyphs[current].unicode < unicode) {
				// have to move up
				min = current;
			}
			else {
				// have to move down
				max = current;
			}
			
			if (max - min <= 1) {
				// unable to locate glyph
				current = 0;
				break;
			}
		}
	}
	
	return(current);
}

void matrixGlyph(ImBuf * ibuf, unsigned short unicode,
		float *centerx, float *centery,
		float *sizex,   float *sizey,
		float *transx,  float *transy,
		float *movex,   float *movey,
		float *advance)
{
	int index;
	bmFont *bmfont;
	
	*centerx = *centery = 0.0;
	*sizex = *sizey = 1.0;
	*transx = *transy = 0.0;
	*movex = *movey = 0.0;
	*advance = 1.0;
		
	if (ibuf) {
		bmfont = ibuf->userdata;
		if (bmfont && (ibuf->userflags & IB_BITMAPFONT)) {
			index = locateGlyph(bmfont, unicode);
			if (index) {
								
				*sizex = (bmfont->glyphs[index].sizex) / (float) (bmfont->glyphs[0].sizex);
				*sizey = (bmfont->glyphs[index].sizey) / (float) (bmfont->glyphs[0].sizey);

				*transx = bmfont->glyphs[index].locx / (float) ibuf->x;
				*transy = (ibuf->y - bmfont->glyphs[index].locy) / (float) ibuf->y;

				*centerx = bmfont->glyphs[0].locx / (float) ibuf->x;
				*centery = (ibuf->y - bmfont->glyphs[0].locy) / (float) ibuf->y;

				// 2.0 units is the default size of an object
				
				*movey = 1.0f - *sizey + 2.0f * (bmfont->glyphs[index].ofsy - bmfont->glyphs[0].ofsy) / (float) bmfont->glyphs[0].sizey;
				*movex = *sizex - 1.0f + 2.0f * (bmfont->glyphs[index].ofsx - bmfont->glyphs[0].ofsx) / (float) bmfont->glyphs[0].sizex;
				
				*advance = 2.0f * bmfont->glyphs[index].advance / (float) bmfont->glyphs[0].advance;

				// printfGlyph(&bmfont->glyphs[index]);
				// printf("%c %d %0.5f %0.5f %0.5f %0.5f %0.5f \n", unicode, index, *sizex, *sizey, *transx, *transy, *advance);
			}
		}
	}
}

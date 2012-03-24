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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
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
 * cineon.c
 * contributors: joeedh
 * I hearby donate this code and all rights to the Blender Foundation.
 */

/** \file blender/imbuf/intern/cineon/cineon_dpx.c
 *  \ingroup imbcineon
 */

 
#include <stdio.h>
#include <string.h> /*for memcpy*/

#include "logImageLib.h"
#include "cineonlib.h"
#include "dpxlib.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_filetype.h"

#include "BKE_global.h"

#include "MEM_guardedalloc.h"

#if 0
static void cineon_conversion_parameters(LogImageByteConversionParameters *params)
{
//	params->blackPoint = scene?scene->r.cineonblack:95;
//	params->whitePoint = scene?scene->r.cineonwhite:685;
//	params->gamma = scene?scene->r.cineongamma:1.7f;
//	params->doLogarithm = scene?scene->r.subimtype & R_CINEON_LOG:0;
	
	params->blackPoint = 95;
	params->whitePoint = 685;
	params->gamma = 1.0f;
	params->doLogarithm = 0;
}
#endif

static struct ImBuf *imb_load_dpx_cineon(unsigned char *mem, int use_cineon, int size, int flags)
{
	ImBuf *ibuf;
	LogImageFile *image;
	int x, y;
	unsigned short *row, *upix;
	int width, height, depth;
	float *frow;

	logImageSetVerbose((G.f & G_DEBUG) ? 1:0);
	
	image = logImageOpenFromMem(mem, size, use_cineon);
	
	if (!image) {
		printf("no image!\n");
		return NULL;
	}
	
	logImageGetSize(image, &width, &height, &depth);
	
	if (depth != 3) { /*need to do greyscale loading eventually.*/
		logImageClose(image);
		return NULL;
	}
	
	if (width == 0 && height == 0) {
		logImageClose(image);
		return NULL;
	}
	
	ibuf = IMB_allocImBuf(width, height, 32, IB_rectfloat | flags);

	row = MEM_mallocN(sizeof(unsigned short)*width*depth, "row in cineon_dpx.c");
	frow = ibuf->rect_float+width*height*4;
	
	for (y = 0; y < height; y++) {
		logImageGetRowBytes(image, row, y); /* checks image->params.doLogarithm and convert */
		upix = row;
		frow -= width*4;
		
		for (x=0; x<width; x++) {
			*(frow++) = ((float)*(upix++)) / 65535.0f;
			*(frow++) = ((float)*(upix++)) / 65535.0f;
			*(frow++) = ((float)*(upix++)) / 65535.0f;
			*(frow++) = 1.0f;
		}
		frow -= width*4;
	}

	MEM_freeN(row);
	logImageClose(image);
	
	if (flags & IB_rect) {
		IMB_rect_from_float(ibuf);
	}
	return ibuf;
}

static int imb_save_dpx_cineon(ImBuf *ibuf, const char *filename, int use_cineon, int flags)
{
	LogImageByteConversionParameters conversion;
	const int width= ibuf->x;
	const int height= ibuf->y;
	const int depth= 3;
	LogImageFile* logImage;
	unsigned short* line, *pixel;
	int i, j;
	float *fline;
	float *fbuf;
	int is_alloc= 0;
	
	(void)flags; /* unused */

	// cineon_conversion_parameters(&conversion);
	logImageGetByteConversionDefaults(&conversion);

	/*
	 * Get the drawable for the current image...
	 */

	fbuf= IMB_float_profile_ensure(ibuf, conversion.doLogarithm ? IB_PROFILE_LINEAR_RGB : IB_PROFILE_NONE, &is_alloc);

	if (fbuf == NULL) { /* in the unlikely event that converting to a float buffer fails */
		return 0;
	}
	
	logImageSetVerbose((G.f & G_DEBUG) ? 1:0);
	logImage = logImageCreate(filename, use_cineon, width, height, depth);

	if (!logImage) return 0;
	
	if (logImageSetByteConversion(logImage, &conversion)==0) {
		printf("error setting args\n");
	}

	line = MEM_mallocN(sizeof(unsigned short)*depth*width, "line");
	
	/*note that image is flipped when sent to logImageSetRowBytes (see last passed parameter).*/
	for (j = 0; j < height; ++j) {
		fline = &fbuf[width*j*4];
		for (i=0; i<width; i++) {
			float *fpix, fpix2[3];
			/*we have to convert to cinepaint's 16-bit-per-channel here*/
			pixel = &line[i*depth];
			fpix = &fline[i*4];
			memcpy(fpix2, fpix, sizeof(float)*3);
			
			if (fpix2[0]>=1.0f) fpix2[0] = 1.0f; else if (fpix2[0]<0.0f) fpix2[0]= 0.0f;
			if (fpix2[1]>=1.0f) fpix2[1] = 1.0f; else if (fpix2[1]<0.0f) fpix2[1]= 0.0f;
			if (fpix2[2]>=1.0f) fpix2[2] = 1.0f; else if (fpix2[2]<0.0f) fpix2[2]= 0.0f;
			
			pixel[0] = (unsigned short)(fpix2[0] * 65535.0f); /*float-float math is faster*/
			pixel[1] = (unsigned short)(fpix2[1] * 65535.0f);
			pixel[2] = (unsigned short)(fpix2[2] * 65535.0f);
		}
		logImageSetRowBytes(logImage, (const unsigned short*)line, height-1-j);
	}
	logImageClose(logImage);

	MEM_freeN(line);
	
	if (is_alloc) {
		MEM_freeN(fbuf);
	}
	
	return 1;
}

int imb_savecineon(struct ImBuf *buf, const char *myfile, int flags)
{
	return imb_save_dpx_cineon(buf, myfile, 1, flags);
}

 
int imb_is_cineon(unsigned char *buf)
{
	return cineonIsMemFileCineon(buf);
}

ImBuf *imb_loadcineon(unsigned char *mem, size_t size, int flags)
{
	if (imb_is_cineon(mem))
		return imb_load_dpx_cineon(mem, 1, size, flags);
	return NULL;
}

int imb_save_dpx(struct ImBuf *buf, const char *myfile, int flags)
{
	return imb_save_dpx_cineon(buf, myfile, 0, flags);
}

int imb_is_dpx(unsigned char *buf)
{
	return dpxIsMemFileCineon(buf);
}

ImBuf *imb_loaddpx(unsigned char *mem, size_t size, int flags)
{
	if (imb_is_dpx(mem))
		return imb_load_dpx_cineon(mem, 0, size, flags);
	return NULL;
}

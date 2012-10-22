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
 * Contributor(s): Julien Enche.
 *
 * ***** END GPL LICENSE BLOCK *****
 * cineon.c
 * contributors: joeedh, Julien Enche
 * I hearby donate this code and all rights to the Blender Foundation.
 * $Id$
 */

/** \file blender/imbuf/intern/cineon/cineon_dpx.c
 *  \ingroup imbcineon
 */


#include <stdio.h>
#include <string.h>
#include <math.h>
#include "logImageCore.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_filetype.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

#include "BKE_global.h"

#include "MEM_guardedalloc.h"

static struct ImBuf *imb_load_dpx_cineon(unsigned char *mem, size_t size, int use_cineon, int flags,
                                         char colorspace[IM_MAX_SPACE])
{
	ImBuf *ibuf;
	LogImageFile *image;
	int width, height, depth;

	colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_FLOAT);

	logImageSetVerbose((G.f & G_DEBUG) ? 1 : 0);

	image = logImageOpenFromMemory(mem, size);

	if (image == 0) {
		printf("DPX/Cineon: error opening image.\n");
		return 0;
	}

	logImageGetSize(image, &width, &height, &depth);

	if (width == 0 || height == 0) {
		logImageClose(image);
		return 0;
	}

	ibuf = IMB_allocImBuf(width, height, 32, IB_rectfloat | flags);
	if (ibuf == 0) {
		logImageClose(image);
		return 0;
	}

	if (logImageGetDataRGBA(image, ibuf->rect_float, 1) != 0) {
		/* Conversion not possible (probably because the format is unsupported) */
		logImageClose(image);
		MEM_freeN(ibuf);
		return 0;
	}

	logImageClose(image);
	ibuf->ftype = use_cineon ? CINEON : DPX;
	IMB_flipy(ibuf);

	if (flags & IB_rect)
		IMB_rect_from_float(ibuf);

	return ibuf;
}

static int imb_save_dpx_cineon(ImBuf *ibuf, const char *filename, int use_cineon, int flags)
{
	LogImageFile *logImage;
	float *fbuf;
	float *fbuf_ptr;
	unsigned char *rect_ptr;
	int x, y, depth, bitspersample, rvalue;

	if (flags & IB_mem) {
		printf("DPX/Cineon: saving in memory is not supported.\n");
		return 0;
	}
	
	logImageSetVerbose((G.f & G_DEBUG) ? 1 : 0);

	depth = (ibuf->planes + 7) >> 3;
	if (depth > 4 || depth < 3) {
		printf("DPX/Cineon: unsupported depth: %d for file: '%s'\n", depth, filename);
		return 0;
	}

	if (ibuf->ftype & CINEON_10BIT)
		bitspersample = 10;
	else if (ibuf->ftype & CINEON_12BIT)
		bitspersample = 12;
	else if (ibuf->ftype & CINEON_16BIT)
		bitspersample = 16;
	else
		bitspersample = 8;

	logImage = logImageCreate(filename, use_cineon, ibuf->x, ibuf->y, bitspersample, (depth == 4),
	                          (ibuf->ftype & CINEON_LOG), -1, -1, -1, "Blender");

	if (logImage == 0) {
		printf("DPX/Cineon: error creating file.\n");
		return 0;
	}

	if (ibuf->rect_float != 0 && bitspersample != 8) {
		/* don't use the float buffer to save 8 bpp picture to prevent color banding
		   (there's no dithering algorithm behing the logImageSetDataRGBA function) */
		IMB_flipy(ibuf);
		rvalue = (logImageSetDataRGBA(logImage, ibuf->rect_float, 1) == 0);
		IMB_flipy(ibuf);
	}
	else {
		if (ibuf->rect == 0)
			IMB_rect_from_float(ibuf);

		fbuf = (float *)MEM_mallocN(ibuf->x * ibuf->y * 4 * sizeof(float), "fbuf in imb_save_dpx_cineon");
		if (fbuf == 0) {
			printf("DPX/Cineon: error allocating memory.\n");
			logImageClose(logImage);
			return 0;
		}
		for (y = 0; y < ibuf->y; y++) {
			for (x = 0; x < ibuf->x; x++) {
				fbuf_ptr = fbuf + 4 * ((ibuf->y - y - 1) * ibuf->x + x);
				rect_ptr = (unsigned char *)ibuf->rect + 4 * (y * ibuf->x + x);
				fbuf_ptr[0] = (float)rect_ptr[0] / 255.0f;
				fbuf_ptr[1] = (float)rect_ptr[1] / 255.0f;
				fbuf_ptr[2] = (float)rect_ptr[2] / 255.0f;
				fbuf_ptr[3] = (depth == 4) ? ((float)rect_ptr[3] / 255.0f) : 1.0f;
			}
		}
		rvalue = (logImageSetDataRGBA(logImage, fbuf, 0) == 0);
		MEM_freeN(fbuf);
	}

	logImageClose(logImage);
	return rvalue;
}

int imb_save_cineon(struct ImBuf *buf, const char *myfile, int flags)
{
	return imb_save_dpx_cineon(buf, myfile, 1, flags);
}

int imb_is_cineon(unsigned char *buf)
{
	return logImageIsCineon(buf);
}

ImBuf *imb_load_cineon(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
	if (imb_is_cineon(mem))
		return imb_load_dpx_cineon(mem, size, 1, flags, colorspace);
	return 0;
}

int imb_save_dpx(struct ImBuf *buf, const char *myfile, int flags)
{
	return imb_save_dpx_cineon(buf, myfile, 0, flags);
}

int imb_is_dpx(unsigned char *buf)
{
	return logImageIsDpx(buf);
}

ImBuf *imb_load_dpx(unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
	if (imb_is_dpx(mem))
		return imb_load_dpx_cineon(mem, size, 0, flags, colorspace);
	return 0;
}

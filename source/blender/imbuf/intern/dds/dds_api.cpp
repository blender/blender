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
 * Contributors: Amorilia (amorilia@users.sourceforge.net)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/dds/dds_api.cpp
 *  \ingroup imbdds
 */


extern "C" {
#include "BLI_utildefines.h"
}

#include <stddef.h>
#include <dds_api.h>
#include <Stream.h>
#include <DirectDrawSurface.h>
#include <FlipDXT.h>
#include <stdio.h> // printf
#include <fstream>

#if defined (WIN32)
#include "utfconv.h"
#endif

extern "C" {

#include "imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_allocimbuf.h"

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

int imb_save_dds(struct ImBuf *ibuf, const char *name, int /*flags*/)
{
	return(0); /* todo: finish this function */

	/* check image buffer */
	if (ibuf == 0) return (0);
	if (ibuf->rect == 0) return (0);

	/* open file for writing */
	std::ofstream fildes;

#if defined (WIN32)
	wchar_t *wname = alloc_utf16_from_8(name, 0);
	fildes.open(wname);
	free(wname);
#else
	fildes.open(name);
#endif

	/* write header */
	fildes << "DDS ";
	fildes.close();

	return(1);
}

int imb_is_a_dds(const unsigned char *mem) // note: use at most first 32 bytes
{
	/* heuristic check to see if mem contains a DDS file */
	/* header.fourcc == FOURCC_DDS */
	if ((mem[0] != 'D') || (mem[1] != 'D') || (mem[2] != 'S') || (mem[3] != ' ')) return(0);
	/* header.size == 124 */
	if ((mem[4] != 124) || mem[5] || mem[6] || mem[7]) return(0);
	return(1);
}

struct ImBuf *imb_load_dds(const unsigned char *mem, size_t size, int flags, char colorspace[IM_MAX_SPACE])
{
	struct ImBuf *ibuf = NULL;
	DirectDrawSurface dds((unsigned char *)mem, size); /* reads header */
	unsigned char bits_per_pixel;
	unsigned int *rect;
	Image img;
	unsigned int numpixels = 0;
	int col;
	unsigned char *cp = (unsigned char *) &col;
	Color32 pixel;
	Color32 *pixels = 0;

	/* OCIO_TODO: never was able to save DDS, so can't test loading
	 *            but profile used to be set to sRGB and can't see rect_float here, so
	 *            default byte space should work fine
	 */
	colorspace_set_default_role(colorspace, IM_MAX_SPACE, COLOR_ROLE_DEFAULT_BYTE);

	if (!imb_is_a_dds(mem))
		return (0);

	/* check if DDS is valid and supported */
	if (!dds.isValid()) {
		/* no need to print error here, just testing if it is a DDS */
		if (flags & IB_test)
			return (0);

		printf("DDS: not valid; header follows\n");
		dds.printInfo();
		return(0);
	}
	if (!dds.isSupported()) {
		printf("DDS: format not supported\n");
		return(0);
	}
	if ((dds.width() > 65535) || (dds.height() > 65535)) {
		printf("DDS: dimensions too large\n");
		return(0);
	}

	/* convert DDS into ImBuf */
	dds.mipmap(&img, 0, 0); /* load first face, first mipmap */
	pixels = img.pixels();
	numpixels = dds.width() * dds.height();
	bits_per_pixel = 24;
	if (img.format() == Image::Format_ARGB) {
		/* check that there is effectively an alpha channel */
		for (unsigned int i = 0; i < numpixels; i++) {
			pixel = pixels[i];
			if (pixel.a != 255) {
				bits_per_pixel = 32;
				break;
			}
		}
	}
	ibuf = IMB_allocImBuf(dds.width(), dds.height(), bits_per_pixel, 0); 
	if (ibuf == 0) return(0); /* memory allocation failed */

	ibuf->ftype = IMB_FTYPE_DDS;
	ibuf->dds_data.fourcc = dds.fourCC();
	ibuf->dds_data.nummipmaps = dds.mipmapCount();

	if ((flags & IB_test) == 0) {
		if (!imb_addrectImBuf(ibuf)) return(ibuf);
		if (ibuf->rect == 0) return(ibuf);

		rect = ibuf->rect;
		cp[3] = 0xff; /* default alpha if alpha channel is not present */

		for (unsigned int i = 0; i < numpixels; i++) {
			pixel = pixels[i];
			cp[0] = pixel.r; /* set R component of col */
			cp[1] = pixel.g; /* set G component of col */
			cp[2] = pixel.b; /* set B component of col */
			if (dds.hasAlpha())
				cp[3] = pixel.a; /* set A component of col */
			rect[i] = col;
		}

		if (ibuf->dds_data.fourcc != FOURCC_DDS) {
			ibuf->dds_data.data = (unsigned char *)dds.readData(ibuf->dds_data.size);

			/* flip compressed texture */
			FlipDXTCImage(dds.width(), dds.height(), dds.mipmapCount(), dds.fourCC(), ibuf->dds_data.data);
		}
		else {
			ibuf->dds_data.data = NULL;
			ibuf->dds_data.size = 0;
		}

		/* flip uncompressed texture */
		IMB_flipy(ibuf);
	}

	return(ibuf);
}

} // extern "C"

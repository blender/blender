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
 * Contributor(s): Campbell Barton.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/thumbs_blend.c
 *  \ingroup imbuf
 */


#include <string.h>

#include "zlib.h"

#include "BLI_utildefines.h"
#include "BLI_endian_switch.h"
#include "BLI_fileops.h"

#include "BLO_blend_defs.h"

#include "BKE_global.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_thumbs.h"

/* extracts the thumbnail from between the 'REND' and the 'GLOB'
 * chunks of the header, don't use typical blend loader because its too slow */

static ImBuf *loadblend_thumb(gzFile gzfile)
{
	char buf[12];
	int bhead[24 / sizeof(int)]; /* max size on 64bit */
	char endian, pointer_size;
	char endian_switch;
	int sizeof_bhead;

	/* read the blend file header */
	if (gzread(gzfile, buf, 12) != 12)
		return NULL;
	if (strncmp(buf, "BLENDER", 7))
		return NULL;

	if (buf[7] == '-')
		pointer_size = 8;
	else if (buf[7] == '_')
		pointer_size = 4;
	else
		return NULL;

	sizeof_bhead = 16 + pointer_size;

	if (buf[8] == 'V')
		endian = B_ENDIAN;  /* big: PPC */
	else if (buf[8] == 'v')
		endian = L_ENDIAN;  /* little: x86 */
	else
		return NULL;

	endian_switch = ((ENDIAN_ORDER != endian)) ? 1 : 0;

	while (gzread(gzfile, bhead, sizeof_bhead) == sizeof_bhead) {
		if (endian_switch)
			BLI_endian_switch_int32(&bhead[1]);  /* length */

		if (bhead[0] == REND) {
			gzseek(gzfile, bhead[1], SEEK_CUR); /* skip to the next */
		}
		else {
			break;
		}
	}

	/* using 'TEST' since new names segfault when loading in old blenders */
	if (bhead[0] == TEST) {
		ImBuf *img = NULL;
		int size[2];

		if (gzread(gzfile, size, sizeof(size)) != sizeof(size))
			return NULL;

		if (endian_switch) {
			BLI_endian_switch_int32(&size[0]);
			BLI_endian_switch_int32(&size[1]);
		}
		/* length */
		bhead[1] -= sizeof(int) * 2;

		/* inconsistent image size, quit early */
		if (bhead[1] != size[0] * size[1] * sizeof(int))
			return NULL;
	
		/* finally malloc and read the data */
		img = IMB_allocImBuf(size[0], size[1], 32, IB_rect | IB_metadata);
	
		if (gzread(gzfile, img->rect, bhead[1]) != bhead[1]) {
			IMB_freeImBuf(img);
			img = NULL;
		}
	
		return img;
	}
	
	return NULL;
}

ImBuf *IMB_loadblend_thumb(const char *path)
{
	gzFile gzfile;
	/* not necessarily a gzip */
	gzfile = BLI_gzopen(path, "rb");

	if (NULL == gzfile) {
		return NULL;
	}
	else {
		ImBuf *img = loadblend_thumb(gzfile);

		/* read ok! */
		gzclose(gzfile);

		return img;
	}
}

/* add a fake passepartout overlay to a byte buffer, use for blend file thumbnails */
#define MARGIN 2

void IMB_overlayblend_thumb(unsigned int *thumb, int width, int height, float aspect)
{
	unsigned char *px = (unsigned char *)thumb;
	int margin_l = MARGIN;
	int margin_b = MARGIN;
	int margin_r = width - MARGIN;
	int margin_t = height - MARGIN;

	if (aspect < 1.0f) {
		margin_l = (int)((width - ((float)width * aspect)) / 2.0f);
		margin_l += MARGIN;
		CLAMP(margin_l, MARGIN, (width / 2));
		margin_r = width - margin_l;
	}
	else if (aspect > 1.0f) {
		margin_b = (int)((height - ((float)height / aspect)) / 2.0f);
		margin_b += MARGIN;
		CLAMP(margin_b, MARGIN, (height / 2));
		margin_t = height - margin_b;
	}

	{
		int x, y;
		int stride_x = (margin_r - margin_l) - 2;
		
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++, px += 4) {
				int hline = 0, vline = 0;
				if ((x > margin_l && x < margin_r) && (y > margin_b && y < margin_t)) {
					/* interior. skip */
					x  += stride_x;
					px += stride_x * 4;
				}
				else if ((hline = (((x == margin_l || x == margin_r)) && y >= margin_b && y <= margin_t)) ||
				         (vline = (((y == margin_b || y == margin_t)) && x >= margin_l && x <= margin_r)))
				{
					/* dashed line */
					if ((hline && y % 2) || (vline && x % 2)) {
						px[0] = px[1] = px[2] = 0;
						px[3] = 255;
					}
				}
				else {
					/* outside, fill in alpha, like passepartout */
					px[0] *= 0.5f;
					px[1] *= 0.5f;
					px[2] *= 0.5f;
					px[3] = (px[3] * 0.5f) + 96;
				}
			}
		}
	}
}

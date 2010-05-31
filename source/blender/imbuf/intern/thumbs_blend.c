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
 * Contributor(s): Campbell Barton.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>

#include "zlib.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_thumbs.h"

#include "MEM_guardedalloc.h"

/* extracts the thumbnail from between the 'REND' and the 'GLOB'
 * chunks of the header, dont use typical blend loader because its too slow */
ImBuf *IMB_loadblend_thumb(const char *path)
{
	char buf[8];
	int code= 0;
	char endian, pointer_size;
	char endian_switch;
	int len, im_len, x, y;
	int *rect= NULL;

	gzFile gzfile;
	
	ImBuf *img;
	
	/* not necessarily a gzip */
	gzfile = gzopen(path, "rb");

	if (NULL == gzfile ) {
		return NULL;
	}
	
	/* read the blend file header */
	if(gzread(gzfile, buf, 8) != 8)		goto thumb_error;
	if(strncmp(buf, "BLENDER", 7))		goto thumb_error;
	
	if(buf[7]=='-')						pointer_size= 8;
	else if(buf[7]=='_')				pointer_size= 4;
	else								goto thumb_error;
	
	/* read the next 4 bytes, only need the first char, ignore the version */
	/* endian and vertsion (ignored) */
	if(gzread(gzfile, buf, 4) != 4)		goto thumb_error;
	
	if(buf[0]=='V')						endian= B_ENDIAN; /* big: PPC */
	else if(buf[0]=='v')				endian= L_ENDIAN; /* little: x86 */
	else								goto thumb_error;

	while(gzread(gzfile, &code, sizeof(int)) == sizeof(int)) {
		endian_switch = ((ENDIAN_ORDER != endian)) ? 1 : 0;
		
		if(gzread(gzfile, buf, sizeof(int)) != sizeof(int))		goto thumb_error;
		len = *( (int *)((void *)buf) );
		if(endian_switch) SWITCH_INT(len);
		
		/* finally read the rest of the bhead struct, pointer and 2 ints */
		if(gzread(gzfile, buf, pointer_size) != pointer_size)	goto thumb_error;
		if(gzread(gzfile, buf, sizeof(int) * 2) != sizeof(int) * 2)	goto thumb_error;
		/* we dont actually care whats in the bhead */
		
		if (code==REND) {
			gzseek(gzfile, len, SEEK_CUR); /* skip to the next */
		}
		else {
			break;
		}
	}
	
	/* using 'TEST' since new names segfault when loading in old blenders */
	if(code != TEST)					goto thumb_error;
	
	if(gzread(gzfile, &x, sizeof(int)) != sizeof(int))		goto thumb_error;
	if(gzread(gzfile, &y, sizeof(int)) != sizeof(int))		goto thumb_error;
	len -= sizeof(int) * 2;

	if(endian_switch) { SWITCH_INT(x); SWITCH_INT(y); }

	/* inconsistant image size, quit early */
	im_len = x * y * sizeof(int);
	if(im_len != len)					goto thumb_error;

	/* finally malloc and read the data */
	rect= MEM_mallocN(len, "imb_loadblend_thumb");

	if(gzread(gzfile, rect, len) != len) goto thumb_error;

	/* read ok! */
	gzclose(gzfile);

	img = IMB_allocImBuf(x, y, 32, IB_rect | IB_metadata, 0);

	memcpy(img->rect, rect, im_len);

	MEM_freeN(rect);
	
	return img;	

thumb_error:
	gzclose(gzfile);
	if(rect) MEM_freeN(rect);
	return NULL;
}

/* add a fake passepartout overlay to a byte buffer, use for blend file thumbnails */
#define MARGIN 2

void IMB_overlayblend_thumb(int *thumb, int width, int height, float aspect)
{
	unsigned char *px= (unsigned char *)thumb;
	int margin_l = MARGIN;
	int margin_b = MARGIN;
	int margin_r = width - MARGIN;
	int margin_t = height - MARGIN;

	if(aspect < 1.0f) {
		margin_l= (int)((width - ((float)width * aspect)) / 2.0f);
		margin_l += MARGIN;
		CLAMP(margin_l, MARGIN, (width/2));
		margin_r = width - margin_l;
	}
	else if (aspect > 1.0f) {
		margin_b= (int)((height - ((float)height / aspect)) / 2.0f);
		margin_b += MARGIN;
		CLAMP(margin_b, MARGIN, (height/2));
		margin_t = height - margin_b;
	}

	{	
		int x, y;
		int hline, vline;
		int stride_x= (margin_r - margin_l) - 2;
		
		for(y=0; y < height; y++) {
			for(x=0; x < width; x++, px+=4) {
				if((x > margin_l && x < margin_r) && (y > margin_b && y < margin_t)) {
					/* interior. skip */
					x  += stride_x;
					px += stride_x * 4;
				} else if(	(hline=(((x == margin_l || x == margin_r)) && y >= margin_b && y <= margin_t)) ||
							(vline=(((y == margin_b || y == margin_t)) && x >= margin_l && x <= margin_r))
				) {
					/* dashed line */
					if((hline && y % 2) || (vline && x % 2)) {
						px[0]= px[1]= px[2]= 0;
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

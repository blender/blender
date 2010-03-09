/**
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
 * iff.c
 *
 * $Id$
 */

#include "BLI_blenlib.h"
#include "imbuf.h"
#include "imbuf_patch.h"
#include "IMB_imbuf_types.h"
#include "IMB_iff.h"
#ifdef WIN32
#include <io.h>
#include "BLI_winstuff.h"
#endif

unsigned short imb_start_iff(struct ImBuf *ibuf, int file)
{
	unsigned int *point, size, *buf;
	
	if ((point=buf=(unsigned int *)malloc(32768))==0) return FALSE;

	*point++ = FORM;				/* FORMxxxxILBM in buffer */
	*point++ = 0;

	if (IS_amiga(ibuf)){
		struct BitMapHeader *bmhd;

		*point++ = ILBM;
		*point++ = CAMG;
		*point++ = 4;
		*point++ = (ibuf->ftype & 0xffff);

		*point++=BMHD;
		*point++=sizeof(struct BitMapHeader);

		bmhd=(struct BitMapHeader *)point;		/* bmhd points to location where bmhd will be */
		point=(unsigned int *)((char *)point+sizeof(struct BitMapHeader));	/* advance pointer already */

		bmhd->w=ibuf->x;
		bmhd->h=ibuf->y;
		bmhd->pageWidth=ibuf->x;
		bmhd->pageHeight=ibuf->y;
		bmhd->x=0;
		bmhd->y=0;
		bmhd->nPlanes=ibuf->depth;
		bmhd->masking=0;
		if (ibuf->flags & IB_vert){
			bmhd->compression=2;
		}
		else{
			bmhd->compression=1;
		}
		bmhd->pad1=0;
		bmhd->transparentColor=0;
		bmhd->xAspect=1;
		bmhd->yAspect=1;
	} else if (IS_anim(ibuf)){
		struct Adat *adat;
		extern float adat_gamma;
		extern float adat_distort;
		
		*point++ = ANIM;
		*point++ = ADAT;
		*point++ = BIG_LONG(sizeof(struct Adat));

		adat = (struct Adat *)point;
		point = (unsigned int *)((char *)point+sizeof(struct Adat));	/* advance pointer already */

		adat->w = BIG_SHORT(ibuf->x);
		adat->h = BIG_SHORT(ibuf->y);

		adat->type = BIG_SHORT(ibuf->ftype);
		adat->xorig = BIG_SHORT(ibuf->xorig);
		adat->yorig = BIG_SHORT(ibuf->yorig);
		adat->pad = 0;
		adat->gamma = adat_gamma;
		adat->distort = adat_distort;
	}

	size=((uchar *)point-(uchar *)buf);
	if (write(file,buf,size)!=size){
		free(buf);
		return (FALSE);
	}

	if (ibuf->cmap){
		if (IS_anim(ibuf)){
			size = ibuf->maxcol * sizeof(int);
			buf[0] = CMAP;
			buf[1] = BIG_LONG(size);
			if (write(file,buf,8) != 8){
				free(buf);
				return (FALSE);
			}
			if (write(file,ibuf->cmap,size) != size){
				free(buf);
				return (FALSE);
			}
		} else{
			uchar *cpoint,*cols;
			unsigned int i,bits;

			point = buf;
			if (IS_amiga(ibuf)){
				*(point++) = CMAP;
				*(point++) = BIG_LONG(3*ibuf->maxcol);
			}

			cpoint = (uchar *) point;
			cols = (uchar *)ibuf->cmap;
			if ((ibuf->cbits > 0) && (ibuf->cbits < 8)){
				bits = ~((1 << (8-ibuf->cbits)) - 1);
			} else bits = -1;

			if (IS_ham(ibuf)) bits = -1;
			
			for (i=0 ; i<ibuf->maxcol ; i++){
				*(cpoint++) = cols[0] & bits;
				*(cpoint++) = cols[1] & bits;
				*(cpoint++) = cols[2] & bits;
				cols += 4;
			}
			if (ibuf->maxcol & 1) *(cpoint++)=0;

			size=(cpoint-(uchar *)buf);
			if (write(file,buf,size)!=size){
				free(buf);
				return (FALSE);
			}
		}
	}

	if (IS_amiga(ibuf)) buf[0] = BODY;
	if (IS_anim(ibuf)) buf[0] = BODY;
	buf[1]=0;

	if (write(file,buf,8)!=8){
		free(buf);
		return(FALSE);
	}

	free(buf);
	return (TRUE);
}


unsigned short imb_update_iff(int file, int code)
{
	int	buf[2], filelen, skip;
	uchar nop;

	if (file<=0) return (FALSE);

	filelen = BLI_filesize(file)-8;			/* calc filelength  */

	lseek(file,0L,2);		/* seek end */

	if (filelen & 1){						/* make length 'even' */
		switch(code){
		case BODY:
			nop = IFFNOP;
			break;
		}
		if (write(file,&nop,1)!=1) return (FALSE);
		filelen++;
	}
	lseek(file,4L,0);

	buf[0] = BIG_LONG(filelen);
	
	if (write(file, buf, 4) != 4) return (FALSE);
	if (code == 0) return (TRUE);

	filelen-=4;
	if(lseek(file,4L,1) == -1) return (FALSE);

	while (filelen>0){		/* seek BODY */
		if(read(file, buf, 8) != 8) return (FALSE);
		filelen -= 8;
		if (buf[0] == code) break;
		
		skip = (BIG_LONG(buf[1]) + 1) & ~1;
		filelen -= skip;
		if(lseek(file, skip, 1) == -1) return (FALSE);
	}
	if (filelen <= 0) {
		printf("update_iff: couldn't find chunk\n");
		return (FALSE);
	}

	lseek(file, -4L, 1);
	
	buf[0] = BIG_LONG(filelen);
	
	if (write(file, buf, 4)!=4) return (FALSE);

	return (TRUE);
}

/**
 * bitplanes.c
 *
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "imbuf.h"
#include "BLI_blenlib.h"

#include "imbuf_patch.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"
#include "IMB_bitplanes.h"


unsigned int **imb_copyplanelist(struct ImBuf *ibuf)
{
	int nobp,i;
	unsigned int **listn,**listo;

	nobp=ibuf->depth;
	listn= malloc(nobp*sizeof(int *));			/* kopie van bitmap maken */
	if (listn==0) return (0);

	listo=ibuf->planes;
	for (i=nobp;i>0;i--){
		*(listn++) = *(listo++);
	}
	listn -= nobp;

	return (listn);
}

static void bptolscanl(unsigned int *buf, int size, unsigned int **list, int nobp, int offset)
{
	/* 	zet bitplanes om in een buffer met ints
	door 4 deelbare hoeveelheid bitplanes,
	breedte bitplanes op	ints afgrond 	*/

	list += nobp;

	for (;nobp>0;)
	{
		int todo,i;
		register int bp1, bp2, bp3, data;
		register unsigned int *point;
		int  bp4, loffset;
		/*register unsigned int bp1, bp2, bp3, bp4;*/

		todo = 0;
		point = buf;
		loffset = offset;

		if (nobp & 1){
			list -= 1;
			nobp -= 1;
			for(i=size;i>0;i--)
			{
				if (todo==0)
				{
					bp1 = BIG_LONG((list[0])[loffset]);
					loffset++;
					todo=32;
				}

				data = *point;
				data<<=1;

				if (bp1<0) data+=1;
				bp1<<=1;

				/*		data += (bp1 >> 31);
					bp1 <<= 1;
				*/
				*(point++)=data;
				todo--;
			}
		} else if (nobp & 2){
			list -= 2;
			nobp -= 2;
			for(i=size;i>0;i--)
			{
				if (todo==0)
				{
					bp1 = BIG_LONG((list[0])[loffset]);
					bp2 = BIG_LONG((list[1])[loffset]);
					loffset++;
					todo=32;
				}

				data = *point;
				data<<=2;

				if (bp1<0) data+=1;
				bp1<<=1;
				if (bp2<0) data+=2;
				bp2<<=1;

				/*		data += (bp1 >> 31) + ((bp2 & 0x80000000) >> 30);
				bp1 <<= 1; bp2 <<= 1;
				*/
				*(point++)=data;
				todo--;
			}
		} else{
			list -= 4;
			nobp -= 4;
			for(i=size;i>0;i--)
			{
				if (todo==0) {
					bp1 = BIG_LONG((list[0])[loffset]);
					bp2 = BIG_LONG((list[1])[loffset]);
					bp3 = BIG_LONG((list[2])[loffset]);
					bp4 = BIG_LONG((list[3])[loffset]);
					loffset++;
					todo=32;
				}

				data = *point;
				data<<=4;

				if (bp1<0) data+=1;
				bp1<<=1;
				if (bp2<0) data+=2;
				bp2<<=1;
				if (bp3<0) data+=4;
				bp3<<=1;
				if (bp4<0) data+=8;
				bp4<<=1;

				/*		data += (bp1 >> 31) \
				+ ((bp2 & 0x80000000) >> 30) \
				+ ((bp3 & 0x80000000) >> 29) \
				+ ((bp4 & 0x80000000) >> 28);
		
				bp1 <<= 1; bp2 <<= 1;
				bp3 <<= 1; bp4 <<= 1;
				*/
				
				*(point++)=data;
				todo--;
			}
		}
	}
}


void imb_bptolong(struct ImBuf *ibuf)
{
	int nobp,i,x;
	unsigned int *rect,offset;

	/* eerst alle ints wissen */

	if (ibuf == 0) return;
	if (ibuf->planes == 0) return;
	if (ibuf->rect == 0) imb_addrectImBuf(ibuf);

	nobp=ibuf->depth;
	if (nobp != 32){
		if (nobp == 24) IMB_rectoptot(ibuf, 0, IMB_rectfill, 0xff000000); /* alpha zetten */
		else IMB_rectoptot(ibuf, 0, IMB_rectfill, 0);
	}

	rect= ibuf->rect;
	x= ibuf->x;
	offset=0;

	for (i= ibuf->y; i>0; i--){
		bptolscanl(rect, x, ibuf->planes, nobp, offset);
		rect += x;
		offset += ibuf->skipx;
	}
}


static void ltobpscanl(unsigned int *rect, int x, unsigned int **list, int nobp, int offset)
{
	/* zet een buffer met ints, om in bitplanes. Opgepast, buffer 
		wordt vernietigd !*/

	if (nobp != 32)
	{
		int *rect2;
		int todo,j;

		rect2 = (int*)rect;

		todo = 32-nobp;
		for (j = x;j>0;j--){
			*(rect2++) <<= todo;
		}
	}

	list += nobp;
	for (;nobp>0;){
		register int bp1=0, bp2=0, bp3=0, data;
		register unsigned int *point;
		int i,todo;
		int bp4=0,loffset;

		point = rect;
		todo=32;
		loffset=offset;

		if (nobp & 1){
			list -= 1;
			nobp -= 1;

			for(i=x;i>0;i--){
				data = *point;

				bp1 <<= 1;
				if (data<0) bp1 += 1;
				data <<= 1;

				*(point++) = data;

				todo--;
				if (todo == 0){
					(list[0])[loffset] = bp1;
					loffset++;
					todo=32;
				}
			}
			if (todo != 32)
			{
				bp1 <<= todo;
				(list[0])[loffset] = bp1;
			}
		} else if (nobp & 2){
			list -= 2;
			nobp -= 2;
			for(i=x;i>0;i--){
				data = *point;

				bp2 <<= 1;
				if (data<0) bp2 += 1;
				data <<= 1;
				bp1 <<= 1;
				if (data<0) bp1 += 1;
				data <<= 1;

				*(point++) = data;

				todo--;
				if (todo == 0){
					(list[0])[loffset] = bp1;
					(list[1])[loffset] = bp2;
					loffset++;
					todo=32;
				}
			}
			if (todo != 32){
				bp1 <<= todo;
				bp2 <<= todo;
				(list[0])[loffset] = bp1;
				(list[1])[loffset] = bp2;
			}
		} else{
			list -= 4;
			nobp -= 4;
			for(i=x;i>0;i--){
				data = *point;

				bp4 <<= 1;
				if (data<0) bp4 += 1;
				data <<= 1;
				bp3 <<= 1;
				if (data<0) bp3 += 1;
				data <<= 1;
				bp2 <<= 1;
				if (data<0) bp2 += 1;
				data <<= 1;
				bp1 <<= 1;
				if (data<0) bp1 += 1;
				data <<= 1;

				*(point++) = data;

				todo--;
				if (todo == 0){
					(list[0])[loffset] = bp1;
					(list[1])[loffset] = bp2;
					(list[2])[loffset] = bp3;
					(list[3])[loffset] = bp4;
					loffset++;
					todo=32;
				}
			}
			if (todo != 32){
				bp1 <<= todo;
				bp2 <<= todo;
				bp3 <<= todo;
				bp4 <<= todo;
				(list[0])[loffset] = bp1;
				(list[1])[loffset] = bp2;
				(list[2])[loffset] = bp3;
				(list[3])[loffset] = bp4;
			}
		}
	}
}


void imb_longtobp(struct ImBuf *ibuf)
{
	/* zet een buffer met ints, om in bitplanes. Opgepast, buffer 
		wordt vernietigd !*/

	int nobp,i,x;
	unsigned int *rect,offset,*buf;
	;

	nobp = ibuf->depth;
	rect=ibuf->rect;
	x=ibuf->x;
	offset=0;
	if ((buf=malloc(x*sizeof(int)))==0) return;

	for (i=ibuf->y;i>0;i--){
		memcpy(buf, rect, x*sizeof(int));
		rect +=x ;
		ltobpscanl(buf, x, ibuf->planes, nobp, offset);
		offset += ibuf->skipx;
	}
	free(buf);
}

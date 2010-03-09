/**
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
 */

#include "BLI_blenlib.h"

#include "imbuf.h"
#include "imbuf_patch.h"
#include "IMB_imbuf_types.h"

void IMB_cspace(struct ImBuf *ibuf, float mat[][4]);

/************************************************************************/
/*				COLORSPACE				*/
/************************************************************************/

static void fillmattab(double val, unsigned short *mattab)
{
	int tot,ival;
	int i;

	val *= (1 << 22);
	ival = val;
	tot = 32767; /* een half */

	for(i = 256; i > 0; i--){
		*(mattab) = (tot >> 16);
		mattab += 3;
		tot += ival;
	}
}


static void cspfill(short *buf, unsigned short *fill, int x)
{
	unsigned short r,g,b;

	b = fill[0];
	g = fill[1];
	r = fill[2];
	for (;x>0;x--){
		buf[0] = b;
		buf[1] = g;
		buf[2] = r;
		buf += 3;
	}
}


static void cspadd(short *buf, unsigned short *cont, unsigned char *rect, int x)
{
	short i;
	for (;x>0;x--){
		i = *(rect);
		rect += 4;
		buf[0] += cont[i*3];
		buf[1] += cont[i*3 + 1];
		buf[2] += cont[i*3 + 2];
		buf += 3;
	}
}


static void cspret(short *buf, unsigned char *rect, int x)
{
	int r,g,b;
	
	for(; x > 0; x--){
		b = buf[0];
		g = buf[1];
		r = buf[2];

		if (b & 0x4000){
			if (b<0) rect[2]=0;
			else rect[2]=255;
		} else rect[2] = b >> 6;

		if (g & 0x4000){
			if (g<0) rect[1]=0;
			else rect[1]=255;
		} else rect[1] = g >> 6;

		if (r & 0x4000){
			if (r<0) rect[0]=0;
			else rect[0]=255;
		} else rect[0] = r >> 6;

		buf += 3;
		rect += 4;
	}
}


static void rotcspace(struct ImBuf *ibuf, unsigned short *cont_1, unsigned short *cont_2, unsigned short *cont_3, unsigned short *add)
{
	short x,y,*buf;
	uchar *rect;

	x=ibuf->x;
	rect= (uchar *)ibuf->rect;

	buf=(short *)malloc(x*3*sizeof(short));
	if (buf){
		for(y=ibuf->y;y>0;y--){
			cspfill(buf,add,x);
			cspadd(buf,cont_1,rect+0,x);
			cspadd(buf,cont_2,rect+1,x);
			cspadd(buf,cont_3,rect+2,x);
			cspret(buf,rect,x);
			rect += x<<2;
		}
		free(buf);
	}
}


void IMB_cspace(struct ImBuf *ibuf, float mat[][4])
{
	unsigned short *cont_1,*cont_2,*cont_3,add[3];

	cont_1=(unsigned short *)malloc(256*3*sizeof(short));
	cont_2=(unsigned short *)malloc(256*3*sizeof(short));
	cont_3=(unsigned short *)malloc(256*3*sizeof(short));

	if (cont_1 && cont_2 && cont_3){

		fillmattab(mat[0][0],cont_1);
		fillmattab(mat[0][1],cont_1+1);
		fillmattab(mat[0][2],cont_1+2);

		fillmattab(mat[1][0],cont_2);
		fillmattab(mat[1][1],cont_2+1);
		fillmattab(mat[1][2],cont_2+2);

		fillmattab(mat[2][0],cont_3);
		fillmattab(mat[2][1],cont_3+1);
		fillmattab(mat[2][2],cont_3+2);

		add[0] = (mat[3][0] * 64.0) + .5;
		add[1] = (mat[3][1] * 64.0) + .5;
		add[2] = (mat[3][2] * 64.0) + .5;

		rotcspace(ibuf, cont_1, cont_2, cont_3, add);
	}

	if (cont_1) free(cont_1);
	if (cont_2) free(cont_2);
	if (cont_3) free(cont_3);
}


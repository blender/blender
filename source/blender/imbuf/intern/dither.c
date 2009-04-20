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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * dither.c
 *
 * $Id$
 */

#include "BLI_blenlib.h"

#include "imbuf.h"
#include "imbuf_patch.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

void IMB_dit0(struct ImBuf * ibuf, short ofs, short bits)
{
	int x, y, and, add, pix;
	uchar *rect;

	rect= (uchar *)ibuf->rect;
	rect +=ofs;

	bits = 8 - bits;
	and = ~((1 << bits)-1);
	add = 1 << (bits - 1);
	
	for (y = ibuf->y; y > 0; y--){
		for (x = ibuf->x; x > 0; x--) {
			pix = *rect + add;
			if (pix > 255) pix = 255; 
			*rect = pix & and;
			rect += 4;
		}
	}
}

void IMB_dit2(struct ImBuf * ibuf, short ofs, short bits)
{
	short x,y,pix,and,add1,add2;
	uchar *rect;
	uchar dit[4];

	rect= (uchar *)ibuf->rect;
	rect +=ofs;

	bits = 8 - bits;
	and = ~((1<<bits)-1);
	bits -= 2;

	ofs = 0;
	
	switch(ofs){
	case 3:
		break;
	case 2:
		dit[0]=0;
		dit[1]=1;
		dit[2]=2;
		dit[3]=3;
		break;
	case 1:
		dit[0]=3;
		dit[1]=1;
		dit[2]=0;
		dit[3]=2;
		break;
	case 0:
		dit[0]=0;
		dit[1]=2;
		dit[2]=3;
		dit[3]=1;
		break;
	}
	
	if (bits < 0){
		dit[0] >>= -bits;
		dit[1] >>= -bits;
		dit[2] >>= -bits;
		dit[3] >>= -bits;
	} else{
		dit[0] <<= bits;
		dit[1] <<= bits;
		dit[2] <<= bits;
		dit[3] <<= bits;
	}

	for(y=ibuf->y;y>0;y--){
		if(y & 1){
			add1=dit[0];
			add2=dit[1];
		}
		else{
			add1=dit[2];
			add2=dit[3];
		}
		for(x=ibuf->x;x>0;x--){
			pix = *rect;
			if (x & 1) pix += add1;
			else pix += add2;

			if (pix>255) pix=255;
			*rect = pix & and;
			rect += 4;
		}
	}
}

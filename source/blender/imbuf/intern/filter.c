/**
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
 * filter.c
 *
 * $Id$
 */

#include "BLI_blenlib.h"

#include "imbuf.h"
#include "imbuf_patch.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_filter.h"


/************************************************************************/
/*				FILTERS					*/
/************************************************************************/

static void filtrow(unsigned char *point, int x)
{
	unsigned int c1,c2,c3,error;

	if (x>1){
		c1 = c2 = *point;
		error = 2;
		for(x--;x>0;x--){
			c3 = point[4];
			c1 += (c2<<1) + c3 + error;
			error = c1 & 3;
			*point = c1 >> 2;
			point += 4;
			c1=c2;
			c2=c3;
		}
		*point = (c1 + (c2<<1) + c2 + error) >> 2;
	}
}


static void filtcolum(unsigned char *point, int y, int skip)
{
	unsigned int c1,c2,c3,error;
	unsigned char *point2;

	if (y>1){
		c1 = c2 = *point;
		point2 = point;
		error = 2;
		for(y--;y>0;y--){
			point2 += skip;
			c3 = *point2;
			c1 += (c2<<1) + c3 +error;
			error = c1 & 3;
			*point = c1 >> 2;
			point=point2;
			c1=c2;
			c2=c3;
		}
		*point = (c1 + (c2<<1) + c2 + error) >> 2;
	}
}


void IMB_filtery(struct ImBuf *ibuf)
{
	unsigned char *point;
	short x,y,skip;

	point = (unsigned char *)ibuf->rect;
	x = ibuf->x;
	y = ibuf->y;
	skip = x<<2;

	for (;x>0;x--){
		if (ibuf->depth > 24) filtcolum(point,y,skip);
		point++;
		filtcolum(point,y,skip);
		point++;
		filtcolum(point,y,skip);
		point++;
		filtcolum(point,y,skip);
		point++;
	}
}


void imb_filterx(struct ImBuf *ibuf)
{
	unsigned char *point;
	short x,y,skip;

	point = (unsigned char *)ibuf->rect;
	x = ibuf->x;
	y = ibuf->y;
	skip = (x<<2) - 3;

	for (;y>0;y--){
		if (ibuf->depth > 24) filtrow(point,x);
		point++;
		filtrow(point,x);
		point++;
		filtrow(point,x);
		point++;
		filtrow(point,x);
		point+=skip;
	}
}


void IMB_filter(struct ImBuf *ibuf)
{
	IMB_filtery(ibuf);
	imb_filterx(ibuf);
}

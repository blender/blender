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
 * allocimbuf.c
 *
 * $Id$
 */

#include "BLI_blenlib.h"

#include "imbuf.h"
#include "imbuf_patch.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"

void IMB_rectcpy(struct ImBuf *dbuf, struct ImBuf *sbuf, int destx, 
	int desty, int srcx, int srcy, int width, int height)
{
	unsigned int *drect, *srect;
	float *drectf = NULL;
	float *srectf = NULL;
	int tmp, do_float = 0;

	if (dbuf == NULL) return;
	
	if (sbuf && sbuf->rect_float && dbuf->rect_float) do_float = 1;

	if (destx < 0){
		srcx -= destx ;
		width += destx ;
		destx = 0;
	}
	if (srcx < 0){
		destx -= srcx ;
		width += destx ;
		srcx = 0;
	}
	if (desty < 0){
		srcy -= desty ;
		height += desty ;
		desty = 0;
	}
	if (srcy < 0){
		desty -= srcy ;
		height += desty ;
		srcy = 0;
	}

	tmp = dbuf->x - destx;
	if (width > tmp) width = tmp;
	tmp = dbuf->y  - desty;
	if (height > tmp) height = tmp;

	drect = dbuf->rect + desty * dbuf->x + destx;
	if (do_float) drectf = dbuf->rect_float + desty * dbuf->x + destx;
	destx = dbuf->x;

	if (sbuf){
		tmp = sbuf->x - srcx;
		if (width > tmp) width = tmp;
		tmp = sbuf->y - srcy;
		if (height > tmp) height = tmp;

		if (width <= 0) return;
		if (height <= 0) return;

		srect = sbuf->rect;
		srect += srcy * sbuf->x;
		srect += srcx;
		if (do_float) {
			srectf = sbuf->rect_float;
			srectf += srcy * sbuf->x;
			srectf += srcx;
		}
		srcx = sbuf->x;
	} else{
		if (width <= 0) return;
		if (height <= 0) return;

		srect = drect;
		srectf = drectf;
		srcx = destx;
	}

	for (;height > 0; height--){

		memcpy(drect,srect, width * sizeof(int));
		drect += destx;
		srect += srcx;

		if (do_float) {
			memcpy(drectf,srectf, width * sizeof(float) * 4);
			drectf += destx;
			srectf += srcx;
		}		
	}
}

void IMB_rectfill(struct ImBuf *drect, float col[4])
{
	int num;
	unsigned int *rrect = drect->rect;
	unsigned char *spot;

	num = drect->x * drect->y;
	for (;num > 0; num--) {
		spot = (unsigned char *)rrect;
		spot[0] = (int)(col[0]*255);
		spot[1] = (int)(col[1]*255);
		spot[2] = (int)(col[2]*255);
		spot[3] = (int)(col[3]*255);
		*rrect++;
	}
	if(drect->rect_float) {
		float *rrectf = drect->rect_float;
		
		num = drect->x * drect->y;
		for (;num > 0; num--) {
			*rrectf++ = col[0];
			*rrectf++ = col[1];
			*rrectf++ = col[2];
			*rrectf++ = col[3];
		}
	}	
}


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
 * rotate.c
 *
 * $Id$
 */

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "BLI_blenlib.h"

#include "imbuf.h"
#include "imbuf_patch.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_allocimbuf.h"

void IMB_flipy(struct ImBuf * ibuf)
{
	short x,y,backx;
	unsigned int *top,*bottom,temp;

	if (ibuf == 0) return;
	if (ibuf->rect == 0) return;

	x = ibuf->x;
	y = ibuf->y;
	backx = x<<1;

	top = ibuf->rect;
	bottom = top + ((y-1) * x);
	y >>= 1;

	for(;y>0;y--){
		for(x = ibuf->x; x > 0; x--){
			temp = *top;
			*(top++) = *bottom;
			*(bottom++) = temp;
		}
		bottom -= backx;
	}
}

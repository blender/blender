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
#include "IMB_divers.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void imb_checkncols(struct ImBuf *ibuf)
{
	unsigned int i;

	if (ibuf==0) return;
	
	if (IS_amiga(ibuf)){
		if (IS_ham(ibuf)){
			if (ibuf->depth == 0) ibuf->depth = 6;
			ibuf->mincol = 0;
			ibuf->maxcol = 1 << (ibuf->depth - 2);
			/*printf("%d %d\n", ibuf->maxcol, ibuf->depth);*/
			return;
		} else if (IS_hbrite(ibuf)){
			ibuf->mincol = 0;
			ibuf->maxcol = 64;
			ibuf->depth = 6;
			return;
		}
	}

	if (ibuf->maxcol == 0){
		if (ibuf->depth <= 8){
			ibuf->mincol = 0;
			ibuf->maxcol = (1 << ibuf->depth);
			return;
		} else if (ibuf->depth == 0){
			ibuf->depth = 5;
			ibuf->mincol = 0;
			ibuf->maxcol = 32;
		}
		return;
	} else {
		/* ibuf->maxcol defines the depth */
		for (i=1 ; ibuf->maxcol > (1 << i); i++);
		ibuf->depth = i;
		return;
	}
}


void IMB_de_interlace(struct ImBuf *ibuf)
{
	struct ImBuf * tbuf1, * tbuf2;
	
	if (ibuf == 0) return;
	if (ibuf->flags & IB_fields) return;
	ibuf->flags |= IB_fields;
	
	if (ibuf->rect) {
		/* make copies */
		tbuf1 = IMB_allocImBuf(ibuf->x, ibuf->y / 2, 32, IB_rect, 0);
		tbuf2 = IMB_allocImBuf(ibuf->x, ibuf->y / 2, 32, IB_rect, 0);
		
		ibuf->x *= 2;	
		IMB_rectop(tbuf1, ibuf, 0, 0, 0, 0, 32767, 32767, IMB_rectcpy, 0);
		IMB_rectop(tbuf2, ibuf, 0, 0, tbuf2->x, 0, 32767, 32767, IMB_rectcpy, 0);
	
		ibuf->x /= 2;
		IMB_rectop(ibuf, tbuf1, 0, 0, 0, 0, 32767, 32767, IMB_rectcpy, 0);
		IMB_rectop(ibuf, tbuf2, 0, tbuf2->y, 0, 0, 32767, 32767, IMB_rectcpy, 0);
		
		IMB_freeImBuf(tbuf1);
		IMB_freeImBuf(tbuf2);
	}
	ibuf->y /= 2;
}

void IMB_interlace(struct ImBuf *ibuf)
{
        struct ImBuf * tbuf1, * tbuf2;

        if (ibuf == 0) return;
        ibuf->flags &= ~IB_fields;

        ibuf->y *= 2;

        if (ibuf->rect) {
                /* make copies */
                tbuf1 = IMB_allocImBuf(ibuf->x, ibuf->y / 2, 32, IB_rect, 0);
                tbuf2 = IMB_allocImBuf(ibuf->x, ibuf->y / 2, 32, IB_rect, 0);

                IMB_rectop(tbuf1, ibuf, 0, 0, 0, 0, 32767, 32767, IMB_rectcpy, 
			0);
                IMB_rectop(tbuf2, ibuf, 0, 0, 0, tbuf2->y, 32767, 32767,
                	IMB_rectcpy,0);

                ibuf->x *= 2;
                IMB_rectop(ibuf, tbuf1, 0, 0, 0, 0, 32767, 32767, IMB_rectcpy,
			0);
                IMB_rectop(ibuf, tbuf2, tbuf2->x, 0, 0, 0, 32767, 32767,
                	IMB_rectcpy,0);
                ibuf->x /= 2;

                IMB_freeImBuf(tbuf1);
                IMB_freeImBuf(tbuf2);
        }
}


void IMB_gamwarp(struct ImBuf *ibuf, double gamma)
{
        uchar gam[256];
        int i;
        uchar *rect;

        if (ibuf == 0) return;
        if (ibuf->rect == 0) return;
        if (gamma == 1.0) return;

        gamma = 1.0 / gamma;
        for (i = 255 ; i >= 0 ; i--) gam[i] = (255.0 * pow(i / 255.0 ,
        gamma))  + 0.5;

        rect = (uchar *) ibuf->rect;
        for (i = ibuf->x * ibuf->y ; i>0 ; i--){
                rect ++;
                *rect ++ = gam[*rect];
                *rect ++ = gam[*rect];
                *rect ++ = gam[*rect];
        }
}

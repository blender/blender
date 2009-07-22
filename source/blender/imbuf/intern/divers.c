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
 * allocimbuf.c
 *
 * $Id$
 */

#include "BLI_blenlib.h"
#include "BLI_rand.h"

#include "imbuf.h"
#include "imbuf_patch.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_allocimbuf.h"
#include "IMB_divers.h"
#include "BKE_utildefines.h"
#include "BKE_colortools.h"

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
		IMB_rectcpy(tbuf1, ibuf, 0, 0, 0, 0, ibuf->x, ibuf->y);
		IMB_rectcpy(tbuf2, ibuf, 0, 0, tbuf2->x, 0, ibuf->x, ibuf->y);
	
		ibuf->x /= 2;
		IMB_rectcpy(ibuf, tbuf1, 0, 0, 0, 0, tbuf1->x, tbuf1->y);
		IMB_rectcpy(ibuf, tbuf2, 0, tbuf2->y, 0, 0, tbuf2->x, tbuf2->y);
		
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

		IMB_rectcpy(tbuf1, ibuf, 0, 0, 0, 0, ibuf->x, ibuf->y);
		IMB_rectcpy(tbuf2, ibuf, 0, 0, 0, tbuf2->y, ibuf->x, ibuf->y);

		ibuf->x *= 2;
		IMB_rectcpy(ibuf, tbuf1, 0, 0, 0, 0, tbuf1->x, tbuf1->y);
		IMB_rectcpy(ibuf, tbuf2, tbuf2->x, 0, 0, 0, tbuf2->x, tbuf2->y);
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
	float *rectf;

	if (ibuf == 0) return;
	if (gamma == 1.0) return;

	rect = (uchar *) ibuf->rect;
	rectf = ibuf->rect_float;

	gamma = 1.0 / gamma;

	if (rect) {
		for (i = 255 ; i >= 0 ; i--) 
			gam[i] = (255.0 * pow(i / 255.0 ,
					      gamma))  + 0.5;

		for (i = ibuf->x * ibuf->y ; i>0 ; i--, rect+=4){
			rect[0] = gam[rect[0]];
			rect[1] = gam[rect[1]];
			rect[2] = gam[rect[2]];
		}
	}

	if (rectf) {
		for (i = ibuf->x * ibuf->y ; i>0 ; i--, rectf+=4){
			rectf[0] = pow(rectf[0] / 255.0, gamma);
			rectf[1] = pow(rectf[1] / 255.0, gamma);
			rectf[2] = pow(rectf[2] / 255.0, gamma);
		}
	}
}


void IMB_rect_from_float(struct ImBuf *ibuf)
{
	/* quick method to convert floatbuf to byte */
	float *tof = (float *)ibuf->rect_float;
	float dither= ibuf->dither;
	float srgb[3];
	int i, channels= ibuf->channels;
	short profile= ibuf->profile;
	unsigned char *to = (unsigned char *) ibuf->rect;
	
	if(tof==NULL) return;
	if(to==NULL) {
		imb_addrectImBuf(ibuf);
		to = (unsigned char *) ibuf->rect;
	}
	
	if (profile == IB_PROFILE_SRGB && (channels == 3 || channels == 4)) {
		if(channels == 3) {
			for (i = ibuf->x * ibuf->y; i > 0; i--, to+=4, tof+=3) {
				srgb[0]= linearrgb_to_srgb(tof[0]);
				srgb[1]= linearrgb_to_srgb(tof[1]);
				srgb[2]= linearrgb_to_srgb(tof[2]);

				to[0] = FTOCHAR(srgb[0]);
				to[1] = FTOCHAR(srgb[1]);
				to[2] = FTOCHAR(srgb[2]);
				to[3] = 255;
			}
		}
		else if (channels == 4) {
			floatbuf_to_srgb_byte(tof, to, 0, ibuf->x, 0, ibuf->y, ibuf->x);
		}
	}
	else if(ELEM(profile, IB_PROFILE_NONE, IB_PROFILE_LINEAR_RGB) && (dither==0.0f || channels!=4)) {
		if(channels==1) {
			for (i = ibuf->x * ibuf->y; i > 0; i--, to+=4, tof++)
				to[1]= to[2]= to[3]= to[0] = FTOCHAR(tof[0]);
		}
		else if(channels==3) {
			for (i = ibuf->x * ibuf->y; i > 0; i--, to+=4, tof+=3) {
				to[0] = FTOCHAR(tof[0]);
				to[1] = FTOCHAR(tof[1]);
				to[2] = FTOCHAR(tof[2]);
				to[3] = 255;
			}
		}
		else {
			for (i = ibuf->x * ibuf->y; i > 0; i--, to+=4, tof+=4) {
				to[0] = FTOCHAR(tof[0]);
				to[1] = FTOCHAR(tof[1]);
				to[2] = FTOCHAR(tof[2]);
				to[3] = FTOCHAR(tof[3]);
			}
		}
	}
	else {
		float dither_value, col;
		dither= dither/255.0f;
		for (i = ibuf->x * ibuf->y; i > 0; i--) {
			dither_value = (BLI_frand()-0.5)*dither; 
			col= tof[0] + dither_value;
			to[0] = FTOCHAR(col);
			col= tof[1] + dither_value;
			to[1] = FTOCHAR(col);
			col= tof[2] + dither_value;
			to[2] = FTOCHAR(col);
			col= tof[3] + dither_value;
			to[3] = FTOCHAR(col);

			to += 4; 
			tof += 4;
		}
	}
}

void IMB_float_from_rect(struct ImBuf *ibuf)
{
	/* quick method to convert byte to floatbuf */
	float *tof = ibuf->rect_float;
	int i;
	unsigned char *to = (unsigned char *) ibuf->rect;
	
	if(to==NULL) return;
	if(tof==NULL) {
		imb_addrectfloatImBuf(ibuf);
		tof = ibuf->rect_float;
	}
	
	if (ibuf->profile == IB_PROFILE_SRGB) {
		/* convert from srgb to linear rgb */
		
		for (i = ibuf->x * ibuf->y; i > 0; i--) 
		{
			tof[0] = srgb_to_linearrgb(((float)to[0])*(1.0f/255.0f));
			tof[1] = srgb_to_linearrgb(((float)to[1])*(1.0f/255.0f));
			tof[2] = srgb_to_linearrgb(((float)to[2])*(1.0f/255.0f));
			tof[3] = ((float)to[3])*(1.0f/255.0f);
			to += 4; 
			tof += 4;
		}
	} else {
		for (i = ibuf->x * ibuf->y; i > 0; i--) 
		{
			tof[0] = ((float)to[0])*(1.0f/255.0f);
			tof[1] = ((float)to[1])*(1.0f/255.0f);
			tof[2] = ((float)to[2])*(1.0f/255.0f);
			tof[3] = ((float)to[3])*(1.0f/255.0f);
			to += 4; 
			tof += 4;
		}
	}
}


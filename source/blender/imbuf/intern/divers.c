/*
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
 * allocimbuf.c
 *
 * $Id$
 */

/** \file blender/imbuf/intern/divers.c
 *  \ingroup imbuf
 */


#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_allocimbuf.h"

#include "BKE_colortools.h"

#include "MEM_guardedalloc.h"

void IMB_de_interlace(struct ImBuf *ibuf)
{
	struct ImBuf * tbuf1, * tbuf2;
	
	if (ibuf == 0) return;
	if (ibuf->flags & IB_fields) return;
	ibuf->flags |= IB_fields;
	
	if (ibuf->rect) {
		/* make copies */
		tbuf1 = IMB_allocImBuf(ibuf->x, ibuf->y / 2, 32, IB_rect);
		tbuf2 = IMB_allocImBuf(ibuf->x, ibuf->y / 2, 32, IB_rect);
		
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
		tbuf1 = IMB_allocImBuf(ibuf->x, ibuf->y / 2, 32, IB_rect);
		tbuf2 = IMB_allocImBuf(ibuf->x, ibuf->y / 2, 32, IB_rect);

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


/* assume converting from linear float to sRGB byte */
void IMB_rect_from_float(struct ImBuf *ibuf)
{
	/* quick method to convert floatbuf to byte */
	float *tof = (float *)ibuf->rect_float;
//	int do_dither = ibuf->dither != 0.f;
	float dither= ibuf->dither / 255.0;
	float srgb[4];
	int i, channels= ibuf->channels;
	short profile= ibuf->profile;
	unsigned char *to = (unsigned char *) ibuf->rect;
	
	if(tof==NULL) return;
	if(to==NULL) {
		imb_addrectImBuf(ibuf);
		to = (unsigned char *) ibuf->rect;
	}
	
	if(channels==1) {
		for (i = ibuf->x * ibuf->y; i > 0; i--, to+=4, tof++)
			to[1]= to[2]= to[3]= to[0] = FTOCHAR(tof[0]);
	}
	else if (profile == IB_PROFILE_LINEAR_RGB) {
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
			if (dither != 0.f) {
				for (i = ibuf->x * ibuf->y; i > 0; i--, to+=4, tof+=4) {
					const float d = (BLI_frand()-0.5)*dither;
					
					srgb[0]= d + linearrgb_to_srgb(tof[0]);
					srgb[1]= d + linearrgb_to_srgb(tof[1]);
					srgb[2]= d + linearrgb_to_srgb(tof[2]);
					srgb[3]= d + tof[3]; 
					
					to[0] = FTOCHAR(srgb[0]);
					to[1] = FTOCHAR(srgb[1]);
					to[2] = FTOCHAR(srgb[2]);
					to[3] = FTOCHAR(srgb[3]);
				}
			} else {
				floatbuf_to_srgb_byte(tof, to, 0, ibuf->x, 0, ibuf->y, ibuf->x);
			}
		}
	}
	else if(ELEM(profile, IB_PROFILE_NONE, IB_PROFILE_SRGB)) {
		if(channels==3) {
			for (i = ibuf->x * ibuf->y; i > 0; i--, to+=4, tof+=3) {
				to[0] = FTOCHAR(tof[0]);
				to[1] = FTOCHAR(tof[1]);
				to[2] = FTOCHAR(tof[2]);
				to[3] = 255;
			}
		}
		else {
			if (dither != 0.f) {
				for (i = ibuf->x * ibuf->y; i > 0; i--, to+=4, tof+=4) {
					const float d = (BLI_frand()-0.5)*dither;
					float col[4];

					col[0]= d + tof[0];
					col[1]= d + tof[1];
					col[2]= d + tof[2];
					col[3]= d + tof[3];

					to[0] = FTOCHAR(col[0]);
					to[1] = FTOCHAR(col[1]);
					to[2] = FTOCHAR(col[2]);
					to[3] = FTOCHAR(col[3]);
				}
			} else {
				for (i = ibuf->x * ibuf->y; i > 0; i--, to+=4, tof+=4) {
					to[0] = FTOCHAR(tof[0]);
					to[1] = FTOCHAR(tof[1]);
					to[2] = FTOCHAR(tof[2]);
					to[3] = FTOCHAR(tof[3]);
				}
			}
		}
	}
	/* ensure user flag is reset */
	ibuf->userflags &= ~IB_RECT_INVALID;
}

static void imb_float_from_rect_nonlinear(struct ImBuf *ibuf, float *fbuf)
{
	float *tof = fbuf;
	int i;
	unsigned char *to = (unsigned char *) ibuf->rect;

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


static void imb_float_from_rect_linear(struct ImBuf *ibuf, float *fbuf)
{
	float *tof = fbuf;
	int i;
	unsigned char *to = (unsigned char *) ibuf->rect;

	for (i = ibuf->x * ibuf->y; i > 0; i--) 
	{
		tof[0] = srgb_to_linearrgb(((float)to[0])*(1.0f/255.0f));
		tof[1] = srgb_to_linearrgb(((float)to[1])*(1.0f/255.0f));
		tof[2] = srgb_to_linearrgb(((float)to[2])*(1.0f/255.0f));
		tof[3] = ((float)to[3])*(1.0f/255.0f);
		to += 4; 
		tof += 4;
	}
}

void IMB_float_from_rect(struct ImBuf *ibuf)
{
	/* quick method to convert byte to floatbuf */
	if(ibuf->rect==NULL) return;
	if(ibuf->rect_float==NULL) {
		if (imb_addrectfloatImBuf(ibuf) == 0) return;
	}
	
	/* Float bufs should be stored linear */

	if (ibuf->profile != IB_PROFILE_NONE) {
		/* if the image has been given a profile then we're working 
		 * with color management in mind, so convert it to linear space */
		imb_float_from_rect_linear(ibuf, ibuf->rect_float);
	} else {
		imb_float_from_rect_nonlinear(ibuf, ibuf->rect_float);
	}
}

/* no profile conversion */
void IMB_float_from_rect_simple(struct ImBuf *ibuf)
{
	if(ibuf->rect_float==NULL)
		imb_addrectfloatImBuf(ibuf);
	imb_float_from_rect_nonlinear(ibuf, ibuf->rect_float);
}

void IMB_convert_profile(struct ImBuf *ibuf, int profile)
{
	int ok= FALSE;
	int i;

	unsigned char *rct= (unsigned char *)ibuf->rect;
	float *rctf= ibuf->rect_float;

	if(ibuf->profile == profile)
		return;

	if(ELEM(ibuf->profile, IB_PROFILE_NONE, IB_PROFILE_SRGB)) { /* from */
		if(profile == IB_PROFILE_LINEAR_RGB) { /* to */
			if(ibuf->rect_float) {
				for (i = ibuf->x * ibuf->y; i > 0; i--, rctf+=4) {
					rctf[0]= srgb_to_linearrgb(rctf[0]);
					rctf[1]= srgb_to_linearrgb(rctf[1]);
					rctf[2]= srgb_to_linearrgb(rctf[2]);
				}
			}
			if(ibuf->rect) {
				for (i = ibuf->x * ibuf->y; i > 0; i--, rct+=4) {
					rct[0]= (unsigned char)((srgb_to_linearrgb((float)rct[0]/255.0f) * 255.0f) + 0.5f);
					rct[1]= (unsigned char)((srgb_to_linearrgb((float)rct[1]/255.0f) * 255.0f) + 0.5f);
					rct[2]= (unsigned char)((srgb_to_linearrgb((float)rct[2]/255.0f) * 255.0f) + 0.5f);
				}
			}
			ok= TRUE;
		}
	}
	else if (ibuf->profile == IB_PROFILE_LINEAR_RGB) { /* from */
		if(ELEM(profile, IB_PROFILE_NONE, IB_PROFILE_SRGB)) { /* to */
			if(ibuf->rect_float) {
				for (i = ibuf->x * ibuf->y; i > 0; i--, rctf+=4) {
					rctf[0]= linearrgb_to_srgb(rctf[0]);
					rctf[1]= linearrgb_to_srgb(rctf[1]);
					rctf[2]= linearrgb_to_srgb(rctf[2]);
				}
			}
			if(ibuf->rect) {
				for (i = ibuf->x * ibuf->y; i > 0; i--, rct+=4) {
					rct[0]= (unsigned char)((linearrgb_to_srgb((float)rct[0]/255.0f) * 255.0f) + 0.5f);
					rct[1]= (unsigned char)((linearrgb_to_srgb((float)rct[1]/255.0f) * 255.0f) + 0.5f);
					rct[2]= (unsigned char)((linearrgb_to_srgb((float)rct[2]/255.0f) * 255.0f) + 0.5f);
				}
			}
			ok= TRUE;
		}
	}

	if(ok==FALSE){
		printf("IMB_convert_profile: failed profile conversion %d -> %d\n", ibuf->profile, profile);
		return;
	}

	ibuf->profile= profile;
}

/* use when you need to get a buffer with a certain profile
 * if the return  */
float *IMB_float_profile_ensure(struct ImBuf *ibuf, int profile, int *alloc)
{
	/* stupid but it works like this everywhere now */
	const short is_lin_from= (ibuf->profile != IB_PROFILE_NONE);
	const short is_lin_to= (profile != IB_PROFILE_NONE);

	
	if(is_lin_from == is_lin_to) {
		*alloc= 0;

		/* simple case, just allocate the buffer and return */
		if(ibuf->rect_float == NULL) {
			IMB_float_from_rect(ibuf);
		}

		return ibuf->rect_float;
	}
	else {
		/* conversion is needed, first check */
		float *fbuf= MEM_mallocN(ibuf->x * ibuf->y * sizeof(float) * 4, "IMB_float_profile_ensure");
		*alloc= 1;

		if(ibuf->rect_float == NULL) {
			if(is_lin_to) {
				imb_float_from_rect_linear(ibuf, fbuf);
			}
			else {
				imb_float_from_rect_nonlinear(ibuf, fbuf);
			}
		}
		else {
			if(is_lin_to) { /* lin -> nonlin */
				linearrgb_to_srgb_rgba_rgba_buf(fbuf, ibuf->rect_float, ibuf->x * ibuf->y);
			}
			else { /* nonlin -> lin */
				srgb_to_linearrgb_rgba_rgba_buf(fbuf, ibuf->rect_float, ibuf->x * ibuf->y);
			}
		}

		return fbuf;
	}
}

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
 */

#include <stdio.h>
#include "plugin.h"

/* ******************** GLOBAL VARIABLES ***************** */


char name[24] = "Blur";

/* structure for buttons, 
 *  butcode      name           default  min  max  0
 */

VarStruct varstr[] = {
	LABEL,          "Input: 1 strip", 0.0, 0.0, 0.0, "",
	NUMSLI | FLO,     "Blur",     0.5,    0.0,    10.0, "Maximum filtersize",
	NUMSLI | FLO,     "Gamma",    1.0,    0.4,    2.0, "Gamma correction",
	TOG | INT,        "Animated", 0.0,    0.0,    1.0, "For (Ipo) animated blur",
	NUM | INT,            "debug",        0.0,    0.0,    2.0,
	"0:off 1: show primary blur buffer 2: show 2nd blur buffer",

};

/* The cast struct is for input in the main doit function
   Varstr and Cast must have the same variables in the same order */ 

typedef struct Cast {
	int dummy;          /* because of the 'label' button */
	float blur;
	float gamma;
	float use_ipo;
	int show;
} Cast;


/* cfra: the current frame */

float cfra;

void plugin_seq_doit(Cast *, float, float, int, int, ImBuf *, ImBuf *, ImBuf *, ImBuf *);

/* ******************** Fixed functions ***************** */

int plugin_seq_getversion(void) 
{
	return B_PLUGIN_VERSION;
}

void plugin_but_changed(int but) 
{
}

void plugin_init(void)
{
}

void plugin_getinfo(PluginInfo *info)
{
	info->name = name;
	info->nvars = sizeof(varstr) / sizeof(VarStruct);
	info->cfra = &cfra;

	info->varstr = varstr;

	info->init = plugin_init;
	info->seq_doit = (SeqDoit) plugin_seq_doit;
	info->callback = plugin_but_changed;
}


void blurbuf(struct ImBuf *ibuf, int nr, Cast *cast)
{
	/* nr = number of blurs */
	/* the ibuf->rect is replaced */
	struct ImBuf *tbuf, *ttbuf;
	int i, x4;
	
	tbuf = dupImBuf(ibuf);
	x4 = ibuf->x / 4;
	
	/* This doesn't seem to work... paprmh */
	if (cast->gamma != 1.0) gamwarp(tbuf, cast->gamma);

	/* reduce */
	for (i = 0; i < nr; i++) {
		ttbuf = onehalf(tbuf);
		if (ttbuf) {
			freeImBuf(tbuf);
			tbuf = ttbuf;
		}
		if (tbuf->x < 4 || tbuf->y < 4) break;
	}

	/* enlarge */
	for (i = 0; i < nr; i++) {
		ttbuf = double_x(tbuf);
		if (ttbuf) {
			freeImBuf(tbuf);
			tbuf = ttbuf;
		}
		ttbuf = double_y(tbuf);
		if (ttbuf) {
			freeImBuf(tbuf);
			tbuf = ttbuf;
		}
		if (tbuf->x > x4) {
			scaleImBuf(tbuf, ibuf->x, ibuf->y);
			break;
		}
	}
	
	/* this doesn't seem to work...paprmh*/
	if (cast->gamma != 1.0) gamwarp(tbuf, 1.0 / cast->gamma);

	if (ibuf->rect) memcpy(ibuf->rect, tbuf->rect, 4 * ibuf->x * ibuf->y);

	if (ibuf->rect_float)
		memcpy(ibuf->rect_float, tbuf->rect_float, 4 * ibuf->x * ibuf->y * sizeof(float));

	freeImBuf(tbuf);
	
}

void doblur(struct ImBuf *mbuf, float fac, Cast *cast)
{
	/* make two filtered images, like a mipmap structure
	 * fac is filtersize in pixels
	 */
	struct ImBuf *ibuf, *pbuf;
	float ifac, pfac, infac;
	int n, b1, b2;
	char *irect, *prect, *mrect;
	float *irectf, *prectf, *mrectf;
	
	/* which buffers ? */
				
	if (fac > 7.0) fac = 7.0;
	if (fac <= 1.0) return;
	
	pfac = 2.0;
	pbuf = dupImBuf(mbuf);
	n = 1;
	while (pfac < fac) {
		blurbuf(pbuf, n, cast);
		blurbuf(pbuf, n, cast);
		
		n++;
		pfac += 1.0;
	}

	ifac = pfac;
	pfac -= 1.0;

	ibuf = dupImBuf(pbuf);
	blurbuf(ibuf, n, cast);
	blurbuf(ibuf, n, cast);
	
	fac = (fac - pfac) / (ifac - pfac);
	n = mbuf->x * mbuf->y;
	
	if (cast->show) fac = cast->show - 1;
	
	if (mbuf->rect_float) {
		if (fac >= 1) {
			memcpy(mbuf->rect_float, ibuf->rect_float, 4 * n * sizeof(float));
		}
		else if (fac <= 0) {
			memcpy(mbuf->rect_float, pbuf->rect_float, 4 * n * sizeof(float));
		}
		else {  /* interpolate */
			infac = 1 - fac;
			
			irectf = (float *)ibuf->rect_float;
			prectf = (float *)pbuf->rect_float;
			mrectf = (float *)mbuf->rect_float;
			while (n--) {
				mrectf[0] = irectf[0] * fac + prectf[0] * infac;
				mrectf[1] = irectf[1] * fac + prectf[1] * infac;
				mrectf[2] = irectf[2] * fac + prectf[2] * infac;
				mrectf[3] = irectf[3] * fac + prectf[3] * infac;
				mrectf += 4;
				irectf += 4;
				prectf += 4;
			}
		}
	}	
	else if (mbuf->rect) {
		b1 = (int)fac * 255.0;
		if (b1 > 255) b1 = 255;
		b2 = 255 - b1;

		if (b1 == 255) {
			memcpy(mbuf->rect, ibuf->rect, 4 * n);
		}
		else if (b1 == 0) {
			memcpy(mbuf->rect, pbuf->rect, 4 * n);
		}
		else {  /* interpolate */
			irect = (char *)ibuf->rect;
			prect = (char *)pbuf->rect;
			mrect = (char *)mbuf->rect;
			while (n--) {
				mrect[0] = (irect[0] * b1 + prect[0] * b2) >> 8;
				mrect[1] = (irect[1] * b1 + prect[1] * b2) >> 8;
				mrect[2] = (irect[2] * b1 + prect[2] * b2) >> 8;
				mrect[3] = (irect[3] * b1 + prect[3] * b2) >> 8;
				mrect += 4;
				irect += 4;
				prect += 4;
			}
		}
	}

	freeImBuf(ibuf);
	freeImBuf(pbuf);
}


void plugin_seq_doit(Cast *cast, float facf0, float facf1, int x, int y, ImBuf *ibuf1, ImBuf *ibuf2, ImBuf *out, ImBuf *use)
{
	float bfacf0, bfacf1;
	
	if (cast->use_ipo == 0) {
		bfacf0 = bfacf1 = cast->blur + 1.0;
	}
	else {
		bfacf0 = (facf0 * 6.0) + 1.0;
		bfacf1 = (facf1 * 6.0) + 1.0;
	}

	if (out->rect) memcpy(out->rect, ibuf1->rect, 4 * out->x * out->y);
	if (out->rect_float) memcpy(out->rect_float, ibuf1->rect_float, 4 * out->x * out->y * sizeof(float));
	
/****************I can't get this field code to work... works ok without...paprmh****************/


	/* it blurs interlaced, only tested with even fields */

/*	de_interlace(out);*/
	/* otherwise scaling goes wrong */
/*	out->flags &= ~IB_fields;*/
	
	doblur(out, bfacf0, cast); /*fieldA*/

/*	if (out->rect)out->rect += out->x * out->y;
    if (out->rect_float)out->rect_float += out->x * out->y;
		
    doblur(out, bfacf1, cast);*/ /*fieldB*/

/*	if (out->rect)out->rect -= out->x * out->y;
    if (out->rect_float)out->rect_float -= out->x * out->y;
    out->flags |= IB_fields;

    interlace(out);*/
	
}


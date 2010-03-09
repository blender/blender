/**
 * cmap.c
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

#include <ctype.h>
#include "BLI_blenlib.h"

#include "imbuf.h"
#include "imbuf_patch.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "IMB_cmap.h"

static short *lastcube = 0;
static uchar *lastcoltab = 0;
static short lastmaxcol;
static short lastmincol;
static short lastcbits;
short alpha_col0 = FALSE;

extern void IMB_free_cache_limiter();

/*
 * there still is a bug here. If you want to convert an image to a 1 bit colormap you get
 * a black image. All conversion to less than 4 bits is too dark anyway.
 */

void IMB_freeImBufdata(void)
{
	if (lastcube) free(lastcube);
	lastcube= 0;
	if (lastcoltab) free(lastcoltab);
	lastcoltab= 0;
	IMB_free_cache_limiter();
}


int IMB_alpha_to_col0(int value)
{
	int old;
	
	old = alpha_col0;
	alpha_col0 = value;
	return (old);
}


void imb_losecmapbits(struct ImBuf *ibuf, unsigned int *coltab)
{
	int i,bits;
	unsigned int col, and1, and2, *rect;

	if (ibuf == 0) return;
	if (ibuf->rect == 0) return;
	if (ibuf->cbits == 0) return;
	if (ibuf->cbits >= 8) return;

/*
	bij cbits = 5:
	and1 = 11100000;
	bij cbits = 6:
	and1 = 11000000;
*/

	bits = ibuf->cbits;
	and1 = ((1 << (8-bits)) - 1) & 0xff;
	and1 |= (and1 << 24) + (and1 << 16) + (and1 << 8);
	and2 = ~and1;
	and1 <<= bits;

	rect = ibuf->rect;
	for (i = ibuf->x * ibuf->y ; i > 0; i--) {
		col = rect[0]; 
		*rect++ = col - ((col & and1) >> bits);
	}
	
	if (coltab){
		for (i = 0 ; i < ibuf->maxcol ; i++) {
			col = coltab[i];
			coltab[i] = (col - ((col & and1) >> bits)) & and2;
		}
	}
}


static void addcmapbits(struct ImBuf *ibuf)
{
	int i,bits;
	int div,mul;
	uchar * cmap;
	
	if (ibuf == 0) return;
	if (ibuf->cmap == 0) return;
	if (ibuf->cbits == 0) return;
	if (ibuf->cbits >= 8) return;

	bits = ibuf->cbits;

	/* bits = 4 -> div = 0xf0
	 * bits = 5 -> div = 0xf8
	 */
	 
	div = ((1 << bits) - 1) << (8 - bits);
	mul = 0xffff / div;
	
	if (ibuf->cmap){
		cmap = (uchar *) ibuf->cmap;
		for (i = 0 ; i < ibuf->maxcol ; i++){
			cmap[1] = (mul * cmap[1]) >> 8;
			cmap[2] = (mul * cmap[2]) >> 8;
			cmap[3] = (mul * cmap[3]) >> 8;
			cmap += 4;
		}
	}
}


static short addplanetocube(short *cube, short *plane, int minx, int miny, int sizep, int addcx, int addcy, int sizec, int col)
{
	short done = FALSE;
	int x, numx, numy, skipc, skipp, temp;

	/* clip first */

	numx = numy = sizep;

	temp = minx + sizep - 1;
	if (temp > sizec) numx -= temp - sizec;

	temp = miny + sizep - 1;
	if (temp > sizec) numy -= temp - sizec;

	if (minx < 0){
		plane -= minx;
		cube -= minx * addcx;
		numx += minx;
	}

	if (miny < 0){
		plane -= miny * sizep;
		cube -= miny * addcy;
		numy += miny;
	}

	skipc = addcy - (numx * addcx);
	skipp = sizep - numx;

	for (; numy > 0 ; numy--){
		for (x = numx ; x > 0; x--) {
			
			if (plane[0] < cube[1]) {
			
				cube[0] = col;
				cube[1] = plane[0];
				done = TRUE;
			}
			plane ++;
			cube += addcx;
		}
		plane += skipp;
		cube += skipc;
	}

	return (done);
}



short *imb_coldeltatab(unsigned char *coltab, short mincol, short maxcol, short cbits)
{
	short max, *quadr, *_quadr, *_cube, *cube, *_plane, done, nocol;
	unsigned int addcb, addcg, addcr, sizep;
	uchar *_colp, *colp, *col;
	int i, j, k, addcube;
	int r, g, b;

	max = (1 << cbits) - 1;
	nocol = maxcol - mincol;
	coltab += 4 * mincol;
	
	/* reduce colors to the right amount of bits */

	{
		unsigned int * lctab, and;

		lctab = (unsigned int *) coltab;
		and = max << (8 - cbits);
		and = and + (and << 8) + (and << 16) + (and << 24);
		for (i=nocol-1 ; i >= 0 ; i--) lctab[i] = (lctab[i] & and) >> (8 - cbits);
	}

	/* is this data the same as previous ? */

	if (lastcube){
		if (mincol == lastmincol && maxcol == lastmaxcol && cbits == lastcbits){
			if (lastcoltab){
				if (memcmp(lastcoltab, coltab, 4 * nocol) == 0) return(lastcube);
			}
		}
	}
	if (lastcube) free(lastcube);
	if (lastcoltab) free(lastcoltab);

	lastcube = 0; 
	lastcoltab = 0;
	_cube = malloc(2 * (1 << (3 * cbits)) * sizeof(short));
	_plane = malloc((2 * max + 1) * (2 * max + 1) * sizeof(short));
	_quadr = malloc((2 * max + 1) * sizeof(short));
	_colp = malloc(6 * nocol);

	if (_cube == 0 || _plane == 0 || _quadr == 0 || _colp == 0){
		if (_cube) free(_cube);
		if (_plane) free(_plane);
		if (_quadr) free(_quadr);
		if (_colp) free(_colp);
		return(0);
	}

	lastcoltab = malloc(4 * nocol);
	if (lastcoltab) memcpy(lastcoltab, coltab, 4 * nocol);
	lastcube = _cube;
	lastmincol = mincol;
	lastmaxcol = maxcol;
	lastcbits = cbits;

	/* cube initialise */

	cube = _cube;
	for (i = (1 << (3 * cbits)); i > 0 ; i--){
		cube[0] = 0;
		cube[1] = 32767;
		cube += 2;
	}

	/* mak error look up table */

	{
		unsigned int delta;

		quadr = _quadr + max + 1;
		quadr[0] = 0;
		delta = 3;
		for (i = 1 ; i <= max ; i++){
			quadr[i] = quadr[-i] = delta;
			delta += i + 3;
		}
	}

	/* colorplane initialise */

	for (i = 6 * nocol - 1; i >= 0; i--) _colp[i] = 1;

	addcr = 2;
	addcg = (addcr << cbits);
	addcb = (addcg << cbits);

	/* fill in first round */

	{
		unsigned int ofs;

		col = coltab;
		cube = _cube;

		for (i = 0 ; i < nocol ; i++){
			ofs = (col[3] * addcr) + (col[2] * addcg) + (col[1] * addcb);
			/* color been filled in -> then skip */
			if (cube[ofs + 1]) cube[ofs] = i + mincol;
			cube[ofs + 1] = 0;
			col += 4;
		}
	}

	for (i = 1; i <= max ; i++){
		colp = _colp;
		col = coltab;
		done = FALSE;
		sizep = 2*i +1;

		/* plane initialise */
		{
			unsigned int delta;
			short *plane;

			plane = _plane;
			for (j = -i ; j <= i; j++){
				delta = quadr[i] + quadr[j];
				for (k = -i; k <= i; k++){
					*plane++ = delta + quadr[k];
				}
			}
		}

		for (j = mincol; j < maxcol; j++){
			b = col[1] - i;
			g = col[2] - i;
			r = col[3] - i;
			
			addcube= (addcr * r) + (addcg * g) + (addcb * b);
			/* PRINT4(d, d, d, d, addcube, r, g, b); */
			/* if(addcube >= 2 * (1 << (3 * cbits))) { */
			/* 	printf("maxerror: %d %d\n", addcube, 2 * (1 << (3 * cbits))); */
				/* add_cube= 2 * (1 << (3 * cbits)) -1; */
			/* } */
			cube = _cube + addcube;

			if (colp[0]){
				if (b < 0) colp[0] = 0;
				else done |= colp[0] = addplanetocube(cube, _plane, r, g, sizep, addcr, addcg, max, j);
			}
			if (colp[1]){
				if (g < 0) colp[1] = 0;
				else done |= colp[1] = addplanetocube(cube, _plane, r, b, sizep, addcr, addcb, max, j);
			}
			if (colp[2]){
				if (r < 0) colp[2] = 0;
				else done |= colp[2] = addplanetocube(cube, _plane, b, g, sizep, addcb, addcg, max, j);
			}
			if (colp[3]){
				if ((b + sizep - 1) > max) colp[3] = 0;
				else done |= colp[3] = addplanetocube(cube + (sizep -1) * addcb, _plane, r, g, sizep, addcr,
					addcg, max, j);
			}
			if (colp[4]){
				if ((g + sizep - 1) > max) colp[4] = 0;
				else done |= colp[4] = addplanetocube(cube + (sizep -1) * addcg, _plane, r, b, sizep, addcr,
					addcb, max, j);
			}
			if (colp[5]){
				if ((r + sizep - 1) > max) colp[5] = 0;
				else done |= colp[5] = addplanetocube(cube + (sizep -1) * addcr, _plane, b, g, sizep, addcb,
					addcg, max, j);
			}

			colp += 6;
			col += 4;
		}
		if (done == 0) break;
	}

	free(_quadr);
	free(_plane);
	free(_colp);
	return(_cube);
}


static void convcmap(struct ImBuf* ibuf, short *deltab, short cbits)
{
	unsigned int *rect;
	short x,y;
	unsigned int col;
	unsigned int bbits,gbits,rbits;
	unsigned int bmask,gmask,rmask;

	bbits = 24 - 3 * cbits - 1;
	gbits = 16 - 2 * cbits - 1;
	rbits = 8 - cbits - 1;

	rmask = ((1 << cbits) - 1) << (8 - cbits);
	gmask = rmask << 8;
	bmask = gmask << 8;

	rect =(unsigned int *)ibuf->rect;

	for(y=ibuf->y;y>0;y--){
		for(x=ibuf->x;x>0;x--){
			col = *rect;
			col = ((col & bmask) >> bbits) + ((col & gmask) >> gbits) + ((col & rmask) >> rbits);
			*rect++ = deltab[col];
		}
	}
}

short IMB_converttocmap(struct ImBuf *ibuf)
{
	unsigned int *coltab;
	short *deltab=0, cbits;
	int i;
	int mincol, mask = 0;
	struct ImBuf * abuf = 0;
	unsigned int * rect, * arect;
	
	cbits = 5;
	if (ibuf->cmap == 0) return(0);
	
	if ((ibuf->cbits > 0) && (ibuf->cbits <8)) cbits = ibuf->cbits;

	coltab = calloc(ibuf->maxcol, sizeof(unsigned int));
	if (coltab == 0) return(0);
	memcpy(coltab, ibuf->cmap, ibuf->maxcol * sizeof(unsigned int));
	
	mincol = ibuf->mincol;	
	if (alpha_col0) {
		if (mincol == 0) mincol = 1;
		abuf = IMB_dupImBuf(ibuf);
	}
	
	imb_losecmapbits(ibuf, coltab);
	deltab = imb_coldeltatab((uchar *) coltab, mincol ,ibuf->maxcol, cbits);
	
	if (deltab == 0) {
		free(coltab);
		if (abuf) IMB_freeImBuf(abuf);
		return(0);
	}


	IMB_dit0(ibuf,1,cbits);
	IMB_dit0(ibuf,2,cbits);
	IMB_dit0(ibuf,3,cbits);
	convcmap(ibuf, deltab, cbits);
	
	if (abuf) {
		/* convert alpha to color 0 */
		rect = ibuf->rect;
		arect = abuf->rect;
		
		if (alpha_col0 == 1) mask = 0xff000000; /* alpha ==  0 -> 0 */
		if (alpha_col0 == 2) mask = 0x80000000; /* alpha < 128 -> 0 */
		
		for (i = ibuf->x * ibuf->y; i > 0; i--) {
			if ((*arect++ & mask) == 0) rect[0] = 0;
			rect++;
		}
		
		IMB_freeImBuf(abuf);
	}

	free(coltab);
	
	return (TRUE);
}


void imb_makecolarray(struct ImBuf *ibuf, unsigned char *mem, short nocols)
{
	short i,bits = 0;
	uchar *cmap;

	/* what's the theory behind this? */
	
	nocols = ibuf->maxcol;

	if (ibuf->cmap){
		cmap = (uchar *) ibuf->cmap;
		for (i = 0; i < nocols; i++){
			cmap[3] = mem[0];
			cmap[2] = mem[1];
			cmap[1] = mem[2];
			cmap[0] = 0;
			
			bits |= mem[0] | mem[1] | mem[2];
			mem += 3;
			cmap += 4;
		}

		/* patch voor AdPro II */
		if (IS_ham(ibuf)){
			i = ibuf->depth - 2;
			bits = ((1 << i) - 1) << (8 - i);
			for (i=0 ; i<nocols ; i++) ibuf->cmap[i] &= (bits << 24) + (bits << 16) + (bits << 8) + bits;
		}

		if ((bits & 0x1f) == 0){
			ibuf->cbits = 3;
		} else if ((bits & 0x0f) == 0){
			ibuf->cbits = 4;
		} else if ((bits & 0x07) == 0){
			ibuf->cbits = 5;
		} else if ((bits & 0x03) == 0){
			ibuf->cbits = 6;
		} else ibuf->cbits = 8;

		addcmapbits(ibuf);

		if (IS_hbrite(ibuf)){
			for (i=31;i>=0;i--){
				ibuf->cmap[i+32] =  (ibuf->cmap[i]  & 0xfefefefe) >> 1;
			}
		}
		
		if (IS_amiga(ibuf)){
			cmap = (uchar * ) (ibuf->cmap + 1);
			for (i = 1; i < nocols; i++){
				cmap[0] = 0xff;
				cmap += 4;
			}
		}
	}
}

/* temporal... rects now are rgba, cmaps are abgr */
#define SWITCH_INT(a)	{char s_i, *p_i; p_i= (char *)&(a); s_i= p_i[0]; p_i[0]= p_i[3]; p_i[3]= s_i; s_i= p_i[1]; p_i[1]= p_i[2]; p_i[2]= s_i; }

void IMB_applycmap(struct ImBuf *ibuf)
{
	unsigned int *rect, *cmap;
	int x, y, i, col, code;
	int *mask = 0;
	
	if (ibuf == 0) return;
	if (ibuf->rect == 0 || ibuf->cmap == 0) return;
	
	rect = ibuf->rect;
	cmap = ibuf->cmap;

	if (IS_ham(ibuf)){
		
		/* generate mask of max (8 + 2) bits */
		mask = malloc(1024 * 2 * sizeof(int));

		x = 1 << (ibuf->depth - 2);
		y = 65535 / (x - 1);
		
		for (i = 0; i < x; i++){
			mask[i] = 0;
			mask[i + x]	= 0x00ffff;
			mask[i + x + x] = 0xffff00;
			mask[i + x + x + x] = 0xff00ff;

			col = (y * i) >> 8;
			
			mask[i + 1024] = 0xff000000 | ibuf->cmap[i];
			mask[i + x + 1024] = 0xff000000 | col << 16;
			mask[i + x + x + 1024] = 0xff000000 | col;
			mask[i + x + x + x + 1024] = 0xff000000 | col << 8;
		}
		
		/* only color 0 transparant */
		mask[0+1024] =ibuf->cmap[0];
		
		for (y = ibuf->y ; y>0 ; y--){
			col = cmap[0];
			for (x=ibuf->x ; x>0 ; x--){
				code = *rect;
				*rect++ = col = (col & mask[code]) | mask[code + 1024];
			}
		}
		free(mask);
	} else {
	
		for(i = ibuf->x * ibuf->y; i>0; i--){
			col = *rect;
			if (col >= 0 && col < ibuf->maxcol) *rect = cmap[col];
			rect++;

			/* *(rect++) = cmap[*rect]; */
		}
	}
}

